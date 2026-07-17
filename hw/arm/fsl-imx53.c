/*
 * Copyright (c) 2015 Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * i.MX53 SOC emulation.
 *
 * Based on hw/arm/fsl-imx31.c
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/fsl-imx53.h"
#include "hw/misc/unimp.h"
#include "hw/core/qdev-properties.h"
#include "system/system.h"
#include "qemu/module.h"
#include "target/arm/cpu-qom.h"
#include "qemu/units.h"

static const struct {
    hwaddr addr;
    size_t size;
    const char *name;
} fsl_imx53_memmap[] = {
    [FSL_IMX53_BOOT_ROM]              = { 0x00000000, 64 * KiB, "Boot ROM" },
    [FSL_IMX53_BOOT_ROM_ALIASING]     = { 0x00010000, 16 * MiB - 64 * KiB, "Boot ROM Aliasing" },

    [FSL_IMX53_RESERVED_0100]         = { 0x01000000, 96 * MiB, "Reserved" },
    [FSL_IMX53_SECURITY_RAM]          = { 0x07000000, 16 * KiB, "Security Controller RAM" },
    [FSL_IMX53_SCC_ALIAS]             = { 0x07004000, 16 * MiB - 16 * KiB, "SCC RAM Aliasing" },
    [FSL_IMX53_RESERVED_0800]         = { 0x08000000, 128 * MiB, "Reserved" },
    [FSL_IMX53_TZIC]                  = { 0x0FFFC000, 16 * KiB, "TZIC" },

    [FSL_IMX53_SATA]                  = { 0x10000000, 16 * KiB, "SATA" },
    [FSL_IMX53_SATA_ALIAS]            = { 0x10004000, 64 * MiB - 16 * KiB, "SATA Aliasing" },
    [FSL_IMX53_RESERVED_1400]         = { 0x14000000, 64 * MiB, "Reserved" },
    [FSL_IMX53_IPU]                   = { 0x18000000, 128 * MiB, "IPU" },
    [FSL_IMX53_GPU2D]                 = { 0x20000000, 256 * MiB, "GPU2D" },
    [FSL_IMX53_GPU3D]                 = { 0x30000000, 256 * MiB, "GPU3D" },

    /* Debug APB */
    [FSL_IMX53_DEBUG_ROM]             = { 0x40000000, 4 * KiB, "Debug ROM" },
    [FSL_IMX53_ETB]                   = { 0x40001000, 4 * KiB, "ETB" },
    [FSL_IMX53_ETM]                   = { 0x40002000, 4 * KiB, "ETM" },
    [FSL_IMX53_TPIU]                  = { 0x40003000, 4 * KiB, "TPIU" },
    [FSL_IMX53_CTI0]                  = { 0x40004000, 4 * KiB, "CTI0" },
    [FSL_IMX53_CTI1]                  = { 0x40005000, 4 * KiB, "CTI1" },
    [FSL_IMX53_CTI2]                  = { 0x40006000, 4 * KiB, "CTI2" },
    [FSL_IMX53_CTI3]                  = { 0x40007000, 4 * KiB, "CTI3" },
    [FSL_IMX53_ARM_DEBUG]             = { 0x40008000, 4 * KiB, "ARM Debug Unit" },
    [FSL_IMX53_DEBUG_RESERVED]        = { 0x40009000, 256 * MiB - 36 * KiB, "Reserved" },

    /* AIPSTZ-1 SPBA */
    [FSL_IMX53_AIPSTZ1_RSVD0]         = { 0x50000000, 16 * KiB, "Reserved" },
    [FSL_IMX53_ESDHC1]                = { 0x50004000, 16 * KiB, "ESDHC1" },
    [FSL_IMX53_ESDHC2]                = { 0x50008000, 16 * KiB, "ESDHC2" },
    [FSL_IMX53_UART3]                 = { 0x5000C000, 16 * KiB, "UART3" },
    [FSL_IMX53_ECSPI1]                = { 0x50010000, 16 * KiB, "ECSPI1" },
    [FSL_IMX53_SSI2]                  = { 0x50014000, 16 * KiB, "SSI2" },
    [FSL_IMX53_ESAI1]                 = { 0x50018000, 16 * KiB, "ESAI1" },
    [FSL_IMX53_SDMA]                  = { 0x5001C000, 16 * KiB, "SDMA Registers" },
    [FSL_IMX53_ESDHC3]                = { 0x50020000, 16 * KiB, "ESDHC3" },
    [FSL_IMX53_ESDHC4]                = { 0x50024000, 16 * KiB, "ESDHC4" },
    [FSL_IMX53_SPDIF]                 = { 0x50028000, 16 * KiB, "SPDIF" },
    [FSL_IMX53_ASRC]                  = { 0x5002C000, 16 * KiB, "ASRC" },
    [FSL_IMX53_PATA_UDMA]             = { 0x50030000, 16 * KiB, "PATA (UDMA)" },
    [FSL_IMX53_SPBA]                  = { 0x5003C000, 16 * KiB, "SPBA" },

    [FSL_IMX53_AIPSTZ1_GLOBAL0]       = { 0x50040000, 32 * MiB - 256 * KiB, "AIPSTZ1 Global Enable 0" },
    [FSL_IMX53_AIPSTZ1_GLOBAL1]       = { 0x52000000, 31 * MiB, "AIPSTZ1 Global Enable 1" },

    [FSL_IMX53_AIPSTZ1_ONPLAT]        = { 0x53F00000, 512 * KiB, "AIPSTZ1 On-Platform" },

    /* AIPSTZ-1 Off Platform */
    [FSL_IMX53_USB1]                  = { 0x53F80000, 512, "USB1 OTG + HS" },
    [FSL_IMX53_USB2]                  = { 0x53F80200, 512, "USB1 OTG + HS" },
    [FSL_IMX53_USB3]                  = { 0x53F80400, 512, "USB1 OTG + HS" },
    [FSL_IMX53_USB4]                  = { 0x53F80600, 512, "USB1 OTG + HS" },
    [FSL_IMX53_USB_MISC]              = { 0x53F80800, 512, "USB MISC" },
    [FSL_IMX53_GPIO1]                 = { 0x53F84000, 16 * KiB, "GPIO1" },
    [FSL_IMX53_GPIO2]                 = { 0x53F88000, 16 * KiB, "GPIO2" },
    [FSL_IMX53_GPIO3]                 = { 0x53F8C000, 16 * KiB, "GPIO3" },
    [FSL_IMX53_GPIO4]                 = { 0x53F90000, 16 * KiB, "GPIO4" },
    [FSL_IMX53_KPP]                   = { 0x53F94000, 16 * KiB, "KPP" },
    [FSL_IMX53_WDOG1]                 = { 0x53F98000, 16 * KiB, "WDOG1" },
    [FSL_IMX53_WDOG2]                 = { 0x53F9C000, 16 * KiB, "WDOG2" },
    [FSL_IMX53_GPT]                   = { 0x53FA0000, 16 * KiB, "GPT" },
    [FSL_IMX53_SRTC]                  = { 0x53FA4000, 16 * KiB, "SRTC" },
    [FSL_IMX53_IOMUXC]                = { 0x53FA8000, 16 * KiB, "IOMUXC" },
    [FSL_IMX53_EPIT1]                 = { 0x53FAC000, 16 * KiB, "EPIT1" },
    [FSL_IMX53_EPIT2]                 = { 0x53FB0000, 16 * KiB, "EPIT2" },
    [FSL_IMX53_PWM1]                  = { 0x53FB4000, 16 * KiB, "PWM1" },
    [FSL_IMX53_PWM2]                  = { 0x53FB8000, 16 * KiB, "PWM2" },
    [FSL_IMX53_UART1]                 = { 0x53FBC000, 16 * KiB, "UART1" },
    [FSL_IMX53_UART2]                 = { 0x53FC0000, 16 * KiB, "UART2" },
    [FSL_IMX53_USB_PL301]             = { 0x53FC4000, 16 * KiB, "USB PL301" },
    [FSL_IMX53_FLEXCAN1]              = { 0x53FC8000, 16 * KiB, "FLEXCAN1" },
    [FSL_IMX53_FLEXCAN2]              = { 0x53FCC000, 16 * KiB, "FLEXCAN2" },
    [FSL_IMX53_SRC]                   = { 0x53FD0000, 16 * KiB, "SRC" },
    [FSL_IMX53_CCM]                   = { 0x53FD4000, 16 * KiB, "CCM" },
    [FSL_IMX53_GPC]                   = { 0x53FD8000, 16 * KiB, "GPC" },
    [FSL_IMX53_GPIO5]                 = { 0x53FDC000, 16 * KiB, "GPIO5" },
    [FSL_IMX53_GPIO6]                 = { 0x53FE0000, 16 * KiB, "GPIO6" },
    [FSL_IMX53_GPIO7]                 = { 0x53FE4000, 16 * KiB, "GPIO7" },
    [FSL_IMX53_PATA_PIO]              = { 0x53FE8000, 16 * KiB, "PATA (PIO)" },
    [FSL_IMX53_I2C3]                  = { 0x53FEC000, 16 * KiB, "I2C3" },
    [FSL_IMX53_UART4]                 = { 0x53FF0000, 16 * KiB, "UART4" },
    [FSL_IMX53_AIPSTZ1_RSVD2]         = { 0x53FF4000, 48 * KiB, "Reserved" },
    [FSL_IMX53_AIPSTZ1_ALIAS]         = { 0x54000000, 448 * MiB, "AIPSTZ1 Aliased" },

    /* AIPSTZ-2 Global */
    [FSL_IMX53_AIPSTZ2_GLOBAL0]       = { 0x60000000, 32 * MiB, "AIPSTZ2 Global 0" },
    [FSL_IMX53_AIPSTZ2_GLOBAL1]       = { 0x62000000, 31 * MiB, "AIPSTZ2 Global 1" },

    [FSL_IMX53_AIPSTZ2_ONPLAT]        = { 0x63F00000, 512 * KiB, "AIPSTZ2 On-Platform" },

    /* AIPSTZ-2 Off Platform */
    [FSL_IMX53_DPLLC1]                = { 0x63F80000, 16 * KiB, "DPLLC1" },
    [FSL_IMX53_DPLLC2]                = { 0x63F84000, 16 * KiB, "DPLLC2" },
    [FSL_IMX53_DPLLC3]                = { 0x63F88000, 16 * KiB, "DPLLC3" },
    [FSL_IMX53_DPLLC4]                = { 0x63F8C000, 16 * KiB, "DPLLC4" },
    [FSL_IMX53_UART5]                 = { 0x63F90000, 16 * KiB, "UART5" },
    [FSL_IMX53_AHBMAX]                = { 0x63F94000, 16 * KiB, "AHBMAX" },
    [FSL_IMX53_IIM]                   = { 0x63F98000, 16 * KiB, "IIM" },
    [FSL_IMX53_CSU]                   = { 0x63F9C000, 16 * KiB, "CSU" },
    [FSL_IMX53_ARM_PLATFORM]          = { 0x63FA0000, 16 * KiB, "ARM Platform" },
    [FSL_IMX53_OWIRE]                 = { 0x63FA4000, 16 * KiB, "OWIRE" },
    [FSL_IMX53_FIRI]                  = { 0x63FA8000, 16 * KiB, "FIRI" },
    [FSL_IMX53_ECSPI2]                = { 0x63FAC000, 16 * KiB, "ECSPI2" },
    [FSL_IMX53_SDMA2]                 = { 0x63FB0000, 16 * KiB, "SDMA (IPS_HOST)" },
    [FSL_IMX53_SCC]                   = { 0x63FB4000, 16 * KiB, "SCC" },
    [FSL_IMX53_ROMC]                  = { 0x63FB8000, 16 * KiB, "ROMC" },
    [FSL_IMX53_RTIC]                  = { 0x63FBC000, 16 * KiB, "RTIC" },
    [FSL_IMX53_CSPI]                  = { 0x63FC0000, 16 * KiB, "CSPI" },
    [FSL_IMX53_I2C2]                  = { 0x63FC4000, 16 * KiB, "I2C2" },
    [FSL_IMX53_I2C1]                  = { 0x63FC8000, 16 * KiB, "I2C1" },
    [FSL_IMX53_SSI1]                  = { 0x63FCC000, 16 * KiB, "SSI1" },
    [FSL_IMX53_AUDMUX]                = { 0x63FD0000, 16 * KiB, "AUDMUX" },
    [FSL_IMX53_RTC]                   = { 0x63FD4000, 16 * KiB, "RTC" },
    [FSL_IMX53_EXTMC]                 = { 0x63FD8000, 16 * KiB, "EXTMC" },
    [FSL_IMX53_APB2IP_2X2]            = { 0x63FDC000, 16 * KiB, "apb2ip_pl301_2x2" },
    [FSL_IMX53_APB2IP_4X1]            = { 0x63FE0000, 16 * KiB, "apb2ip_pl301_4x1" },
    [FSL_IMX53_MLB]                   = { 0x63FE4000, 16 * KiB, "MLB" },
    [FSL_IMX53_SSI3]                  = { 0x63FE8000, 16 * KiB, "SSI3" },
    [FSL_IMX53_FEC]                   = { 0x63FEC000, 16 * KiB, "FEC" },
    [FSL_IMX53_TVE]                   = { 0x63FF0000, 16 * KiB, "TVE" },
    [FSL_IMX53_VPU]                   = { 0x63FF4000, 16 * KiB, "VPU" },
    [FSL_IMX53_SAHARA]                = { 0x63FF8000, 16 * KiB, "SAHARA" },
    [FSL_IMX53_PTP]                   = { 0x63FFC000, 16 * KiB, "PTP" },
    [FSL_IMX53_AIPSTZ2_ALIAS]         = { 0x64000000, 256 * MiB - 512 * KiB, "AIPSTZ2 Aliased" },

    [FSL_IMX53_RESERVED_6FFF]         = { 0x6FFFC000, 16 * KiB, "Reserved" },

    /* External Memory */
    [FSL_IMX53_CSD0]                  = { FSL_IMX53_RAM_START, 1ULL * GiB, "CSD0 DDR" },
    [FSL_IMX53_CSD1]                  = { 0xB0000000, 1ULL * GiB, "CSD1 DDR" },
    [FSL_IMX53_CS]                    = { 0xF0000000, 128 * MiB - 64 * KiB, "CS" },

    /* On-chip memories */
    [FSL_IMX53_NAND_BUF]              = { 0xF7FF0000, 64 * KiB, "NAND Buffer" },
    [FSL_IMX53_OCRAM]                 = { 0xF8000000, 128 * KiB, "OCRAM" },
    [FSL_IMX53_GPU3D_GMEM]            = { 0xF8020000, 256 * KiB, "GPU3D GMEM" },
    [FSL_IMX53_RESERVED_F806]         = { 0xF8060000, 128 * MiB - 384 * KiB, "Reserved" },
};

static void fsl_imx53_init(Object *obj)
{
    FslImx53State *s = FSL_IMX53(obj);

    object_initialize_child(obj, "tzic", &s->tzic, TYPE_FSL_TZIC);

    object_initialize_child(obj, "ccm", &s->ccm, TYPE_IMX53_CCM);

    object_initialize_child(obj, "srtc", &s->srtc, TYPE_IMX_SRTC);

    for (size_t i = 0; i < ARRAY_SIZE(s->uart); i++) {
        g_autofree char *name = g_strdup_printf("uart%zu", i + 1);
        object_initialize_child(obj, name, &s->uart[i], TYPE_IMX_SERIAL);
    }

    object_initialize_child(obj, "gpt", &s->gpt, TYPE_IMX53_GPT);

    for (size_t i = 0; i < ARRAY_SIZE(s->epit); i++) {
        g_autofree char *name = g_strdup_printf("epit%zu", i + 1);
        object_initialize_child(obj, name, &s->epit[i], TYPE_IMX_EPIT);
    }

    for (size_t i = 0; i < ARRAY_SIZE(s->i2c); i++) {
        g_autofree char *name = g_strdup_printf("i2c%zu", i + 1);
        object_initialize_child(obj, name, &s->i2c[i], TYPE_IMX_I2C);
    }

    for (size_t i = 0; i < ARRAY_SIZE(s->gpio); i++) {
        g_autofree char *name = g_strdup_printf("gpio%zu", i + 1);
        object_initialize_child(obj, name, &s->gpio[i], TYPE_IMX_GPIO);
    }

    for (size_t i = 0; i < ARRAY_SIZE(s->esdhc); i++) {
        g_autofree char *name = g_strdup_printf("sdhc%zu", i + 1);
        object_initialize_child(obj, name, &s->esdhc[i], TYPE_FSL_ESDHC_LE);
    }

    for (size_t i = 0; i < ARRAY_SIZE(s->usb); i++) {
        g_autofree char *name = g_strdup_printf("usb%zu", i);
        object_initialize_child(obj, name, &s->usb[i], TYPE_CHIPIDEA);
    }
    object_initialize_child(obj, "usb_misc", &s->usb_misc, TYPE_IMX53_USB_MISC);

    for (size_t i = 0; i < ARRAY_SIZE(s->spi); i++) {
        g_autofree char *name = g_strdup_printf("spi%zu", i + 1);
        object_initialize_child(obj, name, &s->spi[i], TYPE_IMX_SPI);
    }
    for (size_t i = 0; i < ARRAY_SIZE(s->wdt); i++) {
        g_autofree char *name = g_strdup_printf("wdt%zu", i);
        object_initialize_child(obj, name, &s->wdt[i], TYPE_IMX2_WDT);
    }

    for (size_t i = 0; i < ARRAY_SIZE(s->flexcan); i++) {
        g_autofree char *name = g_strdup_printf("flexcan%zu", i);
        object_initialize_child(obj, name, &s->flexcan[i], TYPE_CAN_FLEXCAN2);
    }

    object_initialize_child(obj, "ahci", &s->sata, TYPE_SYSBUS_AHCI);

    object_initialize_child(obj, "eth", &s->eth, TYPE_IMX_FEC);
}

static void fsl_imx53_realize(DeviceState *dev, Error **errp)
{
    FslImx53State *s = FSL_IMX53(dev);
    DeviceState *gic = DEVICE(&s->tzic);

    object_initialize_child(OBJECT(dev), "cpu", &s->cpu,
                            ARM_CPU_TYPE_NAME("cortex-a8"));
    if (!qdev_realize(DEVICE(&s->cpu), NULL, errp)) {
        return;
    }

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->tzic), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->tzic), 0,
                    fsl_imx53_memmap[FSL_IMX53_TZIC].addr);

    sysbus_connect_irq(SYS_BUS_DEVICE(&s->tzic), 0,
                       qdev_get_gpio_in(DEVICE(&s->cpu), ARM_CPU_IRQ));
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->tzic), 1,
                       qdev_get_gpio_in(DEVICE(&s->cpu), ARM_CPU_FIQ));

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->ccm), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ccm), 0,
                    fsl_imx53_memmap[FSL_IMX53_CCM].addr);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ccm), 1,
                    fsl_imx53_memmap[FSL_IMX53_DPLLC1].addr);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ccm), 2,
                    fsl_imx53_memmap[FSL_IMX53_DPLLC2].addr);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ccm), 3,
                    fsl_imx53_memmap[FSL_IMX53_DPLLC3].addr);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->ccm), 4,
                    fsl_imx53_memmap[FSL_IMX53_DPLLC4].addr);

    /* Initialize all UARTs */
    for (size_t i = 0; i < ARRAY_SIZE(s->uart); i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } table[ARRAY_SIZE(s->uart)] = {
            { fsl_imx53_memmap[FSL_IMX53_UART1].addr, FSL_IMX53_UART1_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_UART2].addr, FSL_IMX53_UART2_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_UART3].addr, FSL_IMX53_UART3_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_UART4].addr, FSL_IMX53_UART4_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_UART5].addr, FSL_IMX53_UART5_IRQ },
        };

        qdev_prop_set_chr(DEVICE(&s->uart[i]), "chardev", serial_hd(i));

        if (!sysbus_realize(SYS_BUS_DEVICE(&s->uart[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->uart[i]), 0, table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->uart[i]), 0,
                           qdev_get_gpio_in(gic, table[i].irq));
    }

    s->gpt.ccm = IMX_CCM(&s->ccm);

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpt), errp)) {
        return;
    }

    sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpt), 0, fsl_imx53_memmap[FSL_IMX53_GPT].addr);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpt), 0,
                       qdev_get_gpio_in(gic, FSL_IMX53_GPT_IRQ));

    /* Initialize all EPIT timers */
    for (size_t i = 0; i < ARRAY_SIZE(s->epit); i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } table[ARRAY_SIZE(s->epit)] = {
            { fsl_imx53_memmap[FSL_IMX53_EPIT1].addr, FSL_IMX53_EPIT1_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_EPIT2].addr, FSL_IMX53_EPIT2_IRQ },
        };

        s->epit[i].ccm = IMX_CCM(&s->ccm);

        if (!sysbus_realize(SYS_BUS_DEVICE(&s->epit[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->epit[i]), 0, table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->epit[i]), 0,
                           qdev_get_gpio_in(gic, table[i].irq));
    }

    /* Initialize all I2C */
    for (size_t i = 0; i < ARRAY_SIZE(s->i2c); i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } table[ARRAY_SIZE(s->i2c)] = {
            { fsl_imx53_memmap[FSL_IMX53_I2C1].addr, FSL_IMX53_I2C1_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_I2C2].addr, FSL_IMX53_I2C2_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_I2C3].addr, FSL_IMX53_I2C3_IRQ }
        };

        if (!sysbus_realize(SYS_BUS_DEVICE(&s->i2c[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->i2c[i]), 0, table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->i2c[i]), 0,
                           qdev_get_gpio_in(gic, table[i].irq));
    }

    /* Initialize all GPIOs */
    for (size_t i = 0; i < ARRAY_SIZE(s->gpio); i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq_low;
            unsigned int irq_high;
        } table[ARRAY_SIZE(s->gpio)] = {
            {
                fsl_imx53_memmap[FSL_IMX53_GPIO1].addr,
                FSL_IMX53_GPIO1_LOW_IRQ,
                FSL_IMX53_GPIO1_HIGH_IRQ
            },
            {
                fsl_imx53_memmap[FSL_IMX53_GPIO2].addr,
                FSL_IMX53_GPIO2_LOW_IRQ,
                FSL_IMX53_GPIO2_HIGH_IRQ
            },
            {
                fsl_imx53_memmap[FSL_IMX53_GPIO3].addr,
                FSL_IMX53_GPIO3_LOW_IRQ,
                FSL_IMX53_GPIO3_HIGH_IRQ
            },
            {
                fsl_imx53_memmap[FSL_IMX53_GPIO4].addr,
                FSL_IMX53_GPIO4_LOW_IRQ,
                FSL_IMX53_GPIO4_HIGH_IRQ
            },
            {
                fsl_imx53_memmap[FSL_IMX53_GPIO5].addr,
                FSL_IMX53_GPIO5_LOW_IRQ,
                FSL_IMX53_GPIO5_HIGH_IRQ
            },
            {
                fsl_imx53_memmap[FSL_IMX53_GPIO6].addr,
                FSL_IMX53_GPIO6_LOW_IRQ,
                FSL_IMX53_GPIO6_HIGH_IRQ
            },
            {
                fsl_imx53_memmap[FSL_IMX53_GPIO7].addr,
                FSL_IMX53_GPIO7_LOW_IRQ,
                FSL_IMX53_GPIO7_HIGH_IRQ
            },
        };

        object_property_set_bool(OBJECT(&s->gpio[i]), "has-edge-sel", true,
                                 &error_abort);
        object_property_set_bool(OBJECT(&s->gpio[i]), "has-upper-pin-irq",
                                 true, &error_abort);
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->gpio[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->gpio[i]), 0, table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpio[i]), 0,
                           qdev_get_gpio_in(gic, table[i].irq_low));
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->gpio[i]), 1,
                           qdev_get_gpio_in(gic, table[i].irq_high));
    }

    /* Initialize all ESDHC */
    for (size_t i = 0; i < ARRAY_SIZE(s->esdhc); i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } table[ARRAY_SIZE(s->esdhc)] = {
            { fsl_imx53_memmap[FSL_IMX53_ESDHC1].addr, FSL_IMX53_ESDHC1_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_ESDHC2].addr, FSL_IMX53_ESDHC2_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_ESDHC3].addr, FSL_IMX53_ESDHC3_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_ESDHC4].addr, FSL_IMX53_ESDHC4_IRQ },
        };

        if (!sysbus_realize(SYS_BUS_DEVICE(&s->esdhc[i]), errp)) {
            return;
        }
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->esdhc[i]), 0, table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->esdhc[i]), 0,
                           qdev_get_gpio_in(gic, table[i].irq));
    }

    /* USB */
    for (size_t i = 0; i < ARRAY_SIZE(s->usb); i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } table[ARRAY_SIZE(s->usb)] = {
            { fsl_imx53_memmap[FSL_IMX53_USB1].addr, FSL_IMX53_USB_OTG_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_USB2].addr, FSL_IMX53_USB_HOST2_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_USB3].addr, FSL_IMX53_USB_HOST3_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_USB4].addr, FSL_IMX53_USB_HOST4_IRQ },
        };

        sysbus_realize(SYS_BUS_DEVICE(&s->usb[i]), &error_abort);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->usb[i]), 0, table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->usb[i]), 0,
                           qdev_get_gpio_in(gic, table[i].irq));
    }

    sysbus_realize(SYS_BUS_DEVICE(&s->usb_misc), &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->usb_misc), 0,
                    fsl_imx53_memmap[FSL_IMX53_USB_MISC].addr);

    /* Initialize all ECSPI */
    for (size_t i = 0; i < ARRAY_SIZE(s->spi); i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } table[ARRAY_SIZE(s->spi)] = {
            { fsl_imx53_memmap[FSL_IMX53_ECSPI1].addr, FSL_IMX53_ECSPI1_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_ECSPI2].addr, FSL_IMX53_ECSPI2_IRQ },
        };

        /* Initialize the SPI */
        if (!sysbus_realize(SYS_BUS_DEVICE(&s->spi[i]), errp)) {
            return;
        }

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->spi[i]), 0, table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->spi[i]), 0,
                           qdev_get_gpio_in(gic, table[i].irq));
    }

    object_property_set_int(OBJECT(&s->sata), "num-ports", 1, &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->sata), errp)) {
        return;
    }

    sysbus_mmio_map(SYS_BUS_DEVICE(&s->sata), 0,
                    fsl_imx53_memmap[FSL_IMX53_SATA].addr);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->sata), 0,
                       qdev_get_gpio_in(gic, FSL_IMX53_SATA_IRQ));

    object_property_set_uint(OBJECT(&s->eth), "phy-num", s->phy_num,
                             &error_abort);
    qemu_configure_nic_device(DEVICE(&s->eth), true, NULL);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->eth), errp)) {
        return;
    }
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->eth), 0,
                    fsl_imx53_memmap[FSL_IMX53_FEC].addr);
    sysbus_connect_irq(SYS_BUS_DEVICE(&s->eth), 0,
                       qdev_get_gpio_in(gic, FSL_IMX53_ENET_MAC_IRQ));

    /*
     * SRTC
     */
    sysbus_realize(SYS_BUS_DEVICE(&s->srtc), &error_abort);
    sysbus_mmio_map(SYS_BUS_DEVICE(&s->srtc), 0,
                    fsl_imx53_memmap[FSL_IMX53_SRTC].addr);

    /*
     * Watchdog
     */
    for (size_t i = 0; i < ARRAY_SIZE(s->wdt); i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } table[ARRAY_SIZE(s->wdt)] = {
            { fsl_imx53_memmap[FSL_IMX53_WDOG1].addr, FSL_IMX53_WDOG1_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_WDOG2].addr, FSL_IMX53_WDOG2_IRQ },
        };

        object_property_set_bool(OBJECT(&s->wdt[i]), "pretimeout-support",
                                 true, &error_abort);
        sysbus_realize(SYS_BUS_DEVICE(&s->wdt[i]), &error_abort);

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->wdt[i]), 0, table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->wdt[i]), 0,
                           qdev_get_gpio_in(gic, table[i].irq));
    }

    /* FlexCANs */
    for (size_t i = 0; i < ARRAY_SIZE(s->flexcan); i++) {
        static const struct {
            hwaddr addr;
            unsigned int irq;
        } table[ARRAY_SIZE(s->flexcan)] = {
            { fsl_imx53_memmap[FSL_IMX53_FLEXCAN1].addr, FSL_IMX53_FLEXCAN1_IRQ },
            { fsl_imx53_memmap[FSL_IMX53_FLEXCAN2].addr, FSL_IMX53_FLEXCAN2_IRQ },
        };

        object_property_set_link(OBJECT(&s->flexcan[i]), "clock-control-module",
                                 OBJECT(&s->ccm), &error_abort);
        object_property_set_link(OBJECT(&s->flexcan[i]), "canbus",
                                 OBJECT(s->canbus[i]), &error_abort);

        sysbus_realize(SYS_BUS_DEVICE(&s->flexcan[i]), &error_abort);

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->flexcan[i]), 0, table[i].addr);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->flexcan[i]), 0,
                           qdev_get_gpio_in(gic, table[i].irq));
    }

    /* ROM memory */
    if (!memory_region_init_rom(&s->rom, OBJECT(dev),
                                fsl_imx53_memmap[FSL_IMX53_BOOT_ROM].name,
                                fsl_imx53_memmap[FSL_IMX53_BOOT_ROM].size,
                                errp)) {
        return;
    }
    memory_region_add_subregion(get_system_memory(),
                                fsl_imx53_memmap[FSL_IMX53_BOOT_ROM].addr,
                                &s->rom);

    /* CAAM memory */
    if (!memory_region_init_rom(&s->caam, OBJECT(dev),
                                fsl_imx53_memmap[FSL_IMX53_SECURITY_RAM].name,
                                fsl_imx53_memmap[FSL_IMX53_SECURITY_RAM].size,
                                errp)) {
        return;
    }
    memory_region_add_subregion(get_system_memory(),
                                fsl_imx53_memmap[FSL_IMX53_SECURITY_RAM].addr,
                                &s->caam);

    /* OCRAM memory */
    if (!memory_region_init_ram(&s->ocram, OBJECT(dev),
                                fsl_imx53_memmap[FSL_IMX53_OCRAM].name,
                                fsl_imx53_memmap[FSL_IMX53_OCRAM].size, errp)) {
        return;
    }
    memory_region_add_subregion(get_system_memory(),
                                fsl_imx53_memmap[FSL_IMX53_OCRAM].addr,
                                &s->ocram);


    /* Unimplemented devices */
    for (size_t i = 0; i < ARRAY_SIZE(fsl_imx53_memmap); i++) {
        switch (i) {
        case FSL_IMX53_CCM:
        case FSL_IMX53_CSD0 ... FSL_IMX53_CSD1:
        case FSL_IMX53_DPLLC1 ... FSL_IMX53_DPLLC4:
        case FSL_IMX53_ECSPI1 ... FSL_IMX53_ECSPI2:
        case FSL_IMX53_EPIT1 ... FSL_IMX53_EPIT2:
        case FSL_IMX53_ESDHC1 ... FSL_IMX53_ESDHC4:
        case FSL_IMX53_FEC:
        case FSL_IMX53_FLEXCAN1 ... FSL_IMX53_FLEXCAN2:
        case FSL_IMX53_GPIO1 ... FSL_IMX53_GPIO7:
        case FSL_IMX53_GPT:
        case FSL_IMX53_I2C1 ... FSL_IMX53_I2C3:
        case FSL_IMX53_OCRAM:
        case FSL_IMX53_SATA:
        case FSL_IMX53_SRTC:
        case FSL_IMX53_TZIC:
        case FSL_IMX53_UART1 ... FSL_IMX53_UART4:
        case FSL_IMX53_USB1 ... FSL_IMX53_USB4:
        case FSL_IMX53_USB_MISC:
        case FSL_IMX53_WDOG1 ... FSL_IMX53_WDOG2:
            /* device implemented and treated above */
            break;

        default:
            create_unimplemented_device(fsl_imx53_memmap[i].name,
                                        fsl_imx53_memmap[i].addr,
                                        fsl_imx53_memmap[i].size);
            break;
        }
    }
}

static const Property fsl_imx53_properties[] = {
    DEFINE_PROP_UINT32("fec-phy-num", FslImx53State, phy_num, 0),
    DEFINE_PROP_LINK("canbus0", FslImx53State, canbus[0], TYPE_CAN_BUS,
                     CanBusState *),
    DEFINE_PROP_LINK("canbus1", FslImx53State, canbus[1], TYPE_CAN_BUS,
                     CanBusState *),
};

static void fsl_imx53_class_init(ObjectClass *oc, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    device_class_set_props(dc, fsl_imx53_properties);
    dc->realize = fsl_imx53_realize;
    dc->desc = "i.MX53 SOC";
    /* Reason: Uses serial_hd() in the realize() function */
    dc->user_creatable = false;
}

static const TypeInfo fsl_imx53_type_info = {
    .name = TYPE_FSL_IMX53,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(FslImx53State),
    .instance_init = fsl_imx53_init,
    .class_init = fsl_imx53_class_init,
};

static void fsl_imx53_register_types(void)
{
    type_register_static(&fsl_imx53_type_info);
}

type_init(fsl_imx53_register_types)
