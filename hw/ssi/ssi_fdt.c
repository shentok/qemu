/*
 * Flattened device tree SSI integration
 *
 * Copyright (C) 2024 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "qemu/osdep.h"
#include "hw/qdev-properties.h"
#include "hw/ssi/ssi_fdt.h"
#include "hw/ssi/ssi.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include <libfdt.h>

void ssi_fdt_populate(SysBusDevice *sbd, QDevFdtContext *context, int parent)
{
    BusState *bus = qdev_get_child_bus(DEVICE(sbd), "spi");
    const void *fdt = context->fdt;
    int child;

    fdt_for_each_subnode(child, fdt, parent) {
        const struct fdt_property *compatible;
        uint64_t address = qdev_fdt_get_reg_addr(fdt, child, 0);
        DeviceState *ssi;

        compatible = fdt_get_property(fdt, child, "compatible", NULL);

        if (!compatible) {
            continue;
        }

        ssi = qdev_try_new(compatible->data);
        if (ssi) {
            qdev_handle_device_tree_node_pre(ssi, child, context);
            qdev_prop_set_uint32(ssi, "cs", address);
            qdev_realize_and_unref(ssi, bus, &error_fatal);
            qdev_connect_gpio_out_named(DEVICE(sbd), "cs", address,
                                  qdev_get_gpio_in_named(ssi, SSI_GPIO_CS, 0));
        } else {
            qemu_log_mask(LOG_UNIMP, "Unimplemented device type %s\n",
                          compatible->data);
        }
    }
}
