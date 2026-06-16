/*
 * i.MX USB PHY
 *
 * Copyright (c) 2020 Guenter Roeck <linux@roeck-us.net>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 * We need to implement basic reset control in the PHY control register.
 * For everything else, it is sufficient to set whatever is written.
 */

#include "qemu/osdep.h"
#include "hw/usb/imx53-usb-misc.h"
#include "hw/core/registerfields.h"
#include "migration/vmstate.h"
#include "qemu/module.h"
#include "trace.h"

REG32(USB_USB_CTRL_0,          0x0000)
REG32(USB_USB_OTG_PHY_CTRL_0,  0x0008)
REG32(USB_USB_OTG_PHY_CTRL_1,  0x000C)
REG32(USB_USB_CTRL_1,          0x0010)
REG32(USB_USB_UH2_CTRL,        0x0014)
REG32(USB_USB_UH3_CTRL,        0x0018)
REG32(USB_USB_UH1_PHY_CTRL_0,  0x001C)
REG32(USB_USB_UH1_PHY_CTRL_1,  0x0020)
REG32(USB_USB_CLKONOFF_CTRL,   0x0024)

static const char *imx53_usb_misc_reg_name(uint32_t index)
{
    switch (index) {
    case R_USB_USB_CTRL_0:
        return "USB_USB_CTRL_0";

    case R_USB_USB_OTG_PHY_CTRL_0:
        return "USB_USB_OTG_PHY_CTRL_0";

    case R_USB_USB_OTG_PHY_CTRL_1:
        return "USB_USB_OTG_PHY_CTRL_1";

    case R_USB_USB_CTRL_1:
        return "USB_USB_CTRL_1";

    case R_USB_USB_UH2_CTRL:
        return "USB_USB_UH2_CTRL";

    case R_USB_USB_UH3_CTRL:
        return "USB_USB_UH3_CTRL";

    case R_USB_USB_UH1_PHY_CTRL_0:
        return "USB_USB_UH1_PHY_CTRL_0";

    case R_USB_USB_UH1_PHY_CTRL_1:
        return "USB_USB_UH1_PHY_CTRL_1";

    case R_USB_USB_CLKONOFF_CTRL:
        return "USB_USB_CLKONOFF_CTRL";

    default:
        return "UNKNOWN_USB_MISC_REG";
    }
}

static void imx53_usb_misc_softreset(Imx53UsbMiscState *s)
{
    s->regs[R_USB_USB_CTRL_0]         = 0x40024012;
    s->regs[R_USB_USB_OTG_PHY_CTRL_0] = 0x80001400;
    s->regs[R_USB_USB_OTG_PHY_CTRL_1] = 0x00541402;
    s->regs[R_USB_USB_CTRL_1]         = 0x00000000;
    s->regs[R_USB_USB_UH2_CTRL]       = 0x00001402;
    s->regs[R_USB_USB_UH3_CTRL]       = 0x00001402;
    s->regs[R_USB_USB_UH1_PHY_CTRL_0] = 0x80001408;
    s->regs[R_USB_USB_UH1_PHY_CTRL_1] = 0x00541401;
    s->regs[R_USB_USB_CLKONOFF_CTRL]  = 0x89801042;
}

static void imx53_usb_misc_reset(DeviceState *dev)
{
    Imx53UsbMiscState *s = IMX53_USB_MISC(dev);

    imx53_usb_misc_softreset(s);
}

static uint64_t imx53_usb_misc_read(void *opaque, hwaddr offset, unsigned size)
{
    Imx53UsbMiscState *s = opaque;
    uint32_t index = offset >> 2;
    uint32_t value;

    value = s->regs[index];

    trace_imx53_usb_misc_read(imx53_usb_misc_reg_name(index), offset, value);

    return value;
}

static void imx53_usb_misc_write(void *opaque, hwaddr offset, uint64_t value,
                                 unsigned size)
{
    Imx53UsbMiscState *s = opaque;
    uint32_t index = offset >> 2;

    trace_imx53_usb_misc_write(imx53_usb_misc_reg_name(index), offset, value);

    s->regs[index] = value;
}

static const struct MemoryRegionOps imx53_usb_misc_ops = {
    .read = imx53_usb_misc_read,
    .write = imx53_usb_misc_write,
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

static void imx53_usb_misc_realize(DeviceState *dev, Error **errp)
{
    Imx53UsbMiscState *s = IMX53_USB_MISC(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &imx53_usb_misc_ops, s,
                          TYPE_IMX53_USB_MISC, sizeof(s->regs));
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
}

static const VMStateDescription vmstate_imx53_usb_misc_ = {
    .name = TYPE_IMX53_USB_MISC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, Imx53UsbMiscState, IMX53_USB_MISC_MAX),
        VMSTATE_END_OF_LIST()
    },
};

static void imx53_usb_misc_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, imx53_usb_misc_reset);
    dc->vmsd = &vmstate_imx53_usb_misc_;
    dc->desc = "i.MX53 USB Non-Core Module";
    dc->realize = imx53_usb_misc_realize;
}

static const TypeInfo imx53_usb_misc_info = {
    .name          = TYPE_IMX53_USB_MISC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Imx53UsbMiscState),
    .class_init    = imx53_usb_misc_class_init,
};

static void imx53_usb_misc_register_types(void)
{
    type_register_static(&imx53_usb_misc_info);
}

type_init(imx53_usb_misc_register_types)
