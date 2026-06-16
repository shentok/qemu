#ifndef IMX53_USB_MISC_H
#define IMX53_USB_MISC_H

#include "hw/core/sysbus.h"
#include "system/memory.h"
#include "qom/object.h"

#define IMX53_USB_MISC_MAX (0x28 / 4)

#define TYPE_IMX53_USB_MISC "imx53.usb.misc"
OBJECT_DECLARE_SIMPLE_TYPE(Imx53UsbMiscState, IMX53_USB_MISC)

struct Imx53UsbMiscState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t regs[IMX53_USB_MISC_MAX];
};

#endif /* IMX53_USB_MISC_H */
