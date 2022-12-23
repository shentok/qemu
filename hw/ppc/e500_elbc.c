/*
 * QEMU PowerQUICC III Enhanced Local Bus Controller code
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
#include "qemu/units.h"
#include "hw/registerfields.h"
#include "e500_elbc.h"
#include "trace.h"

#define MPC85XX_ELBC_SIZE          0x1000ULL

#define ELBC_BR0 0x0
#define ELBC_BR1 0x8
#define ELBC_BR2 0x10
#define ELBC_BR3 0x18
#define ELBC_BR4 0x20
#define ELBC_BR5 0x28
#define ELBC_BR6 0x30
#define ELBC_BR7 0x38
REG32(ELBC_BR, 0x00)
    FIELD(ELBC_BR, BA, 15, 17)
    FIELD(ELBC_BR, PS, 11, 2)
    FIELD(ELBC_BR, MSEL, 5, 3)
    FIELD(ELBC_BR, V, 0, 1)

#define ELBC_OR0 0x4
#define ELBC_OR1 0xc
#define ELBC_OR2 0x14
#define ELBC_OR3 0x1c
#define ELBC_OR4 0x24
#define ELBC_OR5 0x2c
#define ELBC_OR6 0x34
#define ELBC_OR7 0x3c
REG32(ELBC_OR, 0x00)
    FIELD(ELBC_OR, AM, 15, 17)

#define ELBC_LBCR 0xd0

static const char *elbc_port_size[] = {
    "0 (reserved)",
    "8",
    "16",
    "32"
};

static const char *elbc_target_interface[] = {
    "GPCM",
    "FCM",
    "2 (reserved)",
    "3 (reserved)",
    "UPMA",
    "UPMB",
    "UPMC",
    "7 (reserved)",
};

static uint64_t ppce500_elbc_read(void *opaque, hwaddr addr, unsigned len)
{
    PPCE500ELbcState *s = opaque;
    uint64_t value = 0;
    size_t index;

    switch (addr) {
    case ELBC_LBCR:
        trace_ppce500_elbc_lbcr_read(0);
        break;

    case ELBC_BR0:
    case ELBC_BR1:
    case ELBC_BR2:
    case ELBC_BR3:
    case ELBC_BR4:
    case ELBC_BR5:
    case ELBC_BR6:
    case ELBC_BR7:
        index = (addr - ELBC_BR0) / (ELBC_BR1 - ELBC_BR0);
        value = s->chip_selects[index].base;
        trace_ppce500_elbc_br_read(
                    index, value,
                    FIELD_EX32(value, ELBC_BR, BA) << 15,
                    elbc_port_size[FIELD_EX32(value, ELBC_BR, PS)],
                    elbc_target_interface[FIELD_EX32(value, ELBC_BR, MSEL)]);
        break;

    case ELBC_OR0:
    case ELBC_OR1:
    case ELBC_OR2:
    case ELBC_OR3:
    case ELBC_OR4:
    case ELBC_OR5:
    case ELBC_OR6:
    case ELBC_OR7:
        index = (addr - ELBC_OR0) / (ELBC_OR1 - ELBC_OR0);
        value = s->chip_selects[index].options;
        trace_ppce500_elbc_or_read(index, value,
                                   FIELD_EX32(value, ELBC_OR, AM) << 15);
        break;

    default:
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented "
                      "[0x%" HWADDR_PRIx "] -> 0\n",
                      __func__, addr);
        break;
    }

    return value;
}

static void elbc_check_update(PPCE500ELbcState *s, size_t index)
{
    ElbcChipSelect *cs = &s->chip_selects[index];
    hwaddr offset = FIELD_EX32(cs->base, ELBC_BR, BA) << 15;
    uint32_t mask = FIELD_EX32(cs->options, ELBC_OR, AM);
    uint64_t size = mask == 0 ? 4 * GiB : (1 << (ctz32(mask) + 15));

    if (index == 0) {
        if (s->boot_page.container) {
            memory_region_del_subregion(s->boot_page.container, &s->boot_page);
        }
        object_unparent(OBJECT(&s->boot_page));
    }

    memory_region_set_enabled(&cs->mr, FIELD_EX32(cs->base, ELBC_BR, V));
    memory_region_set_size(&cs->mr, size);
    memory_region_set_address(&cs->mr, offset);
}

static void ppce500_elbc_write(void *opaque, hwaddr addr, uint64_t value,
                               unsigned len)
{
    PPCE500ELbcState *s = opaque;
    size_t index;

    switch (addr) {
    case ELBC_LBCR:
        trace_ppce500_elbc_lbcr_write(value);
        break;

    case ELBC_BR0:
    case ELBC_BR1:
    case ELBC_BR2:
    case ELBC_BR3:
    case ELBC_BR4:
    case ELBC_BR5:
    case ELBC_BR6:
    case ELBC_BR7:
        index = (addr - ELBC_BR0) / (ELBC_BR1 - ELBC_BR0);
        s->chip_selects[index].base = value;
        elbc_check_update(s, index);
        trace_ppce500_elbc_br_write(
                    index, value,
                    FIELD_EX32(value, ELBC_BR, BA) << 15,
                    elbc_port_size[FIELD_EX32(value, ELBC_BR, PS)],
                    elbc_target_interface[FIELD_EX32(value, ELBC_BR, MSEL)]);
        break;

    case ELBC_OR0:
    case ELBC_OR1:
    case ELBC_OR2:
    case ELBC_OR3:
    case ELBC_OR4:
    case ELBC_OR5:
    case ELBC_OR6:
    case ELBC_OR7:
        index = (addr - ELBC_OR0) / (ELBC_OR1 - ELBC_OR0);
        s->chip_selects[index].options = value;
        elbc_check_update(s, index);
        trace_ppce500_elbc_or_write(index, value,
                                    FIELD_EX32(value, ELBC_OR, AM) << 15);
        break;

    default:
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented "
                      "[0x%" HWADDR_PRIx "] <- 0x%" PRIx32 "\n",
                      __func__, addr, (uint32_t)value);
        break;
    }
}

static const MemoryRegionOps ppce500_elbc_ops = {
    .read = ppce500_elbc_read,
    .write = ppce500_elbc_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static MemTxResult ppce500_elbc_cs_read(void *opaque, hwaddr addr,
                                        uint64_t *value, unsigned len,
                                        MemTxAttrs attrs)
{
    ElbcChipSelect *cs = opaque;
    addr = addr % memory_region_size(cs->dev);

    return memory_region_dispatch_read(cs->dev, addr, value, size_memop(len),
                                       attrs);
}

static MemTxResult ppce500_elbc_cs_write(void *opaque, hwaddr addr,
                                         uint64_t value, unsigned len,
                                         MemTxAttrs attrs)
{
    ElbcChipSelect *cs = opaque;
    addr = addr % memory_region_size(cs->dev);

    return memory_region_dispatch_write(cs->dev, addr, value, size_memop(len),
                                       attrs);
}

static const MemoryRegionOps ppce500_elbc_cs_ops = {
    .read_with_attrs = ppce500_elbc_cs_read,
    .write_with_attrs = ppce500_elbc_cs_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static void ppce500_elbc_init(Object *obj)
{
    PPCE500ELbcState *s = E500_ELBC(obj);
    size_t i;

    memory_region_init_io(&s->ops, obj, &ppce500_elbc_ops, s,
                          "e500 eLBC control registers", MPC85XX_ELBC_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->ops);

    for (i = 0; i < ARRAY_SIZE(s->chip_selects); i++) {
        ElbcChipSelect *cs = &s->chip_selects[i];

        memory_region_init_io(&cs->mr, obj, &ppce500_elbc_cs_ops, cs,
                              "eLBC chip select", 0);
    }

    memory_region_init(&s->address_space, obj, "eLBC", 4 * GiB);
}

static void ppce500_elbc_reset(DeviceState *dev)
{
    PPCE500ELbcState *s = E500_ELBC(dev);
    size_t i;

    for (i = 0; i < ARRAY_SIZE(s->chip_selects); i++) {
        ElbcChipSelect *cs = &s->chip_selects[i];

        cs->base = (i != 0) ? 0 : 1 << R_ELBC_BR_PS_SHIFT |
                                  0 << R_ELBC_BR_MSEL_SHIFT |
                                  1 << R_ELBC_BR_V_SHIFT;
        cs->options = (i != 0) ? 0 : 0xff7;
        elbc_check_update(s, 0);
    }
}

static void ppce500_elbc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = ppce500_elbc_reset;
}

static const TypeInfo ppce500_elbc_info = {
    .name          = TYPE_E500_ELBC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PPCE500ELbcState),
    .instance_init = ppce500_elbc_init,
    .class_init    = ppce500_elbc_class_init,
};

static void ppce500_law_register_types(void)
{
    type_register_static(&ppce500_elbc_info);
}

type_init(ppce500_law_register_types)
