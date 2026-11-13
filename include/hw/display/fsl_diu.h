/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Freescale Integrated Display Unit (DIU)
 */

#ifndef HW_DISPLAY_FSL_DIU_H
#define HW_DISPLAY_FSL_DIU_H

#include "hw/core/sysbus.h"
#include "qom/object.h"
#include "ui/console.h"

#define TYPE_FSL_DIU "fsl,diu"
OBJECT_DECLARE_SIMPLE_TYPE(FslDiuState, FSL_DIU)

#define FSL_DIU_MMIO_SIZE 0x3e8
#define FSL_DIU_NUM_REGS  (FSL_DIU_MMIO_SIZE / 4)

struct FslDiuState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    MemoryRegionSection fbsection;
    QemuConsole *con;
    qemu_irq irq;

    uint32_t regs[FSL_DIU_NUM_REGS];

    /* Decoded plane 0 area descriptor */
    bool invalidate;
    uint32_t fb_base;
    uint32_t fb_stride;
    uint32_t width;
    uint32_t height;
    uint8_t src_bpp;
    uint8_t red_off;
    uint8_t red_len;
    uint8_t green_off;
    uint8_t green_len;
    uint8_t blue_off;
    uint8_t blue_len;
};

#endif /* HW_DISPLAY_FSL_DIU_H */
