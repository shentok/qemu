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
#include "hw/pci-bridge/ich9_dmi.h"
#include "hw/ide/ahci-pci.h"
#include "hw/ide/ide-dev.h"
#include "hw/i2c/ich9_smbus.h"

#define ICH9_D2P_DEVFN          PCI_DEVFN(30, 0)
#define ICH9_SATA1_DEVFN        PCI_DEVFN(31, 2)
#define ICH9_SMB_DEVFN          PCI_DEVFN(31, 3)

#define SATA_PORTS              6

struct ICH9State {
    DeviceState parent_obj;

    I82801b11Bridge d2p;
    AHCIPCIState sata0;
    ICH9SMBState smb;

    PCIBus *pci_bus;
    bool d2p_enabled;
    bool sata_enabled;
    bool smbus_enabled;
};

static Property ich9_props[] = {
    DEFINE_PROP_LINK("mch-pcie-bus", ICH9State, pci_bus,
                     TYPE_PCIE_BUS, PCIBus *),
    DEFINE_PROP_BOOL("d2p-enabled", ICH9State, d2p_enabled, true),
    DEFINE_PROP_BOOL("sata-enabled", ICH9State, sata_enabled, true),
    DEFINE_PROP_BOOL("smbus-enabled", ICH9State, smbus_enabled, true),
    DEFINE_PROP_END_OF_LIST(),
};

static void ich9_init(Object *obj)
{
}

static bool ich9_realize_d2p(ICH9State *s, Error **errp)
{
    if (!module_object_class_by_name(TYPE_ICH_DMI_PCI_BRIDGE)) {
        error_setg(errp, "DMI-to-PCI function not available in this build");
        return false;
    }
    object_initialize_child(OBJECT(s), "d2p", &s->d2p, TYPE_ICH_DMI_PCI_BRIDGE);
    qdev_prop_set_int32(DEVICE(&s->d2p), "addr", ICH9_D2P_DEVFN);
    if (!qdev_realize(DEVICE(&s->d2p), BUS(s->pci_bus), errp)) {
        return false;
    }
    object_property_add_alias(OBJECT(s), "pci.0", OBJECT(&s->d2p), "pci.0");

    return true;
}

static bool ich9_realize_sata(ICH9State *s, Error **errp)
{
    DriveInfo *hd[SATA_PORTS];

    object_initialize_child(OBJECT(s), "sata[0]", &s->sata0, TYPE_ICH9_AHCI);
    qdev_prop_set_int32(DEVICE(&s->sata0), "addr", ICH9_SATA1_DEVFN);
    if (!qdev_realize(DEVICE(&s->sata0), BUS(s->pci_bus), errp)) {
        return false;
    }
    for (unsigned i = 0; i < SATA_PORTS; i++) {
        g_autofree char *portname = g_strdup_printf("ide.%u", i);

        object_property_add_alias(OBJECT(s), portname,
                                  OBJECT(&s->sata0), portname);
    }

    g_assert(SATA_PORTS == s->sata0.ahci.ports);
    ide_drive_get(hd, s->sata0.ahci.ports);
    ahci_ide_create_devs(&s->sata0.ahci, hd);

    return true;
}

static bool ich9_realize_smbus(ICH9State *s, Error **errp)
{
    object_initialize_child(OBJECT(s), "smb", &s->smb, TYPE_ICH9_SMB_DEVICE);
    qdev_prop_set_int32(DEVICE(&s->smb), "addr", ICH9_SMB_DEVFN);
    if (!qdev_realize(DEVICE(&s->smb), BUS(s->pci_bus), errp)) {
        return false;
    }
    object_property_add_alias(OBJECT(s), "i2c", OBJECT(&s->smb), "i2c");

    return true;
}

static void ich9_realize(DeviceState *dev, Error **errp)
{
    ICH9State *s = ICH9_SOUTHBRIDGE(dev);

    if (!s->pci_bus) {
        error_setg(errp, "'pcie-bus' property must be set");
        return;
    }

    if (s->d2p_enabled && !ich9_realize_d2p(s, errp)) {
        return;
    }

    if (s->sata_enabled && !ich9_realize_sata(s, errp)) {
        return;
    }

    if (s->smbus_enabled && !ich9_realize_smbus(s, errp)) {
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