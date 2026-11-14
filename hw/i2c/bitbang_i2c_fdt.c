/*
 * Bit-banged I2C device tree integration
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-dt-interface.h"
#include "hw/i2c/bitbang_i2c_fdt.h"
#include "hw/i2c/i2c_fdt.h"
#include "qemu/bswap.h"

#include <libfdt.h>

static void gpio_i2c_handle_device_tree_node_post(DeviceState *dev, int node,
                                                  QDevFdtContext *context)
{
    I2CBus *bus = I2C_BUS(qdev_get_child_bus(dev, "i2c"));
    const struct fdt_property *gpios;
    DeviceState *gpio_dev;
    int phandle, pin, domain;

    i2c_fdt_populate(bus, context, node);

    /*
     * The generic DT walker connects the GPIO controller's SDA/SCL
     * outputs to our inputs.  The bit-banged SDA line is bidirectional,
     * so feed our output back to the controller's SDA input as well.
     */
    gpios = fdt_get_property(context->fdt, node, "gpios", NULL);
    if (!gpios || be32_to_cpu(gpios->len) < 8) {
        return;
    }

    phandle = ldl_be_p(&gpios->data[0]);
    pin = ldl_be_p(&gpios->data[4]);
    domain = fdt_node_offset_by_phandle(context->fdt, phandle);
    if (domain < 0) {
        return;
    }

    gpio_dev = g_hash_table_lookup(context->device_map,
                                   GINT_TO_POINTER(domain));
    if (!gpio_dev) {
        return;
    }

    qdev_connect_gpio_out(dev, 0, qdev_get_gpio_in(gpio_dev, pin));
}

void gpio_i2c_fdt_class_init(ObjectClass *klass)
{
    DeviceDeviceTreeIfClass *dt = DEVICE_DT_IF_CLASS(klass);

    dt->handle_device_tree_node_post = gpio_i2c_handle_device_tree_node_post;
}
