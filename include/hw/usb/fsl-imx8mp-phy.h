/*
 * i.MX 8M Plus USB PHY emulation
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_USB_FSLIMX8MPUSBPHY_H
#define HW_USB_FSLIMX8MPUSBPHY_H

#include "hw/sysbus.h"
#include "qom/object.h"
#include "system/memory.h"

#define TYPE_FSL_IMX8MP_USB_PHY "fsl-imx8mp-usb-phy"
OBJECT_DECLARE_SIMPLE_TYPE(FslImx8mpUsbPhyState, FSL_IMX8MP_USB_PHY)

#define FSL_IMX8MP_USB_PHY_NUM_SIZE (0x100 / 4)

struct FslImx8mpUsbPhyState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t data[FSL_IMX8MP_USB_PHY_NUM_SIZE];
};

#endif
