/*
 * Freescale MPIC emulation
 *
 * Copyright (c) 2024 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/ppc/openpic.h"
#include "hw/ppc/openpic_kvm.h"
#include "hw/boards.h"
#include "hw/sysbus.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "qom/object.h"
#include "system/kvm.h"

#define TYPE_FSL_MPIC "fsl_mpic"
OBJECT_DECLARE_SIMPLE_TYPE(MpicState, FSL_MPIC)

struct MpicState
{
    SysBusDevice device;

    union {
        OpenPICState openpic;
        KVMOpenPICState openpic_kvm;
    };

    int (*connect_vcpu)(DeviceState *dev, CPUState *cs);
};

static void mpic_init_qemu(Object *obj)
{
    MpicState *s = FSL_MPIC(obj);
    MachineState *machine = MACHINE(qdev_get_machine());
    unsigned int smp_cpus = machine->smp.cpus;

    object_initialize_child(obj, "openpic", &s->openpic, TYPE_OPENPIC);
    qdev_prop_set_uint32(DEVICE(&s->openpic), "nb_cpus", smp_cpus);
    s->connect_vcpu = openpic_connect_vcpu;
}

static void mpic_init_kvm(Object *obj, Error **errp)
{
#ifdef CONFIG_OPENPIC_KVM
    MpicState *s = FSL_MPIC(obj);

    object_initialize_child(obj, "openpic", &s->openpic_kvm, TYPE_KVM_OPENPIC);
    s->connect_vcpu = kvm_openpic_connect_vcpu;
#else
    g_assert_not_reached();
#endif
}

static void mpic_init(Object *obj)
{
    Error *err = NULL;

    if (kvm_enabled()) {
        if (kvm_kernel_irqchip_allowed()) {
            mpic_init_kvm(obj, &err);
        }
        if (kvm_kernel_irqchip_required() && err) {
            error_reportf_err(err,
                              "kernel_irqchip requested but unavailable: ");
            exit(1);
        }
    }

    if (err || !kvm_enabled()) {
        mpic_init_qemu(obj);
    }
}

static void mpic_realize(DeviceState *dev, Error **errp)
{
    ERRP_GUARD();
    MpicState *s = FSL_MPIC(dev);
    SysBusDevice *internal = SYS_BUS_DEVICE(&s->openpic);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    CPUState *cs;
    const uint32_t mpic_version = object_property_get_int(
        OBJECT(dev->parent_bus), "mpic-version", &error_fatal);

    qdev_prop_set_uint32(DEVICE(internal), "model", mpic_version);
    sysbus_realize_and_unref(internal, errp);
    sysbus_init_mmio(sbd, sysbus_mmio_get_region(internal, 0));

    CPU_FOREACH(cs) {
        int error = s->connect_vcpu(DEVICE(internal), cs);
        if (error) {
            error_setg_errno(errp, error, "failed to connect vcpu to irqchip");
            return;
        }
    }

    qdev_pass_gpios(DEVICE(internal), dev, NULL);
}

static void mpic_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = mpic_realize;
}

static const TypeInfo types[] = {
    {
        .name          = TYPE_FSL_MPIC,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(MpicState),
        .class_init    = mpic_class_init,
        .instance_init = mpic_init,
    },
    {
        .name          = "fsl,mpic",
        .parent        = TYPE_FSL_MPIC,
    },
    {
        .name          = "chrp,open-pic",
        .parent        = TYPE_FSL_MPIC,
    },
};

DEFINE_TYPES(types)
