/*
 * i.MX 8M Plus DDR Controller
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FSL_IMX8MP_DDR_H
#define FSL_IMX8MP_DDR_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_IMX8MP_DDR "fsl-imx8mp-ddr"
OBJECT_DECLARE_SIMPLE_TYPE(FslImx8mpDdrState, IMX8MP_DDR)

/*
 * The uMCTL2 register bank spans the base registers and the FSP1/FSP2
 * copies, which start at 0x2000 and 0x4000.
 */
#define IMX8MP_DDR_NUM_REGS      (0x6000 / sizeof(uint32_t))
#define IMX8MP_DDR_PERF_NUM_REGS (0x1000 / sizeof(uint32_t))

struct FslImx8mpDdrState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    MemoryRegion iomem_perf;

    uint32_t regs[IMX8MP_DDR_NUM_REGS];
    uint32_t perf_regs[IMX8MP_DDR_PERF_NUM_REGS];
};

#endif /* FSL_IMX8MP_DDR_H */
