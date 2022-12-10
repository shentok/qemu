/*
 * QEMU PowerPC e500v2 Local Access Window code
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
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/registerfields.h"
#include "e500_law.h"
#include "trace.h"

#define CCSRBAR 0x0
REG32(CCSRBAR, 0x00)
    FIELD(CCSRBAR, BASE_ADDR, 8, 16)

static uint64_t ppce500_law_read(void *opaque, hwaddr addr, unsigned len)
{
    PPCE500LAWState *s = opaque;
    uint64_t value = 0;

    switch (addr) {
    case CCSRBAR:
        value |= (s->mmio.addr >> 20) << R_CCSRBAR_BASE_ADDR_SHIFT;
        trace_ppce500_ccsr_base_read(value, s->mmio.addr);
        break;

    default:
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented "
                      "[0x%" HWADDR_PRIx "] -> 0\n",
                      __func__, addr);
        break;
    }

    return value;
}

static void ppce500_law_write(void *opaque, hwaddr addr, uint64_t value,
                              unsigned len)
{
    PPCE500LAWState *s = opaque;

    switch (addr) {
    case CCSRBAR:
        memory_region_set_address(&s->mmio,
                                  FIELD_EX64(value, CCSRBAR, BASE_ADDR) << 20);
        trace_ppce500_ccsr_base_write(value,
                        FIELD_EX64(value, CCSRBAR, BASE_ADDR) << 20);
        if (s->ccsrbar_changed_notifier.notify) {
            s->ccsrbar_changed_notifier.notify(&s->ccsrbar_changed_notifier,
                                               s->ccsr);
        }
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
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static void ppce500_ccsr_init(Object *obj)
{
    PPCE500LAWState *s = E500_LAW(obj);

    memory_region_init_io(&s->law_ops, obj, &ppce500_law_ops, s,
                          "e500 LAW control registers", MPC85XX_LAW_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->law_ops);
}

static void ppce500_ccsr_realize(DeviceState *dev, Error **errp)
{
    ERRP_GUARD();
    PPCE500LAWState *s = E500_LAW(dev);

    if (!s->ccsr) {
        error_setg(errp, "%s: ccsr property must be set",
                         object_get_typename(OBJECT(dev)));
        return;
    }
}

static void ppce500_ccsr_reset(DeviceState *dev)
{
    PPCE500LAWState *s = E500_LAW(dev);

    memory_region_set_address(&s->mmio, 0xff700000);
    if (s->ccsrbar_changed_notifier.notify) {
        s->ccsrbar_changed_notifier.notify(&s->ccsrbar_changed_notifier,
                                           s->ccsr);
    }
}

static void ppce500_ccsr_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = ppce500_ccsr_realize;
    dc->reset = ppce500_ccsr_reset;
}

static const TypeInfo ppce500_ccsr_info = {
    .name          = TYPE_E500_LAW,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PPCE500LAWState),
    .instance_init = ppce500_ccsr_init,
    .class_init    = ppce500_ccsr_class_init,
};

static void ppce500_ccsr_register_types(void)
{
    type_register_static(&ppce500_ccsr_info);
}

type_init(ppce500_ccsr_register_types)
