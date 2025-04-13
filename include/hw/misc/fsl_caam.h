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
#include "qemu/units.h"
#include "system/memory.h"

#define TYPE_FSL_CAAM "fsl-caam"
OBJECT_DECLARE_SIMPLE_TYPE(FslImxCaamState, FSL_CAAM)

#define FSL_CAAM_DATA_SIZE ((64 * KiB) / 4)

struct FslImxCaamState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t data[FSL_CAAM_DATA_SIZE];
};

#endif
