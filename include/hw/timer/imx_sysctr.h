/*
 * i.MX SYSCTR Timer
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IMX_SYSCTR_H
#define IMX_SYSCTR_H

#include "hw/sysbus.h"
#include "system/memory.h"
#include "qemu/timer.h"
#include "qom/object.h"

/*
 * SYSCTR : System Counter
 *
 * This timer counts up continuously while it is enabled, resetting itself
 * to 0 when it reaches SYSCTR_TIMER_MAX (in freerun mode) or when it
 * reaches the value of one of the ocrX (in periodic mode).
 */

#define TYPE_IMX_SYSCTR "imx.sysctr"

OBJECT_DECLARE_SIMPLE_TYPE(IMXSysCtrState, IMX_SYSCTR)

struct IMXSysCtrState {
    SysBusDevice parent_obj;

    struct {
        MemoryRegion rd;
        MemoryRegion cmp;
        MemoryRegion ctrl;
    } iomem;

    uint64_t cntcv;
    uint32_t cntcr;

    struct {
        uint64_t cv;
        uint32_t cr;
        QEMUTimer *timer;
        qemu_irq irq;
    } cmp[2];

    uint32_t base_clk;
    uint32_t slow_clk;
};

static inline uint32_t imx_sysctr_base_freq(const IMXSysCtrState *s)
{
    return s->base_clk / 3;
}

#endif /* IMX_SYSCTR_H */
