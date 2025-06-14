/*
 * QEMU PowerPC E500 embedded processors CCSR space emulation
 *
 * Copyright (C) 2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Yu Liu,     <yu.liu@freescale.com>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of  the GNU General  Public License as published by
 * the Free Software Foundation;  either version 2 of the  License, or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "ppce500_ccsr.h"
#include "hw/qdev-dt-interface.h"
#include "trace.h"

static void ppce500_ccsr_handle_device_tree_node_post(DeviceState *dev,
                                                      int node,
                                                      QDevFdtContext *context)
{
    fdt_plaform_populate(SYS_BUS_DEVICE(dev), context, node);
}

static uint64_t ppce500_ccsr_io_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t value = 0;

    trace_ppce500_ccsr_io_read(addr, value, size);

    return value;
}

static void ppce500_ccsr_io_write(void *opaque, hwaddr addr, uint64_t value,
                                  unsigned size)
{
    trace_ppce500_ccsr_io_write(addr, value, size);
}

static const MemoryRegionOps ppce500_ccsr_ops = {
    .read = ppce500_ccsr_io_read,
    .write = ppce500_ccsr_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void ppce500_ccsr_init(Object *obj)
{
    PPCE500CCSRState *s = CCSR(obj);

    memory_region_init_io(&s->ccsr_space, obj, &ppce500_ccsr_ops, obj,
                          "e500-ccsr", MPC85XX_CCSRBAR_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->ccsr_space);
}

static void ppce500_ccsr_class_init(ObjectClass *klass, const void *data)
{
    DeviceDeviceTreeIfClass *dt = DEVICE_DT_IF_CLASS(klass);

    dt->handle_device_tree_node_post = ppce500_ccsr_handle_device_tree_node_post;
}

static const TypeInfo ppce500_ccsr_types[] = {
    {
        .name          = TYPE_CCSR,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(PPCE500CCSRState),
        .instance_init = ppce500_ccsr_init,
        .class_init    = ppce500_ccsr_class_init,
        .interfaces    = (InterfaceInfo[]) {
            { TYPE_DEVICE_DT_IF },
            { },
        },
    },
    {
        .name          = "fsl,p1020-immr",
        .parent        = TYPE_CCSR,
    },
    {
        .name          = "fsl,p1022-immr",
        .parent        = TYPE_CCSR,
    },
    {
        .name          = "fsl,mpc8544-immr",
        .parent        = TYPE_CCSR,
    },
};

DEFINE_TYPES(ppce500_ccsr_types)
