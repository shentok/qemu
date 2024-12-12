/*
 * Copyright (c) 2018, Impinj, Inc.
 *
 * i.MX7 SoC definitions
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * Based on hw/arm/fsl-imx6.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qapi/qmp/qlist.h"
#include "hw/arm/fsl-imx8mp.h"
#include "hw/arm/bsa.h"
#include "hw/misc/unimp.h"
#include "hw/boards.h"
#include "sysemu/sysemu.h"
#include "qemu/module.h"
#include "target/arm/cpu-qom.h"

#define NAME_SIZE 20

static MemTxResult ioport80_write(void *opaque, hwaddr addr,
                           uint64_t value, unsigned size,
                           MemTxAttrs attrs)
{
    return MEMTX_DECODE_ERROR;
}

static MemTxResult ioport80_read(void *opaque, hwaddr addr,
                              uint64_t *data, unsigned size,
                              MemTxAttrs attrs)
{
    return MEMTX_DECODE_ERROR;
}

static const MemoryRegionOps ioport80_io_ops = {
    .write_with_attrs = ioport80_write,
    .read_with_attrs = ioport80_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void fsl_imx8mp_init(Object *obj)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    FslIMX8MPState *s = FSL_IMX8MP(obj);
    char name[NAME_SIZE];
    int i;

    memory_region_init_io(&s->caam, obj, &ioport80_io_ops, NULL, "fallback",
                          FSL_IMX8MP_DDR_ADDR);

    /*
     * CPUs
     */
    for (i = 0; i < MIN(ms->smp.cpus, FSL_IMX8MP_NUM_CPUS); i++) {
        snprintf(name, NAME_SIZE, "cpu%d", i);
        object_initialize_child(obj, name, &s->cpu[i],
                                ARM_CPU_TYPE_NAME("cortex-a53"));
    }

    /*
     * GIC
     */
    object_initialize_child(obj, "gic", &s->gic, TYPE_ARM_GICV3);

    /*
     * UARTs
     */
    for (i = 0; i < FSL_IMX8MP_NUM_UARTS; i++) {
        snprintf(name, NAME_SIZE, "uart%d", i + 1);
        object_initialize_child(obj, name, &s->uart[i], TYPE_IMX_SERIAL);
    }
}

static void fsl_imx8mp_realize(DeviceState *dev, Error **errp)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    FslIMX8MPState *s = FSL_IMX8MP(dev);
    MemoryRegion *sysmem = get_system_memory();
    Object *o;
    int i;
    qemu_irq irq;
    char name[NAME_SIZE];
    unsigned int smp_cpus = ms->smp.cpus;

    if (smp_cpus > FSL_IMX8MP_NUM_CPUS) {
        error_setg(errp, "%s: Only %d CPUs are supported (%d requested)",
                   TYPE_FSL_IMX8MP, FSL_IMX8MP_NUM_CPUS, smp_cpus);
        return;
    }

    /*
     * CPUs
     */
    for (i = 0; i < smp_cpus; i++) {
        o = OBJECT(&s->cpu[i]);

        /* On uniprocessor, the CBAR is set to 0 */
        if (smp_cpus > 1) {
            object_property_set_int(o, "reset-cbar", FSL_IMX8MP_GIC_ADDR,
                                    &error_abort);
        }

        if (i) {
            /*
             * Secondary CPUs start in powered-down state (and can be
             * powered up via the SRC system reset controller)
             */
            object_property_set_bool(o, "start-powered-off", true,
                                     &error_abort);
        }

        qdev_realize(DEVICE(o), NULL, &error_abort);
    }

    memory_region_add_subregion_overlap(get_system_memory(), 0, &s->caam,
                                        -10000);

    /*
     * GIC
     */
    {
        DeviceState *gicdev;
        QList *redist_region_count;

        object_initialize_child(OBJECT(ms), "gic", &s->gic, TYPE_ARM_GICV3);
        gicdev = DEVICE(&s->gic);
        qdev_prop_set_uint32(gicdev, "num-cpu", ms->smp.cpus);
        qdev_prop_set_uint32(gicdev, "num-irq", FSL_IMX8MP_NUM_SPIS + GIC_INTERNAL);
        redist_region_count = qlist_new();
        qlist_append_int(redist_region_count, ms->smp.cpus);
        qdev_prop_set_array(gicdev, "redist-region-count", redist_region_count);
        object_property_set_link(OBJECT(&s->gic), "sysmem",
                                 OBJECT(sysmem), &error_fatal);
        sysbus_realize(SYS_BUS_DEVICE(&s->gic), &error_fatal);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 0, FSL_IMX8MP_GIC_ADDR);
        sysbus_mmio_map(SYS_BUS_DEVICE(&s->gic), 1, FSL_IMX8MP_GIC_ADDR + 0x80000);
        /*
         * Wire the outputs from each CPU's generic timer and the GICv3
         * maintenance interrupt signal to the appropriate GIC PPI inputs,
         * and the GIC's IRQ/FIQ/VIRQ/VFIQ interrupt outputs to the CPU's inputs.
         */
        for (i = 0; i < ms->smp.cpus; i++) {
            DeviceState *cpudev = DEVICE(&s->cpu[i]);
            SysBusDevice *gicsbd = SYS_BUS_DEVICE(&s->gic);
            int intidbase = FSL_IMX8MP_NUM_SPIS + i * GIC_INTERNAL;
            /*
             * Mapping from the output timer irq lines from the CPU to the
             * GIC PPI inputs used for this board. This isn't a BSA board,
             * but it uses the standard convention for the PPI numbers.
             */
            const int timer_irq[] = {
                [GTIMER_PHYS] = ARCH_TIMER_NS_EL1_IRQ,
                [GTIMER_VIRT] = ARCH_TIMER_VIRT_IRQ,
                [GTIMER_HYP]  = ARCH_TIMER_NS_EL2_IRQ,
            };

            for (int j = 0; j < ARRAY_SIZE(timer_irq); j++) {
                qdev_connect_gpio_out(cpudev, j,
                                      qdev_get_gpio_in(gicdev,
                                                       intidbase + timer_irq[j]));
            }

            qdev_connect_gpio_out_named(cpudev, "gicv3-maintenance-interrupt", 0,
                                        qdev_get_gpio_in(gicdev,
                                                         intidbase + ARCH_GIC_MAINT_IRQ));

            qdev_connect_gpio_out_named(cpudev, "pmu-interrupt", 0,
                                        qdev_get_gpio_in(gicdev,
                                                         intidbase + VIRTUAL_PMU_IRQ));

            sysbus_connect_irq(gicsbd, i,
                               qdev_get_gpio_in(cpudev, ARM_CPU_IRQ));
            sysbus_connect_irq(gicsbd, i + ms->smp.cpus,
                               qdev_get_gpio_in(cpudev, ARM_CPU_FIQ));
            sysbus_connect_irq(gicsbd, i + 2 * ms->smp.cpus,
                               qdev_get_gpio_in(cpudev, ARM_CPU_VIRQ));
            sysbus_connect_irq(gicsbd, i + 3 * ms->smp.cpus,
                               qdev_get_gpio_in(cpudev, ARM_CPU_VFIQ));
        }
    }

    create_unimplemented_device("nxp,sysctr-timer-cmp", FSL_IMX8MP_SYSCNT_CMP_ADDR,
                                FSL_IMX8MP_SYSCNT_CMP_SIZE);
    create_unimplemented_device("nxp,sysctr-timer-rd", FSL_IMX8MP_SYSCNT_RD_ADDR,
                                FSL_IMX8MP_SYSCNT_RD_SIZE);

    /*
     * GPTs
     */
    for (i = 0; i < FSL_IMX8MP_NUM_GPTS; i++) {
        static const hwaddr FSL_IMX8MP_GPTn_ADDR[FSL_IMX8MP_NUM_GPTS] = {
            FSL_IMX8MP_GPT1_ADDR,
            FSL_IMX8MP_GPT2_ADDR,
            FSL_IMX8MP_GPT3_ADDR,
            FSL_IMX8MP_GPT4_ADDR,
            FSL_IMX8MP_GPT5_ADDR,
            FSL_IMX8MP_GPT6_ADDR,
        };

        snprintf(name, NAME_SIZE, "gpt%d", i);
        create_unimplemented_device(name, FSL_IMX8MP_GPTn_ADDR[i],
                                    FSL_IMX8MP_GPIOn_SIZE);
    }

    /*
     * GPIOs
     */
    for (i = 0; i < FSL_IMX8MP_NUM_GPIOS; i++) {
        static const hwaddr FSL_IMX8MP_GPIOn_ADDR[FSL_IMX8MP_NUM_GPIOS] = {
            FSL_IMX8MP_GPIO1_ADDR,
            FSL_IMX8MP_GPIO2_ADDR,
            FSL_IMX8MP_GPIO3_ADDR,
            FSL_IMX8MP_GPIO4_ADDR,
            FSL_IMX8MP_GPIO5_ADDR,
        };

        snprintf(name, NAME_SIZE, "gpio%d", i);
        create_unimplemented_device(name, FSL_IMX8MP_GPIOn_ADDR[i],
                                    FSL_IMX8MP_GPIOn_SIZE);
    }

    /*
     * IOMUXC
     */
    create_unimplemented_device("iomuxc", FSL_IMX8MP_IOMUXC_ADDR,
                                FSL_IMX8MP_IOMUXC_SIZE);

    /*
     * CCM
     */
    create_unimplemented_device("ccm", FSL_IMX8MP_CCM_ADDR,
                                FSL_IMX8MP_CCM_SIZE);

    /*
     * Analog
     */
    create_unimplemented_device("analog", FSL_IMX8MP_ANA_PLL_ADDR,
                                FSL_IMX8MP_ANA_PLL_SIZE);

    /*
     * GPCv2
     */
    create_unimplemented_device("gpcv2", FSL_IMX8MP_GPC_ADDR,
                                FSL_IMX8MP_GPC_SIZE);

    create_unimplemented_device("HSIO BLK_CTL", FSL_IMX8MP_HSIO_BLK_CTL_ADDR,
                                FSL_IMX8MP_HSIO_BLK_CTL_SIZE);

    create_unimplemented_device("MEDIA_BLK_CTL", FSL_IMX8MP_MEDIA_BLK_CTL_ADDR,
                                FSL_IMX8MP_MEDIA_BLK_CTL_SIZE);

    create_unimplemented_device("HDMI_TX", FSL_IMX8MP_HDMI_TX_ADDR,
                                FSL_IMX8MP_HDMI_TX_SIZE);

    create_unimplemented_device("interconnect", FSL_IMX8MP_INTERCONNECT_ADDR,
                                FSL_IMX8MP_INTERCONNECT_SIZE);

    create_unimplemented_device("LCDIF1", FSL_IMX8MP_LCDIF1_ADDR,
                                FSL_IMX8MP_LCDIF1_SIZE);
    create_unimplemented_device("LCDIF2", FSL_IMX8MP_LCDIF2_ADDR,
                                FSL_IMX8MP_LCDIF2_SIZE);

    create_unimplemented_device("QSPI", FSL_IMX8MP_QSPI_ADDR,
                                FSL_IMX8MP_QSPI_SIZE);

    create_unimplemented_device("GPU2D", FSL_IMX8MP_GPU2D_ADDR,
                                FSL_IMX8MP_GPU2D_SIZE);
    create_unimplemented_device("GPU3D", FSL_IMX8MP_GPU3D_ADDR,
                                FSL_IMX8MP_GPU3D_SIZE);

    create_unimplemented_device("MU_1_A", FSL_IMX8MP_MU_1_A_ADDR,
                                FSL_IMX8MP_MU_1_A_SIZE);

    create_unimplemented_device("ANA_TSENSOR", FSL_IMX8MP_ANA_TSENSOR_ADDR,
                                FSL_IMX8MP_ANA_TSENSOR_SIZE);

    /*
     * ECSPIs
     */
    for (i = 0; i < FSL_IMX8MP_NUM_ECSPIS; i++) {
        static const hwaddr FSL_IMX8MP_SPIn_ADDR[FSL_IMX8MP_NUM_ECSPIS] = {
            FSL_IMX8MP_ECSPI1_ADDR,
            FSL_IMX8MP_ECSPI2_ADDR,
            FSL_IMX8MP_ECSPI3_ADDR,
        };

        snprintf(name, NAME_SIZE, "spi%d", i + 1);
        create_unimplemented_device(name, FSL_IMX8MP_SPIn_ADDR[i],
                                    FSL_IMX8MP_ECSPIn_SIZE);
    }

    /*
     * I2Cs
     */
    for (i = 0; i < FSL_IMX8MP_NUM_I2CS; i++) {
        static const hwaddr FSL_IMX8MP_I2Cn_ADDR[FSL_IMX8MP_NUM_I2CS] = {
            FSL_IMX8MP_I2C1_ADDR,
            FSL_IMX8MP_I2C2_ADDR,
            FSL_IMX8MP_I2C3_ADDR,
            FSL_IMX8MP_I2C4_ADDR,
            FSL_IMX8MP_I2C5_ADDR,
            FSL_IMX8MP_I2C6_ADDR,
        };

        snprintf(name, NAME_SIZE, "i2c%d", i + 1);
        create_unimplemented_device(name, FSL_IMX8MP_I2Cn_ADDR[i],
                                    FSL_IMX8MP_I2Cn_SIZE);
    }

    /*
     * UARTs
     */
    for (i = 0; i < FSL_IMX8MP_NUM_UARTS; i++) {
        static const hwaddr FSL_IMX8MP_UARTn_ADDR[FSL_IMX8MP_NUM_UARTS] = {
            FSL_IMX8MP_UART1_ADDR,
            FSL_IMX8MP_UART2_ADDR,
            FSL_IMX8MP_UART3_ADDR,
            FSL_IMX8MP_UART4_ADDR,
        };

        static const int FSL_IMX8MP_UARTn_IRQ[FSL_IMX8MP_NUM_UARTS] = {
            FSL_IMX8MP_UART1_IRQ,
            FSL_IMX8MP_UART2_IRQ,
            FSL_IMX8MP_UART3_IRQ,
            FSL_IMX8MP_UART4_IRQ,
        };


        qdev_prop_set_chr(DEVICE(&s->uart[i]), "chardev", serial_hd(i));

        sysbus_realize(SYS_BUS_DEVICE(&s->uart[i]), &error_abort);

        sysbus_mmio_map(SYS_BUS_DEVICE(&s->uart[i]), 0, FSL_IMX8MP_UARTn_ADDR[i]);

        irq = qdev_get_gpio_in(DEVICE(&s->gic), FSL_IMX8MP_UARTn_IRQ[i]);
        sysbus_connect_irq(SYS_BUS_DEVICE(&s->uart[i]), 0, irq);
    }

    /*
     * Ethernets
     */
    for (i = 0; i < FSL_IMX8MP_NUM_ETHS; i++) {
        static const hwaddr FSL_IMX8MP_ENETn_ADDR[FSL_IMX8MP_NUM_ETHS] = {
            FSL_IMX8MP_ENET1_ADDR,
        };

        snprintf(name, NAME_SIZE, "eth%d", i + 1);
        create_unimplemented_device(name, FSL_IMX8MP_ENETn_ADDR[i],
                                    FSL_IMX8MP_ENETn_SIZE);
    }

    /*
     * USDHCs
     */
    for (i = 0; i < FSL_IMX8MP_NUM_USDHCS; i++) {
        static const hwaddr FSL_IMX8MP_USDHCn_ADDR[FSL_IMX8MP_NUM_USDHCS] = {
            FSL_IMX8MP_USDHC1_ADDR,
            FSL_IMX8MP_USDHC2_ADDR,
            FSL_IMX8MP_USDHC3_ADDR,
        };

        snprintf(name, NAME_SIZE, "usdhc%d", i + 1);
        create_unimplemented_device(name, FSL_IMX8MP_USDHCn_ADDR[i],
                                    FSL_IMX8MP_USDHCn_SIZE);
    }

    /*
     * SNVS
     */
    create_unimplemented_device("snvs", FSL_IMX8MP_SNVS_HP_ADDR,
                                FSL_IMX8MP_SNVS_HP_SIZE);

    /*
     * Watchdogs
     */
    for (i = 0; i < FSL_IMX8MP_NUM_WDTS; i++) {
        static const hwaddr FSL_IMX8MP_WDOGn_ADDR[FSL_IMX8MP_NUM_WDTS] = {
            FSL_IMX8MP_WDOG1_ADDR,
            FSL_IMX8MP_WDOG2_ADDR,
            FSL_IMX8MP_WDOG3_ADDR,
        };

        snprintf(name, NAME_SIZE, "wdt%d", i + 1);
        create_unimplemented_device(name, FSL_IMX8MP_WDOGn_ADDR[i],
                                    FSL_IMX8MP_WDOGn_SIZE);
    }

    /*
     * PWMs
     */
    for (i = 0; i < FSL_IMX8MP_NUM_PWMS; i++) {
        static const hwaddr FSL_IMX8MP_PWMn_ADDR[FSL_IMX8MP_NUM_PWMS] = {
            FSL_IMX8MP_PWM1_ADDR,
            FSL_IMX8MP_PWM2_ADDR,
            FSL_IMX8MP_PWM3_ADDR,
            FSL_IMX8MP_PWM4_ADDR,
        };

        snprintf(name, NAME_SIZE, "pwm%d", i + 1);
        create_unimplemented_device(name, FSL_IMX8MP_PWMn_ADDR[i],
                                    FSL_IMX8MP_PWMn_SIZE);
    }

    /*
     * OCOTP
     */
    create_unimplemented_device("ocotp", FSL_IMX8MP_OCOTP_CTRL_ADDR,
                                FSL_IMX8MP_OCOTP_CTRL_SIZE);

    /*
     * USBs
     */
    for (i = 0; i < FSL_IMX8MP_NUM_USBS; i++) {
        static const hwaddr FSL_IMX8MP_USBMISCn_ADDR[FSL_IMX8MP_NUM_USBS] = {
            FSL_IMX8MP_USB1_ADDR,
            FSL_IMX8MP_USB2_ADDR,
        };

        snprintf(name, NAME_SIZE, "usb%d", i + 1);
        create_unimplemented_device(name, FSL_IMX8MP_USBMISCn_ADDR[i],
                                    FSL_IMX8MP_USBn_SIZE);
    }

    /*
     * DMA APBH
     */
    create_unimplemented_device("dma-apbh", FSL_IMX8MP_APBH_DMA_ADDR,
                                FSL_IMX8MP_APBH_DMA_SIZE);

    create_unimplemented_device("DDRC 4MB DDR CTL", FSL_IMX8MP_DDR_CTL_ADDR,
                                FSL_IMX8MP_DDR_CTL_SIZE);
}

static Property fsl_imx8mp_properties[] = {
    DEFINE_PROP_UINT32("fec1-phy-num", FslIMX8MPState, phy_num[0], 0),
    DEFINE_PROP_UINT32("fec2-phy-num", FslIMX8MPState, phy_num[1], 1),
    DEFINE_PROP_BOOL("fec1-phy-connected", FslIMX8MPState, phy_connected[0],
                     true),
    DEFINE_PROP_BOOL("fec2-phy-connected", FslIMX8MPState, phy_connected[1],
                     true),
    DEFINE_PROP_END_OF_LIST(),
};

static void fsl_imx8mp_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    device_class_set_props(dc, fsl_imx8mp_properties);
    dc->realize = fsl_imx8mp_realize;

    /* Reason: Uses serial_hds and nd_table in realize() directly */
    dc->user_creatable = false;
    dc->desc = "i.MX7 SOC";
}

static const TypeInfo fsl_imx8mp_type_info = {
    .name = TYPE_FSL_IMX8MP,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(FslIMX8MPState),
    .instance_init = fsl_imx8mp_init,
    .class_init = fsl_imx8mp_class_init,
};

static void fsl_imx8mp_register_types(void)
{
    type_register_static(&fsl_imx8mp_type_info);
}
type_init(fsl_imx8mp_register_types)
