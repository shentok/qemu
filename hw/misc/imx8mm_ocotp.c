/*
 * i.MX8 M OTP emulation
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/imx8mm_ocotp.h"
#include "hw/resettable.h"
#include "migration/vmstate.h"

static uint64_t fsl_imx8mm_ocotp_read(void *opaque, hwaddr offset,
                                      unsigned size)
{
    FslImx8mmOcotpState *s = opaque;
    uint32_t ret;

    switch (offset) {
    case 0 ... 0xc:
        ret = s->data[0] & ~(uint64_t)0x800;
        break;
    default:
        ret = s->data[offset];
    }

    return ret;
}

static void fsl_imx8mm_ocotp_write(void *opaque, hwaddr offset,
                                   uint64_t value, unsigned size)
{
    FslImx8mmOcotpState *s = opaque;

    switch (offset) {
    case 0:
        s->data[0] = value;
        break;
    case 4:
        s->data[0] |= value;
        break;
    case 8:
        s->data[0] &= value;
        break;
    case 0xc:
        s->data[0] ^= value;
        break;
    default:
        s->data[offset] = value;
        break;
    }
}

static const MemoryRegionOps fsl_imx8mm_ocotp_ops = {
    .read = fsl_imx8mm_ocotp_read,
    .write = fsl_imx8mm_ocotp_write,
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

static void fsl_imx8mm_ocotp_realize(DeviceState *dev, Error **errp)
{
    FslImx8mmOcotpState *s = FSL_IMX8MM_OCOTP(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &fsl_imx8mm_ocotp_ops, s,
                          "ocotp_ctrl", ARRAY_SIZE(s->data));
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
}

static void fsl_imx8mm_ocotp_reset_hold(Object *obj, ResetType type)
{
    FslImx8mmOcotpState *s = FSL_IMX8MM_OCOTP(obj);

    memset(s->data, 0, sizeof(s->data));
    s->data[0x480 / 4] = 1 >> 16;
}

static const VMStateDescription fsl_imx8mm_ocotp_vmstate = {
    .name = "fsl-imx8mm-ocotp",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(data, FslImx8mmOcotpState,
                             FSL_IMX8MM_OCOTP_DATA_SIZE),
        VMSTATE_END_OF_LIST()
    }
};

static void fsl_imx8mm_ocotp_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = fsl_imx8mm_ocotp_realize;
    dc->vmsd = &fsl_imx8mm_ocotp_vmstate;
    rc->phases.hold = fsl_imx8mm_ocotp_reset_hold;
}

static const TypeInfo fsl_imx8mm_ocotp_types[] = {
    {
        .name = TYPE_FSL_IMX8MM_OCOTP,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FslImx8mmOcotpState),
        .class_init = fsl_imx8mm_ocotp_class_init,
    }
};

DEFINE_TYPES(fsl_imx8mm_ocotp_types)
