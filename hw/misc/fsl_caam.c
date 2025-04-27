/*
 * Freescale Cryptographic Acceleration and Assurance Module Emulation
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/irq.h"
#include "hw/misc/fsl_caam.h"
#include "hw/registerfields.h"
#include "hw/resettable.h"
#include "migration/vmstate.h"
#include "trace.h"
#include "system/address-spaces.h"
#include "system/dma.h"
#include "system/runstate.h"

REG32(IRBA_HI, 0x0)
REG32(IRBA_LO, 0x4)
REG32(IRS, 0xc)
REG32(IRSA, 0x14)
REG32(IRJA, 0x1c)

REG32(ORBA_HI, 0x20)
REG32(ORBA_LO, 0x24)
REG32(ORS, 0x2c)
REG32(ORJR, 0x34)
REG32(ORSF, 0x3c)

REG32(JRINT, 0x4c)
#define JRINT_ERR_HALT_COMPLETE 0x8
#define JRINT_JR_INT 0x1

REG32(JRCFG_LO, 0x54)
REG32(IRRI, 0x5c)
REG32(ORWI, 0x64)

REG32(JRCR, 0x6c)
#define JRCR_RESET 0x1

REG32(MCFGR, 0x4)
#define MCFGR_LONG_PTR 0x00010000 /* Use >32-bit desc addressing */

REG32(RNG_VERSION, 0xeb4)
REG32(CCBVID, 0xfe4)

REG32(CTPR_MS, 0xfa8)
#define CTPR_MS_PS BIT(17)

static const char *fsl_caam_ctrl_reg_name(hwaddr offset)
{
    switch (offset) {
    /* Basic Configuration Section */
    case 0x4:
        return " (MCFGR)";
    case 0xc:
        return " (SCFGR)";

    case 0x10:
    case 0x18:
    case 0x20:
    case 0x28:
        return " (JRLIODNR_MS)";
    case 0x14:
    case 0x1c:
    case 0x24:
    case 0x2c:
        return " (JRLIODNR_LS)";
    case 0x5c:
        return " (JRSTART)";

    case 0x600:
        return " (TRNG_MCTL)";
    case 0x604:
        return " (TRNG_RTSCMISC)";
    case 0x608:
        return " (TRNG_RTPKRRNG)";
    case 0x60c:
        return " (TRNG_RTPKRMAX)";
    case 0x610:
        return " (TRNG_SDCTL)";
    case 0x618:
        return " (TRNG_FRQMIN)";
    case 0x61c:
        return " (TRNG_FRQMAX)";
    case 0x620:
        return " (TRNG_RTSCML)";
    case 0x624:
        return " (TRNG_RTSCR1L)";
    case 0x628:
        return " (TRNG_RTSCR2L)";
    case 0x62c:
        return " (TRNG_RTSCR3L)";
    case 0x630:
        return " (TRNG_RTSCR4L)";
    case 0x634:
        return " (TRNG_RTSCR5L)";
    case 0x638:
        return " (TRNG_RTSCR6PL)";
    case 0x6c0:
        return " (RNG_STA)";

    /* Secure Memory */
    case 0xa04:
        return " (SM_SMAPR)";
    case 0xa08:
        return " (SM_SMAG2)";
    case 0xa0c:
        return " (SM_SMAG1)";
    case 0xbe4:
        return " (SM_SMCR)";
    case 0xbec:
        return " (SM_SMCSR)";

    /* Version */
    case 0xeb4:
        return " (RNG_VERSION)";

    /* CAAM Hardware Instantiation Parameters */
    case 0xfa0:
        return " (CRNR_MS)";
    case 0xfa4:
        return " (CRNR_LS)";
    case 0xfa8:
        return " (CTPR_MS)";
    case 0xfac:
        return " (CTPR_LS)";

    /* Secure Memory */
    case 0xfb4:
        return " (SM_reserved)";
    case 0xfbc:
        return " (SMPO)";

    /* CAAM Global Status */
    case 0xfc0:
        return " (FAR_MS)";
    case 0xfc4:
        return " (FAR_LS)";
    case 0xfc8:
        return " (FALR)";
    case 0xfcc:
        return " (FADR)";
    case 0xfd4:
        return " (CSTA)";
    case 0xfd8:
        return " (SMVID_MS)";
    case 0xfdc:
        return " (SMVID_LS)";

    /* Component Instantiation Parameters */
    case 0xfe4:
        return " (CCBVID)";
    case 0xfec:
        return " (CHAVID_LS)";
    case 0xff0:
        return " (CHANUM_MS)";
    case 0xff4:
        return " (CHANUM_LS)";
    case 0xff8:
        return " (CAAMVID_MS)";
    case 0xffc:
        return " (CAAMVID_LS)";
    }

    return "";
}

static uint64_t fsl_caam_ctrl_read(void *opaque, hwaddr offset, unsigned size)
{
    FslImxCaamState *s = opaque;
    uint32_t value = s->ctrl.data[offset / 4];

    trace_fsl_caam_ctrl_read(offset, fsl_caam_ctrl_reg_name(offset), value);

    return value;
}

static void fsl_caam_ctrl_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    FslImxCaamState *s = opaque;

    trace_fsl_caam_ctrl_write(offset, fsl_caam_ctrl_reg_name(offset), value);

    s->ctrl.data[offset / 4] = value;
}

static const MemoryRegionOps fsl_caam_ctrl_ops = {
    .read = fsl_caam_ctrl_read,
    .write = fsl_caam_ctrl_write,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const char *fsl_caam_jr_reg_name(hwaddr offset)
{
    switch (offset) {
    /* Input ring */
    case 0x0:
        return " (IRBA_HI)";
    case 0x4:
        return " (IRBA_LO)";
    case 0xc:
        return " (IRS)";
    case 0x14:
        return " (IRSA)";
    case 0x1c:
        return " (IRJA)";

    /* Output ring */
    case 0x20:
        return " (ORBA_HI)";
    case 0x24:
        return " (ORBA_LO)";
    case 0x2c:
        return " (ORS)";
    case 0x34:
        return " (ORJR)";
    case 0x3c:
        return " (ORSF)";

    /* Status/Configuration */
    case 0x44:
        return " (JRSTA)";
    case 0x4c:
        return " (JRINT)";
    case 0x50:
        return " (JRCFG_HI)";
    case 0x54:
        return " (JRCFG_LO)";

    /* Indices. CAAM maintains as "heads" of each queue */
    case 0x5c:
        return " (IRRI)";
    case 0x64:
        return " (ORWI)";

    /* Command/control */
    case 0x6c:
        return " (JRCR)";
    }

    return "";
}

static uint64_t fsl_caam_jr_read(void *opaque, hwaddr offset, unsigned size)
{
    FslImxCaamJrState *s = opaque;
    const hwaddr reg = offset / 4;
    uint32_t value;

    switch (reg) {
    case R_IRSA:
        value = s->data[R_IRS];
        break;
    case R_ORSF:
        value = s->data[R_ORSF];
        break;
    case R_JRINT:
        value = s->data[reg];
        /* qemu_system_vmstop_request(RUN_STATE_PAUSED); */
        break;
    default:
        value = s->data[reg];
        break;
    }

    trace_fsl_caam_jr_read(offset, fsl_caam_jr_reg_name(offset), value);

    return value;
}

static void dump(dma_addr_t addr)
{
    dma_addr_t addr2 = ldl_le_phys(&address_space_memory, addr);
    dma_addr_t val = ldq_le_phys(&address_space_memory, addr2);
    printf("0x%" PRIx64 " -> 0x%" PRIx64 " -> 0x%" PRIx64 "\n", addr, addr2, val);
}

static void fsl_caam_jr_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    FslImxCaamJrState *s = opaque;
    const hwaddr reg = offset / 4;

    trace_fsl_caam_jr_write(offset, fsl_caam_jr_reg_name(offset), value);

    switch (reg) {
    case R_IRBA_LO:
        s->data[reg] = value;
        s->data[R_IRRI] = 0;
        break;
    case R_IRJA:
        dump(s->data[R_IRBA_LO] + 4 * s->data[R_IRRI]);
        stq_le_phys(&address_space_memory, s->data[R_ORBA_LO] + 8 * s->data[R_ORWI],
                    ldq_le_phys(&address_space_memory,
                                s->data[R_IRBA_LO] + 4 * s->data[R_IRRI]));
        dump(s->data[R_ORBA_LO] + 8 * s->data[R_ORWI]);
        s->data[R_IRRI] = (s->data[R_IRRI] + value) % s->data[R_IRS];
        s->data[R_JRINT] |= JRINT_JR_INT;
        s->data[R_ORSF] = value;
        qemu_set_irq(s->irq, !!(s->data[R_JRINT] & 0x1));
        /* qemu_system_vmstop_request(RUN_STATE_PAUSED); */
        break;
    case R_ORBA_LO:
        s->data[reg] = value;
        s->data[R_ORWI] = 0;
        break;
    case R_ORJR:
        s->data[R_ORWI] = (s->data[R_ORWI] + value) % s->data[R_ORS];
        s->data[R_ORSF] -= value;
        break;
    case R_JRCR:
        s->data[R_JRINT] |= JRINT_ERR_HALT_COMPLETE;
        break;
    case R_JRINT:
        s->data[reg] &= ~value;
        qemu_set_irq(s->irq, !!(s->data[reg] & 0x1));
        break;
    default:
        s->data[reg] = value;
    }
}

static const MemoryRegionOps fsl_caam_jr_ops = {
    .read = fsl_caam_jr_read,
    .write = fsl_caam_jr_write,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void fsl_caam_reset_hold(Object *obj, ResetType type)
{
    FslImxCaamState *s = FSL_CAAM(obj);

    memset(s->ctrl.data, 0, sizeof(s->ctrl.data));
    for (int i = 0; i < ARRAY_SIZE(s->jr); i++) {
        memset(s->jr[i].data, 0, sizeof(s->jr[i].data));
    }

    s->ctrl.data[R_CTPR_MS] = CTPR_MS_PS;
    s->ctrl.data[R_RNG_VERSION] = 1;
    s->ctrl.data[R_CCBVID] = 10 << 24;
}

static void fsl_caam_init(Object *obj)
{
    FslImxCaamState *s = FSL_CAAM(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(s);

    memory_region_init(&s->iomem, obj, TYPE_FSL_CAAM, 256 * KiB);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->ctrl.irq);

    memory_region_init_io(&s->ctrl.io, obj, &fsl_caam_ctrl_ops, s,
                          TYPE_FSL_CAAM ".ctrl", sizeof(s->ctrl.data));
    memory_region_add_subregion(&s->iomem, 0, &s->ctrl.io);

    for (int i = 0; i < ARRAY_SIZE(s->jr); i++) {
        memory_region_init_io(&s->jr[i].io, obj, &fsl_caam_jr_ops, &s->jr[i],
                              TYPE_FSL_CAAM ".jr", sizeof(s->jr[i].data));
        memory_region_add_subregion(&s->iomem, (i + 1) * 0x1000, &s->jr[i].io);

        memory_region_init_alias(&s->jr[i].alias, obj, TYPE_FSL_CAAM ".alias",
                                 &s->ctrl.io, 0x600, 0x1000 - 0x600);
        memory_region_add_subregion(&s->iomem, (i + 1) * 0x1000 + 0x600,
                                    &s->jr[i].alias);

        sysbus_init_irq(sbd, &s->jr[i].irq);
    }
}

static const VMStateDescription fsl_caam_vmstate = {
    .name = "fsl-caam",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(ctrl.data, FslImxCaamState, FSL_CAAM_CTRL_ARRAY_SIZE),
        VMSTATE_UINT32_ARRAY(jr[0].data, FslImxCaamState, FSL_CAAM_JR_ARRAY_SIZE),
        VMSTATE_UINT32_ARRAY(jr[1].data, FslImxCaamState, FSL_CAAM_JR_ARRAY_SIZE),
        VMSTATE_UINT32_ARRAY(jr[2].data, FslImxCaamState, FSL_CAAM_JR_ARRAY_SIZE),
        VMSTATE_END_OF_LIST()
    }
};

static void fsl_caam_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->vmsd = &fsl_caam_vmstate;
    rc->phases.hold = fsl_caam_reset_hold;
}

static const TypeInfo fsl_caam_types[] = {
    {
        .name = TYPE_FSL_CAAM,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FslImxCaamState),
        .instance_init = fsl_caam_init,
        .class_init = fsl_caam_class_init,
    }
};

DEFINE_TYPES(fsl_caam_types)
