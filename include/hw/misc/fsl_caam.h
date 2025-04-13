/*
 * Freescale Cryptographic Acceleration and Assurance Module Emulation
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_FSLIMXCAAM_H
#define HW_MISC_FSLIMXCAAM_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "system/memory.h"

#define TYPE_FSL_CAAM "fsl-caam"
OBJECT_DECLARE_SIMPLE_TYPE(FslImxCaamState, FSL_CAAM)

#define FSL_CAAM_CTRL_ARRAY_SIZE (0x1000 / 4)
#define FSL_CAAM_JR_ARRAY_SIZE (0x600 / 4)

typedef struct FslImxCaamJrState {
    uint32_t data[FSL_CAAM_JR_ARRAY_SIZE];
    MemoryRegion io;
    MemoryRegion alias;
    qemu_irq irq;
} FslImxCaamJrState;

struct FslImxCaamState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    struct {
        MemoryRegion io;
        uint32_t data[FSL_CAAM_CTRL_ARRAY_SIZE];
        qemu_irq irq;
    } ctrl;

    FslImxCaamJrState jr[3];
};

#endif
