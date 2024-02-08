/*
 * QEMU Intel ICH9 south bridge emulation
 *
 * SPDX-FileCopyrightText: 2024 Linaro Ltd
 * SPDX-FileContributor: Philippe Mathieu-Daudé
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/southbridge/ich9.h"
#include "hw/pci/pci.h"

struct ICH9State {
    DeviceState parent_obj;

    PCIBus *pci_bus;
};

static Property ich9_props[] = {
    DEFINE_PROP_LINK("mch-pcie-bus", ICH9State, pci_bus,
                     TYPE_PCIE_BUS, PCIBus *),
    DEFINE_PROP_END_OF_LIST(),
};

static void ich9_init(Object *obj)
{
}

static void ich9_realize(DeviceState *dev, Error **errp)
{
    ICH9State *s = ICH9_SOUTHBRIDGE(dev);

    if (!s->pci_bus) {
        error_setg(errp, "'pcie-bus' property must be set");
        return;
    }
}

static void ich9_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = ich9_realize;
    device_class_set_props(dc, ich9_props);
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
}

static const TypeInfo ich9_types[] = {
    {
        .name           = TYPE_ICH9_SOUTHBRIDGE,
        .parent         = TYPE_DEVICE,
        .instance_size  = sizeof(ICH9State),
        .instance_init  = ich9_init,
        .class_init     = ich9_class_init,
    }
};

DEFINE_TYPES(ich9_types)
