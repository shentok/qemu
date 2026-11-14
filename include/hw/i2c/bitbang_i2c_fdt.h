/*
 * Bit-banged I2C device tree integration
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef HW_I2C_BITBANG_I2C_FDT_H
#define HW_I2C_BITBANG_I2C_FDT_H

#include "qom/object.h"

void gpio_i2c_fdt_class_init(ObjectClass *klass);

#endif
