/*
 * Freescale i.MX53 Quick Start Board emulation.
 *
 * Copyright (c) 2015 Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This code is licensed under the GPL, version 2 or later.
 * See the file `COPYING' in the top level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/fsl-imx53.h"
#include "hw/arm/boot.h"
#include "hw/arm/machines-qom.h"
#include "hw/core/boards.h"
#include "qemu/error-report.h"
#include "system/qtest.h"

struct FslImx53QsbMachineState {
    MachineState parent_obj;

    FslImx53State soc;
    struct arm_boot_info boot_info;
};

#define TYPE_IMX53QSB_MACHINE MACHINE_TYPE_NAME("imx53-qsb")
OBJECT_DECLARE_SIMPLE_TYPE(FslImx53QsbMachineState, IMX53QSB_MACHINE)

static void imx53_qsb_init(MachineState *machine)
{
    FslImx53QsbMachineState *s = IMX53QSB_MACHINE(machine);

    /* Check the amount of memory is compatible with the SOC */
    if (machine->ram_size > FSL_IMX53_RAM_SIZE_MAX) {
        error_report("RAM size " RAM_ADDR_FMT " above max supported (%" PRIu64 ")",
                     machine->ram_size, FSL_IMX53_RAM_SIZE_MAX);
        exit(1);
    }

    s->boot_info = (struct arm_boot_info) {
        .ram_size = machine->ram_size,
        .loader_start = FSL_IMX53_RAM_START,
        .board_id = -1,
        .secure_boot = false,
    };

    object_initialize_child(OBJECT(machine), "soc", &s->soc, TYPE_FSL_IMX53);

    /* Ethernet PHY address is 6 */
    object_property_set_int(OBJECT(&s->soc), "fec-phy-num", 6, &error_fatal);

    qdev_realize(DEVICE(&s->soc), NULL, &error_fatal);

    memory_region_add_subregion(get_system_memory(), FSL_IMX53_RAM_START,
                                machine->ram);

    for (int i = 0; i < FSL_IMX53_NUM_ESDHCS; i++) {
        BusState *bus;
        DeviceState *carddev;
        BlockBackend *blk;
        DriveInfo *di = drive_get(IF_SD, i, 0);

        if (!di) {
            continue;
        }

        blk = blk_by_legacy_dinfo(di);
        bus = qdev_get_child_bus(DEVICE(&s->soc.esdhc[i]), "sd-bus");
        carddev = qdev_new(TYPE_SD_CARD);
        qdev_prop_set_drive_err(carddev, "drive", blk, &error_fatal);
        qdev_realize_and_unref(carddev, bus, &error_fatal);
    }

    if (!qtest_enabled()) {
        arm_load_kernel(&s->soc.cpu, machine, &s->boot_info);
    }
}

static void imx53_qsb_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Freescale i.MX53 Quick Start Board (Cortex-A8)";
    mc->init = imx53_qsb_init;
    mc->default_ram_id = "imx53-qsb.ram";
    mc->default_ram_size = 1 * GiB;
}

static const TypeInfo imx53_qsb_machine_init_types[] = {
    {
        .name          = TYPE_IMX53QSB_MACHINE,
        .parent        = TYPE_MACHINE,
        .class_init    = imx53_qsb_machine_class_init,
        .instance_size = sizeof(FslImx53QsbMachineState),
        .interfaces    = arm_machine_interfaces,
    }
};

DEFINE_TYPES(imx53_qsb_machine_init_types)
