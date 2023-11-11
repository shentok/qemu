/*
 * QEMU VT82C694T North Bridge Emulation
 *
 * Copyright (c) 2006 Fabrice Bellard
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef HW_PCI_VT82C694T_H
#define HW_PCI_VT82C694T_H

#include "hw/pci/pci_device.h"
#include "hw/pci-host/pam.h"
#include "system/memory.h"
#include "qom/object.h"

#define TYPE_VT82C694T_PCI_HOST_BRIDGE "vt82c694t"
#define TYPE_VT82C694T_PCI_DEVICE "vt82c694t-pci"

OBJECT_DECLARE_SIMPLE_TYPE(VT82C694TPCIState, VT82C694T_PCI_DEVICE)

struct VT82C694TPCIState {
    PCIDevice parent_obj;

    PAMMemoryRegion pam_regions[10];
    MemoryRegion smram_region;
    MemoryRegion smram, low_smram;
    MemoryRegion port22;
    uint8_t port22_value;
};

#endif
