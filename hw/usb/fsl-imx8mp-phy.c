/*
 * i.MX 8M Plus USB PHY emulation
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/usb/fsl-imx8mp-phy.h"
#include "hw/registerfields.h"
#include "hw/resettable.h"
#include "migration/vmstate.h"

REG32(PHY_STS0, 0x80)
#define UTMI_CLK_VLD BIT(31)

static uint64_t fsl_imx8mp_usb_phy_read(void *opaque, hwaddr offset,
                                        unsigned size)
{
    FslImx8mpUsbPhyState *s = opaque;
    const int reg = offset / 4;

    if (offset == A_PHY_STS0) {
        return s->data[R_PHY_STS0] | UTMI_CLK_VLD;
    }

    return s->data[reg];
}

static void fsl_imx8mp_usb_phy_write(void *opaque, hwaddr offset,
                                     uint64_t value, unsigned size)
{
    FslImx8mpUsbPhyState *s = opaque;

    s->data[offset / 4] = value;
}

static const MemoryRegionOps fsl_imx8mp_usb_phy_ops = {
    .read = fsl_imx8mp_usb_phy_read,
    .write = fsl_imx8mp_usb_phy_write,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
    .valid = {
        .min_access_size = 1,
        .max_access_size = 8,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void fsl_imx8mp_usb_phy_realize(DeviceState *dev, Error **errp)
{
    FslImx8mpUsbPhyState *s = FSL_IMX8MP_USB_PHY(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &fsl_imx8mp_usb_phy_ops, s,
                          TYPE_FSL_IMX8MP_USB_PHY, 0x100);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
}

static void fsl_imx8mp_usb_phy_reset_hold(Object *obj, ResetType type)
{
    FslImx8mpUsbPhyState *s = FSL_IMX8MP_USB_PHY(obj);

    memset(s->data, 0, sizeof(s->data));
}

static const VMStateDescription fsl_imx8mp_usb_phy_vmstate = {
    .name = "fsl-imx8mp-usb-phy",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(data, FslImx8mpUsbPhyState,
                             FSL_IMX8MP_USB_PHY_NUM_SIZE),
        VMSTATE_END_OF_LIST()
    }
};

static void fsl_imx8mp_usb_phy_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = fsl_imx8mp_usb_phy_realize;
    dc->vmsd = &fsl_imx8mp_usb_phy_vmstate;
    rc->phases.hold = fsl_imx8mp_usb_phy_reset_hold;
}

static const TypeInfo fsl_imx8mp_usb_phy_types[] = {
    {
        .name = TYPE_FSL_IMX8MP_USB_PHY,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FslImx8mpUsbPhyState),
        .class_init = fsl_imx8mp_usb_phy_class_init,
    }
};

DEFINE_TYPES(fsl_imx8mp_usb_phy_types)
