/*
 * QEMU PowerPC e500-based platforms
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
#include "e500-ccsr.h"
#include "hw/sysbus.h"

static void e500_ccsr_init(Object *obj)
{
    PPCE500CCSRState *ccsr = CCSR(obj);

    memory_region_init(&ccsr->ccsr_space, obj, "e500-ccsr",
                       MPC8544_CCSRBAR_SIZE);
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

DEFINE_TYPES(types);
