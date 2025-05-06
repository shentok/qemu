/*
 * i.MX 8M Plus DDR Controller
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/imx8mp_ddr.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/units.h"

static uint64_t fsl_imx8mp_ddr_read(void *opaque, hwaddr offset,
                                    unsigned size)
{
    uint32_t value = 0;

    switch (offset) {
    case 0x4:
        value = 1;
        break;
    case 0x1bc:
        value = BIT(0);
        break;
    case 0x324:
        value = BIT(0);
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "[%s]%s: Unimplemented register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX8MP_DDR, __func__,
                      offset);
        break;
    }

    return value;
}


static void fsl_imx8mp_ddr_write(void *opaque, hwaddr offset, uint64_t value,
                                 unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "[%s]%s: Unimplemented register at offset 0x%"
                  HWADDR_PRIx "\n", TYPE_IMX8MP_DDR, __func__,
                  offset);
}

static const struct MemoryRegionOps imx8mp_ddr_ops = {
    .read = fsl_imx8mp_ddr_read,
    .write = fsl_imx8mp_ddr_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
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

static void imx8mp_ddr_realize(DeviceState *dev, Error **errp)
{
    FslImx8mpDdrState *s = IMX8MP_DDR(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &imx8mp_ddr_ops, s,
                          TYPE_IMX8MP_DDR, 4 * MiB);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static const VMStateDescription vmstate_imx8mp_ddr = {
    .name = TYPE_IMX8MP_DDR,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_END_OF_LIST()
    },
};

static void imx8mp_ddr_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = imx8mp_ddr_realize;
    dc->vmsd = &vmstate_imx8mp_ddr;
    dc->desc = "i.MX 8M Plus DDR Controller";
}

static const TypeInfo imx8mp_ddr_types[] = {
    {
        .name          = TYPE_IMX8MP_DDR,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FslImx8mpDdrState),
        .class_init    = imx8mp_ddr_class_init,
    },
};

DEFINE_TYPES(imx8mp_ddr_types)
