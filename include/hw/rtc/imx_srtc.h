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
#ifndef IMX_SRTC_H
#define IMX_SRTC_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_IMX_SRTC "imx.srtc"
OBJECT_DECLARE_SIMPLE_TYPE(ImxSrtcState, IMX_SRTC)

struct ImxSrtcState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint64_t count_offset;
    uint32_t regs[15];
};

#endif /* IMX_SRTC_H */
