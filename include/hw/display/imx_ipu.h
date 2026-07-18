/*
 * i.MX Secure Real Time Clock
 *
 * This implementation is based on the following reference manual:
 * i.MX53 Multimedia Applications Processor Reference Manual
 * Document Number: iMX53RM, Rev. 2.1, 06/2012
 *
 * Copyright (c) 2026 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef IMX_IPU_H
#define IMX_IPU_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_IMX_IPUV3M "imx.ipuv3m"
#define TYPE_IMX_IPUV3H "imx.ipuv3h"
#define TYPE_IMX_IPU "imx.ipu"
OBJECT_DECLARE_SIMPLE_TYPE(ImxIpuState, IMX_IPU)

#define IMX_IPU_COMMON_SIZE 0x300
#define IMX_IPU_IDMAC_SIZE ((0x104 / 4) + 1)
#define IMX_IPU_CPMEM_SIZE (80 * 0x40)

struct ImxIpuState {
    SysBusDevice parent_obj;

    MemoryRegion io;
    MemoryRegion io_regs;
    MemoryRegionSection fbsection;
    qemu_irq irq_sync;
    QemuConsole *con;
    uint32_t fb_base;
    uint32_t src_width;
    uint32_t rows;
    bool invalidate;

    uint32_t common[IMX_IPU_COMMON_SIZE];
    uint32_t idmac[IMX_IPU_IDMAC_SIZE];
    uint32_t cpmem[IMX_IPU_CPMEM_SIZE];
};

#endif /* IMX_IPU_H */
