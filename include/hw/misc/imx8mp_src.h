/*
 * i.MX 8M Plus System Reset Controller
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FSL_IMX8MP_SRC_H
#define FSL_IMX8MP_SRC_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_IMX8MP_SRC "fsl-imx8mp-src"
OBJECT_DECLARE_SIMPLE_TYPE(FslImx8mpSrcState, IMX8MP_SRC)

#define FSL_IMX8MP_SRC_NUM_REGS (0x100 / 4)

struct FslImx8mpSrcState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t regs[FSL_IMX8MP_SRC_NUM_REGS];

};

#endif /* FSL_IMX8MP_SRC_H */
