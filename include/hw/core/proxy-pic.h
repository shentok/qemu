/*
 * Proxy interrupt controller device.
 *
 * Copyright (c) 2022 Bernhard Beschow <shentey@gmail.com>
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

/* This is a simple device which has one GPIO input line and multiple
 * GPIO output lines. Any change on the input line is forwarded to all
 * of the outputs.
 *
 * QEMU interface:
 *  + one unnamed GPIO input: the input line
 *  + N unnamed GPIO outputs: the output lines
 *  + QOM property "num-lines": sets the number of output lines
 */
#ifndef HW_PROXY_PIC_H
#define HW_PROXY_PIC_H

#include "hw/qdev-core.h"
#include "qom/object.h"
#include "hw/irq.h"
#include "hw/isa/isa.h"

#define TYPE_PROXY_PIC "proxy-pic"
OBJECT_DECLARE_SIMPLE_TYPE(ProxyPICState, PROXY_PIC)

struct ProxyPICState {
    /*< private >*/
    struct DeviceState parent_obj;
    /*< public >*/

    qemu_irq in_irqs[ISA_NUM_IRQS];
    qemu_irq out_irqs[ISA_NUM_IRQS];
};

#endif /* HW_PROXY_PIC_H */
