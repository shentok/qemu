/*
 * Flattened device tree I2C integration
 *
 * Copyright (C) 2024 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "qemu/osdep.h"
#include "hw/i2c/i2c_fdt.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include <libfdt.h>

void i2c_fdt_populate(I2CBus *i2c, QDevFdtContext *context, int parent)
{
    const void *fdt = context->fdt;
    int child;

    fdt_for_each_subnode(child, fdt, parent) {
        const struct fdt_property *compatible;
        uint64_t address = qdev_fdt_get_reg_addr(fdt, child, 0);
        I2CSlave *i2cs;

        compatible = fdt_get_property(fdt, child, "compatible", NULL);

        if (!compatible) {
            continue;
        }

        i2cs = I2C_SLAVE(qdev_try_new(compatible->data));
        if (i2cs) {
            qdev_handle_device_tree_node_pre(DEVICE(i2cs), child, context);
            qdev_prop_set_uint8(DEVICE(i2cs), "address", address);
            i2c_slave_realize_and_unref(i2cs, i2c, &error_fatal);
        } else {
            qemu_log_mask(LOG_UNIMP, "Unimplemented device type %s\n",
                          compatible->data);
        }
    }
}
