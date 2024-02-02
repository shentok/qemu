/*
 * QEMU SMSC FDC37M81X Super I/O
 *
 * Copyright (c) 2024 Bernhard Beschow <shentey@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef QEMU_FDC37M81X_H
#define QEMU_FDC37M81X_H

#include "hw/isa/superio.h"
#include "qom/object.h"

#define TYPE_FDC37M81X "fdc37m81x-superio"
OBJECT_DECLARE_SIMPLE_TYPE(FDC37M81XState, FDC37M81X)

struct FDC37M81XState {
    ISASuperIODevice parent_dev;

    MemoryRegion config_io;
    MemoryRegion index_data_io;

    struct {
        bool floppy;
        bool serial[2];
        bool parallel;
    } enabled;

    uint8_t logical_device_number;
    uint8_t selected_index;
};

#endif
