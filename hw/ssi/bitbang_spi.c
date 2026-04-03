/*
 * IMX SPI Controller
 *
 * Copyright (c) 2016 Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "hw/ssi/bitbang_spi.h"
#include "trace.h"

static void bitbang_spi_out_handler(void *opaque, int n, int level)
{
    GpioSpiState *s = opaque;

    trace_bitbang_spi_out_handler(DEVICE(s)->canonical_path, level);

    s->out = !!level;
}

static void bitbang_spi_clock_handler(void *opaque, int n, int level)
{
    GpioSpiState *s = opaque;

    trace_bitbang_spi_clock_handler(DEVICE(s)->canonical_path, level);

    if (s->cur_bit_pos == -1) {
        return;
    }

    if (!level) {
        if (s->cur_bit_pos == 0) {
            s->miso = ssi_read(s->bus);
        }

        qemu_set_irq(s->irq_in, !!(s->miso & BIT(7)));
        s->miso <<= 1;
    }

    if (level) {
        s->mosi = (s->mosi << 1) | s->out;

        if (s->cur_bit_pos == 7) {
            ssi_write(s->bus, s->mosi);
            s->cur_bit_pos = 0;
        } else {
            s->cur_bit_pos++;
        }
    }
}

static void bitbang_spi_cs_handler(void *opaque, int n, int level)
{
    GpioSpiState *s = opaque;

    trace_bitbang_spi_cs_handler(DEVICE(s)->canonical_path, level);

    s->cur_bit_pos = level ? -1 : 0;
    s->mosi = 0;
}

static void bitbang_spi_init(Object *obj)
{
    GpioSpiState *s = GPIO_SPI(obj);
    DeviceState *dev = DEVICE(s);

    s->bus = ssi_create_bus(dev, "spi");

    qdev_init_gpio_in_named(dev, bitbang_spi_out_handler, "out", 1);
    qdev_init_gpio_in_named(dev, bitbang_spi_clock_handler, "clock", 1);
    qdev_init_gpio_in_named(dev, bitbang_spi_cs_handler, "cs", 1);
    qdev_init_gpio_out_named(dev, &s->irq_in, "in", 1);
}

static void bitbang_spi_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "Bitbang SPI Controller";
}

static const TypeInfo bitbang_spi_types[] = {
    {
        .name          = TYPE_GPIO_SPI,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(GpioSpiState),
        .instance_init = bitbang_spi_init,
        .class_init    = bitbang_spi_class_init,
    }
};

DEFINE_TYPES(bitbang_spi_types)
