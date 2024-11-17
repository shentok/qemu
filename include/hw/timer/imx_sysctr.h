/*
 * i.MX SYSCTR Timer
 *
 * Copyright (c) 2008 OK Labs
 * Copyright (c) 2011 NICTA Pty Ltd
 * Originally written by Hans Jiang
 * Updated by Peter Chubb
 * Updated by Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef IMX_SYSCTR_H
#define IMX_SYSCTR_H

#include "hw/sysbus.h"
#include "exec/memory.h"
#include "qemu/timer.h"
#include "qom/object.h"

/*
 * SYSCTR : System Counter
 *
 * This timer counts up continuously while it is enabled, resetting itself
 * to 0 when it reaches SYSCTR_TIMER_MAX (in freerun mode) or when it
 * reaches the value of one of the ocrX (in periodic mode).
 */

#define TYPE_IMX8MP_SYSCTR "imx8mp.sysctr"

#define TYPE_IMX_SYSCTR "imx.sysctr"

OBJECT_DECLARE_SIMPLE_TYPE(IMXSysCtrState, IMX_SYSCTR)

struct IMXSysCtrState {
    SysBusDevice parent_obj;

    QEMUTimer *timer;
    MemoryRegion iomem;

    uint64_t start;
    uint32_t cntfid[3];
    uint32_t cmpcr;
    uint64_t cmpcv;

    uint32_t base_clk;
    uint32_t slow_clk;
    bool slow;

    qemu_irq irq;
};

#endif /* IMX_SYSCTR_H */
