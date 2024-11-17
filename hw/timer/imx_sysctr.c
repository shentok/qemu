/*
 * i.MX SYSCTR Timer
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/timer/imx_sysctr.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/registerfields.h"
#include "migration/vmstate.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "trace.h"

REG32(CNTCR, 0x0)
#define CNTCR_EN BIT(0)

REG32(CNTSR, 0x4)
#define CNTSR_FCA0 BIT(8)

REG32(CNTCV0, 0x8)
REG32(CNTCV1, 0xc)
REG32(CNTFID0, 0x20)
REG32(CNTFID1, 0x24)
REG32(CNTFID2, 0x28)
REG32(CNTID0, 0xfd0)

REG32(CMPCVL0, 0x20)
REG32(CMPCVH0, 0x24)
REG32(CMPCR0, 0x2c)
REG32(CMPCVL1, 0x120)
REG32(CMPCVH1, 0x124)
REG32(CMPCR1, 0x12c)

/* Control register. */
#define CMPCR_EN     BIT(0)
#define CMPCR_IMASK  BIT(1)
#define CMPCR_ISTAT  BIT(2)

static uint64_t imx_sysctr_to_ticks(const IMXSysCtrState *s, uint64_t ns)
{
    return (ns * imx_sysctr_base_freq(s)) / NANOSECONDS_PER_SECOND;
}

static uint64_t imx_sysctr_to_ns(IMXSysCtrState *s, uint64_t ticks)
{
    return (ticks * NANOSECONDS_PER_SECOND) / imx_sysctr_base_freq(s);
}

static uint64_t imx_sysctr_get_cntcv(const IMXSysCtrState *s)
{
    if (s->cntcr & CNTCR_EN) {
        return imx_sysctr_to_ticks(s, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) - s->cntcv);
    }

    return s->cntcv;
}

static void imx_sysctr_update_int(IMXSysCtrState *s, int index)
{
    if ((s->cmp[index].cr & CMPCR_EN) &&
        imx_sysctr_get_cntcv(s) >= s->cmp[index].cv) {
        s->cmp[index].cr |= CMPCR_ISTAT;
    } else {
        s->cmp[index].cr &= ~CMPCR_ISTAT;
    }

    if ((s->cmp[index].cr & CMPCR_IMASK) && (s->cmp[index].cr & CMPCR_ISTAT)) {
        qemu_irq_raise(s->cmp[index].irq);
    } else {
        qemu_irq_lower(s->cmp[index].irq);
    }
}

static void imx_sysctr_timeout0(void *opaque)
{
    IMXSysCtrState *s = opaque;

    trace_imx_sysctr_timeout(0);

    imx_sysctr_update_int(s, 0);
}

static void imx_sysctr_timeout1(void *opaque)
{
    IMXSysCtrState *s = opaque;

    trace_imx_sysctr_timeout(1);

    imx_sysctr_update_int(s, 1);
}

static void imx_sysctr_timer_update(IMXSysCtrState *s, int index)
{
    if (!(s->cmp[index].cr & CMPCR_EN) || !(s->cntcr & CNTCR_EN)) {
        timer_del(s->cmp[index].timer);
    } else {
        timer_mod(s->cmp[index].timer,
                  qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
                    imx_sysctr_to_ns(s, s->cmp[index].cv -
                                        imx_sysctr_get_cntcv(s)));
    }

    imx_sysctr_update_int(s, index);
}

static const char *imx_sysctr_rd_reg_name(uint32_t reg)
{
    switch (reg) {
    case R_CNTCV0:
        return "CNTCV0";
    case R_CNTCV1:
        return "CNTCV1";
    case R_CNTID0:
        return "CNTID0";
    default:
        return "[?]";
    }
}

static uint64_t imx_sysctr_rd_read(void *opaque, hwaddr offset, unsigned size)
{
    IMXSysCtrState *s = opaque;
    const uint32_t reg = offset >> 2;
    uint32_t value = 0;

    switch (reg) {
    case R_CNTCV0:
    case R_CNTCV1:
        value = extract64(imx_sysctr_get_cntcv(s),
                          reg == R_CNTCV0 ? 0 : 32, 32);
        break;

    case R_CNTID0:
        value = 0;
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX_SYSCTR, __func__, offset);
        break;
    }

    trace_imx_sysctr_rd_read(offset, imx_sysctr_rd_reg_name(reg), value);

    return value;
}

static void imx_sysctr_rd_write(void *opaque, hwaddr offset, uint64_t value,
                                unsigned size)
{
    trace_imx_sysctr_rd_write(offset, imx_sysctr_rd_reg_name(offset >> 2),
                              value);

    qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                  HWADDR_PRIx "\n", TYPE_IMX_SYSCTR, __func__, offset);
}

static const char *imx_sysctr_cmp_reg_name(uint32_t reg)
{
    switch (reg) {
    case R_CMPCVL0:
        return "CMPCVL0";
    case R_CMPCVH0:
        return "CMPCVH0";
    case R_CMPCR0:
        return "CMPCR0";
    case R_CMPCVL1:
        return "CMPCVL1";
    case R_CMPCVH1:
        return "CMPCVH1";
    case R_CMPCR1:
        return "CMPCR1";
    case R_CNTID0:
        return "CNTID0";
    default:
        return "[?]";
    }
}

static uint64_t imx_sysctr_cmp_read(void *opaque, hwaddr offset,
                                    unsigned size)
{
    IMXSysCtrState *s = opaque;
    const uint32_t reg = offset >> 2;
    uint32_t value = 0;
    int index;

    switch (reg) {
    case R_CMPCVL0:
    case R_CMPCVH0:
        value = extract64(s->cmp[0].cv, reg == R_CMPCVL0 ? 0 : 32, 32);
        break;

    case R_CMPCVL1:
    case R_CMPCVH1:
        value = extract64(s->cmp[1].cv, reg == R_CMPCVL1 ? 0 : 32, 32);
        break;

    case R_CMPCR0:
    case R_CMPCR1:
        index = reg == R_CMPCR0 ? 0 : 1;
        value = s->cmp[index].cr;
        break;

    case R_CNTID0:
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX_SYSCTR, __func__, offset);
        break;
    }

    trace_imx_sysctr_cmp_read(offset, imx_sysctr_cmp_reg_name(reg), value);

    return value;
}

static void imx_sysctr_cmp_write(void *opaque, hwaddr offset, uint64_t value,
                                 unsigned size)
{
    IMXSysCtrState *s = opaque;
    const uint32_t reg = offset >> 2;
    int index;

    trace_imx_sysctr_cmp_write(offset, imx_sysctr_cmp_reg_name(reg), value);

    switch (reg) {
    case R_CMPCVL0:
    case R_CMPCVH0:
        s->cmp[0].cv = deposit64(s->cmp[0].cv, reg == R_CMPCVL0 ? 0 : 32,
                                 32, value);
        imx_sysctr_timer_update(s, 0);
        break;

    case R_CMPCVL1:
    case R_CMPCVH1:
        s->cmp[1].cv = deposit64(s->cmp[1].cv, reg == R_CMPCVL1 ? 0 : 32,
                                 32, value);
        imx_sysctr_timer_update(s, 1);
        break;

    case R_CMPCR0:
    case R_CMPCR1:
        index = reg == R_CMPCR0 ? 0 : 1;
        s->cmp[index].cr = (value & ~CMPCR_ISTAT) | (s->cmp[index].cr & CMPCR_ISTAT);
        imx_sysctr_timer_update(s, index);
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX_SYSCTR, __func__, offset);
        break;
    }
}

static const char *imx_sysctr_cnt_reg_name(uint32_t reg)
{
    switch (reg) {
    case R_CNTCR:
        return "CNTCR";
    case R_CNTSR:
        return "CNTSR";
    case R_CNTCV0:
        return "CNTCV0";
    case R_CNTCV1:
        return "CNTCV1";
    case R_CNTFID0:
        return "CNTFID0";
    case R_CNTFID1:
        return "CNTFID1";
    case R_CNTFID2:
        return "CNTFID2";
    case R_CNTID0:
        return "CNTID0";
    default:
        return "[?]";
    }
}

static uint64_t imx_sysctr_cnt_read(void *opaque, hwaddr offset, unsigned size)
{
    IMXSysCtrState *s = opaque;
    const uint32_t reg = offset >> 2;
    uint32_t value = 0;

    switch (reg) {
    case R_CNTCR:
        value = s->cntcr;
        break;

    case R_CNTSR:
        value = CNTSR_FCA0;
        break;

    case R_CNTCV0:
    case R_CNTCV1:
        value = extract64(imx_sysctr_get_cntcv(s),
                          reg == R_CNTCV0 ? 0 : 32, 32);
        break;

    case R_CNTFID0:
        value = imx_sysctr_base_freq(s);
        break;

    case R_CNTFID1:
        value = s->slow_clk / 64;
        break;

    case R_CNTFID2:
    case R_CNTID0:
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX_SYSCTR, __func__, offset);
        break;
    }

    trace_imx_sysctr_cnt_read(offset, imx_sysctr_cnt_reg_name(reg), value);

    return value;
}

static void imx_sysctr_cnt_write(void *opaque, hwaddr offset, uint64_t value,
                                 unsigned size)
{
    IMXSysCtrState *s = opaque;
    const uint32_t reg = offset >> 2;

    trace_imx_sysctr_cnt_write(offset, imx_sysctr_cnt_reg_name(reg), value);

    switch (reg) {
    case R_CNTCR:
        if (value & CNTCR_EN) {
            if (!(s->cntcr & CNTCR_EN)) {
                s->cntcv = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)
                           - imx_sysctr_to_ns(s, s->cntcv);
            }
        } else {
            if (s->cntcr & CNTCR_EN) {
                s->cntcv = imx_sysctr_get_cntcv(s);
            }
        }
        s->cntcr = value;
        imx_sysctr_timer_update(s, 0);
        imx_sysctr_timer_update(s, 1);
        break;

    case R_CNTCV0:
    case R_CNTCV1: {
            uint64_t cntcv = deposit64(imx_sysctr_get_cntcv(s),
                                       reg == R_CNTCV0 ? 0 : 32, 32, value);
            if (s->cntcr & CNTCR_EN) {
                s->cntcv = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)
                           - imx_sysctr_to_ns(s, cntcv);
            } else {
                s->cntcv = cntcv;
            }
            imx_sysctr_timer_update(s, 0);
            imx_sysctr_timer_update(s, 1);
        } break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX_SYSCTR, __func__, offset);
        break;
    }
}

static void imx_sysctr_reset(DeviceState *dev)
{
    IMXSysCtrState *s = IMX_SYSCTR(dev);

    s->cntcr = 0;
    s->cntcv = 0;

    for (int i = 0; i < ARRAY_SIZE(s->cmp); i++) {
        s->cmp[i].cr = CMPCR_IMASK;
        s->cmp[i].cv = 0;

        imx_sysctr_timer_update(s, i);
    }
}

static const MemoryRegionOps imx_sysctr_rd_ops = {
    .read = imx_sysctr_rd_read,
    .write = imx_sysctr_rd_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps imx_sysctr_cmp_ops = {
    .read = imx_sysctr_cmp_read,
    .write = imx_sysctr_cmp_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps imx_sysctr_cnt_ops = {
    .read = imx_sysctr_cnt_read,
    .write = imx_sysctr_cnt_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void imx_sysctr_init(Object *obj)
{
    IMXSysCtrState *s = IMX_SYSCTR(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(s);

    memory_region_init_io(&s->iomem.rd, OBJECT(s), &imx_sysctr_rd_ops, s,
                          TYPE_IMX_SYSCTR ".rd", 0x10000);
    sysbus_init_mmio(sbd, &s->iomem.rd);
    memory_region_init_io(&s->iomem.cmp, OBJECT(s), &imx_sysctr_cmp_ops, s,
                          TYPE_IMX_SYSCTR ".cmp", 0x10000);
    sysbus_init_mmio(sbd, &s->iomem.cmp);
    memory_region_init_io(&s->iomem.ctrl, OBJECT(s), &imx_sysctr_cnt_ops, s,
                          TYPE_IMX_SYSCTR ".ctrl", 0x10000);
    sysbus_init_mmio(sbd, &s->iomem.ctrl);

    sysbus_init_irq(sbd, &s->cmp[0].irq);
    sysbus_init_irq(sbd, &s->cmp[1].irq);

    s->cmp[0].timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, imx_sysctr_timeout0, s);
    s->cmp[1].timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, imx_sysctr_timeout1, s);
}

static const VMStateDescription imx_sysctr_vmstate = {
    .name = TYPE_IMX_SYSCTR,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT64(cntcv, IMXSysCtrState),
        VMSTATE_UINT32(cntcr, IMXSysCtrState),
        VMSTATE_UINT32(cmp[0].cr, IMXSysCtrState),
        VMSTATE_UINT64(cmp[0].cv, IMXSysCtrState),
        VMSTATE_TIMER_PTR(cmp[0].timer, IMXSysCtrState),
        VMSTATE_UINT32(cmp[1].cr, IMXSysCtrState),
        VMSTATE_UINT64(cmp[1].cv, IMXSysCtrState),
        VMSTATE_TIMER_PTR(cmp[1].timer, IMXSysCtrState),
        VMSTATE_END_OF_LIST()
    }
};

static const Property imx_sysctr_properties[] = {
    DEFINE_PROP_UINT32("base_clk", IMXSysCtrState, base_clk, 24000000),
    DEFINE_PROP_UINT32("slow_clk", IMXSysCtrState, slow_clk, 32000),
};

static void imx_sysctr_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, imx_sysctr_reset);
    device_class_set_props(dc, imx_sysctr_properties);
    dc->vmsd = &imx_sysctr_vmstate;
    dc->desc = "i.MX System Counter";
}

static const TypeInfo imx_sysctr_types[] = {
    {
        .name = TYPE_IMX_SYSCTR,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(IMXSysCtrState),
        .instance_init = imx_sysctr_init,
        .class_init = imx_sysctr_class_init,
    },
};

DEFINE_TYPES(imx_sysctr_types)
