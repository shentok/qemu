/*
 * Flattened device tree SSI integration
 *
 * Copyright (C) 2024 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "hw/qdev-dt-interface.h"
#include "hw/sysbus.h"

void ssi_fdt_populate(SysBusDevice *sbd, QDevFdtContext *context, int node);
