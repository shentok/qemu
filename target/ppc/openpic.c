/*
 * Freescale MPIC emulation
 *
 * Copyright (c) 2004-2007 Fabrice Bellard
 * Copyright (c) 2007 Jocelyn Mayer
 * Copyright (c) 2024 Bernhard Beschow <shentey@gmail.com>
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

#include "qemu/osdep.h"
#include "hw/ppc/openpic.h"
#include "hw/sysbus.h"
#include "qemu/error-report.h"
#include "target/ppc/cpu.h"

int openpic_connect_vcpu(DeviceState *dev, CPUState *cs)
{
    PowerPCCPU *cpu = POWERPC_CPU(cs);
    SysBusDevice *s = SYS_BUS_DEVICE(dev);
    IrqLines irqs;

    switch (PPC_INPUT(&cpu->env)) {
    case PPC_FLAGS_INPUT_6xx:
        irqs.irq[OPENPIC_OUTPUT_INT] =
            qdev_get_gpio_in(dev, PPC6xx_INPUT_INT);
        irqs.irq[OPENPIC_OUTPUT_CINT] =
            qdev_get_gpio_in(dev, PPC6xx_INPUT_INT);
        irqs.irq[OPENPIC_OUTPUT_MCK] =
            qdev_get_gpio_in(dev, PPC6xx_INPUT_MCP);
        /* Not connected ? */
        irqs.irq[OPENPIC_OUTPUT_DEBUG] = NULL;
        /* Check this */
        irqs.irq[OPENPIC_OUTPUT_RESET] =
            qdev_get_gpio_in(dev, PPC6xx_INPUT_HRESET);
        break;
    case PPC_FLAGS_INPUT_BookE:
        irqs.irq[OPENPIC_OUTPUT_INT] =
            qdev_get_gpio_in(DEVICE(cpu), PPCE500_INPUT_INT);
        irqs.irq[OPENPIC_OUTPUT_CINT] =
            qdev_get_gpio_in(DEVICE(cpu), PPCE500_INPUT_CINT);
        irqs.irq[OPENPIC_OUTPUT_MCK] =
            qdev_get_gpio_in(dev, PPCE500_INPUT_MCK);
        irqs.irq[OPENPIC_OUTPUT_DEBUG] = NULL;
        /* Check this */
        irqs.irq[OPENPIC_OUTPUT_RESET] =
            qdev_get_gpio_in(dev, PPCE500_INPUT_RESET_CORE);
        break;
#if defined(TARGET_PPC64)
    case PPC_FLAGS_INPUT_970:
        irqs.irq[OPENPIC_OUTPUT_INT] =
            qdev_get_gpio_in(dev, PPC970_INPUT_INT);
        irqs.irq[OPENPIC_OUTPUT_CINT] =
            qdev_get_gpio_in(dev, PPC970_INPUT_INT);
        irqs.irq[OPENPIC_OUTPUT_MCK] =
            qdev_get_gpio_in(dev, PPC970_INPUT_MCP);
        /* Not connected ? */
        irqs.irq[OPENPIC_OUTPUT_DEBUG] = NULL;
        /* Check this */
        irqs.irq[OPENPIC_OUTPUT_RESET] =
            qdev_get_gpio_in(dev, PPC970_INPUT_HRESET);
        break;
#endif /* defined(TARGET_PPC64) */
    default:
        error_report("Bus model currently not supported on openpic");
        exit(1);
    }

    for (int irq_no = 0; irq_no < OPENPIC_OUTPUT_NB; irq_no++) {
        sysbus_connect_irq(s, cs->cpu_index + irq_no, irqs.irq[irq_no]);
    }

    return 0;
}
