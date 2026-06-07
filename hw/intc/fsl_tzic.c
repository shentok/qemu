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

#include "qemu/osdep.h"
#include "hw/intc/fsl_tzic.h"
#include "hw/core/irq.h"
#include "hw/core/registerfields.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "trace.h"

REG32(TZIC_INTCTRL,   0x0000)
REG32(TZIC_INTTYPE,   0x0004)
REG32(TZIC_PRIOMASK,  0x000c)
REG32(TZIC_SYNCCTRL,  0x0010)

REG32(TZIC_DSMINT,    0x0014)
    FIELD(TZIC_DSMINT, DSM, 0, 1)

/* Interrupt Security Registers */
REG32(TZIC_INTSEC0,   0x0080)
REG32(TZIC_INTSEC3,   0x008c)

/* Enable Set Registers */
REG32(TZIC_ENSET0,    0x0100)
REG32(TZIC_ENSET3,    0x010c)

/* Enable Clear Registers */
REG32(TZIC_ENCLEAR0,  0x0180)
REG32(TZIC_ENCLEAR3,  0x018c)

/* Source Set Registers */
REG32(TZIC_SRCSET0,   0x0200)
REG32(TZIC_SRCSET3,   0x020c)

/* Source Clear Registers */
REG32(TZIC_SRCCLEAR0, 0x0280)
REG32(TZIC_SRCCLEAR3, 0x028c)

/* Priority Registers (0–31) */
REG32(TZIC_PRIORITY0, 0x0400)
REG32(TZIC_PRIORITY31,0x047c)

/* Pending Registers */
REG32(TZIC_PND0,      0x0d00)
REG32(TZIC_PND3,      0x0d0c)

/* High Priority Pending Registers */
REG32(TZIC_HIPND0,    0x0d80)
REG32(TZIC_HIPND3,    0x0d8c)

/* Wakeup Registers */
REG32(TZIC_WAKEUP0,   0x0e00)
REG32(TZIC_WAKEUP3,   0x0e0c)

/* Software Interrupt Trigger */
REG32(TZIC_SWINT,     0x0f00)

static const char *fsl_tzic_regname(hwaddr offset)
{
    switch (offset) {

    case 0x0000: return "TZIC_INTCTRL";
    case 0x0004: return "TZIC_INTTYPE";
    case 0x000c: return "TZIC_PRIOMASK";
    case 0x0010: return "TZIC_SYNCCTRL";
    case 0x0014: return "TZIC_DSMINT";

    case 0x0080: return "TZIC_INTSEC0";
    case 0x0084: return "TZIC_INTSEC1";
    case 0x0088: return "TZIC_INTSEC2";
    case 0x008c: return "TZIC_INTSEC3";

    case 0x0100: return "TZIC_ENSET0";
    case 0x0104: return "TZIC_ENSET1";
    case 0x0108: return "TZIC_ENSET2";
    case 0x010c: return "TZIC_ENSET3";

    case 0x0180: return "TZIC_ENCLEAR0";
    case 0x0184: return "TZIC_ENCLEAR1";
    case 0x0188: return "TZIC_ENCLEAR2";
    case 0x018c: return "TZIC_ENCLEAR3";

    case 0x0200: return "TZIC_SRCSET0";
    case 0x0204: return "TZIC_SRCSET1";
    case 0x0208: return "TZIC_SRCSET2";
    case 0x020c: return "TZIC_SRCSET3";

    case 0x0280: return "TZIC_SRCCLEAR0";
    case 0x0284: return "TZIC_SRCCLEAR1";
    case 0x0288: return "TZIC_SRCCLEAR2";
    case 0x028c: return "TZIC_SRCCLEAR3";

    case 0x0400: return "TZIC_PRIORITY0";
    case 0x0404: return "TZIC_PRIORITY1";
    case 0x0408: return "TZIC_PRIORITY2";
    case 0x040c: return "TZIC_PRIORITY3";
    case 0x0410: return "TZIC_PRIORITY4";
    case 0x0414: return "TZIC_PRIORITY5";
    case 0x0418: return "TZIC_PRIORITY6";
    case 0x041c: return "TZIC_PRIORITY7";
    case 0x0420: return "TZIC_PRIORITY8";
    case 0x0424: return "TZIC_PRIORITY9";
    case 0x0428: return "TZIC_PRIORITY10";
    case 0x042c: return "TZIC_PRIORITY11";
    case 0x0430: return "TZIC_PRIORITY12";
    case 0x0434: return "TZIC_PRIORITY13";
    case 0x0438: return "TZIC_PRIORITY14";
    case 0x043c: return "TZIC_PRIORITY15";
    case 0x0440: return "TZIC_PRIORITY16";
    case 0x0444: return "TZIC_PRIORITY17";
    case 0x0448: return "TZIC_PRIORITY18";
    case 0x044c: return "TZIC_PRIORITY19";
    case 0x0450: return "TZIC_PRIORITY20";
    case 0x0454: return "TZIC_PRIORITY21";
    case 0x0458: return "TZIC_PRIORITY22";
    case 0x045c: return "TZIC_PRIORITY23";
    case 0x0460: return "TZIC_PRIORITY24";
    case 0x0464: return "TZIC_PRIORITY25";
    case 0x0468: return "TZIC_PRIORITY26";
    case 0x046c: return "TZIC_PRIORITY27";
    case 0x0470: return "TZIC_PRIORITY28";
    case 0x0474: return "TZIC_PRIORITY29";
    case 0x0478: return "TZIC_PRIORITY30";
    case 0x047c: return "TZIC_PRIORITY31";

    case 0x0d00: return "TZIC_PND0";
    case 0x0d04: return "TZIC_PND1";
    case 0x0d08: return "TZIC_PND2";
    case 0x0d0c: return "TZIC_PND3";

    case 0x0d80: return "TZIC_HIPND0";
    case 0x0d84: return "TZIC_HIPND1";
    case 0x0d88: return "TZIC_HIPND2";
    case 0x0d8c: return "TZIC_HIPND3";

    case 0x0e00: return "TZIC_WAKEUP0";
    case 0x0e04: return "TZIC_WAKEUP1";
    case 0x0e08: return "TZIC_WAKEUP2";
    case 0x0e0c: return "TZIC_WAKEUP3";

    case 0x0f00: return "TZIC_SWINT";

    default:
        return "UNKNOWN";
    }
}

static inline int fsl_tzic_prio(const FslTzicState *s, int irq)
{
    uint32_t word = irq / PRIO_PER_WORD;
    uint32_t part = 4 * (irq % PRIO_PER_WORD);
    return 0xf & (s->prio[word] >> part);
}

/* Update interrupts.  */
static void fsl_tzic_update(FslTzicState *s)
{
    bool is_set;

    is_set = false;
    for (int i = 0; i < PENDING_WORDS; i++) {
        is_set |= !!(s->pending[i] & s->enabled[i] & ~s->is_fiq[i]);
    }
    if (is_set) {
        s->dsmint &= ~R_TZIC_DSMINT_DSM_MASK;
    }
    qemu_set_irq(s->fiq, is_set);

    is_set = false;
    for (int i = 0; i < PENDING_WORDS; i++) {
        is_set |= !!(s->pending[i] & s->enabled[i] & s->is_fiq[i]);
    }
    if (is_set) {
        s->dsmint &= ~R_TZIC_DSMINT_DSM_MASK;
    }
    if (!is_set || s->intmask == 0x1f) {
        qemu_set_irq(s->irq, is_set);
        return;
    }

    /*
     * Take interrupt if there's a pending interrupt with
     * priority higher than the value of intmask
     */
    for (int i = 0; i < FSL_TZIC_NUM_IRQS; i++) {
        is_set = !!(s->pending[i] & s->enabled[i] & s->is_fiq[i]
                    & BIT(i % PENDING_PER_WORD));
        if (is_set && fsl_tzic_prio(s, i) > s->intmask) {
            qemu_set_irq(s->irq, 1);
            return;
        }
    }
    qemu_set_irq(s->irq, 0);
}

static void fsl_tzic_set_irq(void *opaque, int irq, int level)
{
    FslTzicState *s = opaque;
    uint8_t word = irq / PENDING_PER_WORD;
    uint8_t index = irq % PENDING_PER_WORD;

    if (level) {
        trace_fsl_tzic_irq_raise(irq, fsl_tzic_prio(s, irq));
        s->pending[word] |= BIT(index);
    } else {
        trace_fsl_tzic_irq_clear(irq, fsl_tzic_prio(s, irq));
        s->pending[word] &= ~BIT(index);
    }

    fsl_tzic_update(s);
}

static uint64_t fsl_tzic_read(void *opaque, hwaddr offset, unsigned size)
{
    FslTzicState *s = opaque;
    uint16_t reg = offset >> 2;
    uint64_t val = 0;

    switch (reg) {
    case R_TZIC_INTCTRL:
        val = s->intcntl;
        break;

    case R_TZIC_INTTYPE:
        val = 0x403;
        break;

    case R_TZIC_PRIOMASK:
        val = s->intmask;
        break;

    case R_TZIC_INTSEC0 ... R_TZIC_INTSEC3:
        val = s->is_fiq[reg - R_TZIC_INTSEC0];
        break;

    case R_TZIC_ENSET0 ... R_TZIC_ENSET3:
        val = s->enabled[reg - R_TZIC_ENSET0];
        break;

    case R_TZIC_ENCLEAR0 ... R_TZIC_ENCLEAR3:
        val = s->enabled[reg - R_TZIC_ENCLEAR0];
        break;

    case R_TZIC_PRIORITY0 ... R_TZIC_PRIORITY31:
        val = s->prio[reg - R_TZIC_PRIORITY0];
        break;

    case R_TZIC_PND0 ... R_TZIC_PND3:
    {
#if 0
        uint8_t word = reg - R_TZIC_PND0;
        uint32_t flags = s->pending[word] & s->enabled[word];
        if (!arm_is_secure()) {
            flags &= ~s->is_fiq[word];
        }
        int i = ctz64(flags);
        if (i < 64) {
            fsl_tzic_set_irq(opaque, i, 0);
            val = i;
        } else {
            val = 0xffffffffULL;
        }
#endif
        break;
    }

    case R_TZIC_HIPND0 ... R_TZIC_HIPND3:
    {
        uint8_t word = reg - R_TZIC_HIPND0;
        uint32_t flags = s->pending[word] & s->enabled[word];
#if 0
        int prio = -1;
        int irq = -1;
        for (int i = FSL_TZIC_NUM_IRQS - 1; i >= 0; --i) {
            if (flags & (1ULL<<i)) {
                int irq_prio = fsl_tzic_prio(s, i);
                if (irq_prio > prio) {
                    irq = i;
                    prio = irq_prio;
                }
            }
        }
        if (irq >= 0) {
            fsl_tzic_set_irq(s, irq, 0);
            val = (irq << 16) | prio;
        } else {
            val = 0xffffffffULL;
        }
#endif
        val = flags;
        break;
    }

    case R_TZIC_DSMINT:
        val = s->dsmint;
        break;

    case R_TZIC_SYNCCTRL:
    case R_TZIC_SRCSET0 ... R_TZIC_SRCSET3:
    case R_TZIC_SRCCLEAR0 ... R_TZIC_SRCCLEAR3:
    case R_TZIC_WAKEUP0 ... R_TZIC_WAKEUP3:
    case R_TZIC_SWINT:
        val = 0;
        qemu_log_mask(LOG_UNIMP,
                      "%s: Reading from register %s is uniplemented\n",
                      TYPE_FSL_TZIC, fsl_tzic_regname(offset));
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_FSL_TZIC, __func__, offset);
        break;
    }

    trace_fsl_tzic_read(offset, fsl_tzic_regname(offset), val);

    return val;
}

static void fsl_tzic_write(void *opaque, hwaddr offset,
                           uint64_t val, unsigned size)
{
    FslTzicState *s = opaque;
    uint16_t reg = offset >> 2;

    trace_fsl_tzic_write(offset, fsl_tzic_regname(offset), val);

    switch (reg) {
    case R_TZIC_INTCTRL:
        s->intcntl = val & (ABFEN | NIDIS | FIDIS | NIAD | FIAD | NM);
        if (s->intcntl & ABFEN) {
            s->intcntl &= ~(val & ABFLAG);
        }
        break;

    case R_TZIC_PRIOMASK:
        s->intmask = val & 0x1f;
        break;

    case R_TZIC_INTSEC0 ... R_TZIC_INTSEC3:
        s->is_fiq[reg - R_TZIC_INTSEC0] = val;
        break;

    case R_TZIC_ENSET0 ... R_TZIC_ENSET3:
        s->enabled[reg - R_TZIC_ENSET0] |= val;
        break;

    case R_TZIC_ENCLEAR0 ... R_TZIC_ENCLEAR3:
        s->enabled[reg - R_TZIC_ENCLEAR0] &= ~val;
        break;

    case R_TZIC_PRIORITY0 ... R_TZIC_PRIORITY31:
        s->prio[reg - R_TZIC_PRIORITY0] = val;
        break;

        /* Read-only registers, writes ignored */
    case R_TZIC_INTTYPE:
    case R_TZIC_PND0 ... R_TZIC_PND3:
    case R_TZIC_HIPND0 ... R_TZIC_HIPND3:
        return;

    case R_TZIC_DSMINT:
        s->dsmint = val & R_TZIC_DSMINT_DSM_MASK;
        break;

    case R_TZIC_SYNCCTRL:
    case R_TZIC_SRCSET0 ... R_TZIC_SRCSET3:
    case R_TZIC_SRCCLEAR0 ... R_TZIC_SRCCLEAR3:
    case R_TZIC_WAKEUP0 ... R_TZIC_WAKEUP3:
    case R_TZIC_SWINT:
        qemu_log_mask(LOG_UNIMP,
                      "%s: Writing to register %s is uniplemented\n",
                      TYPE_FSL_TZIC, fsl_tzic_regname(offset));
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_FSL_TZIC, __func__, offset);
    }

    fsl_tzic_update(s);
}

static const MemoryRegionOps fsl_tzic_ops = {
    .read = fsl_tzic_read,
    .write = fsl_tzic_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void fsl_tzic_reset(DeviceState *dev)
{
    FslTzicState *s = FSL_TZIC(dev);

    memset(s->pending, 0, sizeof s->pending);
    memset(s->enabled, 0, sizeof s->enabled);
    memset(s->is_fiq, 0, sizeof s->is_fiq);
    memset(s->prio, 0, sizeof s->prio);
    s->intmask = 0;
    s->intcntl = 0;
    s->dsmint = 0;
}

static void fsl_tzic_init(Object *obj)
{
    DeviceState *dev = DEVICE(obj);
    FslTzicState *s = FSL_TZIC(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &fsl_tzic_ops, s,
                          TYPE_FSL_TZIC, 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);

    qdev_init_gpio_in(dev, fsl_tzic_set_irq, FSL_TZIC_NUM_IRQS);
    sysbus_init_irq(sbd, &s->irq);
    sysbus_init_irq(sbd, &s->fiq);
}

static const VMStateDescription vmstate_fsl_tzic = {
    .name = TYPE_FSL_TZIC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(pending, FslTzicState, PENDING_WORDS),
        VMSTATE_UINT32_ARRAY(enabled, FslTzicState, ENABLED_WORDS),
        VMSTATE_UINT32_ARRAY(is_fiq, FslTzicState, SECURE_WORDS),
        VMSTATE_UINT32_ARRAY(prio, FslTzicState, PRIO_WORDS),
        VMSTATE_UINT32(intcntl, FslTzicState),
        VMSTATE_UINT32(intmask, FslTzicState),
        VMSTATE_UINT32(dsmint, FslTzicState),
        VMSTATE_END_OF_LIST()
    },
};

static void fsl_tzic_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_fsl_tzic;
    device_class_set_legacy_reset(dc, fsl_tzic_reset);
    dc->desc = "i.MX Advanced Vector Interrupt Controller";
}

static const TypeInfo fsl_tzic_info = {
    .name = TYPE_FSL_TZIC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FslTzicState),
    .instance_init = fsl_tzic_init,
    .class_init = fsl_tzic_class_init,
};

static void fsl_tzic_register_types(void)
{
    type_register_static(&fsl_tzic_info);
}

type_init(fsl_tzic_register_types)
