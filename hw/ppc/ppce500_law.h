/*
 * QEMU PowerPC e500v2 Local Access Window code
 *
 * Copyright (C) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PPCE500_LAW_H
#define PPCE500_LAW_H

#include "hw/sysbus.h"
#include "exec/memory.h"
#include "qom/object.h"

#define TYPE_E500_LAW "ppce500-law"
OBJECT_DECLARE_SIMPLE_TYPE(PPCE500LAWState, E500_LAW)

struct PPCE500LAWState {
    SysBusDevice parent;

    hwaddr ccsrbar_base;
    MemoryRegion mmio;
};

#endif /* PPCE500_LAW_H */
