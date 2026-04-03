/*
 * MAX14906 OLED controller with OSRAM Pictiva 128x64 display.
 *
 * Copyright (c) 2006-2007 CodeSourcery.
 * Written by Paul Brook
 *
 * This code is licensed under the GPL.
 */

/* The controller can support a variety of different displays, but we only
   implement one.  Most of the commands relating to brightness and geometry
   setup are ignored. */

#include "qemu/osdep.h"
#include "hw/ssi/max14906.h"
#include "hw/core/qdev-properties.h"
#include "trace.h"

#define MAX14906_CRC5_INITIAL_VALUE 0x1f

static inline uint8_t max14906_crc5(uint8_t crc, uint8_t byte,
                                    int offset_bit, int end_bit)
{
    uint8_t crc_result = crc;
    static const uint8_t crc_polynom = 0x15;

    for (int i = offset_bit; i < end_bit; ++i) {
        if ((((byte >> (7 - i)) & 0x01) ^ ((crc_result & 0x10) >> 4)) > 0) {
            crc_result = (uint8_t)(crc_polynom ^ ((crc_result << 1) & 0x1f));
        } else {
            crc_result = (uint8_t)((crc_result << 1) & 0x1f);
        }
    }

    return crc_result;
}

static uint32_t max14906_recv(SSIPeripheral *dev)
{
    Max14906State *s = MAX14906(dev);
    uint32_t ret = 0;

    switch (s->bytes_left) {
    case -1:
        s->crc_read = max14906_crc5(MAX14906_CRC5_INITIAL_VALUE, ret, 2, 8);
        break;

    case 0:
        if (s->crc_en) {
            ret = max14906_crc5(s->crc_read, 0, 0, 3);
            break;
        }
        QEMU_FALLTHROUGH;
    default:
        if (!s->is_writing) {
            ret = s->regs[s->cur_reg];
            s->cur_reg = (s->cur_reg + 1) % ARRAY_SIZE(s->regs);
        }
        s->crc_read = max14906_crc5(s->crc_read, ret, 0, 8);
        break;
    }

    trace_max14906_read(DEVICE(dev)->canonical_path, ret);

    return ret;
}

static void max14906_send(SSIPeripheral *dev, uint32_t data)
{
    Max14906State *s = MAX14906(dev);

    trace_max14906_write(DEVICE(dev)->canonical_path, data);

    if (s->bytes_left == -1) {
        s->is_writing = !!(data & BIT(0));
        s->cur_reg = (data > 1) & 0xf;
        s->bytes_total = ((data & BIT(5)) ? 6 : 0) + (s->crc_en ? 1 : 0);
        s->bytes_left = s->bytes_total;
    } else {
        s->bytes_left--;

        if (s->is_writing) {
            if (s->bytes_left >= 0 || !s->crc_en) {
                s->regs[s->cur_reg] = data;
                s->cur_reg = (s->cur_reg + 1) % ARRAY_SIZE(s->regs);
            } else {
                /* check crc */
            }
        }
    }
}

static void max14906_realize(SSIPeripheral *ss, Error **errp)
{
}

static void max14906_reset(DeviceState *dev)
{
    Max14906State *s = MAX14906(dev);

    s->regs[0x3] = 0x40;
    s->regs[0x7] = 0x1f;
    s->regs[0xa] = 0x53;
    s->regs[0xc] = 0x08;
    s->regs[0xf] = 0xbe;

    s->mode = MAX14906_MODE_IDLE;
    s->bytes_left = -1;
}

static const Property max14906_properties[] = {
    DEFINE_PROP_BOOL("crc-enable", Max14906State, crc_en, false),
};

static void max14906_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SSIPeripheralClass *k = SSI_PERIPHERAL_CLASS(klass);

    k->realize = max14906_realize;
    k->recv = max14906_recv;
    k->send = max14906_send;
    k->cs_polarity = SSI_CS_NONE;
    device_class_set_props(dc, max14906_properties);
    device_class_set_legacy_reset(dc, max14906_reset);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static const TypeInfo max14906_types[] = {
    {
        .name          = TYPE_MAX14906,
        .parent        = TYPE_SSI_PERIPHERAL,
        .instance_size = sizeof(Max14906State),
        .class_init    = max14906_class_init,
    }
};

DEFINE_TYPES(max14906_types)
