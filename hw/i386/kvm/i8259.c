/*
 * KVM in-kernel PIC (i8259) support
 *
 * Copyright (c) 2011 Siemens AG
 *
 * Authors:
 *  Jan Kiszka          <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL version 2.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/isa/i8259_internal.h"
#include "hw/intc/i8259.h"
#include "qemu/module.h"
#include "hw/intc/kvm_irqcount.h"
#include "hw/irq.h"
#include "sysemu/kvm.h"
#include "qom/object.h"

#define TYPE_KVM_I8259 "kvm-i8259"
typedef struct KVMI8259Class KVMI8259Class;
DECLARE_CLASS_CHECKERS(KVMI8259Class, KVM_I8259,
                       TYPE_KVM_I8259)

/**
 * KVMI8259Class:
 * @parent_realize: The parent's realizefn.
 */
struct KVMI8259Class {
    I8259CommonClass parent_class;

    DeviceRealize parent_realize;
};

static void kvm_pic_get(I8259CommonState *s)
{
    struct kvm_irqchip chip;
    struct kvm_pic_state *kpic;
    int ret;

    chip.chip_id = s->master ? KVM_IRQCHIP_PIC_MASTER : KVM_IRQCHIP_PIC_SLAVE;
    ret = kvm_vm_ioctl(kvm_state, KVM_GET_IRQCHIP, &chip);
    if (ret < 0) {
        fprintf(stderr, "KVM_GET_IRQCHIP failed: %s\n", strerror(-ret));
        abort();
    }

    kpic = &chip.chip.pic;

    s->last_irr = kpic->last_irr;
    s->irr = kpic->irr;
    s->imr = kpic->imr;
    s->isr = kpic->isr;
    s->priority_add = kpic->priority_add;
    s->irq_base = kpic->irq_base;
    s->read_reg_select = kpic->read_reg_select;
    s->poll = kpic->poll;
    s->special_mask = kpic->special_mask;
    s->init_state = kpic->init_state;
    s->auto_eoi = kpic->auto_eoi;
    s->rotate_on_auto_eoi = kpic->rotate_on_auto_eoi;
    s->special_fully_nested_mode = kpic->special_fully_nested_mode;
    s->init4 = kpic->init4;
    s->elcr = kpic->elcr;
    s->elcr_mask = kpic->elcr_mask;
}

static void kvm_pic_put(I8259CommonState *s)
{
    struct kvm_irqchip chip;
    struct kvm_pic_state *kpic;
    int ret;

    chip.chip_id = s->master ? KVM_IRQCHIP_PIC_MASTER : KVM_IRQCHIP_PIC_SLAVE;

    kpic = &chip.chip.pic;

    kpic->last_irr = s->last_irr;
    kpic->irr = s->irr;
    kpic->imr = s->imr;
    kpic->isr = s->isr;
    kpic->priority_add = s->priority_add;
    kpic->irq_base = s->irq_base;
    kpic->read_reg_select = s->read_reg_select;
    kpic->poll = s->poll;
    kpic->special_mask = s->special_mask;
    kpic->init_state = s->init_state;
    kpic->auto_eoi = s->auto_eoi;
    kpic->rotate_on_auto_eoi = s->rotate_on_auto_eoi;
    kpic->special_fully_nested_mode = s->special_fully_nested_mode;
    kpic->init4 = s->init4;
    kpic->elcr = s->elcr;
    kpic->elcr_mask = s->elcr_mask;

    ret = kvm_vm_ioctl(kvm_state, KVM_SET_IRQCHIP, &chip);
    if (ret < 0) {
        fprintf(stderr, "KVM_SET_IRQCHIP failed: %s\n", strerror(-ret));
        abort();
    }
}

static void kvm_pic_reset(DeviceState *dev)
{
    I8259CommonState *s = I8259_COMMON(dev);

    s->elcr = 0;
    i8259_reset_common(s);

    kvm_pic_put(s);
}

static void kvm_pic_set_irq(void *opaque, int irq, int level)
{
    I8259CommonState *s = opaque;
    int irq_index = s->master ? irq : irq - 8;
    int delivered;

    i8259_stat_update_irq(s, irq_index, level);
    delivered = kvm_set_irq(kvm_state, irq, level);
    kvm_report_irq_delivered(delivered);
}

static void kvm_pic_realize(DeviceState *dev, Error **errp)
{
    I8259CommonState *s = I8259_COMMON(dev);
    KVMI8259Class *kpc = KVM_I8259_GET_CLASS(dev);

    memory_region_init_io(&s->base_io, OBJECT(dev), NULL, NULL, "kvm-pic", 2);
    memory_region_init_io(&s->elcr_io, OBJECT(dev), NULL, NULL, "kvm-elcr", 1);

    kpc->parent_realize(dev, errp);
}

qemu_irq *kvm_i8259_init(ISABus *bus)
{
    qemu_irq *irq_set;
    DeviceState *dev;
    ISADevice *isadev;

    isadev = i8259_init_chip(TYPE_KVM_I8259, bus, true);
    dev = DEVICE(isadev);
    irq_set = qemu_allocate_irqs(kvm_pic_set_irq, dev, 8);

    isadev = i8259_init_chip(TYPE_KVM_I8259, bus, false);
    dev = DEVICE(isadev);
    irq_set = qemu_extend_irqs(irq_set, 8, kvm_pic_set_irq, dev, 8);

    return irq_set;
}

static void kvm_i8259_class_init(ObjectClass *klass, void *data)
{
    KVMI8259Class *kpc = KVM_I8259_CLASS(klass);
    I8259CommonClass *k = I8259_COMMON_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset     = kvm_pic_reset;
    device_class_set_parent_realize(dc, kvm_pic_realize, &kpc->parent_realize);
    k->pre_save   = kvm_pic_get;
    k->post_load  = kvm_pic_put;
}

static const TypeInfo kvm_i8259_info = {
    .name = TYPE_KVM_I8259,
    .parent = TYPE_I8259_COMMON,
    .instance_size = sizeof(I8259CommonState),
    .class_init = kvm_i8259_class_init,
    .class_size = sizeof(KVMI8259Class),
};

static void kvm_pic_register_types(void)
{
    type_register_static(&kvm_i8259_info);
}

type_init(kvm_pic_register_types)
