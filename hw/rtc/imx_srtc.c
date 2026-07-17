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
#include "qemu/osdep.h"
#include "hw/rtc/imx_srtc.h"
#include "hw/core/registerfields.h"
#include "migration/vmstate.h"
#include "qemu/cutils.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "system/rtc.h"
#include "system/system.h"
#include "trace.h"

#define RTC_FREQ    32768ULL
#define RTC_COUNTER_SHIFT 17

/* Secure RTC (SRTC) registers */
REG32(LPSCMR,  0x0000) /* LP Secure Counter MSB Register */
REG32(LPSCLR,  0x0004) /* LP Secure Counter LSB Register */
REG32(LPSAR,   0x0008) /* LP Secure Alarm Register */
REG32(LPSMCR,  0x000C) /* LP Secure Monotonic Counter Register */

REG32(LPCR,    0x0010) /* LP Control Register */
    FIELD(LPCR, SCAL_LP,  18, 5)
    FIELD(LPCR, SCALM_LP, 16, 2)
    FIELD(LPCR, IE,       15, 1)
    FIELD(LPCR, NVE,      14, 1)
    FIELD(LPCR, IEIE,     13, 1)
    FIELD(LPCR, NVEIE,    12, 1)
    FIELD(LPCR, NSA,      11, 1)
    FIELD(LPCR, SV,       10, 1)
    FIELD(LPCR, LMC,       9, 1)
    FIELD(LPCR, LTC,       8, 1)
    FIELD(LPCR, ALP,       7, 1)
    FIELD(LPCR, SI,        6, 1)
    FIELD(LPCR, SAE,       5, 1)
    FIELD(LPCR, WAE,       4, 1)
    FIELD(LPCR, EN_LP,     3, 1)
    FIELD(LPCR, SWR_LP,    0, 1)

REG32(LPSR,    0x0014) /* LP Status Register */
    FIELD(LPSR, IES,      15, 1)
    FIELD(LPSR, NVES,     14, 1)
    FIELD(LPSR, STATE_LP, 12, 2)
    FIELD(LPSR, SM,       10, 2)
    FIELD(LPSR, IT,        7, 3)
    FIELD(LPSR, EAD,       6, 1)
    FIELD(LPSR, TR,        5, 1)
    FIELD(LPSR, MR,        4, 1)
    FIELD(LPSR, ALP,       3, 1)
    FIELD(LPSR, CTD,       2, 1)
    FIELD(LPSR, PGD,       1, 1)
    FIELD(LPSR, TRI,       0, 1)

REG32(LPPDR,   0x0018) /* LP Power Supply Glitch Detector Register */
REG32(LPGR,    0x001C) /* LP General Purpose Register */
REG32(HPCMR,   0x0020) /* HP Counter MSB Register */
REG32(HPCLR,   0x0024) /* HP Counter LSB Register */
REG32(HPAMR,   0x0028) /* HP Alarm MSB Register */
REG32(HPALR,   0x002C) /* HP Alarm LSB Register */
REG32(HPCR,    0x0030) /* HP Control Register */
REG32(HPISR,   0x0034) /* HP Interrupt Status Register */
REG32(HPIENR,  0x0038) /* HP Interrupt Enable Register */

static const char *imx_srtc_regname(unsigned int reg)
{
    switch (reg) {
    case R_LPSCMR:
        return "LPSCMR";
    case R_LPSCLR:
        return "LPSCLR";
    case R_LPSAR:
        return "LPSAR";
    case R_LPSMCR:
        return "LPSMCR";
    case R_LPCR:
        return "LPCR";
    case R_LPSR:
        return "LPSR";
    case R_LPPDR:
        return "LPPDR";
    case R_LPGR:
        return "LPGR";
    case R_HPCMR:
        return "HPCMR";
    case R_HPCLR:
        return "HPCLR";
    case R_HPAMR:
        return "HPAMR";
    case R_HPALR:
        return "HPALR";
    case R_HPCR:
        return "HPCR";
    case R_HPISR:
        return "HPISR";
    case R_HPIENR:
        return "HPIENR";
    }

    return "unknown";
}

static uint64_t imx_srtc_get_count(ImxSrtcState *s)
{
    uint64_t count;

    count = muldiv64(qemu_clock_get_ns(rtc_clock), RTC_FREQ,
                     NANOSECONDS_PER_SECOND);

    return s->count_offset + count;
}

static uint64_t imx_srtc_read(void *opaque, hwaddr offset, unsigned size)
{
    ImxSrtcState *s = opaque;
    const uint16_t index = offset >> 2;
    uint64_t ret;

    switch (index) {
    case R_LPSCMR:
        ret = extract64(imx_srtc_get_count(s) << RTC_COUNTER_SHIFT, 32, 32);
        break;

    case R_LPSCLR:
        ret = extract64(imx_srtc_get_count(s) << RTC_COUNTER_SHIFT, 0, 32);
        break;

    default:
        ret = s->regs[index];
        break;
    }

    trace_imx_srtc_read(offset, imx_srtc_regname(index), ret);

    return ret;
}

static void imx_srtc_write(void *opaque, hwaddr offset,
                           uint64_t val, unsigned size)
{
    ImxSrtcState *s = opaque;
    const uint16_t index = offset >> 2;
    uint64_t current_count = 0, new_count = 0;

    trace_imx_srtc_write(offset, imx_srtc_regname(index), val);

    if (index == R_LPSCMR || index == R_LPSCLR) {
        current_count = imx_srtc_get_count(s) << RTC_COUNTER_SHIFT;
    }

    switch (index) {
    case R_LPSCMR:
        new_count = deposit64(current_count, 32, 32, val);
        break;

    case R_LPSCLR:
        new_count = deposit64(current_count, 0, 32, val);
        break;

    case R_LPSR:
        s->regs[index] &= ~val;
        break;

    case R_HPISR:
        s->regs[index] &= ~val;
        break;

    case R_LPCR:
        s->regs[index] = val;

        if (FIELD_EX32(val, LPCR, IE)) {
            s->regs[R_LPSR] = FIELD_DP32(s->regs[R_LPSR], LPSR, IES, 1);
            s->regs[R_LPSR] = FIELD_DP32(s->regs[R_LPSR], LPSR, STATE_LP, 1);
        }

        if (FIELD_EX32(val, LPCR, NVE)) {
            s->regs[R_LPSR] = FIELD_DP32(s->regs[R_LPSR], LPSR, NVES, 1);
            s->regs[R_LPSR] = FIELD_DP32(s->regs[R_LPSR], LPSR, STATE_LP, 2);
        }
        break;

    default:
        s->regs[index] = val;
        break;
    }

    if (index == R_LPSCMR || index == R_LPSCLR) {
        s->count_offset += (new_count - current_count) >> RTC_COUNTER_SHIFT;
    }
}

static const MemoryRegionOps imx_srtc_ops = {
    .read = imx_srtc_read,
    .write = imx_srtc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void imx_srtc_reset(DeviceState *dev)
{
    ImxSrtcState *s = IMX_SRTC(dev);

    s->regs[R_LPCR] = 0;
    s->regs[R_LPSR] = 0;
    s->regs[R_HPCMR] = 0;
    s->regs[R_HPCLR] = 0;
    s->regs[R_HPAMR] = 0;
    s->regs[R_HPALR] = 0;
    s->regs[R_HPCR] = 0x8;
    s->regs[R_HPISR] = 0;
    s->regs[R_HPIENR] = 0;
}

static void imx_srtc_init(Object *obj)
{
    ImxSrtcState *s = IMX_SRTC(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(s);
    struct tm tm;

    memory_region_init_io(&s->iomem, obj, &imx_srtc_ops, s, TYPE_IMX_SRTC,
                          sizeof(s->regs));
    sysbus_init_mmio(sbd, &s->iomem);

    memset(s->regs, 0, sizeof(s->regs));

    qemu_get_timedate(&tm, 0);
    s->count_offset = (uint64_t)mktimegm(&tm) * RTC_FREQ -
        imx_srtc_get_count(s);
}

static const VMStateDescription vmstate_imx_srtc = {
    .name = TYPE_IMX_SRTC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT64(count_offset, ImxSrtcState),
        VMSTATE_UINT32_ARRAY(regs, ImxSrtcState, 15),
        VMSTATE_END_OF_LIST()
    },
};

static void imx_srtc_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_imx_srtc;
    device_class_set_legacy_reset(dc, imx_srtc_reset);
    dc->desc = "i.MX Secure Real Time Clock";
}

static const TypeInfo imx_srtc_info = {
    .name = TYPE_IMX_SRTC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ImxSrtcState),
    .instance_init = imx_srtc_init,
    .class_init = imx_srtc_class_init,
};

static void imx_srtc_register_types(void)
{
    type_register_static(&imx_srtc_info);
}

type_init(imx_srtc_register_types)
