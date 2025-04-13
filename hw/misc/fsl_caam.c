/*
 * Freescale Cryptographic Acceleration and Assurance Module Emulation
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/fsl_caam.h"
#include "hw/resettable.h"
#include "migration/vmstate.h"
#include "trace.h"

static uint64_t fsl_caam_read(void *opaque, hwaddr offset, unsigned size)
{
    FslImxCaamState *s = opaque;
    uint32_t value = s->data[offset];

    trace_fsl_caam_read(offset, value);

    return value;
}

static void fsl_caam_write(void *opaque, hwaddr offset,
                           uint64_t value, unsigned size)
{
    FslImxCaamState *s = opaque;

    trace_fsl_caam_write(offset, value);

    s->data[offset] = value;
}

static const MemoryRegionOps fsl_caam_ops = {
    .read = fsl_caam_read,
    .write = fsl_caam_write,
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void fsl_caam_realize(DeviceState *dev, Error **errp)
{
    FslImxCaamState *s = FSL_CAAM(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &fsl_caam_ops, s,
                          TYPE_FSL_CAAM, ARRAY_SIZE(s->data));
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
}

static void fsl_caam_reset_hold(Object *obj, ResetType type)
{
    FslImxCaamState *s = FSL_CAAM(obj);

    memset(s->data, 0, sizeof(s->data));
}

static const VMStateDescription fsl_caam_vmstate = {
    .name = "fsl-caam",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(data, FslImxCaamState, FSL_CAAM_DATA_SIZE),
        VMSTATE_END_OF_LIST()
    }
};

static void fsl_caam_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = fsl_caam_realize;
    dc->vmsd = &fsl_caam_vmstate;
    rc->phases.hold = fsl_caam_reset_hold;
}

static const TypeInfo fsl_caam_types[] = {
    {
        .name = TYPE_FSL_CAAM,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FslImxCaamState),
        .class_init = fsl_caam_class_init,
    }
};

DEFINE_TYPES(fsl_caam_types)
