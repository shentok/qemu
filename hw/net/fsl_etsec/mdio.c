/*
 * QEMU Freescale eTSEC Emulator
 *
 * Copyright (c) 2024 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/net/lan9118_phy.h"
#include "hw/registerfields.h"
#include "exec/memory.h"
#include "qemu/log.h"
#include "qom/object.h"
#include "../trace.h"

#define MPC8XXX_DDR_REGS_SIZE     0x1000ULL

#define TYPE_ETSEC2_MDIO "fsl-etsec2-mdio"
OBJECT_DECLARE_SIMPLE_TYPE(Etsec2MdioState, ETSEC2_MDIO)

REG32(MDIO_MIIMADD, 0x528)
    FIELD(MDIO_MIIMADD, REG, 0, 5)
    FIELD(MDIO_MIIMADD, PHY, 8, 5)
REG32(MDIO_MIIMCON, 0x52c)
    FIELD(MDIO_MIIMCON, PHY, 0, 16)
REG32(MDIO_MIIMSTAT, 0x530)
REG32(MDIO_MIIMIND, 0x534)

struct Etsec2MdioState {
    SysBusDevice parent;

    MemoryRegion memory_regions[2];
    qemu_irq irqs[3];
    Lan9118PhyState mii[2];
    uint32_t madd;
};

static uint64_t fsl_etsec2_mdio_io_read(void *opaque, hwaddr addr, unsigned size)
{
    Etsec2MdioState *s = opaque;
    uint64_t value = 0xffff;

    switch (addr) {
    case A_MDIO_MIIMADD:
        value = s->madd;
        break;
    case A_MDIO_MIIMSTAT:
        value = lan9118_phy_read(&s->mii[0],
                                 FIELD_EX32(s->madd, MDIO_MIIMADD, REG));
        break;
    case A_MDIO_MIIMIND:
        value = 0;
        break;

    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: unimplemented [0x%" HWADDR_PRIx "] -> 0\n",
                      __func__, addr);
        break;
    }

    trace_fsl_etsec2_mdio_io_read(addr, value, size);

    return value;
}

static void fsl_etsec2_mdio_io_write(void *opaque, hwaddr addr, uint64_t value,
                                     unsigned size)
{
    Etsec2MdioState *s = opaque;

    trace_fsl_etsec2_mdio_io_write(addr, value, size);

    switch (addr) {
    case A_MDIO_MIIMADD:
        s->madd = value;
        break;
    case A_MDIO_MIIMCON:
        lan9118_phy_write(&s->mii[0], FIELD_EX32(s->madd, MDIO_MIIMADD, REG),
                          FIELD_EX32(value, MDIO_MIIMCON, PHY));
        break;

    default:
        qemu_log_mask(LOG_UNIMP,
                      "%s: unimplemented [0x%" HWADDR_PRIx "] <- 0x%" PRIx32 "\n",
                      __func__, addr, (uint32_t)value);
        break;
    }
}

static const MemoryRegionOps fsl_etsec2_mdio_ops = {
    .read = fsl_etsec2_mdio_io_read,
    .write = fsl_etsec2_mdio_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void fsl_etsec2_update_irq(void *opaque, int n, int level)
{
}

static void fsl_etsec2_mdio_init(Object *obj)
{
    Etsec2MdioState *s = ETSEC2_MDIO(obj);
    DeviceState *dev = DEVICE(s);

    memory_region_init_io(&s->memory_regions[0], obj, &fsl_etsec2_mdio_ops, obj,
                          "mdio0", MPC8XXX_DDR_REGS_SIZE);
    memory_region_init(&s->memory_regions[1], obj, "mdio1", 0);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->memory_regions[0]);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->memory_regions[1]);
    memory_region_set_enabled(&s->memory_regions[1], false);

    for (int i = 0; i < ARRAY_SIZE(s->irqs); i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irqs[i]);
    }

    for (int i = 0; i < ARRAY_SIZE(s->mii); i++) {
        qemu_init_irq(&s->mii[i].irq, fsl_etsec2_update_irq, 0, i);
    }
}

static const TypeInfo types[] = {
    {
        .name          = TYPE_ETSEC2_MDIO,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_init = fsl_etsec2_mdio_init,
        .instance_size = sizeof(Etsec2MdioState)
    },
    {
        .name          = "fsl,etsec2-mdio",
        .parent        = TYPE_ETSEC2_MDIO,
    }
};

DEFINE_TYPES(types);
