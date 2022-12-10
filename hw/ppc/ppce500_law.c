/*
 * QEMU PowerPC e500v2 Local Access Window code
 *
 * Copyright (C) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "ppce500_law.h"
#include "e500.h"
#include "qemu/log.h"
#include "hw/qdev-properties.h"
#include "hw/registerfields.h"
#include "hw/resettable.h"
#include "target/ppc/cpu.h"
#include "trace.h"

#define MPC85XX_LAW_SIZE 0x1000ULL

REG32(CCSRBAR, 0x00)

static uint64_t ppce500_law_read(void *opaque, hwaddr addr, unsigned len)
{
    PPCE500LAWState *s = opaque;
    uint64_t value = 0;

    switch (addr) {
    case A_CCSRBAR:
        value |= (s->mmio.container->addr >> 12) & 0xffff00;
        trace_ppce500_ccsr_base_read(value, s->mmio.container->addr);
        break;

    default:
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented "
                      "[0x%" HWADDR_PRIx "] -> 0\n",
                      __func__, addr);
        break;
    }

    return value;
}

static void ppce500_handle_ccsrbar_changed(PPCE500LAWState *s)
{
    MemoryRegion *ccsr = s->mmio.container;
    CPUState *cs;

    CPU_FOREACH(cs) {
        CPUPPCState *env = &POWERPC_CPU(cs)->env;
        env->mpic_iack = ccsr->addr + MPC8544_MPIC_REGS_OFFSET + 0xa0;
    }
}

static void ppce500_law_write(void *opaque, hwaddr addr, uint64_t value,
                              unsigned len)
{
    PPCE500LAWState *s = opaque;

    switch (addr) {
    case A_CCSRBAR:
        memory_region_set_address(s->mmio.container,
                                  (value & 0xffff00) << 12);
        ppce500_handle_ccsrbar_changed(s);
        trace_ppce500_ccsr_base_write(value, s->mmio.container->addr);
        break;

    default:
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented "
                      "[0x%" HWADDR_PRIx "] <- 0x%" PRIx32 "\n",
                      __func__, addr, (uint32_t)value);
        break;
    }
}

static const MemoryRegionOps ppce500_law_ops = {
    .read = ppce500_law_read,
    .write = ppce500_law_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    }
};

static void ppce500_law_init(Object *obj)
{
    PPCE500LAWState *s = E500_LAW(obj);

    memory_region_init_io(&s->mmio, obj, &ppce500_law_ops, s,
                          "e500 LAW control registers", MPC85XX_LAW_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio);
}

static void ppce500_law_reset_hold(Object *obj, ResetType type)
{
    PPCE500LAWState *s = E500_LAW(obj);

    memory_region_set_address(s->mmio.container, s->ccsrbar_base);
    ppce500_handle_ccsrbar_changed(s);
}

static const Property ppce500_law_properties[] = {
    DEFINE_PROP_UINT64("ccsrbar-reset", PPCE500LAWState, ccsrbar_base,
                       0xFF700000ULL),
};

static void ppce500_law_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    rc->phases.hold = ppce500_law_reset_hold;

    device_class_set_props(dc, ppce500_law_properties);
}

static const TypeInfo ppce500_law_types[] = {
    {
        .name          = TYPE_E500_LAW,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(PPCE500LAWState),
        .instance_init = ppce500_law_init,
        .class_init    = ppce500_law_class_init,
    },
};

DEFINE_TYPES(ppce500_law_types);
