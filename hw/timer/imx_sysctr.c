/*
 * i.MX SYSCTR Timer
 *
 * Copyright (c) 2008 OK Labs
 * Copyright (c) 2011 NICTA Pty Ltd
 * Originally written by Hans Jiang
 * Updated by Peter Chubb
 * Updated by Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This code is licensed under GPL version 2 or later.  See
 * the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/registerfields.h"
#include "hw/timer/imx_sysctr.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "trace.h"

#define CMP_OFFSET 0x10000

REG32(CNTCV_LO, 0x8)
REG32(CNTCV_HI, 0xc)
REG32(CMPCV_L0, CMP_OFFSET + 0x20)
REG32(CMPCV_H0, CMP_OFFSET + 0x24)
REG32(CMPCR0, CMP_OFFSET + 0x2c)

#define SYSCTR_TIMER_MAX  0XFFFFFFFFUL

/* Control register.  Not all of these bits have any effect (yet) */
#define CMPCR_EN     (1 << 0)  /* SYSCTR Enable */
#define CMPCR_MASK   (1 << 1)  /* SYSCTR Enable Mode */
#define CMPCR_ISTAT  (1 << 2)  /* SYSCTR Debug mode enable */
#if 0
#define CMPCR_WAITEN (1 << 3)  /* SYSCTR Wait Mode Enable  */
#define CMPCR_DOZEN  (1 << 4)  /* SYSCTR Doze mode enable */
#define CMPCR_STOPEN (1 << 5)  /* SYSCTR Stop Mode Enable */
#define CMPCR_CLKSRC_SHIFT (6)
#define CMPCR_CLKSRC_MASK  (0x7)

#define CMPCR_FRR    (1 << 9)  /* Freerun or Restart */
#define CMPCR_SWR    (1 << 15) /* Software Reset */
#define CMPCR_IM1    (3 << 16) /* Input capture channel 1 mode (2 bits) */
#define CMPCR_IM2    (3 << 18) /* Input capture channel 2 mode (2 bits) */
#define CMPCR_OM1    (7 << 20) /* Output Compare Channel 1 Mode (3 bits) */
#define CMPCR_OM2    (7 << 23) /* Output Compare Channel 2 Mode (3 bits) */
#define CMPCR_OM3    (7 << 26) /* Output Compare Channel 3 Mode (3 bits) */
#define CMPCR_FO1    (1 << 29) /* Force Output Compare Channel 1 */
#define CMPCR_FO2    (1 << 30) /* Force Output Compare Channel 2 */
#define CMPCR_FO3    (1 << 31) /* Force Output Compare Channel 3 */
#endif

#define SYSCTR_SR_OF1  (1 << 0)
#define SYSCTR_SR_OF2  (1 << 1)
#define SYSCTR_SR_OF3  (1 << 2)
#define SYSCTR_SR_ROV  (1 << 5)

#define SYSCTR_IR_OF1IE  (1 << 0)
#define SYSCTR_IR_ROVIE  (1 << 5)

static const char *imx_sysctr_reg_name(uint32_t reg)
{
    switch (reg) {
    case R_CNTCV_LO:
        return "CNTCVLO";
    case R_CNTCV_HI:
        return "CNTCVHI";
    case R_CMPCV_L0:
        return "CMPCVL0";
    case R_CMPCV_H0:
        return "CMPCVH0";
    case R_CMPCR0:
        return "CMPCR0";
    default:
        return "[?]";
    }
}

static const VMStateDescription vmstate_imx_sysctr = {
    .name = TYPE_IMX_SYSCTR,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(cmpcr, IMXSysCtrState),
        VMSTATE_UINT64(cmpcv, IMXSysCtrState),
        VMSTATE_UINT64(start, IMXSysCtrState),
        VMSTATE_UINT32(cntfid[0], IMXSysCtrState),
        VMSTATE_UINT32(cntfid[1], IMXSysCtrState),
        VMSTATE_TIMER_PTR(timer, IMXSysCtrState),
        VMSTATE_END_OF_LIST()
    }
};

static uint64_t imx_sysctr_to_ns(IMXSysCtrState *s, uint64_t ticks)
{
    return (ticks * 1000000000) / s->base_clk;
}

static uint64_t imx_sysctr_from_ns(IMXSysCtrState *s, uint64_t ns)
{
    return (ns * s->base_clk) / 1000000000;
}

static uint64_t imx_sysctr_get_cntcv(IMXSysCtrState *s)
{
    return imx_sysctr_from_ns(s, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) - s->start);
}

static void imx_sysctr_update_int(IMXSysCtrState *s)
{
    printf("mask: %d, en: %d, %" PRIu64 " >= %" PRIu64 "\n", (s->cmpcr & CMPCR_MASK), (s->cmpcr & CMPCR_EN),
                             imx_sysctr_get_cntcv(s), s->cmpcv);

    if ((s->cmpcr & CMPCR_MASK) && (s->cmpcr & CMPCR_EN) &&
        (imx_sysctr_get_cntcv(s) >= s->cmpcv)) {
        qemu_irq_raise(s->irq);
    } else {
        qemu_irq_lower(s->irq);
    }
}

static void imx_sysctr_timeout(void *opaque)
{
    IMXSysCtrState *s = opaque;

    trace_imx_sysctr_timeout();

    imx_sysctr_update_int(s);
}

static uint64_t imx_sysctr_read(void *opaque, hwaddr offset, unsigned size)
{
    IMXSysCtrState *s = opaque;
    uint32_t reg_value = 0;

    switch (offset >> 2) {
    case R_CNTCV_LO:
        reg_value = imx_sysctr_get_cntcv(s) & (uint32_t)-1;
        break;

    case R_CNTCV_HI:
        reg_value = imx_sysctr_get_cntcv(s) >> 32;
        break;

    case R_CMPCR0:
        reg_value = s->cmpcr;
        if (imx_sysctr_get_cntcv(s) >= s->cmpcv) {
            reg_value |= CMPCR_ISTAT;
        }
        break;

    case R_CMPCV_H0:
        reg_value = s->cmpcv >> 32;
        break;

    case R_CMPCV_L0:
        reg_value = s->cmpcv & (uint32_t)(-1);
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX_SYSCTR, __func__, offset);
        break;
    }

    trace_imx_sysctr_read(offset, imx_sysctr_reg_name(offset >> 2), reg_value);

    return reg_value;
}

static void imx_sysctr_write(void *opaque, hwaddr offset, uint64_t value,
                             unsigned size)
{
    IMXSysCtrState *s = opaque;

    trace_imx_sysctr_write(offset, imx_sysctr_reg_name(offset >> 2), value);

    switch (offset >> 2) {
#if 0
    case R_CNTCV_LO:
        imx_sysctr_update_count(s);
        ptimer_set_count(s->timer, (value << 32) | (s->cntcv & (uint32_t)-1));
        break;

    case R_CNTCV_HI:
        imx_sysctr_update_count(s);
        ptimer_set_count(s->timer, (s->cntcv & 0xffffffff00000000) |
                                   (value & (uint32_t)-1));
        break;
#endif

    case R_CMPCR0:
        s->cmpcr = value;
        break;

    case R_CMPCV_H0:
    case R_CMPCV_L0:
        if (offset == A_CMPCV_H0) {
            s->cmpcv = (value << 32) | (s->cmpcv & (uint32_t)-1);
        } else {
            s->cmpcv = (s->cmpcv & 0xffffffff00000000) | (value & (uint32_t)-1);
        }

        timer_mod(s->timer, s->start + imx_sysctr_to_ns(s, s->cmpcv));

        printf("nanoseconds: %" PRIu64 "\n", s->start + imx_sysctr_to_ns(s, s->cmpcv) - qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL));

        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX_SYSCTR, __func__, offset);
        break;
    }
}

static const MemoryRegionOps imx_sysctr_ops = {
    .read = imx_sysctr_read,
    .write = imx_sysctr_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void imx_sysctr_realize(DeviceState *dev, Error **errp)
{
    IMXSysCtrState *s = IMX_SYSCTR(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    sysbus_init_irq(sbd, &s->irq);
    memory_region_init_io(&s->iomem, OBJECT(s), &imx_sysctr_ops, s,
                          TYPE_IMX_SYSCTR, 0x20000);
    sysbus_init_mmio(sbd, &s->iomem);

    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, imx_sysctr_timeout, s);
}

static void imx_sysctr_reset(DeviceState *dev)
{
    IMXSysCtrState *s = IMX_SYSCTR(dev);

    /* stop timer */
    timer_del(s->timer);

    s->start = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    s->cntfid[0] = 8000000; /* 8 MHz */
    s->cntfid[1] = 512; /* Hz */
    s->slow = false;

    s->cmpcr = CMPCR_MASK;
    s->cmpcv = 0;
}

static void imx_sysctr_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = imx_sysctr_realize;
    device_class_set_legacy_reset(dc, imx_sysctr_reset);
    dc->vmsd = &vmstate_imx_sysctr;
    dc->desc = "i.MX general timer";
}

static void imx8mp_sysctr_init(Object *obj)
{
    IMXSysCtrState *s = IMX_SYSCTR(obj);

    s->base_clk = 24000000;
    s->slow_clk = 32000;
}

static const TypeInfo imx_sysctr_types[] = {
    {
        .name = TYPE_IMX_SYSCTR,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(IMXSysCtrState),
        .class_init = imx_sysctr_class_init,
    },
    {
        .name = TYPE_IMX8MP_SYSCTR,
        .parent = TYPE_IMX_SYSCTR,
        .instance_init = imx8mp_sysctr_init,
    }
};

DEFINE_TYPES(imx_sysctr_types)
