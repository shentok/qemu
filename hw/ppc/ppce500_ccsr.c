/*
 * QEMU PowerPC E500 embedded processors CCSR space emulation
 *
 * Copyright (C) 2009 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Yu Liu,     <yu.liu@freescale.com>
 *
 * This file is derived from hw/ppc440_bamboo.c,
 * the copyright for that material belongs to the original owners.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of  the GNU General  Public License as published by
 * the Free Software Foundation;  either version 2 of the  License, or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "e500-ccsr.h"
#include "trace.h"

static uint64_t ppce500_ccsr_io_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t value = 0;

    trace_ppce500_ccsr_io_read(addr, value, size);
    qemu_log_mask(LOG_UNIMP,
                  "%s: unimplemented [0x%" HWADDR_PRIx "] -> 0\n",
                  __func__, addr);

    return value;
}

static void ppce500_ccsr_io_write(void *opaque, hwaddr addr, uint64_t value,
                                  unsigned size)
{
    trace_ppce500_ccsr_io_write(addr, value, size);
    qemu_log_mask(LOG_UNIMP,
                  "%s: unimplemented [0x%" HWADDR_PRIx "] <- 0x%" PRIx32 "\n",
                  __func__, addr, (uint32_t)value);
}

static const MemoryRegionOps ppce500_ccsr_ops = {
    .read = ppce500_ccsr_io_read,
    .write = ppce500_ccsr_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void e500_ccsr_init(Object *obj)
{
    PPCE500CCSRState *ccsr = CCSR(obj);

    memory_region_init_io(&ccsr->ccsr_space, obj, &ppce500_ccsr_ops, obj,
                          "e500-ccsr", MPC8544_CCSRBAR_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(ccsr), &ccsr->ccsr_space);
}

static const TypeInfo types[] = {
    {
        .name          = TYPE_CCSR,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(PPCE500CCSRState),
        .instance_init = e500_ccsr_init,
    },
};

DEFINE_TYPES(types)
