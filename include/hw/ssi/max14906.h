/*
 * IMX SPI Controller
 *
 * Copyright 2016 Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef MAX14906_H
#define MAX14906_H

#include "hw/core/irq.h"
#include "hw/ssi/ssi.h"
#include "qom/object.h"

#define TYPE_MAX14906 "max14906"

OBJECT_DECLARE_SIMPLE_TYPE(Max14906State, MAX14906)

typedef enum Max14906Mode {
    MAX14906_MODE_IDLE,
    MAX14906_MODE_TRANSFER,
} Max14906Mode;

struct Max14906State {
    SSIPeripheral parent_obj;

    uint8_t regs[0x10];

    Max14906Mode mode;
    int8_t bytes_total;
    int8_t bytes_left;
    uint8_t cur_reg;
    uint8_t crc_read;
    bool crc_en;
    bool is_writing;
};

#endif /* MAX14906_H */
