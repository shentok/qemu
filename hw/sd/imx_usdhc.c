/*
 * SD Association Host Standard Specification v2.0 controller emulation
 *
 * Datasheet: PartA2_SD_Host_Controller_Simplified_Specification_Ver2.00.pdf
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
#include "qemu/units.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "sysemu/dma.h"
#include "qemu/timer.h"
#include "qemu/bitops.h"
#include "hw/sd/sdhci.h"
#include "migration/vmstate.h"
#include "sdhci-internal.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "trace.h"
#include "qom/object.h"

#define ESDHC_MIX_CTRL                  0x48

#define ESDHC_VENDOR_SPEC               0xc0
#define ESDHC_IMX_FRC_SDCLK_ON          (1 << 8)

#define ESDHC_DLL_CTRL                  0x60

#define ESDHC_TUNING_CTRL               0xcc
#define ESDHC_TUNE_CTRL_STATUS          0x68
#define ESDHC_WTMK_LVL                  0x44

/* Undocumented register used by guests working around erratum ERR004536 */
#define ESDHC_UNDOCUMENTED_REG27        0x6c

#define ESDHC_CTRL_4BITBUS              (0x1 << 1)
#define ESDHC_CTRL_8BITBUS              (0x2 << 1)

#define ESDHC_PRNSTS_SDSTB              (1 << 3)

static uint64_t usdhc_read(void *opaque, hwaddr offset, unsigned size)
{
    SDHCIState *s = SYSBUS_SDHCI(opaque);
    uint32_t ret;
    uint16_t hostctl1;

    switch (offset) {
    default:
        return sdhci_read(opaque, offset, size);

    case SDHC_HOSTCTL:
        /*
         * For a detailed explanation on the following bit
         * manipulation code see comments in a similar part of
         * usdhc_write()
         */
        hostctl1 = SDHC_DMA_TYPE(s->hostctl1) << (8 - 3);

        if (s->hostctl1 & SDHC_CTRL_8BITBUS) {
            hostctl1 |= ESDHC_CTRL_8BITBUS;
        }

        if (s->hostctl1 & SDHC_CTRL_4BITBUS) {
            hostctl1 |= ESDHC_CTRL_4BITBUS;
        }

        ret  = hostctl1;
        ret |= (uint32_t)s->blkgap << 16;
        ret |= (uint32_t)s->wakcon << 24;

        break;

    case SDHC_PRNSTS:
        /* Add SDSTB (SD Clock Stable) bit to PRNSTS */
        ret = sdhci_read(opaque, offset, size) & ~ESDHC_PRNSTS_SDSTB;
        if (s->clkcon & SDHC_CLOCK_INT_STABLE) {
            ret |= ESDHC_PRNSTS_SDSTB;
        }
        break;

    case ESDHC_VENDOR_SPEC:
        ret = s->vendor_spec;
        break;
    case ESDHC_DLL_CTRL:
    case ESDHC_TUNE_CTRL_STATUS:
    case ESDHC_UNDOCUMENTED_REG27:
    case ESDHC_TUNING_CTRL:
    case ESDHC_MIX_CTRL:
    case ESDHC_WTMK_LVL:
        ret = 0;
        break;
    }

    return ret;
}

static void
usdhc_write(void *opaque, hwaddr offset, uint64_t val, unsigned size)
{
    SDHCIState *s = SYSBUS_SDHCI(opaque);
    uint8_t hostctl1;
    uint32_t value = (uint32_t)val;

    switch (offset) {
    case ESDHC_DLL_CTRL:
    case ESDHC_TUNE_CTRL_STATUS:
    case ESDHC_UNDOCUMENTED_REG27:
    case ESDHC_TUNING_CTRL:
    case ESDHC_WTMK_LVL:
        break;

    case ESDHC_VENDOR_SPEC:
        s->vendor_spec = value;
        switch (s->vendor) {
        case SDHCI_VENDOR_IMX:
            if (value & ESDHC_IMX_FRC_SDCLK_ON) {
                s->prnsts &= ~SDHC_IMX_CLOCK_GATE_OFF;
            } else {
                s->prnsts |= SDHC_IMX_CLOCK_GATE_OFF;
            }
            break;
        default:
            break;
        }
        break;

    case SDHC_HOSTCTL:
        /*
         * Here's What ESDHCI has at offset 0x28 (SDHC_HOSTCTL)
         *
         *       7         6     5      4      3      2        1      0
         * |-----------+--------+--------+-----------+----------+---------|
         * | Card      | Card   | Endian | DATA3     | Data     | Led     |
         * | Detect    | Detect | Mode   | as Card   | Transfer | Control |
         * | Signal    | Test   |        | Detection | Width    |         |
         * | Selection | Level  |        | Pin       |          |         |
         * |-----------+--------+--------+-----------+----------+---------|
         *
         * and 0x29
         *
         *  15      10 9    8
         * |----------+------|
         * | Reserved | DMA  |
         * |          | Sel. |
         * |          |      |
         * |----------+------|
         *
         * and here's what SDCHI spec expects those offsets to be:
         *
         * 0x28 (Host Control Register)
         *
         *     7        6         5       4  3      2         1        0
         * |--------+--------+----------+------+--------+----------+---------|
         * | Card   | Card   | Extended | DMA  | High   | Data     | LED     |
         * | Detect | Detect | Data     | Sel. | Speed  | Transfer | Control |
         * | Signal | Test   | Transfer |      | Enable | Width    |         |
         * | Sel.   | Level  | Width    |      |        |          |         |
         * |--------+--------+----------+------+--------+----------+---------|
         *
         * and 0x29 (Power Control Register)
         *
         * |----------------------------------|
         * | Power Control Register           |
         * |                                  |
         * | Description omitted,             |
         * | since it has no analog in ESDHCI |
         * |                                  |
         * |----------------------------------|
         *
         * Since offsets 0x2A and 0x2B should be compatible between
         * both IP specs we only need to reconcile least 16-bit of the
         * word we've been given.
         */

        /*
         * First, save bits 7 6 and 0 since they are identical
         */
        hostctl1 = value & (SDHC_CTRL_LED |
                            SDHC_CTRL_CDTEST_INS |
                            SDHC_CTRL_CDTEST_EN);
        /*
         * Second, split "Data Transfer Width" from bits 2 and 1 in to
         * bits 5 and 1
         */
        if (value & ESDHC_CTRL_8BITBUS) {
            hostctl1 |= SDHC_CTRL_8BITBUS;
        }

        if (value & ESDHC_CTRL_4BITBUS) {
            hostctl1 |= ESDHC_CTRL_4BITBUS;
        }

        /*
         * Third, move DMA select from bits 9 and 8 to bits 4 and 3
         */
        hostctl1 |= SDHC_DMA_TYPE(value >> (8 - 3));

        /*
         * Now place the corrected value into low 16-bit of the value
         * we are going to give standard SDHCI write function
         *
         * NOTE: This transformation should be the inverse of what can
         * be found in drivers/mmc/host/sdhci-esdhc-imx.c in Linux
         * kernel
         */
        value &= ~UINT16_MAX;
        value |= hostctl1;
        value |= (uint16_t)s->pwrcon << 8;

        sdhci_write(opaque, offset, value, size);
        break;

    case ESDHC_MIX_CTRL:
        /*
         * So, when SD/MMC stack in Linux tries to write to "Transfer
         * Mode Register", ESDHC i.MX quirk code will translate it
         * into a write to ESDHC_MIX_CTRL, so we do the opposite in
         * order to get where we started
         *
         * Note that Auto CMD23 Enable bit is located in a wrong place
         * on i.MX, but since it is not used by QEMU we do not care.
         *
         * We don't want to call sdhci_write(.., SDHC_TRNMOD, ...)
         * here becuase it will result in a call to
         * sdhci_send_command(s) which we don't want.
         *
         */
        s->trnmod = value & UINT16_MAX;
        break;
    case SDHC_TRNMOD:
        /*
         * Similar to above, but this time a write to "Command
         * Register" will be translated into a 4-byte write to
         * "Transfer Mode register" where lower 16-bit of value would
         * be set to zero. So what we do is fill those bits with
         * cached value from s->trnmod and let the SDHCI
         * infrastructure handle the rest
         */
        sdhci_write(opaque, offset, val | s->trnmod, size);
        break;
    case SDHC_BLKSIZE:
        /*
         * ESDHCI does not implement "Host SDMA Buffer Boundary", and
         * Linux driver will try to zero this field out which will
         * break the rest of SDHCI emulation.
         *
         * Linux defaults to maximum possible setting (512K boundary)
         * and it seems to be the only option that i.MX IP implements,
         * so we artificially set it to that value.
         */
        val |= 0x7 << 12;
        /* FALLTHROUGH */
    default:
        sdhci_write(opaque, offset, val, size);
        break;
    }
}

static const MemoryRegionOps usdhc_mmio_ops = {
    .read = usdhc_read,
    .write = usdhc_write,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
        .unaligned = false
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void imx_usdhc_init(Object *obj)
{
    SDHCIState *s = SYSBUS_SDHCI(obj);

    s->io_ops = &usdhc_mmio_ops;
    s->quirks = SDHCI_QUIRK_NO_BUSY_IRQ;
}

static const TypeInfo imx_usdhc_info = {
    .name = TYPE_IMX_USDHC,
    .parent = TYPE_SYSBUS_SDHCI,
    .instance_init = imx_usdhc_init,
};

static void sdhci_register_types(void)
{
    type_register_static(&imx_usdhc_info);
}

type_init(sdhci_register_types)
