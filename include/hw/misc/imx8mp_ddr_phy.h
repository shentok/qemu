/*
 * i.MX 8M Plus DDR PHY
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FSL_IMX8MP_DDR_PHY_H
#define FSL_IMX8MP_DDR_PHY_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_IMX8MP_DDR_PHY "fsl-imx8mp-ddr-phy"
OBJECT_DECLARE_SIMPLE_TYPE(FslImx8mpDdrPhyState, IMX8MP_DDR_PHY)

struct FslImx8mpDdrPhyState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t reg_340010;
};

#endif /* FSL_IMX8MP_DDR_PHY_H */
