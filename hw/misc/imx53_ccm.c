/*
 * IMX53 Clock Control Module
 *
 * Copyright (c) 2015 Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * To get the timer frequencies right, we need to emulate at least part of
 * the CCM.
 */

#include "qemu/osdep.h"
#include "hw/misc/imx53_ccm.h"
#include "hw/core/registerfields.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "trace.h"

REG32(CCM_CCR,      0x0000)
REG32(CCM_CCDR,     0x0004)
REG32(CCM_CSR,      0x0008)

REG32(CCM_CCSR,     0x000C)
    FIELD(CCM_CCSR, LP_APM, 10, 1)

REG32(CCM_CACRR,    0x0010)

REG32(CCM_CBCDR,    0x0014)
    FIELD(CCM_CBCDR, PERIPH_CLK_SEL, 25, 1);
    FIELD(CCM_CBCDR, AHB_PODF, 10, 3)
    FIELD(CCM_CBCDR, IPG_PODF, 8, 2)
    FIELD(CCM_CBCDR, PERCLK_PODF, 0, 2)

REG32(CCM_CBCMR,    0x0018)
    FIELD(CCM_CBCMR, PERIPH_APM_SEL, 12, 2)

REG32(CCM_CSCMR1,   0x001C)
REG32(CCM_CSCMR2,   0x0020)
REG32(CCM_CSCDR1,   0x0024)
REG32(CCM_CS1CDR,   0x0028)
REG32(CCM_CS2CDR,   0x002C)
REG32(CCM_CDCDR,    0x0030)
REG32(CCM_CHSCCDR,  0x0034)
REG32(CCM_CSCDR2,   0x0038)
REG32(CCM_CSCDR3,   0x003C)
REG32(CCM_CSCDR4,   0x0040)
REG32(CCM_CDHIPR,   0x0048)
REG32(CCM_CDCR,     0x004C)
REG32(CCM_CLPCR,    0x0054)
REG32(CCM_CISR,     0x0058)
REG32(CCM_CIMR,     0x005C)
REG32(CCM_CCOSR,    0x0060)
REG32(CCM_CGPR,     0x0064)
REG32(CCM_CCGR0,    0x0068)
REG32(CCM_CCGR1,    0x006C)
REG32(CCM_CCGR2,    0x0070)
REG32(CCM_CCGR3,    0x0074)
REG32(CCM_CCGR4,    0x0078)
REG32(CCM_CCGR5,    0x007C)
REG32(CCM_CCGR6,    0x0080)
REG32(CCM_CCGR7,    0x0084)
REG32(CCM_CMEOR,    0x0088)

REG32(DPLLC_CTL,        0x0000)
REG32(DPLLC_CONFIG,     0x0004)
REG32(DPLLC_OP,         0x0008)
REG32(DPLLC_MFD,        0x000C)
REG32(DPLLC_MFN,        0x0010)
REG32(DPLLC_MFNMINUS,   0x0014)
REG32(DPLLC_MFNPLUS,    0x0018)
REG32(DPLLC_HFS_OP,     0x001C)
REG32(DPLLC_HFS_MFD,    0x0020)
REG32(DPLLC_HFS_MFN,    0x0024)
REG32(DPLLC_MFN_TOGC,   0x0028)
REG32(DPLLC_DESTAT,     0x002C)

#define CKIH_FREQ 24000000 /* 24MHz crystal input */

static const char *imx53_ccm_reg_name(uint32_t reg)
{
    static char unknown[20];

    switch (reg) {
    case R_CCM_CCR: return "CCM_CCR";
    case R_CCM_CCDR: return "CCM_CCDR";
    case R_CCM_CCSR: return "CCM_CCSR";
    case R_CCM_CACRR: return "CCM_CACRR";
    case R_CCM_CBCDR: return "CCM_CBCDR";
    case R_CCM_CBCMR: return "CCM_CBCMR";
    case R_CCM_CSCMR1: return "CCM_CSCMR1";
    case R_CCM_CSCMR2: return "CCM_CSCMR2";
    case R_CCM_CSCDR1: return "CCM_CSCDR1";
    case R_CCM_CS1CDR: return "CCM_CS1CDR";
    case R_CCM_CS2CDR: return "CCM_CS2CDR";
    case R_CCM_CDCDR: return "CCM_CDCDR";
    case R_CCM_CHSCCDR: return "CCM_CHSCCDR";
    case R_CCM_CSCDR2: return "CCM_CSCDR2";
    case R_CCM_CSCDR3: return "CCM_CSCDR3";
    case R_CCM_CSCDR4: return "CCM_CSCDR4";
    case R_CCM_CDHIPR: return "CCM_CDHIPR";
    case R_CCM_CDCR: return "CCM_CDCR";
    case R_CCM_CLPCR: return "CCM_CLPCR";
    case R_CCM_CISR: return "CCM_CISR";
    case R_CCM_CIMR: return "CCM_CIMR";
    case R_CCM_CCOSR: return "CCM_CCOSR";
    case R_CCM_CGPR: return "CCM_CGPR";
    case R_CCM_CCGR0: return "CCM_CCGR0";
    case R_CCM_CCGR1: return "CCM_CCGR1";
    case R_CCM_CCGR2: return "CCM_CCGR2";
    case R_CCM_CCGR3: return "CCM_CCGR3";
    case R_CCM_CCGR4: return "CCM_CCGR4";
    case R_CCM_CCGR5: return "CCM_CCGR5";
    case R_CCM_CCGR6: return "CCM_CCGR6";
    case R_CCM_CCGR7: return "CCM_CCGR7";
    case R_CCM_CMEOR: return "CCM_CMEOR";
    default:
        snprintf(unknown, sizeof(unknown), "%u ?", reg);
        return unknown;
    }
}

static const char *imx53_dpllc_reg_name(unsigned index)
{
    switch (index) {
    case R_DPLLC_CTL: return "DPLLC_CTL";
    case R_DPLLC_CONFIG: return "DPLLC_CONFIG";
    case R_DPLLC_OP: return "DPLLC_OP";
    case R_DPLLC_MFD: return "DPLLC_MFD";
    case R_DPLLC_MFN: return "DPLLC_MFN";
    case R_DPLLC_MFNMINUS: return "DPLLC_MFNMINUS";
    case R_DPLLC_MFNPLUS: return "DPLLC_MFNPLUS";
    case R_DPLLC_HFS_OP: return "DPLLC_HFS_OP";
    case R_DPLLC_HFS_MFD: return "DPLLC_HFS_MFD";
    case R_DPLLC_HFS_MFN: return "DPLLC_HFS_MFN";
    case R_DPLLC_MFN_TOGC: return "DPLLC_MFN_TOGC";
    case R_DPLLC_DESTAT: return "DPLLC_DESTAT";
    default: return "UNKNOWN";
    }
}

static const VMStateDescription vmstate_imx53_ccm = {
    .name = TYPE_IMX53_CCM,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(ccm, Imx53CcmState, CCM_MAX),
        VMSTATE_END_OF_LIST()
    },
};

static uint64_t imx53_dpllc_get_clk(Imx53CcmState *dev, int pll_index)
{
    return 4 * CKIH_FREQ * 3;
}

static uint64_t imx53_ccm_get_lp_apm_clk(Imx53CcmState *dev)
{
    uint64_t freq;

    switch (FIELD_EX32(dev->ccm[R_CCM_CCSR], CCM_CCSR, LP_APM)) {
    case 0:
        freq = CKIH_FREQ;
        break;
    case 1:
        freq = imx53_dpllc_get_clk(dev, 3);
        break;
    }

    trace_imx53_ccm_get_lp_apm_clk(freq);

    return freq;
}

static uint64_t imx53_ccm_get_periph_apm_clk(Imx53CcmState *dev)
{
    uint64_t freq;

    switch (FIELD_EX32(dev->ccm[R_CCM_CBCMR], CCM_CBCMR, PERIPH_APM_SEL)) {
    case 0:
        freq = imx53_dpllc_get_clk(dev, 0);
        break;
    case 1:
        freq = imx53_dpllc_get_clk(dev, 2);
        break;
    case 2:
        freq = imx53_ccm_get_lp_apm_clk(dev);
        break;
    case 3: /* reserved */
        freq = 0;
        break;
    }

    trace_imx53_ccm_get_periph_apm_clk(freq);

    return freq;
}

static uint64_t imx53_ccm_get_periph_clk(Imx53CcmState *dev)
{
    uint64_t freq;

    switch (FIELD_EX32(dev->ccm[R_CCM_CBCDR], CCM_CBCDR, PERIPH_CLK_SEL)) {
    case 0:
        freq = imx53_dpllc_get_clk(dev, 1);
        break;
    case 1:
        freq = imx53_ccm_get_periph_apm_clk(dev);
        break;
    }

    trace_imx53_ccm_get_periph_clk(freq);

    return freq;
}

static uint64_t imx53_ccm_get_ahb_clk(Imx53CcmState *dev)
{
    uint64_t freq = 0;

    freq = imx53_ccm_get_periph_clk(dev)
           / (1 + FIELD_EX32(dev->ccm[R_CCM_CBCDR], CCM_CBCDR, AHB_PODF));

    trace_imx53_ccm_get_ahb_clk(freq);

    return freq;
}

static uint64_t imx53_ccm_get_ipg_clk(Imx53CcmState *dev)
{
    uint64_t freq = 0;

    freq = imx53_ccm_get_ahb_clk(dev)
           / (1 + FIELD_EX32(dev->ccm[R_CCM_CBCDR], CCM_CBCDR, IPG_PODF));

    trace_imx53_ccm_get_ipg_clk(freq);

    return freq;
}

static uint64_t imx53_ccm_get_per_clk(Imx53CcmState *dev)
{
    uint64_t freq = 0;

    freq = imx53_ccm_get_ipg_clk(dev)
           / (1 + FIELD_EX32(dev->ccm[R_CCM_CBCDR], CCM_CBCDR, PERCLK_PODF));

    trace_imx53_ccm_get_per_clk(freq);

    return freq;
}

static uint32_t imx53_ccm_get_clock_frequency(IMXCCMState *dev, IMXClk clock)
{
    uint32_t freq = 0;
    Imx53CcmState *s = IMX53_CCM(dev);

    switch (clock) {
    case CLK_NONE:
        break;
    case CLK_IPG:
        freq = imx53_ccm_get_ipg_clk(s);
        break;
    case CLK_IPG_HIGH:
        freq = imx53_ccm_get_per_clk(s);
        break;
    case CLK_32k:
        freq = CKIL_FREQ;
        break;
    case CLK_HIGH:
        freq = 24000000;
        break;
    case CLK_HIGH_DIV:
        freq = 24000000 / 8;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: unsupported clock %d\n",
                      TYPE_IMX53_CCM, __func__, clock);
        break;
    }

    trace_imx53_ccm_get_clock_frequency(clock, freq);

    return freq;
}

static void imx53_ccm_reset(DeviceState *dev)
{
    Imx53CcmState *s = IMX53_CCM(dev);

    trace_imx53_ccm_reset();

    s->ccm[R_CCM_CCR] = 0x000016ff;
    s->ccm[R_CCM_CCDR] = 0x00000000;
    s->ccm[R_CCM_CSR] = 0x00000010;
    s->ccm[R_CCM_CCSR] = 0x00000000;
    s->ccm[R_CCM_CACRR] = 0x00000000;
    s->ccm[R_CCM_CBCDR] = 0x00888945;
    s->ccm[R_CCM_CBCMR] = 0x00016154;
    s->ccm[R_CCM_CSCMR1] = 0xa6a2a020;
    s->ccm[R_CCM_CSCMR2] = 0x00b12f02;
    s->ccm[R_CCM_CSCDR1] = 0x00430318;
    s->ccm[R_CCM_CS1CDR] = 0x02860241;
    s->ccm[R_CCM_CS2CDR] = 0x00860041;
    s->ccm[R_CCM_CDCDR] = 0x143701d2;
    s->ccm[R_CCM_CHSCCDR] = 0x000000a0;
    s->ccm[R_CCM_CSCDR2] = 0x12080844;
    s->ccm[R_CCM_CSCDR3] = 0x00000041;
    s->ccm[R_CCM_CSCDR4] = 0x00000241;
    s->ccm[R_CCM_CDHIPR] = 0x00000000;
    s->ccm[R_CCM_CDCR] = 0x00000000;
    s->ccm[R_CCM_CLPCR] = 0x00000079;
    s->ccm[R_CCM_CISR] = 0x00000000;
    s->ccm[R_CCM_CIMR] = 0xffffffff;
    s->ccm[R_CCM_CCOSR] = 0x000a0001;
    s->ccm[R_CCM_CGPR] = 0x00000000;
    s->ccm[R_CCM_CCGR0] = 0xffffffff;
    s->ccm[R_CCM_CCGR1] = 0xffffffff;
    s->ccm[R_CCM_CCGR2] = 0xffffffff;
    s->ccm[R_CCM_CCGR3] = 0xffffffff;
    s->ccm[R_CCM_CCGR4] = 0xffffffff;
    s->ccm[R_CCM_CCGR5] = 0xffffffff;
    s->ccm[R_CCM_CCGR6] = 0xffffffff;
    s->ccm[R_CCM_CCGR7] = 0xffffffff;
    s->ccm[R_CCM_CMEOR] = 0xffffffff;

    for (int i = 0; i < ARRAY_SIZE(s->dpllc); i++) {
        s->dpllc[i][R_DPLLC_CTL] = 0x00aa0222;
        s->dpllc[i][R_DPLLC_CONFIG] = 0x00aa0006;
        s->dpllc[i][R_DPLLC_OP] = 0x00aa0000;
        s->dpllc[i][R_DPLLC_MFD] = 0x00aa0000;
        s->dpllc[i][R_DPLLC_MFN] = 0x00aa0000;
        s->dpllc[i][R_DPLLC_MFNMINUS] = 0x00000000;
        s->dpllc[i][R_DPLLC_MFNPLUS] = 0x00000000;
        s->dpllc[i][R_DPLLC_HFS_OP] = 0x00aa0000;
        s->dpllc[i][R_DPLLC_HFS_MFD] = 0x00aa0000;
        s->dpllc[i][R_DPLLC_HFS_MFN] = 0x00aa0000;
        s->dpllc[i][R_DPLLC_MFN_TOGC] = 0x00aa0000;
        s->dpllc[i][R_DPLLC_DESTAT] = 0x00aa0000;
    }
}

static uint64_t imx53_ccm_read(void *opaque, hwaddr offset, unsigned size)
{
    uint32_t value = 0;
    uint32_t index = offset >> 2;
    Imx53CcmState *s = (Imx53CcmState *)opaque;

    value = s->ccm[index];

    trace_imx53_ccm_read(imx53_ccm_reg_name(index), value);

    return value;
}

static void imx53_ccm_write(void *opaque, hwaddr offset, uint64_t value,
                            unsigned size)
{
    uint32_t index = offset >> 2;
    Imx53CcmState *s = (Imx53CcmState *)opaque;

    trace_imx53_ccm_write(imx53_ccm_reg_name(index), (uint32_t)value);

    /*
     * We will do a better implementation later. In particular some bits
     * cannot be written to.
     */
    s->ccm[index] = (uint32_t)value;
}

static const struct MemoryRegionOps imx53_ccm_ops = {
    .read = imx53_ccm_read,
    .write = imx53_ccm_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        /*
         * Our device would not work correctly if the guest was doing
         * unaligned access. This might not be a limitation on the real
         * device but in practice there is no reason for a guest to access
         * this device unaligned.
         */
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static uint64_t imx53_dpllc_read(void *opaque, hwaddr offset, unsigned size)
{
    uint32_t *dpllc = opaque;
    uint32_t index = offset >> 2;
    uint32_t value = dpllc[index];

    if (index == R_DPLLC_CTL) {
        value |= BIT(0);
    }

    trace_imx53_dpllc_read(imx53_dpllc_reg_name(index), value);

    return value;
}

static void imx53_dpllc_write(void *opaque, hwaddr offset, uint64_t value,
                              unsigned size)
{
    uint32_t *dpllc = opaque;
    uint32_t index = offset >> 2;

    trace_imx53_dpllc_write(imx53_dpllc_reg_name(index), value);

    dpllc[index] = value;
}

static const struct MemoryRegionOps imx53_dpllc_ops = {
    .read = imx53_dpllc_read,
    .write = imx53_dpllc_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        /*
         * Our device would not work correctly if the guest was doing
         * unaligned access. This might not be a limitation on the real
         * device but in practice there is no reason for a guest to access
         * this device unaligned.
         */
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void imx53_ccm_init(Object *obj)
{
    Imx53CcmState *s = IMX53_CCM(obj);

    memory_region_init_io(&s->ioccm, OBJECT(s), &imx53_ccm_ops, s,
                          TYPE_IMX53_CCM ".ccm", CCM_MAX * sizeof(uint32_t));
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->ioccm);

    for (int i = 0; i < ARRAY_SIZE(s->iodpllc); i++) {
        memory_region_init_io(&s->iodpllc[i], OBJECT(s), &imx53_dpllc_ops,
                              &s->dpllc[i], TYPE_IMX53_CCM ".dpllc",
                              sizeof(s->iodpllc[i]));
        sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iodpllc[i]);
    }
}

static void imx53_ccm_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    IMXCCMClass *ccm = IMX_CCM_CLASS(klass);

    device_class_set_legacy_reset(dc, imx53_ccm_reset);
    dc->vmsd = &vmstate_imx53_ccm;
    dc->desc = "i.MX53 Clock Control Module";

    ccm->get_clock_frequency = imx53_ccm_get_clock_frequency;
}

static const TypeInfo imx53_ccm_info = {
    .name          = TYPE_IMX53_CCM,
    .parent        = TYPE_IMX_CCM,
    .instance_size = sizeof(Imx53CcmState),
    .instance_init = imx53_ccm_init,
    .class_init    = imx53_ccm_class_init,
};

static void imx53_ccm_register_types(void)
{
    type_register_static(&imx53_ccm_info);
}

type_init(imx53_ccm_register_types)
