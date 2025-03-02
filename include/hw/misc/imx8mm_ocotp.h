/*
 * i.MX8 M OTP emulation
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_MISC_FSLIMX8MMOCOTP_H
#define HW_MISC_FSLIMX8MMOCOTP_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "qemu/units.h"
#include "system/memory.h"

#define TYPE_FSL_IMX8MM_OCOTP "fsl-imx8mm-ocotp"
OBJECT_DECLARE_SIMPLE_TYPE(FslImx8mmOcotpState, FSL_IMX8MM_OCOTP)

#define FSL_IMX8MM_OCOTP_DATA_SIZE ((64 * KiB) / 4)

struct FslImx8mmOcotpState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t data[FSL_IMX8MM_OCOTP_DATA_SIZE];
};

#endif
