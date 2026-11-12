/*
 *  GPIO Controller for a lot of Freescale SoCs
 *
 * Copyright (C) 2014 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Alexander Graf, <agraf@suse.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/core/sysbus.h"
#include "migration/vmstate.h"
#include "qom/object.h"
#include "trace.h"

#define TYPE_MPC8XXX_GPIO "mpc8xxx_gpio"
OBJECT_DECLARE_SIMPLE_TYPE(MPC8XXXGPIOState, MPC8XXX_GPIO)

struct MPC8XXXGPIOState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
    qemu_irq out[32];
    char *name;

    uint32_t dir;
    uint32_t odr;
    uint32_t dat;
    uint32_t last_data;
    uint32_t ext;
    uint32_t ier;
    uint32_t imr;
    uint32_t icr;
};

static const VMStateDescription vmstate_mpc8xxx_gpio = {
    .name = "mpc8xxx_gpio",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32(dir, MPC8XXXGPIOState),
        VMSTATE_UINT32(odr, MPC8XXXGPIOState),
        VMSTATE_UINT32(dat, MPC8XXXGPIOState),
        VMSTATE_UINT32(last_data, MPC8XXXGPIOState),
        VMSTATE_UINT32(ext, MPC8XXXGPIOState),
        VMSTATE_UINT32(ier, MPC8XXXGPIOState),
        VMSTATE_UINT32(imr, MPC8XXXGPIOState),
        VMSTATE_UINT32(icr, MPC8XXXGPIOState),
        VMSTATE_END_OF_LIST()
    }
};

static void mpc8xxx_gpio_update(MPC8XXXGPIOState *s)
{
    qemu_set_irq(s->irq, !!(s->ier & s->imr));
}

static uint64_t mpc8xxx_gpio_read(void *opaque, hwaddr offset,
                                  unsigned size)
{
    MPC8XXXGPIOState *s = (MPC8XXXGPIOState *)opaque;

    if (size != 4) {
        /* All registers are 32bit */
        return 0;
    }

    switch (offset) {
    case 0x0: /* Direction */
        return s->dir;
    case 0x4: /* Open Drain */
        return s->odr;
    case 0x8: /* Data */
        return s->dat;
    case 0xC: /* Interrupt Event */
        return s->ier;
    case 0x10: /* Interrupt Mask */
        return s->imr;
    case 0x14: /* Interrupt Control */
        return s->icr;
    default:
        return 0;
    }
}

static void mpc8xxx_write_data(MPC8XXXGPIOState *s, uint32_t new_data)
{
    uint32_t old_out = s->dat & s->dir;
    uint32_t new_out = new_data & s->dir;
    uint32_t changed = old_out ^ new_out;
    int i;

    s->last_data = new_data;

    for (i = 0; i < 32; i++) {
        uint32_t mask = 0x80000000 >> i;

        if (!(changed & mask)) {
            continue;
        }

        trace_mpc8xxx_gpio_output(s->name, i, (new_out & mask) != 0);
        qemu_set_irq(s->out[i], (new_out & mask) != 0);
    }

    /* Output pins follow the latch; input pins track the external line. */
    s->dat = (s->dat & ~s->dir) | new_out;
}

static void mpc8xxx_gpio_set_dir(MPC8XXXGPIOState *s, uint32_t new_dir)
{
    uint32_t changed = s->dir ^ new_dir;
    int i;

    for (i = 0; i < 32; i++) {
        uint32_t mask = 0x80000000 >> i;

        if (!(changed & mask)) {
            continue;
        }

        if (new_dir & mask) {
            /* Pin becomes an output: drive the latched data value. */
            bool level = (s->last_data & mask) != 0;

            s->dat = (s->dat & ~mask) | (level ? mask : 0);
            trace_mpc8xxx_gpio_output(s->name, i, level);
            qemu_set_irq(s->out[i], level);
        } else {
            /* Pin becomes an input: release it and sample the external line. */
            bool level = (s->ext & mask) != 0;

            s->dat = (s->dat & ~mask) | (level ? mask : 0);
            trace_mpc8xxx_gpio_output(s->name, i, 1);
            qemu_set_irq(s->out[i], 1);
        }
    }

    s->dir = new_dir;
}

static void mpc8xxx_gpio_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
    MPC8XXXGPIOState *s = (MPC8XXXGPIOState *)opaque;

    trace_mpc8xxx_gpio_write(s->name, offset, value);

    if (size != 4) {
        /* All registers are 32bit */
        return;
    }

    switch (offset) {
    case 0x0: /* Direction */
        mpc8xxx_gpio_set_dir(s, value);
        break;
    case 0x4: /* Open Drain */
        s->odr = value;
        break;
    case 0x8: /* Data */
        mpc8xxx_write_data(s, value);
        break;
    case 0xC: /* Interrupt Event */
        s->ier &= ~value;
        break;
    case 0x10: /* Interrupt Mask */
        s->imr = value;
        break;
    case 0x14: /* Interrupt Control */
        s->icr = value;
        break;
    }

    mpc8xxx_gpio_update(s);
}

static void mpc8xxx_gpio_reset(DeviceState *dev)
{
    MPC8XXXGPIOState *s = MPC8XXX_GPIO(dev);
    int i;

    s->dir = 0;
    s->odr = 0;
    s->dat = 0xffffffff;
    s->last_data = 0;
    s->ext = 0xffffffff;
    s->ier = 0;
    s->imr = 0;
    s->icr = 0;

    /* All pins are inputs after reset; model the pull-ups as high. */
    for (i = 0; i < 32; i++) {
        qemu_set_irq(s->out[i], 1);
    }
}

static void mpc8xxx_gpio_set_irq(void * opaque, int irq, int level)
{
    MPC8XXXGPIOState *s = (MPC8XXXGPIOState *)opaque;
    uint32_t mask;

    trace_mpc8xxx_gpio_set_irq(s->name, irq, level);
    mask = 0x80000000 >> irq;
    s->ext = (s->ext & ~mask) | (level ? mask : 0);
    if ((s->dir & mask) == 0) {
        uint32_t old_value = s->dat & mask;

        s->dat = (s->dat & ~mask) | (level ? mask : 0);

        if (!(s->icr & irq) || (old_value && !level)) {
            s->ier |= mask;
        }

        mpc8xxx_gpio_update(s);
    }
}

static const MemoryRegionOps mpc8xxx_gpio_ops = {
    .read = mpc8xxx_gpio_read,
    .write = mpc8xxx_gpio_write,
    .endianness = DEVICE_BIG_ENDIAN,
};

static void mpc8xxx_gpio_initfn(Object *obj)
{
    DeviceState *dev = DEVICE(obj);
    MPC8XXXGPIOState *s = MPC8XXX_GPIO(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &mpc8xxx_gpio_ops,
                          s, "mpc8xxx_gpio", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);
    qdev_init_gpio_in(dev, mpc8xxx_gpio_set_irq, 32);
    qdev_init_gpio_out(dev, s->out, 32);
}

static void mpc8xxx_gpio_realize(DeviceState *dev, Error **errp)
{
    MPC8XXXGPIOState *s = MPC8XXX_GPIO(dev);

    s->name = object_get_canonical_path(OBJECT(dev));
}

static void mpc8xxx_gpio_finalize(Object *obj)
{
    MPC8XXXGPIOState *s = MPC8XXX_GPIO(obj);

    g_free(s->name);
}

static void mpc8xxx_gpio_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_mpc8xxx_gpio;
    dc->realize = mpc8xxx_gpio_realize;
    device_class_set_legacy_reset(dc, mpc8xxx_gpio_reset);
}

static const TypeInfo mpc8xxx_gpio_types[] = {
    {
        .name          = TYPE_MPC8XXX_GPIO,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(MPC8XXXGPIOState),
        .instance_init = mpc8xxx_gpio_initfn,
        .instance_finalize = mpc8xxx_gpio_finalize,
        .class_init    = mpc8xxx_gpio_class_init,
    },
    {
        .name          = "fsl,mpc8572-gpio",
        .parent        = TYPE_MPC8XXX_GPIO,
    },
    {
        .name          = "fsl,qoriq-gpio",
        .parent        = TYPE_MPC8XXX_GPIO,
    },
    {
        .name          = "fsl,pq3-gpio",
        .parent        = TYPE_MPC8XXX_GPIO,
    },
};

DEFINE_TYPES(mpc8xxx_gpio_types)
