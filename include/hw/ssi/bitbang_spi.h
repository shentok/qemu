/*
 * IMX SPI Controller
 *
 * Copyright 2016 Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef GPIO_SPI_H
#define GPIO_SPI_H

#include "hw/core/sysbus.h"
#include "hw/core/irq.h"
#include "hw/ssi/ssi.h"
#include "qom/object.h"

#define TYPE_GPIO_SPI "gpio_spi"
OBJECT_DECLARE_SIMPLE_TYPE(GpioSpiState, GPIO_SPI)

struct GpioSpiState {
    SysBusDevice parent_obj;

    SSIBus *bus;

    qemu_irq irq_in;
    uint8_t mosi;
    uint8_t miso;
    int8_t cur_bit_pos;
    bool out;
};

#endif /* GPIO_SPI_H */
