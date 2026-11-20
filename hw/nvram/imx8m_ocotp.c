/*
 * i.MX 8M On-Chip OTP Controller
 *
 * Copyright (c) 2026 Bernhard Beschow <shentey@gmail.com>
 *
 * Models the fuse array, the shadow registers and the programming and direct
 * read interfaces of the OCOTP controller of the i.MX 8M Mini and Plus.
 * Fuses can be backed by a block device to make programmed fuses persistent.
 * The shadow registers are loaded from the fuses on reset or when requested
 * via HW_OCOTP_CTRL[RELOAD_SHADOWS].
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/nvram/imx8m_ocotp.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/qdev-properties-system.h"
#include "hw/core/registerfields.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/bswap.h"
#include "qemu/log.h"
#include "qemu/units.h"
#include "system/block-backend.h"

REG32(HW_OCOTP_CTRL, 0x000)
    FIELD(HW_OCOTP_CTRL, WR_UNLOCK, 16, 16)
REG32(HW_OCOTP_CTRL_SET, 0x004)
REG32(HW_OCOTP_CTRL_CLR, 0x008)
REG32(HW_OCOTP_CTRL_TOG, 0x00c)
REG32(HW_OCOTP_TIMING, 0x010)
REG32(HW_OCOTP_DATA, 0x020)
REG32(HW_OCOTP_READ_CTRL, 0x030)
    FIELD(HW_OCOTP_READ_CTRL, READ_FUSE, 0, 1)
REG32(HW_OCOTP_READ_FUSE_DATA, 0x040)
REG32(HW_OCOTP_SW_STICKY, 0x050)
REG32(HW_OCOTP_SCS, 0x060)
REG32(HW_OCOTP_SCS_SET, 0x064)
REG32(HW_OCOTP_SCS_CLR, 0x068)
REG32(HW_OCOTP_SCS_TOG, 0x06c)
REG32(HW_OCOTP_VERSION, 0x090)

#define OCOTP_SHADOW_BASE       0x400
#define OCOTP_SHADOW_STRIDE     0x10
#define OCOTP_WORDS_PER_BANK    4
#define OCOTP_WR_UNLOCK_KEY     0x3e77
#define OCOTP_VERSION           0x06000000

/* The control register layout differs between the i.MX 8M Mini and Plus */
static uint32_t ocotp_addr_mask(IMX8MOcotpState *s)
{
    return s->imx8mp ? 0x1ff : 0xff;
}

static uint32_t ocotp_error_bit(IMX8MOcotpState *s)
{
    return s->imx8mp ? BIT(10) : BIT(9);
}

static uint32_t ocotp_reload_bit(IMX8MOcotpState *s)
{
    return s->imx8mp ? BIT(11) : BIT(10);
}

static void ocotp_reload_shadows(IMX8MOcotpState *s)
{
    memcpy(s->shadow, s->fuses, s->num_words * sizeof(*s->fuses));
}

static void ocotp_program(IMX8MOcotpState *s, uint32_t val)
{
    uint32_t addr = s->ctrl & ocotp_addr_mask(s);

    if (FIELD_EX32(s->ctrl, HW_OCOTP_CTRL, WR_UNLOCK) != OCOTP_WR_UNLOCK_KEY ||
        addr >= s->num_words) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: invalid fuse programming of word 0x%x\n",
                      TYPE_IMX8M_OCOTP, addr);
        s->ctrl |= ocotp_error_bit(s);
        return;
    }

    /* Programming can only set bits */
    s->fuses[addr] |= val;
    s->ctrl = FIELD_DP32(s->ctrl, HW_OCOTP_CTRL, WR_UNLOCK, 0);

    if (s->blk) {
        uint32_t le = cpu_to_le32(s->fuses[addr]);

        if (blk_pwrite(s->blk, addr * sizeof(le), sizeof(le), &le, 0) < 0) {
            qemu_log_mask(LOG_UNIMP, "%s: failed to write fuse 0x%x\n",
                          TYPE_IMX8M_OCOTP, addr);
        }
    }
}

static void ocotp_write_ctrl(IMX8MOcotpState *s, uint32_t val)
{
    s->ctrl = val;

    if (s->ctrl & ocotp_reload_bit(s)) {
        ocotp_reload_shadows(s);
        s->ctrl &= ~ocotp_reload_bit(s);
    }
}

static uint64_t imx8m_ocotp_read(void *opaque, hwaddr offset, unsigned size)
{
    IMX8MOcotpState *s = opaque;

    if (offset >= OCOTP_SHADOW_BASE) {
        uint32_t index = (offset - OCOTP_SHADOW_BASE) / OCOTP_SHADOW_STRIDE;

        if (offset % OCOTP_SHADOW_STRIDE || index >= s->num_words) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: bad read offset 0x%" HWADDR_PRIx
                          "\n", TYPE_IMX8M_OCOTP, offset);
            return 0;
        }
        return s->shadow[index];
    }

    switch (offset) {
    case A_HW_OCOTP_CTRL:
    case A_HW_OCOTP_CTRL_SET:
    case A_HW_OCOTP_CTRL_CLR:
    case A_HW_OCOTP_CTRL_TOG:
        /* Never busy since operations complete immediately */
        return s->ctrl;
    case A_HW_OCOTP_TIMING:
        return s->timing;
    case A_HW_OCOTP_DATA:
        return s->data;
    case A_HW_OCOTP_READ_CTRL:
        return s->read_ctrl;
    case A_HW_OCOTP_READ_FUSE_DATA:
        return s->read_fuse_data;
    case A_HW_OCOTP_SW_STICKY:
        return s->sw_sticky;
    case A_HW_OCOTP_SCS:
    case A_HW_OCOTP_SCS_SET:
    case A_HW_OCOTP_SCS_CLR:
    case A_HW_OCOTP_SCS_TOG:
        return s->scs;
    case A_HW_OCOTP_VERSION:
        return OCOTP_VERSION;
    default:
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented read at 0x%" HWADDR_PRIx
                      "\n", TYPE_IMX8M_OCOTP, offset);
        return 0;
    }
}

static void imx8m_ocotp_write(void *opaque, hwaddr offset, uint64_t value,
                              unsigned size)
{
    IMX8MOcotpState *s = opaque;
    uint32_t val = value;

    if (offset >= OCOTP_SHADOW_BASE) {
        uint32_t index = (offset - OCOTP_SHADOW_BASE) / OCOTP_SHADOW_STRIDE;

        if (offset % OCOTP_SHADOW_STRIDE || index >= s->num_words) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: bad write offset 0x%"
                          HWADDR_PRIx "\n", TYPE_IMX8M_OCOTP, offset);
            return;
        }
        /* Overriding shadow registers is allowed until the next reload */
        s->shadow[index] = val;
        return;
    }

    switch (offset) {
    case A_HW_OCOTP_CTRL:
        ocotp_write_ctrl(s, val);
        break;
    case A_HW_OCOTP_CTRL_SET:
        ocotp_write_ctrl(s, s->ctrl | val);
        break;
    case A_HW_OCOTP_CTRL_CLR:
        ocotp_write_ctrl(s, s->ctrl & ~val);
        break;
    case A_HW_OCOTP_CTRL_TOG:
        ocotp_write_ctrl(s, s->ctrl ^ val);
        break;
    case A_HW_OCOTP_TIMING:
        s->timing = val;
        break;
    case A_HW_OCOTP_DATA:
        s->data = val;
        ocotp_program(s, val);
        break;
    case A_HW_OCOTP_READ_CTRL:
        if (FIELD_EX32(val, HW_OCOTP_READ_CTRL, READ_FUSE)) {
            uint32_t addr = s->ctrl & ocotp_addr_mask(s);

            if (addr < s->num_words) {
                s->read_fuse_data = s->fuses[addr];
            } else {
                s->ctrl |= ocotp_error_bit(s);
            }
        }
        s->read_ctrl = 0;
        break;
    case A_HW_OCOTP_SW_STICKY:
        /* Sticky bits can only be set until the next reset */
        s->sw_sticky |= val;
        break;
    case A_HW_OCOTP_SCS:
        s->scs = val;
        break;
    case A_HW_OCOTP_SCS_SET:
        s->scs |= val;
        break;
    case A_HW_OCOTP_SCS_CLR:
        s->scs &= ~val;
        break;
    case A_HW_OCOTP_SCS_TOG:
        s->scs ^= val;
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented write at 0x%" HWADDR_PRIx
                      "\n", TYPE_IMX8M_OCOTP, offset);
        break;
    }
}

static const MemoryRegionOps imx8m_ocotp_ops = {
    .read = imx8m_ocotp_read,
    .write = imx8m_ocotp_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void imx8m_ocotp_reset_hold(Object *obj, ResetType type)
{
    IMX8MOcotpState *s = IMX8M_OCOTP(obj);

    s->ctrl = 0;
    s->timing = 0;
    s->data = 0;
    s->read_ctrl = 0;
    s->read_fuse_data = 0;
    s->sw_sticky = 0;
    s->scs = 0;
    ocotp_reload_shadows(s);
}

static void imx8m_ocotp_realize(DeviceState *dev, Error **errp)
{
    IMX8MOcotpState *s = IMX8M_OCOTP(dev);
    size_t size;

    if (!s->num_banks) {
        error_setg(errp, "%s: num-banks must be set", TYPE_IMX8M_OCOTP);
        return;
    }

    s->num_words = s->num_banks * OCOTP_WORDS_PER_BANK;
    size = s->num_words * sizeof(*s->fuses);
    s->fuses = g_new0(uint32_t, s->num_words);
    s->shadow = g_new0(uint32_t, s->num_words);

    if (s->blk) {
        int64_t len = blk_getlength(s->blk);

        if (len != size) {
            error_setg(errp, "%s: backing file size %" PRId64 " != %zu",
                       TYPE_IMX8M_OCOTP, len, size);
            return;
        }

        if (blk_set_perm(s->blk, BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE,
                         BLK_PERM_ALL, errp) < 0) {
            return;
        }

        if (blk_pread(s->blk, 0, size, s->fuses, 0) < 0) {
            error_setg(errp, "%s: failed to read backing file",
                       TYPE_IMX8M_OCOTP);
            return;
        }

        for (uint32_t i = 0; i < s->num_words; i++) {
            s->fuses[i] = le32_to_cpu(s->fuses[i]);
        }
    }

    memory_region_init_io(&s->iomem, OBJECT(dev), &imx8m_ocotp_ops, s,
                          TYPE_IMX8M_OCOTP, 64 * KiB);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static const VMStateDescription imx8m_ocotp_vmstate = {
    .name = TYPE_IMX8M_OCOTP,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(ctrl, IMX8MOcotpState),
        VMSTATE_UINT32(timing, IMX8MOcotpState),
        VMSTATE_UINT32(data, IMX8MOcotpState),
        VMSTATE_UINT32(read_ctrl, IMX8MOcotpState),
        VMSTATE_UINT32(read_fuse_data, IMX8MOcotpState),
        VMSTATE_UINT32(sw_sticky, IMX8MOcotpState),
        VMSTATE_UINT32(scs, IMX8MOcotpState),
        VMSTATE_VARRAY_UINT32_ALLOC(fuses, IMX8MOcotpState, num_words, 0,
                                    vmstate_info_uint32, uint32_t),
        VMSTATE_VARRAY_UINT32_ALLOC(shadow, IMX8MOcotpState, num_words, 0,
                                    vmstate_info_uint32, uint32_t),
        VMSTATE_END_OF_LIST()
    },
};

static const Property imx8m_ocotp_properties[] = {
    DEFINE_PROP_UINT32("num-banks", IMX8MOcotpState, num_banks, 0),
    DEFINE_PROP_BOOL("imx8mp", IMX8MOcotpState, imx8mp, false),
    DEFINE_PROP_DRIVE("drive", IMX8MOcotpState, blk),
};

static void imx8m_ocotp_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = imx8m_ocotp_realize;
    rc->phases.hold = imx8m_ocotp_reset_hold;
    dc->vmsd = &imx8m_ocotp_vmstate;
    device_class_set_props(dc, imx8m_ocotp_properties);
    dc->desc = "i.MX 8M On-Chip OTP Controller";
}

static const TypeInfo imx8m_ocotp_types[] = {
    {
        .name          = TYPE_IMX8M_OCOTP,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(IMX8MOcotpState),
        .class_init    = imx8m_ocotp_class_init,
    },
};

DEFINE_TYPES(imx8m_ocotp_types)
