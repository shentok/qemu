/*
 * i.MX 8M Plus DDR Controller
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FSL_IMX8MP_DDR_H
#define FSL_IMX8MP_DDR_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_IMX8MP_DDR "fsl-imx8mp-ddr"
OBJECT_DECLARE_SIMPLE_TYPE(FslImx8mpDdrState, IMX8MP_DDR)

struct FslImx8mpDdrState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
};

#endif /* FSL_IMX8MP_DDR_H */
