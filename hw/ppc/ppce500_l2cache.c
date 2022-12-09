/*
 * QEMU PowerPC e500v2 Level 2 Cache Registers code
 *
 * Copyright (C) 2022 Bernhard Beschow <shentey@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/sysbus.h"
#include "hw/resettable.h"
#include "qapi/error.h"
#include "qom/object.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "trace.h"

#define MPC8XXX_SRAM_REGS_SIZE     0x1000ULL

#define TYPE_E500_L2CACHE "fsl,p1020-l2-cache-controller"
OBJECT_DECLARE_SIMPLE_TYPE(PPCE500L2CacheState, E500_L2CACHE)

struct PPCE500L2CacheState {
    SysBusDevice parent;

    MemoryRegion mmio;
    MemoryRegion sram;
    qemu_irq irq;
};

static void ppce500_l2cache_io_write(void *opaque, hwaddr addr, uint64_t data,
                                     unsigned size)
{
    PPCE500L2CacheState *s = opaque;

    trace_ppce500_l2cache_io_write(addr, data, size);

    switch (addr) {
    case 0x0:
        memory_region_set_enabled(&s->sram, !!(data & 0x00070000));
        break;
    case 0x100:
        memory_region_set_address(&s->sram, data & 0xffffc000);
        break;
    }
}

static uint64_t ppce500_l2cache_io_read(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t ret = 0;

    trace_ppce500_l2cache_io_read(addr, ret, size);
    return ret;
}

static const MemoryRegionOps l2cache_io_ops = {
    .read = ppce500_l2cache_io_read,
    .write = ppce500_l2cache_io_write,
    .endianness = DEVICE_BIG_ENDIAN,
};

static void ppce500_l2cache_init(Object *obj)
{
    PPCE500L2CacheState *s = E500_L2CACHE(obj);

    memory_region_init_io(&s->mmio, obj, &l2cache_io_ops, obj,
                          "l2 cache control", MPC8XXX_SRAM_REGS_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio);

    memory_region_init_ram(&s->sram, NULL, "e500-sram", 256 * KiB,
                           &error_fatal);
    memory_region_add_subregion(get_system_memory(), 0, &s->sram);

    sysbus_init_irq(SYS_BUS_DEVICE(s), &s->irq);
}

static void ppce500_l2cache_reset_hold(Object *obj, ResetType type)
{
    PPCE500L2CacheState *s = E500_L2CACHE(obj);

    memory_region_set_enabled(&s->sram, false);
    memory_region_set_address(&s->sram, 0);
}

static void ppce500_l2cache_class_init(ObjectClass *klass, const void *data)
{
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    rc->phases.hold = ppce500_l2cache_reset_hold;
}

static const TypeInfo types[] = {
    {
        .name          = TYPE_E500_L2CACHE,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(PPCE500L2CacheState),
        .instance_init = ppce500_l2cache_init,
        .class_init    = ppce500_l2cache_class_init,
    },
    {
        .name          = "fsl,p1022-l2-cache-controller",
        .parent        = TYPE_E500_L2CACHE,
    },
    {
        .name          = "fsl,mpc8572-l2-cache-controller",
        .parent        = TYPE_E500_L2CACHE,
    }
};

DEFINE_TYPES(types);
