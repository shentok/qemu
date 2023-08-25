/*
 * QEMU 8259 - common bits of emulated and KVM kernel model
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 * Copyright (c) 2011      Jan Kiszka, Siemens AG
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/intc/i8259.h"
#include "hw/isa/i8259_internal.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "monitor/monitor.h"
#include "qapi/error.h"

void i8259_reset_common(I8259CommonState *s)
{
    s->last_irr = 0;
    s->irr &= s->elcr;
    s->imr = 0;
    s->isr = 0;
    s->priority_add = 0;
    s->irq_base = 0;
    s->read_reg_select = 0;
    s->poll = 0;
    s->special_mask = 0;
    s->init_state = 0;
    s->auto_eoi = 0;
    s->rotate_on_auto_eoi = 0;
    s->special_fully_nested_mode = 0;
    s->init4 = 0;
    s->single_mode = 0;
    /* Note: ELCR and LTIM are not reset */
}

static int pic_dispatch_pre_save(void *opaque)
{
    I8259CommonState *s = opaque;
    I8259CommonClass *info = I8259_COMMON_GET_CLASS(s);

    if (info->pre_save) {
        info->pre_save(s);
    }

    return 0;
}

static int pic_dispatch_post_load(void *opaque, int version_id)
{
    I8259CommonState *s = opaque;
    I8259CommonClass *info = I8259_COMMON_GET_CLASS(s);

    if (info->post_load) {
        info->post_load(s);
    }
    return 0;
}

static void pic_common_realize(DeviceState *dev, Error **errp)
{
    I8259CommonState *s = I8259_COMMON(dev);
    ISADevice *isa = ISA_DEVICE(dev);

    isa_register_ioport(isa, &s->base_io, s->iobase);
    if (s->elcr_addr != -1) {
        isa_register_ioport(isa, &s->elcr_io, s->elcr_addr);
    }

    qdev_set_legacy_instance_id(dev, s->iobase, 1);
}

ISADevice *i8259_init_chip(const char *name, ISABus *bus, bool master)
{
    DeviceState *dev;
    ISADevice *isadev;

    isadev = isa_new(name);
    dev = DEVICE(isadev);
    qdev_prop_set_uint32(dev, "iobase", master ? 0x20 : 0xa0);
    qdev_prop_set_uint32(dev, "elcr_addr", master ? 0x4d0 : 0x4d1);
    qdev_prop_set_uint8(dev, "elcr_mask", master ? 0xf8 : 0xde);
    qdev_prop_set_bit(dev, "master", master);
    isa_realize_and_unref(isadev, bus, &error_fatal);

    return isadev;
}

void i8259_stat_update_irq(I8259CommonState* s, int irq, int level)
{
    if (level != s->irq_level[irq]) {
        s->irq_level[irq] = level;
        if (level == 1) {
            s->irq_count[irq]++;
        }
    }
}

static bool pic_get_statistics(InterruptStatsProvider *obj,
                               uint64_t **irq_counts, unsigned int *nb_irqs)
{
    I8259CommonState *s = I8259_COMMON(obj);

    *irq_counts = s->irq_count;
    *nb_irqs = ARRAY_SIZE(s->irq_count);

    return true;
}

static void pic_print_info(InterruptStatsProvider *obj, Monitor *mon)
{
    I8259CommonState *s = I8259_COMMON(obj);

    pic_dispatch_pre_save(s);
    monitor_printf(mon, "pic%d: irr=%02x imr=%02x isr=%02x hprio=%d "
                   "irq_base=%02x rr_sel=%d elcr=%02x fnm=%d\n",
                   s->master ? 0 : 1, s->irr, s->imr, s->isr, s->priority_add,
                   s->irq_base, s->read_reg_select, s->elcr,
                   s->special_fully_nested_mode);
}

static bool ltim_state_needed(void *opaque)
{
    I8259CommonState *s = I8259_COMMON(opaque);

    return !!s->ltim;
}

static const VMStateDescription vmstate_pic_ltim = {
    .name = "i8259/ltim",
    .version_id = 1,
    .minimum_version_id = 1,
    .needed = ltim_state_needed,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(ltim, I8259CommonState),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_pic_common = {
    .name = "i8259",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_save = pic_dispatch_pre_save,
    .post_load = pic_dispatch_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(last_irr, I8259CommonState),
        VMSTATE_UINT8(irr, I8259CommonState),
        VMSTATE_UINT8(imr, I8259CommonState),
        VMSTATE_UINT8(isr, I8259CommonState),
        VMSTATE_UINT8(priority_add, I8259CommonState),
        VMSTATE_UINT8(irq_base, I8259CommonState),
        VMSTATE_UINT8(read_reg_select, I8259CommonState),
        VMSTATE_UINT8(poll, I8259CommonState),
        VMSTATE_UINT8(special_mask, I8259CommonState),
        VMSTATE_UINT8(init_state, I8259CommonState),
        VMSTATE_UINT8(auto_eoi, I8259CommonState),
        VMSTATE_UINT8(rotate_on_auto_eoi, I8259CommonState),
        VMSTATE_UINT8(special_fully_nested_mode, I8259CommonState),
        VMSTATE_UINT8(init4, I8259CommonState),
        VMSTATE_UINT8(single_mode, I8259CommonState),
        VMSTATE_UINT8(elcr, I8259CommonState),
        VMSTATE_END_OF_LIST()
    },
    .subsections = (const VMStateDescription*[]) {
        &vmstate_pic_ltim,
        NULL
    }
};

static Property pic_properties_common[] = {
    DEFINE_PROP_UINT32("iobase", I8259CommonState, iobase,  -1),
    DEFINE_PROP_UINT32("elcr_addr", I8259CommonState, elcr_addr,  -1),
    DEFINE_PROP_UINT8("elcr_mask", I8259CommonState, elcr_mask,  -1),
    DEFINE_PROP_BIT("master", I8259CommonState, master,  0, false),
    DEFINE_PROP_END_OF_LIST(),
};

static void pic_common_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    InterruptStatsProviderClass *ic = INTERRUPT_STATS_PROVIDER_CLASS(klass);

    dc->vmsd = &vmstate_pic_common;
    device_class_set_props(dc, pic_properties_common);
    dc->realize = pic_common_realize;
    /*
     * Reason: unlike ordinary ISA devices, the PICs need additional
     * wiring: its IRQ input lines are set up by board code, and the
     * wiring of the slave to the master is hard-coded in device model
     * code.
     */
    dc->user_creatable = false;
    ic->get_statistics = pic_get_statistics;
    ic->print_info = pic_print_info;
}

static const TypeInfo pic_common_type = {
    .name = TYPE_I8259_COMMON,
    .parent = TYPE_ISA_DEVICE,
    .instance_size = sizeof(I8259CommonState),
    .class_size = sizeof(I8259CommonClass),
    .class_init = pic_common_class_init,
    .abstract = true,
    .interfaces = (InterfaceInfo[]) {
        { TYPE_INTERRUPT_STATS_PROVIDER },
        { }
    },
};

static void pic_common_register_types(void)
{
    type_register_static(&pic_common_type);
}

type_init(pic_common_register_types)
