/*
 * i.MX8 M OTP emulation
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/nvram/imx8mm_ocotp.h"
#include "hw/core/resettable.h"
#include "migration/vmstate.h"
#include "trace.h"

static const char *fsl_imx8mm_ocotp_reg_name(hwaddr offset)
{
    switch (offset) {
    case 0x0: return "OCOTP_CTRL";
    case 0x4: return "OCOTP_CTRL_SET";
    case 0x8: return "OCOTP_CTRL_CLR";
    case 0xC: return "OCOTP_CTRL_TOG";
    case 0x10: return "OCOTP_TIMING";
    case 0x20: return "OCOTP_DATA";
    case 0x30: return "OCOTP_READ_CTRL";
    case 0x40: return "OCOTP_READ_FUSE_DATA";
    case 0x90: return "OCOTP_VERSION";
    case 0x400: return "OCOTP_LOCK0";
    case 0x410: return "OCOTP_LOCK1";
    case 0x420: return "OCOTP_UNIQUE_ID0";
    case 0x430: return "OCOTP_UNIQUE_ID1";
    case 0x440: return "OCOTP_DISABLE0";
    case 0x450: return "OCOTP_DISABLE1";
    case 0x460: return "OCOTP_GP4";
    case 0x470: return "OCOTP_BOOT_CFG0";
    case 0x480: return "OCOTP_BOOT_CFG1";
    case 0x490: return "OCOTP_BOOT_CFG2";
    case 0x4A0: return "OCOTP_BOOT_CFG3";
    case 0x4B0: return "OCOTP_BOOT_CFG4";
    case 0x620: return "OCOTP_USB_ID";
    case 0x640: return "OCOTP_MAC_ADDR0";
    case 0x650: return "OCOTP_MAC_ADDR1";
    case 0x660: return "OCOTP_MAC_ADDR2";
    case 0x780: return "OCOTP_GP1_0";
    case 0x790: return "OCOTP_GP1_1";
    case 0x7A0: return "OCOTP_GP2_0";
    case 0x7B0: return "OCOTP_GP2_1";
    case 0xE40: return "OCOTP_GP6_0";
    case 0xE50: return "OCOTP_GP6_1";
    case 0xE60: return "OCOTP_GP6_2";
    case 0xE70: return "OCOTP_GP6_3";
    case 0xE80: return "OCOTP_GP7_0";
    case 0xE90: return "OCOTP_GP7_1";
    case 0xEA0: return "OCOTP_GP7_2";
    case 0xEB0: return "OCOTP_GP7_3";
    case 0xEC0: return "OCOTP_GP8_0";
    case 0xED0: return "OCOTP_GP8_1";
    case 0xEE0: return "OCOTP_GP8_2";
    case 0xEF0: return "OCOTP_GP8_3";
    case 0xF00: return "OCOTP_GP9_0";
    case 0xF10: return "OCOTP_GP9_1";
    case 0xF20: return "OCOTP_GP9_2";
    case 0xF30: return "OCOTP_GP9_3";
    }

    return "OCOTP_unknown";
}

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
        ret = s->data[offset / 4];
    }

    trace_fsl_imx8mm_ocotp_read(offset, fsl_imx8mm_ocotp_reg_name(offset), ret);

    return ret;
}

static void fsl_imx8mm_ocotp_write(void *opaque, hwaddr offset,
                                   uint64_t val, unsigned size)
{
    FslImx8mmOcotpState *s = opaque;

    trace_fsl_imx8mm_ocotp_write(offset, fsl_imx8mm_ocotp_reg_name(offset), val);

    switch (offset) {
    case 0:
        s->data[0] = val;
        break;
    case 4:
        s->data[0] |= val;
        break;
    case 8:
        s->data[0] &= val;
        break;
    case 0xc:
        s->data[0] ^= val;
        break;
    default:
        s->data[offset / 4] = val;
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
                          "ocotp_ctrl", ARRAY_SIZE(s->data) * 4);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
}

static void fsl_imx8mm_ocotp_reset_hold(Object *obj, ResetType type)
{
    FslImx8mmOcotpState *s = FSL_IMX8MM_OCOTP(obj);

    memset(s->data, 0, sizeof(s->data));
    s->data[0x10 / 4] = 0x01481299;
    s->data[0x90 / 4] = 0x04000000;
    s->data[0x410 / 4] = 0xFFFFFFFF;
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
