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

REG32(LAWBAR, 0x00)
    FIELD(LAWBAR, BASE_ADDR, 0, 24)

#define LAWBAR0 0xc08
#define LAWBAR1 0xc28
#define LAWBAR2 0xc48
#define LAWBAR3 0xc68
#define LAWBAR4 0xc88
#define LAWBAR5 0xca8
#define LAWBAR6 0xcc8
#define LAWBAR7 0xce8
#define LAWBAR8 0xd08
#define LAWBAR9 0xd28
#define LAWBAR10 0xd48
#define LAWBAR11 0xd68

REG32(LAWAR, 0x00)
    FIELD(LAWAR, EN, 31, 1)
    FIELD(LAWAR, TGT_ID, 20, 5)
    FIELD(LAWAR, SIZE, 0, 7)

#define LAWAR0 0xc10
#define LAWAR1 0xc30
#define LAWAR2 0xc50
#define LAWAR3 0xc70
#define LAWAR4 0xc90
#define LAWAR5 0xcb0
#define LAWAR6 0xcd0
#define LAWAR7 0xcf0
#define LAWAR8 0xd10
#define LAWAR9 0xd30
#define LAWAR10 0xd50
#define LAWAR11 0xd70

static const char *law_target_interface[] = {
    "PCI",
    "PCI Express 2",
    "PCI Express 1",
    "PCI Express 3",
    "Local bus memory controller",
    "5 (reserved)",
    "6 (reserved)",
    "7 (reserved)",
    "8 (reserved)",
    "9 (reserved)",
    "10 (reserved)",
    "11 (reserved)",
    "12 (reserved)",
    "13 (reserved)",
    "14 (reserved)",
    "DDR SDRAM",
    "16 (reserved)",
    "17 (reserved)",
    "18 (reserved)",
    "19 (reserved)",
    "20 (reserved)",
    "21 (reserved)",
    "22 (reserved)",
    "23 (reserved)",
    "24 (reserved)",
    "25 (reserved)",
    "26 (reserved)",
    "27 (reserved)",
    "28 (reserved)",
    "29 (reserved)",
    "30 (reserved)",
    "31 (reserved)",
};

static MemoryRegion *check_update_unimplemented(PPCE500LAWState *s)
{
    return NULL;
}

typedef MemoryRegion *(*ppce500_check_update_fn)(PPCE500LAWState *s);

static ppce500_check_update_fn ppce500_check_update_fns[32] = {
    [0] = check_update_unimplemented,
    [1] = check_update_unimplemented,
    [2] = check_update_unimplemented,
    [3] = check_update_unimplemented,
    [4] = check_update_unimplemented,
    [15] = check_update_unimplemented,
};

static void ppce500_check_update(PPCE500LAWState *s, LawInfo *info)
{
    uint64_t offset = FIELD_EX64(info->bar, LAWBAR, BASE_ADDR) << 12;
    uint64_t size = 2ULL << FIELD_EX32(info->attributes, LAWAR, SIZE);
    size_t tgt_id = FIELD_EX32(info->attributes, LAWAR, TGT_ID);
    ppce500_check_update_fn check_update = ppce500_check_update_fns[tgt_id];
    MemoryRegion *mr;

    if (info->mr.container) {
        memory_region_del_subregion(s->system_memory, &info->mr);
    }
    object_unparent(OBJECT(&info->mr));

    if (!check_update) {
        return;
    }

    mr = check_update(s);

    if (!mr) {
        /* unimplemented */
        return;
    }

    memory_region_init_alias(&info->mr, OBJECT(s), "LAW", mr, offset, size);
    memory_region_add_subregion(s->system_memory, offset, &info->mr);
}

static uint64_t ppce500_law_read(void *opaque, hwaddr addr, unsigned len)
{
    PPCE500LAWState *s = opaque;
    uint64_t value = 0;
    size_t index;

    switch (addr) {
    case CCSRBAR:
        value |= (s->mmio.addr >> 20) << R_CCSRBAR_BASE_ADDR_SHIFT;
        trace_ppce500_ccsr_base_read(value, s->mmio.addr);
        break;

    case LAWBAR0:
    case LAWBAR1:
    case LAWBAR2:
    case LAWBAR3:
    case LAWBAR4:
    case LAWBAR5:
    case LAWBAR6:
    case LAWBAR7:
    case LAWBAR8:
    case LAWBAR9:
    case LAWBAR10:
    case LAWBAR11:
        index = (addr - LAWBAR0) / (LAWBAR1 - LAWBAR0);
        value = s->law_info[index].bar;
        trace_ppce500_law_bar_read(index, value,
                    FIELD_EX64(value, LAWBAR, BASE_ADDR) << 12);
        break;

    case LAWAR0:
    case LAWAR1:
    case LAWAR2:
    case LAWAR3:
    case LAWAR4:
    case LAWAR5:
    case LAWAR6:
    case LAWAR7:
    case LAWAR8:
    case LAWAR9:
    case LAWAR10:
    case LAWAR11:
        index = (addr - LAWAR0) / (LAWAR1 - LAWAR0);
        value = s->law_info[index].attributes;
        trace_ppce500_law_attr_read(index, value,
                    law_target_interface[FIELD_EX32(value, LAWAR, TGT_ID)],
                    2ULL << FIELD_EX32(value, LAWAR, SIZE));
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
    size_t index;

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

    case LAWBAR0:
    case LAWBAR1:
    case LAWBAR2:
    case LAWBAR3:
    case LAWBAR4:
    case LAWBAR5:
    case LAWBAR6:
    case LAWBAR7:
    case LAWBAR8:
    case LAWBAR9:
    case LAWBAR10:
    case LAWBAR11:
        index = (addr - LAWBAR0) / (LAWBAR1 - LAWBAR0);
        trace_ppce500_law_bar_write(index, value,
                    FIELD_EX64(value, LAWBAR, BASE_ADDR) << 12);
        s->law_info[index].bar = value;
        ppce500_check_update(s, &s->law_info[index]);
        break;

    case LAWAR0:
    case LAWAR1:
    case LAWAR2:
    case LAWAR3:
    case LAWAR4:
    case LAWAR5:
    case LAWAR6:
    case LAWAR7:
    case LAWAR8:
    case LAWAR9:
    case LAWAR10:
    case LAWAR11:
        index = (addr - LAWAR0) / (LAWAR1 - LAWAR0);
        trace_ppce500_law_attr_write(index, value,
                    law_target_interface[FIELD_EX32(value, LAWAR, TGT_ID)],
                    2ULL << FIELD_EX32(value, LAWAR, SIZE));
        s->law_info[index].attributes = value;
        ppce500_check_update(s, &s->law_info[index]);
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

    if (!s->system_memory) {
        error_setg(errp, "%s: system_memory property must be set",
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
    memset(s->law_info, 0, sizeof(s->law_info));
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
