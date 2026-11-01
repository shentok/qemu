/*
 * QEMU USB MTP device tests
 *
 * Exercise the bulk-in data phase termination.  When a data container ends
 * exactly on a max-packet boundary and the host asks for more data (as UHCI
 * and EHCI do for every transfer descriptor but the last), the device must
 * answer with a zero-length packet.  Otherwise the host controller keeps
 * reading and the following response container becomes part of the data
 * phase, which makes the initiator time out.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/core/hotplug.h"
#include "hw/core/qdev-properties.h"
#include "hw/usb/usb.h"
#include "monitor/hmp.h"
#include "qom/object.h"

#define EP_DATA_IN   1
#define EP_DATA_OUT  2

#define MAX_PKT      64
#define QEMU_STORAGE_ID 0x00010001

#define CMD_OPEN_SESSION      0x1002
#define CMD_GET_OBJECT_HANDLES 0x1007
#define CMD_GET_OBJECT_INFO   0x1008
#define CMD_GET_OBJECT        0x1009

#define TYPE_COMMAND   1
#define TYPE_DATA      2
#define TYPE_RESPONSE  3

#define RES_OK         0x2001

typedef struct {
    uint32_t length;
    uint16_t type;
    uint16_t code;
    uint32_t trans;
} QEMU_PACKED mtp_container;

/*
 * The USB bus needs a parent device.  The default system bus is not
 * available in unit tests, so use a bare device as the host controller
 * stand-in.
 */
#define TYPE_TEST_USB_HOST "test-usb-host"

typedef struct {
    DeviceState parent_obj;
} TestUSBHost;

typedef struct {
    TestUSBHost host;
    USBBus bus;
    USBPort port;
    USBDevice *dev;
    char *dir;
    uint32_t trans;
} MTPTest;

static void test_port_attach(USBPort *port)
{
}

static void test_port_detach(USBPort *port)
{
}

static USBPortOps test_port_ops = {
    .attach = test_port_attach,
    .detach = test_port_detach,
};

/*
 * usb-bus.c references the HMP pretty-printer and the generic unplug
 * callback.  Neither is used by this test, so provide no-op implementations
 * instead of pulling in the monitor and the system hotplug code.
 */
int monitor_hmp_printf(MonitorHMP *mon, const char *fmt, ...)
{
    return 0;
}

void qdev_simple_device_unplug_cb(const HotplugHandler *hotplug_dev,
                                  DeviceState *dev, Error **errp)
{
    g_assert_not_reached();
}

static void mtp_test_setup(MTPTest *t, const char *name, const void *content,
                           size_t content_len)
{
    char tmpl[] = "/tmp/qemu-usb-mtp-XXXXXX";
    g_autofree char *path = NULL;
    g_autoptr(GError) err = NULL;

    t->dir = g_strdup(g_mkdtemp(tmpl));
    g_assert_nonnull(t->dir);
    path = g_build_filename(t->dir, name, NULL);
    g_assert_true(g_file_set_contents(path, content, content_len, &err));
    g_assert_no_error(err);

    object_initialize(&t->host, sizeof(t->host), TYPE_TEST_USB_HOST);
    usb_bus_new(&t->bus, sizeof(t->bus), NULL, DEVICE(&t->host));
    usb_register_port(&t->bus, &t->port, NULL, 0, &test_port_ops,
                      USB_SPEED_MASK_FULL);

    t->dev = USB_DEVICE(qdev_new("usb-mtp"));
    qdev_prop_set_string(DEVICE(t->dev), "rootdir", t->dir);
    qdev_prop_set_string(DEVICE(t->dev), "port", "1");
    usb_realize_and_unref(t->dev, &t->bus, &error_abort);
    usb_device_reset(t->dev);
    t->trans = 1;
}

static void mtp_test_teardown(MTPTest *t)
{
    const char *name;
    GDir *dir;

    /* Reset the device so it releases its file monitor and objects. */
    usb_device_reset(t->dev);
    object_unparent(OBJECT(t->dev));
    usb_bus_release(&t->bus);
    object_unref(OBJECT(&t->host));

    dir = g_dir_open(t->dir, 0, NULL);
    g_assert_nonnull(dir);
    while ((name = g_dir_read_name(dir)) != NULL) {
        g_autofree char *path = g_build_filename(t->dir, name, NULL);

        g_remove(path);
    }
    g_dir_close(dir);

    g_rmdir(t->dir);
    g_free(t->dir);
}

static void send_out(MTPTest *t, const void *data, size_t len)
{
    USBPacket p;
    USBEndpoint *ep = usb_ep_get(t->dev, USB_TOKEN_OUT, EP_DATA_OUT);

    usb_packet_init(&p);
    usb_packet_setup(&p, USB_TOKEN_OUT, ep, 0, 0, false, false);
    usb_packet_addbuf(&p, (void *)data, len);
    usb_handle_packet(t->dev, &p);

    g_assert_cmpint(p.status, ==, USB_RET_SUCCESS);
    g_assert_cmpuint(p.actual_length, ==, len);
    qemu_iovec_destroy(&p.iov);
}

static size_t read_in(MTPTest *t, void *buf, size_t len, bool short_not_ok)
{
    USBPacket p;
    USBEndpoint *ep = usb_ep_get(t->dev, USB_TOKEN_IN, EP_DATA_IN);
    size_t ret;

    usb_packet_init(&p);
    usb_packet_setup(&p, USB_TOKEN_IN, ep, 0, 0, short_not_ok, false);
    usb_packet_addbuf(&p, buf, len);
    usb_handle_packet(t->dev, &p);

    g_assert_cmpint(p.status, ==, USB_RET_SUCCESS);
    ret = p.actual_length;
    qemu_iovec_destroy(&p.iov);
    return ret;
}

static void send_command(MTPTest *t, uint16_t code, int nparams,
                         const uint32_t *params)
{
    uint8_t buf[12 + 5 * sizeof(uint32_t)];
    mtp_container c = {
        .length = cpu_to_le32(12 + nparams * sizeof(uint32_t)),
        .type   = cpu_to_le16(TYPE_COMMAND),
        .code   = cpu_to_le16(code),
        .trans  = cpu_to_le32(t->trans++),
    };
    int i;

    memcpy(buf, &c, sizeof(c));
    for (i = 0; i < nparams; i++) {
        stl_le_p(buf + 12 + i * sizeof(uint32_t), params[i]);
    }
    send_out(t, buf, 12 + nparams * sizeof(uint32_t));
}

static void read_response(MTPTest *t, uint16_t expect_code)
{
    uint8_t buf[MAX_PKT];
    size_t n = read_in(t, buf, sizeof(buf), false);

    g_assert_cmpuint(n, >=, sizeof(mtp_container));
    g_assert_cmpuint(lduw_le_p(buf + 4), ==, TYPE_RESPONSE);
    g_assert_cmpuint(lduw_le_p(buf + 6), ==, expect_code);
}

/*
 * Read a data container the way a UHCI host does for a multi-packet bulk-in
 * transfer: every packet is a full max-packet read with short_not_ok set.
 * If the container ends on a packet boundary, the device must send a ZLP;
 * otherwise the host would read the response container as data.
 */
static uint8_t *read_data(MTPTest *t, uint32_t *lenp, bool expect_zlp)
{
    uint8_t pkt[MAX_PKT];
    uint8_t *data = NULL;
    uint32_t len = 0, done = 0;
    size_t n;

    for (;;) {
        n = read_in(t, pkt, sizeof(pkt), true);
        if (data == NULL) {
            g_assert_cmpuint(n, >=, sizeof(mtp_container));
            g_assert_cmpuint(lduw_le_p(pkt + 4), ==, TYPE_DATA);
            len = ldl_le_p(pkt);
            data = g_malloc(len);
        }
        if (n == 0) {
            break;
        }
        g_assert_cmpuint(done + n, <=, len);
        memcpy(data + done, pkt, n);
        done += n;
        if (done == len) {
            break;
        }
    }

    g_assert_cmpuint(done, ==, len);
    if (expect_zlp) {
        n = read_in(t, pkt, sizeof(pkt), true);
        g_assert_cmpuint(n, ==, 0);
    }

    *lenp = len;
    return data;
}

static uint32_t get_single_handle(MTPTest *t)
{
    uint32_t params[3] = { QEMU_STORAGE_ID, 0, 0 };
    uint8_t *data;
    uint32_t len, count, handle;

    send_command(t, CMD_GET_OBJECT_HANDLES, ARRAY_SIZE(params), params);
    data = read_data(t, &len, false);
    g_assert_cmpuint(len, >=, 12 + 8);
    count = ldl_le_p(data + 12);
    g_assert_cmpuint(count, ==, 1);
    handle = ldl_le_p(data + 16);
    read_response(t, RES_OK);
    g_free(data);

    return handle;
}

static void test_object_info_zlp(void)
{
    MTPTest t;
    char name[30];
    char content[116];
    uint8_t *data;
    uint32_t len, handle;

    memset(name, 'a', sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    memset(content, 0x5a, sizeof(content));

    mtp_test_setup(&t, name, content, sizeof(content));

    send_command(&t, CMD_OPEN_SESSION, 1, (uint32_t[]) { 1 });
    read_response(&t, RES_OK);

    handle = get_single_handle(&t);

    /* ObjectInfo: 2 * strlen(name) + 134 == 192, a multiple of 64. */
    send_command(&t, CMD_GET_OBJECT_INFO, 1, &handle);
    data = read_data(&t, &len, true);
    g_assert_cmpuint(len, ==, 192);
    g_assert_cmpuint(data[64], ==, strlen(name) + 1);
    g_assert_cmpuint(lduw_le_p(data + 65), ==, 'a');
    read_response(&t, RES_OK);
    g_free(data);

    /* GetObject: 12 + sizeof(content) == 128, a multiple of 64. */
    send_command(&t, CMD_GET_OBJECT, 1, &handle);
    data = read_data(&t, &len, true);
    g_assert_cmpuint(len, ==, 12 + sizeof(content));
    g_assert_cmpmem(data + 12, sizeof(content), content, sizeof(content));
    read_response(&t, RES_OK);
    g_free(data);

    mtp_test_teardown(&t);
}

static void test_object_info_no_zlp(void)
{
    MTPTest t;
    char name[11];
    char content[53];
    uint8_t *data;
    uint32_t len, handle;

    memset(name, 'b', sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    memset(content, 0xa5, sizeof(content));

    mtp_test_setup(&t, name, content, sizeof(content));

    send_command(&t, CMD_OPEN_SESSION, 1, (uint32_t[]) { 1 });
    read_response(&t, RES_OK);

    handle = get_single_handle(&t);

    /* ObjectInfo: 2 * strlen(name) + 134 == 154, not a multiple of 64. */
    send_command(&t, CMD_GET_OBJECT_INFO, 1, &handle);
    data = read_data(&t, &len, false);
    g_assert_cmpuint(len, ==, 154);
    g_assert_cmpuint(data[64], ==, strlen(name) + 1);
    read_response(&t, RES_OK);
    g_free(data);

    /* GetObject: 12 + sizeof(content) == 65, not a multiple of 64. */
    send_command(&t, CMD_GET_OBJECT, 1, &handle);
    data = read_data(&t, &len, false);
    g_assert_cmpuint(len, ==, 12 + sizeof(content));
    g_assert_cmpmem(data + 12, sizeof(content), content, sizeof(content));
    read_response(&t, RES_OK);
    g_free(data);

    mtp_test_teardown(&t);
}

/*
 * Anonymous qdevs (those without a parent object) are attached to the
 * machine's "unattached" container when they are realized, so provide the
 * same fake machine that test-qdev.c uses.
 */
static void test_init_machine(void)
{
    Object *machine = object_property_add_new_container(object_get_root(),
                                                       "machine");

    object_property_add_new_container(machine, "unattached");
}

int main(int argc, char **argv)
{
    /*
     * hw/usb/bus.c makes the bus itself its hotplug handler, so the
     * interface type must exist.  hw/core/hotplug.c is not part of the
     * unit-test build (its helpers are stubbed out), so register the
     * interface here.
     */
    static const TypeInfo hotplug_handler_info = {
        .name       = TYPE_HOTPLUG_HANDLER,
        .parent     = TYPE_INTERFACE,
        .class_size = sizeof(HotplugHandlerClass),
    };
    static const TypeInfo test_usb_host_info = {
        .name          = TYPE_TEST_USB_HOST,
        .parent        = TYPE_DEVICE,
        .instance_size = sizeof(TestUSBHost),
    };

    g_test_init(&argc, &argv, NULL);

    module_call_init(MODULE_INIT_QOM);
    type_register_static(&hotplug_handler_info);
    type_register_static(&test_usb_host_info);
    test_init_machine();

    g_test_add_func("/usb-mtp/data-in/zlp", test_object_info_zlp);
    g_test_add_func("/usb-mtp/data-in/no-zlp", test_object_info_no_zlp);

    return g_test_run();
}
