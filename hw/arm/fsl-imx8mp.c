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

static void fsl_imx8mp_init(Object *obj)
{
    MachineState *ms = MACHINE(qdev_get_machine());
    FslIMX8MPState *s = FSL_IMX8MP(obj);
    char name[NAME_SIZE];
    int i;

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
        snprintf(name, NAME_SIZE, "uart%d", i);
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

    /*
     * IOMUXC
     */
    create_unimplemented_device("iomuxc", FSL_IMX8MP_IOMUXC_ADDR,
                                FSL_IMX8MP_IOMUXC_SIZE);

    /*
     * CCM
     */
    create_unimplemented_device("ccm", FSL_IMX8MP_CCM_ADDR, 64 * KiB);

    /*
     * Analog
     */
    create_unimplemented_device("analog", FSL_IMX8MP_ANA_PLL_ADDR, 64 * KiB);

    /*
     * GPCv2
     */
    create_unimplemented_device("gpcv2", FSL_IMX8MP_GPC_ADDR, 0x10000);

    create_unimplemented_device("HSIO BLK_CTL", 0x32F10000, 64 * KiB);

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
     * OCOTP
     */
    create_unimplemented_device("ocotp", FSL_IMX8MP_OCOTP_CTRL_ADDR,
                                FSL_IMX8MP_OCOTP_CTRL_SIZE);
    /*
     * DMA APBH
     */
    create_unimplemented_device("dma-apbh", FSL_IMX8MP_APBH_DMA_ADDR,
                                FSL_IMX8MP_APBH_DMA_SIZE);
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
