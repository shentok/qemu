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
#include "hw/core/qdev-properties.h"
#include "hw/core/sysbus.h"
#include "hw/core/resettable.h"
#include "qapi/error.h"
#include "qom/object.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "trace.h"

#define MPC8XXX_SRAM_REGS_SIZE     0x1000ULL

/* L2CTL/L2SR register fields */
#define L2CTL_L2E                  (1u << 31)
#define L2CTL_L2SIZ_SHIFT          28
#define L2CTL_L2SIZ_MASK           (3u << L2CTL_L2SIZ_SHIFT)
#define L2CTL_L2SRAM_MASK          0x00070000

#define TYPE_E500_L2CACHE "fsl,p1020-l2-cache-controller"
OBJECT_DECLARE_SIMPLE_TYPE(PPCE500L2CacheState, E500_L2CACHE)

struct PPCE500L2CacheState {
    SysBusDevice parent;

    MemoryRegion mmio;
    MemoryRegion sram;
    qemu_irq irq;

    uint32_t l2ctl;
    uint32_t cache_size;
};

/*
 * The cache size is reported in the read-only L2SIZ field. Note that the
 * encoding differs between SoCs (on e500v2 SoCs 1 means 256 KiB), so only
 * the sizes known to be valid for the modelled SoCs are accepted.
 */
static uint32_t ppce500_l2cache_size_field(uint32_t cache_size)
{
    switch (cache_size) {
    case 256 * KiB:
        return 1;
    case 512 * KiB:
        return 2;
    case 1024 * KiB:
        return 3;
    default:
        g_assert_not_reached();
    }
}

static void ppce500_l2cache_io_write(void *opaque, hwaddr addr, uint64_t data,
                                     unsigned size)
{
    PPCE500L2CacheState *s = opaque;

    trace_ppce500_l2cache_io_write(addr, data, size);

    switch (addr) {
    case 0x0:
        /* L2SIZ is read-only, everything else is writable */
        s->l2ctl = (data & ~L2CTL_L2SIZ_MASK) | (s->l2ctl & L2CTL_L2SIZ_MASK);
        memory_region_set_enabled(&s->sram, !!(data & L2CTL_L2SRAM_MASK));
        break;
    case 0x100:
        memory_region_set_address(&s->sram, data & 0xffffc000);
        break;
    }
}

static uint64_t ppce500_l2cache_io_read(void *opaque, hwaddr addr, unsigned size)
{
    PPCE500L2CacheState *s = opaque;
    uint64_t ret = 0;

    switch (addr) {
    case 0x0:
        ret = s->l2ctl;
        break;
    }

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

    s->l2ctl = ppce500_l2cache_size_field(s->cache_size) << L2CTL_L2SIZ_SHIFT;
    memory_region_set_enabled(&s->sram, false);
    memory_region_set_address(&s->sram, 0);
}

static const Property ppce500_l2cache_props[] = {
    DEFINE_PROP_UINT32("cache-size", PPCE500L2CacheState, cache_size,
                       256 * KiB),
};

static void ppce500_l2cache_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    device_class_set_props(dc, ppce500_l2cache_props);
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
