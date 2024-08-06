/*
 * Flattened device tree I2C integration
 *
 * Copyright (C) 2024 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "hw/i2c/i2c.h"
#include "hw/qdev-dt-interface.h"

void i2c_fdt_populate(I2CBus *i2c, QDevFdtContext *context, int node);
