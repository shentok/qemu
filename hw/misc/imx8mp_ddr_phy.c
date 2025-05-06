/*
 * i.MX 8M Plus DDR PHY
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/imx8mp_ddr_phy.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/units.h"

static uint64_t fsl_imx8mp_ddr_phy_read(void *opaque, hwaddr offset,
                                        unsigned size)
{
    FslImx8mpDdrPhyState *s = opaque;
    uint32_t value = 0;

    switch (offset) {
    case 0x10:
        value = BIT(0);
        break;
    case 0x340010:
        value = s->reg_340010;
        break;
    case 0x3400c8:
        value = 7;
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "[%s]%s: Unimplemented register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX8MP_DDR_PHY, __func__,
                      offset);
        break;
    }

    return value;
}


static void fsl_imx8mp_ddr_phy_write(void *opaque, hwaddr offset,
                                     uint64_t value, unsigned size)
{
    FslImx8mpDdrPhyState *s = opaque;

    switch (offset) {
    case 0x340264:
        if (value == 0) {
            s->reg_340010 = 0;
        }
        break;
    case 0x3400c4:
        if (value == 0) {
            s->reg_340010 = 1;
        }
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "[%s]%s: Unimplemented register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX8MP_DDR_PHY, __func__,
                      offset);
        break;
    }
}

static const struct MemoryRegionOps imx8mp_ddr_phy_ops = {
    .read = fsl_imx8mp_ddr_phy_read,
    .write = fsl_imx8mp_ddr_phy_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void imx8mp_ddr_phy_realize(DeviceState *dev, Error **errp)
{
    FslImx8mpDdrPhyState *s = IMX8MP_DDR_PHY(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &imx8mp_ddr_phy_ops, s,
                          TYPE_IMX8MP_DDR_PHY, 16 * MiB);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static const VMStateDescription vmstate_imx8mp_ddr_phy = {
    .name = TYPE_IMX8MP_DDR_PHY,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_END_OF_LIST()
    },
};

static void imx8mp_ddr_phy_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = imx8mp_ddr_phy_realize;
    dc->vmsd = &vmstate_imx8mp_ddr_phy;
    dc->desc = "i.MX 8M Plus DDR PHY";
}

static const TypeInfo imx8mp_ddr_phy_types[] = {
    {
        .name          = TYPE_IMX8MP_DDR_PHY,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FslImx8mpDdrPhyState),
        .class_init    = imx8mp_ddr_phy_class_init,
    },
};

DEFINE_TYPES(imx8mp_ddr_phy_types)
