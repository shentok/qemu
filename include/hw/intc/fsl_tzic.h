/*
 * i.MX31 Vectored Interrupt Controller
 *
 * Note this is NOT the PL192 provided by ARM, but
 * a custom implementation by Freescale.
 *
 * Copyright (c) 2008 OKL
 * Copyright (c) 2011 NICTA Pty Ltd
 * Originally written by Hans Jiang
 * Updated by Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This code is licensed under the GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 *
 * TODO: implement vectors.
 */
#ifndef FSL_TZIC_H
#define FSL_TZIC_H

#include "hw/core/sysbus.h"
#include "system/memory.h"
#include "qom/object.h"

#define TYPE_FSL_TZIC "fsl_tzic"
OBJECT_DECLARE_SIMPLE_TYPE(FslTzicState, FSL_TZIC)

#define FSL_TZIC_NUM_IRQS 128

/* Interrupt Control Bits */
#define ABFLAG (1<<25)
#define ABFEN  (1<<24)
#define NIDIS  (1<<22) /* Normal Interrupt disable */
#define FIDIS  (1<<21) /* Fast interrupt disable */
#define NIAD   (1<<20) /* Normal Interrupt Arbiter Rise ARM level */
#define FIAD   (1<<19) /* Fast Interrupt Arbiter Rise ARM level */
#define NM     (1<<18) /* Normal interrupt mode */

#define PENDING_PER_WORD 32
#define PENDING_WORDS (FSL_TZIC_NUM_IRQS / PENDING_PER_WORD)

#define ENABLED_PER_WORD 32
#define ENABLED_WORDS (FSL_TZIC_NUM_IRQS / ENABLED_PER_WORD)

#define SECURE_PER_WORD 32
#define SECURE_WORDS (FSL_TZIC_NUM_IRQS / SECURE_PER_WORD)

#define PRIO_PER_WORD 4
#define PRIO_WORDS (FSL_TZIC_NUM_IRQS / PRIO_PER_WORD)

struct FslTzicState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
    qemu_irq fiq;

    uint32_t pending[PENDING_WORDS];
    uint32_t enabled[ENABLED_WORDS];
    uint32_t is_fiq[SECURE_WORDS];
    uint32_t prio[PRIO_WORDS];
    uint32_t intcntl;
    uint32_t intmask;
    uint32_t dsmint;
};

#endif /* FSL_TZIC_H */
