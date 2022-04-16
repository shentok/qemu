/*
 * Samsung s3c controller emulation
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Mitsyanko Igor <i.mitsyanko@samsung.com>
 * Peter A.G. Crosthwaite <peter.crosthwaite@petalogix.com>
 *
 * Based on MMC controller for Samsung S5PC1xx-based board emulation
 * by Alexey Merkulov and Vladimir Monakhov.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/sd/sdhci.h"
#include "sdhci-internal.h"
#include "qemu/module.h"
#include "qom/object.h"

#define S3C_SDHCI_CONTROL2      0x80
#define S3C_SDHCI_CONTROL3      0x84
#define S3C_SDHCI_CONTROL4      0x8c

static uint64_t sdhci_s3c_read(void *opaque, hwaddr offset, unsigned size)
{
    uint64_t ret;

    switch (offset) {
    case S3C_SDHCI_CONTROL2:
    case S3C_SDHCI_CONTROL3:
    case S3C_SDHCI_CONTROL4:
        /* ignore */
        ret = 0;
        break;
    default:
        ret = sdhci_read(opaque, offset, size);
        break;
    }

    return ret;
}

static void sdhci_s3c_write(void *opaque, hwaddr offset, uint64_t val,
                            unsigned size)
{
    switch (offset) {
    case S3C_SDHCI_CONTROL2:
    case S3C_SDHCI_CONTROL3:
    case S3C_SDHCI_CONTROL4:
        /* ignore */
        break;
    default:
        sdhci_write(opaque, offset, val, size);
        break;
    }
}

static const MemoryRegionOps sdhci_s3c_mmio_ops = {
    .read = sdhci_s3c_read,
    .write = sdhci_s3c_write,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
        .unaligned = false
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void sdhci_s3c_init(Object *obj)
{
    SDHCIState *s = SYSBUS_SDHCI(obj);

    s->io_ops = &sdhci_s3c_mmio_ops;
}

static const TypeInfo sdhci_s3c_info = {
    .name = TYPE_S3C_SDHCI,
    .parent = TYPE_SYSBUS_SDHCI,
    .instance_init = sdhci_s3c_init,
};

static void sdhci_register_types(void)
{
    type_register_static(&sdhci_s3c_info);
}

type_init(sdhci_register_types)
