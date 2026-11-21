/*
 * i.MX IC Identification Module (IIM)
 *
 * Copyright (c) 2026 Bernhard Beschow <shentey@gmail.com>
 *
 * Models the fuse banks of the IIM of the i.MX 5 series with their explicit
 * sense, programming and override interfaces. Each fuse row holds eight
 * fuses. Fuses can be backed by a block device, one byte per row, to make
 * programmed fuses persistent. Fuse protection and the interrupt are not
 * modelled.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/nvram/imx_iim.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/qdev-properties-system.h"
#include "hw/core/registerfields.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/units.h"
#include "system/block-backend.h"

REG32(IIM_STAT, 0x00)
    FIELD(IIM_STAT, SNSD, 0, 1)
    FIELD(IIM_STAT, PRGD, 1, 1)
REG32(IIM_STATM, 0x04)
REG32(IIM_ERR, 0x08)
    FIELD(IIM_ERR, WPE, 6, 1)
    FIELD(IIM_ERR, PRGE, 7, 1)
REG32(IIM_EMASK, 0x0c)
REG32(IIM_FCTL, 0x10)
    FIELD(IIM_FCTL, PRG, 0, 1)
    FIELD(IIM_FCTL, ESNS_1, 1, 1)
    FIELD(IIM_FCTL, ESNS_0, 2, 1)
    FIELD(IIM_FCTL, ESNS_N, 3, 1)
REG32(IIM_UA, 0x14)
    FIELD(IIM_UA, ROWH, 0, 3)
    FIELD(IIM_UA, BANK, 3, 3)
REG32(IIM_LA, 0x18)
    FIELD(IIM_LA, BIT, 0, 3)
    FIELD(IIM_LA, ROWL, 3, 5)
REG32(IIM_SDAT, 0x1c)
REG32(IIM_PREV, 0x20)
REG32(IIM_SREV, 0x24)
REG32(IIM_PRG_P, 0x28)
REG32(IIM_SCS0, 0x2c)
REG32(IIM_SCS3, 0x38)

#define IIM_BANK_BASE           0x800
#define IIM_BANK_SIZE           0x400
#define IIM_PRG_P_UNLOCK        0xaa

/* Operations started via IIM_FCTL complete immediately */
#define IIM_FCTL_OPS            (R_IIM_FCTL_PRG_MASK | \
                                 R_IIM_FCTL_ESNS_1_MASK | \
                                 R_IIM_FCTL_ESNS_0_MASK | \
                                 R_IIM_FCTL_ESNS_N_MASK)

/* Returns the index of the row addressed by IIM_UA and IIM_LA, or -1 */
static int iim_addressed_row(IMXIIMState *s)
{
    uint32_t bank = FIELD_EX32(s->ua, IIM_UA, BANK);
    uint32_t row = FIELD_EX32(s->ua, IIM_UA, ROWH) << 5 |
                   FIELD_EX32(s->la, IIM_LA, ROWL);

    if (bank >= s->num_banks || row >= IMX_IIM_ROWS_PER_BANK) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: bank %u row %u doesn't exist\n",
                      TYPE_IMX_IIM, bank, row);
        return -1;
    }

    return bank * IMX_IIM_ROWS_PER_BANK + row;
}

static void iim_program(IMXIIMState *s)
{
    int index = iim_addressed_row(s);

    if (s->prg_p != IIM_PRG_P_UNLOCK) {
        s->err |= R_IIM_ERR_WPE_MASK;
        return;
    }
    if (index < 0) {
        s->err |= R_IIM_ERR_PRGE_MASK;
        return;
    }

    /* Programming blows a single fuse */
    s->fuses[index] |= BIT(FIELD_EX32(s->la, IIM_LA, BIT));
    s->shadow[index] = s->fuses[index];
    s->stat |= R_IIM_STAT_PRGD_MASK;

    if (s->blk && blk_pwrite(s->blk, index, 1, &s->fuses[index], 0) < 0) {
        qemu_log_mask(LOG_UNIMP, "%s: failed to write fuse row %d\n",
                      TYPE_IMX_IIM, index);
    }
}

static void iim_sense(IMXIIMState *s)
{
    int index = iim_addressed_row(s);

    s->sdat = index < 0 ? 0 : s->fuses[index];
    s->stat |= R_IIM_STAT_SNSD_MASK;
}

static void iim_write_fctl(IMXIIMState *s, uint32_t val)
{
    if (val & R_IIM_FCTL_PRG_MASK) {
        iim_program(s);
    } else if (val & (R_IIM_FCTL_ESNS_1_MASK | R_IIM_FCTL_ESNS_0_MASK |
                      R_IIM_FCTL_ESNS_N_MASK)) {
        iim_sense(s);
    }

    s->fctl = val & ~IIM_FCTL_OPS;
}

/* Returns the index of the row of a bank register, or -1 */
static int iim_bank_row(IMXIIMState *s, hwaddr offset)
{
    uint32_t bank = (offset - IIM_BANK_BASE) / IIM_BANK_SIZE;
    uint32_t row = (offset - IIM_BANK_BASE) % IIM_BANK_SIZE / 4;

    if (bank >= s->num_banks || row >= IMX_IIM_ROWS_PER_BANK) {
        return -1;
    }

    return bank * IMX_IIM_ROWS_PER_BANK + row;
}

static uint64_t imx_iim_read(void *opaque, hwaddr offset, unsigned size)
{
    IMXIIMState *s = opaque;

    if (offset >= IIM_BANK_BASE) {
        int index = iim_bank_row(s, offset);

        if (index < 0) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: bad read offset 0x%"
                          HWADDR_PRIx "\n", TYPE_IMX_IIM, offset);
            return 0;
        }
        return s->shadow[index];
    }

    switch (offset) {
    case A_IIM_STAT:
        /* Never busy since operations complete immediately */
        return s->stat;
    case A_IIM_STATM:
        return s->statm;
    case A_IIM_ERR:
        return s->err;
    case A_IIM_EMASK:
        return s->emask;
    case A_IIM_FCTL:
        return s->fctl;
    case A_IIM_UA:
        return s->ua;
    case A_IIM_LA:
        return s->la;
    case A_IIM_SDAT:
        return s->sdat;
    case A_IIM_PREV:
        return s->prev;
    case A_IIM_SREV:
        return s->srev;
    case A_IIM_PRG_P:
        return s->prg_p;
    case A_IIM_SCS0 ... A_IIM_SCS3:
        return s->scs[(offset - A_IIM_SCS0) / 4];
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: bad read offset 0x%" HWADDR_PRIx
                      "\n", TYPE_IMX_IIM, offset);
        return 0;
    }
}

static void imx_iim_write(void *opaque, hwaddr offset, uint64_t value,
                          unsigned size)
{
    IMXIIMState *s = opaque;
    uint32_t val = value & 0xff;

    if (offset >= IIM_BANK_BASE) {
        int index = iim_bank_row(s, offset);

        if (index < 0) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: bad write offset 0x%"
                          HWADDR_PRIx "\n", TYPE_IMX_IIM, offset);
            return;
        }
        /* Overrides the value read until the next reset */
        s->shadow[index] = val;
        return;
    }

    switch (offset) {
    case A_IIM_STAT:
        s->stat &= ~val;
        break;
    case A_IIM_STATM:
        s->statm = val;
        break;
    case A_IIM_ERR:
        s->err &= ~val;
        break;
    case A_IIM_EMASK:
        s->emask = val;
        break;
    case A_IIM_FCTL:
        iim_write_fctl(s, val);
        break;
    case A_IIM_UA:
        s->ua = val & (R_IIM_UA_BANK_MASK | R_IIM_UA_ROWH_MASK);
        break;
    case A_IIM_LA:
        s->la = val;
        break;
    case A_IIM_PRG_P:
        s->prg_p = val;
        break;
    case A_IIM_SCS0 ... A_IIM_SCS3:
        s->scs[(offset - A_IIM_SCS0) / 4] = val;
        break;
    case A_IIM_SDAT:
    case A_IIM_PREV:
    case A_IIM_SREV:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: write to read-only register 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX_IIM, offset);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: bad write offset 0x%" HWADDR_PRIx
                      "\n", TYPE_IMX_IIM, offset);
        break;
    }
}

static const MemoryRegionOps imx_iim_ops = {
    .read = imx_iim_read,
    .write = imx_iim_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void imx_iim_reset_hold(Object *obj, ResetType type)
{
    IMXIIMState *s = IMX_IIM(obj);

    s->stat = 0;
    s->statm = 0;
    s->err = 0;
    s->emask = 0;
    s->fctl = 0;
    s->ua = 0;
    s->la = 0;
    s->sdat = 0;
    s->prg_p = 0;
    memset(s->scs, 0, sizeof(s->scs));
    memcpy(s->shadow, s->fuses, s->num_rows);
}

static void imx_iim_realize(DeviceState *dev, Error **errp)
{
    IMXIIMState *s = IMX_IIM(dev);

    if (!s->num_banks || s->num_banks > 8) {
        error_setg(errp, "%s: num-banks must be between 1 and 8",
                   TYPE_IMX_IIM);
        return;
    }

    s->num_rows = s->num_banks * IMX_IIM_ROWS_PER_BANK;
    s->fuses = g_malloc0(s->num_rows);
    s->shadow = g_malloc0(s->num_rows);

    if (s->blk) {
        int64_t len = blk_getlength(s->blk);

        /* Raw images are rounded up to whole sectors */
        if (len < s->num_rows) {
            error_setg(errp, "%s: backing file size %" PRId64 " < %u",
                       TYPE_IMX_IIM, len, s->num_rows);
            return;
        }

        if (blk_set_perm(s->blk, BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE,
                         BLK_PERM_ALL, errp) < 0) {
            return;
        }

        if (blk_pread(s->blk, 0, s->num_rows, s->fuses, 0) < 0) {
            error_setg(errp, "%s: failed to read backing file", TYPE_IMX_IIM);
            return;
        }
    }

    memory_region_init_io(&s->iomem, OBJECT(dev), &imx_iim_ops, s,
                          TYPE_IMX_IIM, 16 * KiB);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static const VMStateDescription imx_iim_vmstate = {
    .name = TYPE_IMX_IIM,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(stat, IMXIIMState),
        VMSTATE_UINT32(statm, IMXIIMState),
        VMSTATE_UINT32(err, IMXIIMState),
        VMSTATE_UINT32(emask, IMXIIMState),
        VMSTATE_UINT32(fctl, IMXIIMState),
        VMSTATE_UINT32(ua, IMXIIMState),
        VMSTATE_UINT32(la, IMXIIMState),
        VMSTATE_UINT32(sdat, IMXIIMState),
        VMSTATE_UINT32(prg_p, IMXIIMState),
        VMSTATE_UINT32_ARRAY(scs, IMXIIMState, 4),
        VMSTATE_VBUFFER_ALLOC_UINT32(fuses, IMXIIMState, 0, NULL, num_rows),
        VMSTATE_VBUFFER_ALLOC_UINT32(shadow, IMXIIMState, 0, NULL, num_rows),
        VMSTATE_END_OF_LIST()
    },
};

static const Property imx_iim_properties[] = {
    DEFINE_PROP_UINT32("num-banks", IMXIIMState, num_banks, 0),
    DEFINE_PROP_UINT8("prev", IMXIIMState, prev, 0),
    DEFINE_PROP_UINT8("srev", IMXIIMState, srev, 0),
    DEFINE_PROP_DRIVE("drive", IMXIIMState, blk),
};

static void imx_iim_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = imx_iim_realize;
    rc->phases.hold = imx_iim_reset_hold;
    dc->vmsd = &imx_iim_vmstate;
    device_class_set_props(dc, imx_iim_properties);
    dc->desc = "i.MX IC Identification Module";
}

static const TypeInfo imx_iim_types[] = {
    {
        .name          = TYPE_IMX_IIM,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(IMXIIMState),
        .class_init    = imx_iim_class_init,
    },
};

DEFINE_TYPES(imx_iim_types)
