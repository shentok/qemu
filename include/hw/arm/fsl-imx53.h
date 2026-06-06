/*
 * Freescale i.MX31 SoC emulation
 *
 * Copyright (C) 2015 Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef FSL_IMX53_H
#define FSL_IMX53_H

#include "hw/intc/fsl_tzic.h"
#include "hw/watchdog/wdt_imx2.h"
#include "hw/char/imx_serial.h"
#include "hw/i2c/imx_i2c.h"
#include "hw/gpio/imx_gpio.h"
#include "hw/sd/sdhci.h"
#include "hw/ssi/imx_spi.h"
#include "hw/net/imx_fec.h"
#include "hw/usb/chipidea.h"
#include "system/memory.h"
#include "target/arm/cpu.h"
#include "qom/object.h"
#include "qemu/units.h"

#define TYPE_FSL_IMX53 "fsl-imx53"
OBJECT_DECLARE_SIMPLE_TYPE(FslImx53State, FSL_IMX53)

#define FSL_IMX53_RAM_START        0x70000000
#define FSL_IMX53_RAM_SIZE_MAX     (2 * GiB)

enum FslImx8mpConfiguration {
    FSL_IMX53_NUM_ESDHCS = 4,
};

struct FslImx53State {
    DeviceState parent_obj;

    ARMCPU             cpu;
    FslTzicState       tzic;
    IMXSerialState     uart[5];
    IMXI2CState        i2c[3];
    IMXGPIOState       gpio[7];
    SDHCIState         esdhc[FSL_IMX53_NUM_ESDHCS];
    IMXSPIState        spi[2];
    IMX2WdtState       wdt[2];
    ChipideaState      usb[4];
    IMXFECState        eth;
    MemoryRegion       rom;
    MemoryRegion       caam;
    MemoryRegion       ocram;
    uint32_t           phy_num;
};

enum FslImx53MemoryRegions {
    FSL_IMX53_AHBMAX,
    FSL_IMX53_AIPSTZ1_ALIAS,
    FSL_IMX53_AIPSTZ1_GLOBAL0,
    FSL_IMX53_AIPSTZ1_GLOBAL1,
    FSL_IMX53_AIPSTZ1_ONPLAT,
    FSL_IMX53_AIPSTZ1_RSVD0,
    FSL_IMX53_AIPSTZ1_RSVD2,
    FSL_IMX53_AIPSTZ2_ALIAS,
    FSL_IMX53_AIPSTZ2_GLOBAL0,
    FSL_IMX53_AIPSTZ2_GLOBAL1,
    FSL_IMX53_AIPSTZ2_ONPLAT,
    FSL_IMX53_APB2IP_2X2,
    FSL_IMX53_APB2IP_4X1,
    FSL_IMX53_ARM_DEBUG,
    FSL_IMX53_ARM_PLATFORM,
    FSL_IMX53_ASRC,
    FSL_IMX53_AUDMUX,
    FSL_IMX53_BOOT_ROM,
    FSL_IMX53_BOOT_ROM_ALIASING,
    FSL_IMX53_CCM,
    FSL_IMX53_CS,
    FSL_IMX53_CSD0,
    FSL_IMX53_CSD1,
    FSL_IMX53_CSPI,
    FSL_IMX53_CSU,
    FSL_IMX53_CTI0,
    FSL_IMX53_CTI1,
    FSL_IMX53_CTI2,
    FSL_IMX53_CTI3,
    FSL_IMX53_DEBUG_RESERVED,
    FSL_IMX53_DEBUG_ROM,
    FSL_IMX53_DPLLC1,
    FSL_IMX53_DPLLC2,
    FSL_IMX53_DPLLC3,
    FSL_IMX53_DPLLC4,
    FSL_IMX53_ECSPI1,
    FSL_IMX53_ECSPI2,
    FSL_IMX53_EPIT1,
    FSL_IMX53_EPIT2,
    FSL_IMX53_ESAI1,
    FSL_IMX53_ESDHC1,
    FSL_IMX53_ESDHC2,
    FSL_IMX53_ESDHC3,
    FSL_IMX53_ESDHC4,
    FSL_IMX53_ETB,
    FSL_IMX53_ETM,
    FSL_IMX53_EXTMC,
    FSL_IMX53_FEC,
    FSL_IMX53_FIRI,
    FSL_IMX53_FLEXCAN1,
    FSL_IMX53_FLEXCAN2,
    FSL_IMX53_GPC,
    FSL_IMX53_GPIO1,
    FSL_IMX53_GPIO2,
    FSL_IMX53_GPIO3,
    FSL_IMX53_GPIO4,
    FSL_IMX53_GPIO5,
    FSL_IMX53_GPIO6,
    FSL_IMX53_GPIO7,
    FSL_IMX53_GPT,
    FSL_IMX53_GPU2D,
    FSL_IMX53_GPU3D,
    FSL_IMX53_GPU3D_GMEM,
    FSL_IMX53_I2C1,
    FSL_IMX53_I2C2,
    FSL_IMX53_I2C3,
    FSL_IMX53_IIM,
    FSL_IMX53_IOMUXC,
    FSL_IMX53_IPU,
    FSL_IMX53_KPP,
    FSL_IMX53_MLB,
    FSL_IMX53_NAND_BUF,
    FSL_IMX53_OCRAM,
    FSL_IMX53_OWIRE,
    FSL_IMX53_PATA_PIO,
    FSL_IMX53_PATA_UDMA,
    FSL_IMX53_PTP,
    FSL_IMX53_PWM1,
    FSL_IMX53_PWM2,
    FSL_IMX53_RESERVED_0100,
    FSL_IMX53_RESERVED_0800,
    FSL_IMX53_RESERVED_1400,
    FSL_IMX53_RESERVED_6FFF,
    FSL_IMX53_RESERVED_F806,
    FSL_IMX53_ROMC,
    FSL_IMX53_RTC,
    FSL_IMX53_RTIC,
    FSL_IMX53_SAHARA,
    FSL_IMX53_SATA,
    FSL_IMX53_SATA_ALIAS,
    FSL_IMX53_SCC,
    FSL_IMX53_SCC_ALIAS,
    FSL_IMX53_SDMA,
    FSL_IMX53_SDMA2,
    FSL_IMX53_SECURITY_RAM,
    FSL_IMX53_SPBA,
    FSL_IMX53_SPDIF,
    FSL_IMX53_SRC,
    FSL_IMX53_SRTC,
    FSL_IMX53_SSI1,
    FSL_IMX53_SSI2,
    FSL_IMX53_SSI3,
    FSL_IMX53_TPIU,
    FSL_IMX53_TVE,
    FSL_IMX53_TZIC,
    FSL_IMX53_UART1,
    FSL_IMX53_UART2,
    FSL_IMX53_UART3,
    FSL_IMX53_UART4,
    FSL_IMX53_UART5,
    FSL_IMX53_USB1,
    FSL_IMX53_USB2,
    FSL_IMX53_USB3,
    FSL_IMX53_USB4,
    FSL_IMX53_USB_MISC,
    FSL_IMX53_USB_PL301,
    FSL_IMX53_VPU,
    FSL_IMX53_WDOG1,
    FSL_IMX53_WDOG2,
};

enum FslImx53Irqs {
    FSL_IMX53_ESDHC1_IRQ      = 1,
    FSL_IMX53_ESDHC2_IRQ      = 2,
    FSL_IMX53_ESDHC3_IRQ      = 3,
    FSL_IMX53_ESDHC4_IRQ      = 4,

    FSL_IMX53_UART1_IRQ       = 31,
    FSL_IMX53_UART2_IRQ       = 32,
    FSL_IMX53_UART3_IRQ       = 33,
    FSL_IMX53_UART4_IRQ       = 13,
    FSL_IMX53_UART5_IRQ       = 86,

    FSL_IMX53_ECSPI1_IRQ      = 36,
    FSL_IMX53_ECSPI2_IRQ      = 37,

    FSL_IMX53_I2C1_IRQ        = 62,
    FSL_IMX53_I2C2_IRQ        = 63,
    FSL_IMX53_I2C3_IRQ        = 64,

    FSL_IMX53_USB_HOST2_IRQ   = 14,
    FSL_IMX53_USB_HOST3_IRQ   = 16,
    FSL_IMX53_USB_HOST4_IRQ   = 17,
    FSL_IMX53_USB_OTG_IRQ     = 18,

    FSL_IMX53_GPT_IRQ         = 39,
    FSL_IMX53_EPIT1_IRQ       = 40,
    FSL_IMX53_EPIT2_IRQ       = 41,

    FSL_IMX53_GPIO1_LOW_IRQ   = 50,
    FSL_IMX53_GPIO1_HIGH_IRQ  = 51,
    FSL_IMX53_GPIO2_LOW_IRQ   = 52,
    FSL_IMX53_GPIO2_HIGH_IRQ  = 53,
    FSL_IMX53_GPIO3_LOW_IRQ   = 54,
    FSL_IMX53_GPIO3_HIGH_IRQ  = 55,
    FSL_IMX53_GPIO4_LOW_IRQ   = 56,
    FSL_IMX53_GPIO4_HIGH_IRQ  = 57,
    FSL_IMX53_GPIO5_LOW_IRQ   = 103,
    FSL_IMX53_GPIO5_HIGH_IRQ  = 104,
    FSL_IMX53_GPIO6_LOW_IRQ   = 105,
    FSL_IMX53_GPIO6_HIGH_IRQ  = 106,
    FSL_IMX53_GPIO7_LOW_IRQ   = 107,
    FSL_IMX53_GPIO7_HIGH_IRQ  = 108,

    FSL_IMX53_WDOG1_IRQ       = 58,
    FSL_IMX53_WDOG2_IRQ       = 59,

    FSL_IMX53_ENET_MAC_IRQ    = 87
};

#endif /* FSL_IMX53_H */
