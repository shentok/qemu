/*
 * QEMU ICH4 i82801b11 dmi-to-pci Bridge Emulation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_PCI_BRIDGE_ICH9_DMI_H
#define HW_PCI_BRIDGE_ICH9_DMI_H

#include "qom/object.h"
#include "hw/pci/pci_bridge.h"

#define TYPE_ICH_DMI_PCI_BRIDGE "i82801b11-bridge"
OBJECT_DECLARE_SIMPLE_TYPE(I82801b11Bridge, ICH_DMI_PCI_BRIDGE)

struct I82801b11Bridge {
    PCIBridge parent_obj;
};

#endif
