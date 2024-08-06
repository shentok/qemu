#include "qemu/osdep.h"
#include "hw/core/qdev-dt-interface.h"
#include "hw/core/sysbus.h"
#include "qapi/error.h"
#include "qemu/log.h"

#include <libfdt.h>

void qdev_handle_device_tree_node_pre(DeviceState *dev, int node,
                                      QDevFdtContext *context)
{
    g_assert_not_reached();
}

void qdev_handle_device_tree_node_post(DeviceState *dev, int node,
                                       QDevFdtContext *context)
{
    g_assert_not_reached();
}

void machine_fdt_populate(SysBusDevice *sbd, const void *fdt)
{
    g_assert_not_reached();
}

void fdt_plaform_populate(SysBusDevice *sbd, QDevFdtContext *context, int parent)
{
    g_assert_not_reached();
}

int qdev_fdt_get_num_regs(const void *fdt, int node)
{
    g_assert_not_reached();
}

uint64_t qdev_fdt_get_reg_addr(const void *fdt, int node, int i)
{
    g_assert_not_reached();
}

uint64_t qemu_fdt_get_reg_size(const void *fdt, int node, int i)
{
    g_assert_not_reached();
}

int qemu_fdt_get_num_ranges(const void *fdt, int node)
{
    g_assert_not_reached();
}

uint64_t qemu_fdt_get_range_size(const void *fdt, int node, int i)
{
    g_assert_not_reached();
}

const struct fdt_property *fdt_get_property(const void *fdt, int nodeoffset,
                                            const char *name, int *lenp)
{
    g_assert_not_reached();
}

const char *fdt_get_name(const void *fdt, int nodeoffset, int *lenp)
{
    g_assert_not_reached();
}

static const TypeInfo types[] = {
    {
        .name       = TYPE_DEVICE_DT_IF,
        .parent     = TYPE_INTERFACE,
        .class_size = sizeof(DeviceDeviceTreeIfClass),
    },
};

DEFINE_TYPES(types)
