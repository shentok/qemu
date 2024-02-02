/*
 * SMSC FDC37M81X Super I/O
 *
 * Copyright (c) 2018 Philippe Mathieu-Daudé
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/isa/fdc37m81x-superio.h"
#include "hw/block/fdc.h"
#include "hw/char/parallel-isa.h"
#include "hw/char/serial-isa.h"
#include "hw/resettable.h"
#include "trace.h"

static void fdc37m81x_realize(DeviceState *dev, Error **errp)
{
    FDC37M81XState *s = FDC37M81X(dev);
    ISADevice *isa = ISA_DEVICE(s);
    ISASuperIOClass *ic = ISA_SUPERIO_GET_CLASS(s);

    isa_register_ioport(isa, &s->config_io, 0x3f0);

    isa_register_ioport(isa, &s->index_data_io, 0x3f0);
    memory_region_set_enabled(&s->index_data_io, false);

    ic->parent_realize(dev, errp);
}

static void fdc37m81x_reset_hold(Object *obj, ResetType type)
{
    FDC37M81XState *s = FDC37M81X(obj);
    ISASuperIODevice *sio = ISA_SUPERIO(s);

    memory_region_set_enabled(&s->index_data_io, false);
    memory_region_set_enabled(&s->config_io, true);

    isa_fdc_set_enabled(sio->floppy, false);
}

static void fdc37m81x_index_data_io_write(void *opaque, hwaddr addr,
                                          uint64_t val, unsigned int size)
{
    FDC37M81XState *s = opaque;
    ISASuperIODevice *sio = ISA_SUPERIO(s);

    if (addr == 0) {
        /* index register */

        trace_fdc37m81x_index_io_write(val);

        if (val == 0xaa) {
            ISASuperIOClass *ic = ISA_SUPERIO_GET_CLASS(s);

            memory_region_set_enabled(&s->index_data_io, false);
            memory_region_set_enabled(&s->config_io, true);
            isa_parallel_set_enabled(sio->parallel[0], s->enabled.parallel);
            for (size_t i = 0; i < ic->serial.count; i++) {
                isa_serial_set_enabled(sio->serial[i], s->enabled.serial[i]);
            }
            isa_fdc_set_enabled(sio->floppy, s->enabled.floppy);
        } else {
            s->selected_index = val;
        }

        return;
    } else {
        trace_fdc37m81x_data_io_write(val);
    }

    trace_fdc37m81x_index_data_io_write(s->logical_device_number,
                                        s->selected_index, val);

    switch (s->selected_index) {
    case 0x7:
        s->logical_device_number = val;
        break;
    case 0x30:
        switch (s->logical_device_number) {
        case 0:
            s->enabled.floppy = val & 1;
            break;
        case 3:
            s->enabled.parallel = val & 1;
            break;
        case 4 ... 5:
            s->enabled.serial[s->logical_device_number - 4] = val & 1;
            break;
        }
        break;
    case 0x60:
        switch (s->logical_device_number) {
        case 0:
            isa_fdc_set_iobase(sio->floppy,
                               (sio->floppy->ioport_id & 0xff) | val << 8);
            break;
        case 3:
            isa_parallel_set_iobase(sio->parallel[0],
                               (sio->parallel[0]->ioport_id & 0xff) | val << 8);
            break;
        case 4:
            isa_serial_set_iobase(sio->serial[0],
                                (sio->serial[0]->ioport_id & 0xff) | val << 8);
            break;
        case 5:
            isa_serial_set_iobase(sio->serial[1],
                                (sio->serial[1]->ioport_id & 0xff) | val << 8);
            break;
        }
        break;
    case 0x61:
        switch (s->logical_device_number) {
        case 0:
            isa_fdc_set_iobase(sio->floppy,
                               (sio->floppy->ioport_id & 0xff00) | val);
            break;
        case 3:
            isa_parallel_set_iobase(sio->parallel[0],
                                (sio->parallel[0]->ioport_id & 0xff00) | val);
            break;
        case 4:
            isa_serial_set_iobase(sio->serial[0],
                                  (sio->serial[0]->ioport_id & 0xff00) | val);
            break;
        case 5:
            isa_serial_set_iobase(sio->serial[1],
                                  (sio->serial[1]->ioport_id & 0xff00) | val);
            break;
        }
        break;
    }
}

static uint64_t fdc37m81x_index_data_io_read(void *opaque, hwaddr addr,
                                             unsigned int size)
{
    uint32_t val = 0;

#if 0
    FDC37M81XState *s = opaque;
    if (s->selected_index < 3) {
        val = s->regs[s->selected_index];
    }
#endif

    trace_fdc37m81x_data_io_read(addr, val);
    return val;
}

static const MemoryRegionOps fdc37m81x_data_io_ops = {
    .read  = fdc37m81x_index_data_io_read,
    .write = fdc37m81x_index_data_io_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void fdc37m81x_config_io_write(void *opaque, hwaddr addr, uint64_t val,
                             unsigned int size)
{
    FDC37M81XState *s = opaque;
    ISASuperIODevice *sio = ISA_SUPERIO(s);

    trace_fdc37m81x_config_io_write(addr, val);

    if (val == 0x55) {
        isa_fdc_set_enabled(sio->floppy, false);
        memory_region_set_enabled(&s->config_io, false);
        memory_region_set_enabled(&s->index_data_io, true);
    }
}

static const MemoryRegionOps fdc37m81x_config_index_io_ops = {
    .write = fdc37m81x_config_io_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void fdc37m81x_init(Object *obj)
{
    FDC37M81XState *s = FDC37M81X(obj);

    memory_region_init_io(&s->config_io, obj, &fdc37m81x_config_index_io_ops, s,
                          TYPE_FDC37M81X "_config_index_io", 1);

    memory_region_init_io(&s->index_data_io, obj, &fdc37m81x_data_io_ops, s,
                          TYPE_FDC37M81X "_data_io", 2);
}

static void fdc37m81x_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);
    ISASuperIOClass *sc = ISA_SUPERIO_CLASS(klass);

    sc->parent_realize = dc->realize;

    dc->realize = fdc37m81x_realize;
    rc->phases.hold = fdc37m81x_reset_hold;

    sc->serial.count = 2; /* NS16C550A */
    sc->parallel.count = 1;
    sc->floppy.count = 1; /* SMSC 82077AA Compatible */
    sc->ide.count = 0;
}

static const TypeInfo types[] = {
    {
        .name          = TYPE_FDC37M81X,
        .parent        = TYPE_ISA_SUPERIO,
        .instance_size = sizeof(FDC37M81XState),
        .instance_init = fdc37m81x_init,
        .class_init    = fdc37m81x_class_init,
    },
};

DEFINE_TYPES(types)
