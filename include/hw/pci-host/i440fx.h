/*
 * QEMU i440FX North Bridge Emulation
 *
 * Copyright (c) 2006 Fabrice Bellard
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef HW_PCI_I440FX_H
#define HW_PCI_I440FX_H

#include "hw/pci/pci_device.h"
#include "hw/pci-host/pam.h"
#include "exec/memory.h"
#include "qom/object.h"

#define I440FX_PAM 0x59
#define I440FX_PAM_SIZE 7
#define I440FX_SMRAM 0x72

#define I440FX_HOST_PROP_RAM_MEM "ram-mem"
#define I440FX_HOST_PROP_SMRAM_MEM "smram-mem"
#define I440FX_HOST_PROP_PCI_MEM "pci-mem"
#define I440FX_HOST_PROP_SYSTEM_MEM "system-mem"
#define I440FX_HOST_PROP_IO_MEM "io-mem"

#define TYPE_I440FX_PCI_HOST_BRIDGE "i440FX-pcihost"
#define TYPE_I440FX_PCI_DEVICE "i440FX"

OBJECT_DECLARE_SIMPLE_TYPE(PCII440FXState, I440FX_PCI_DEVICE)

struct PCII440FXState {
    /*< private >*/
    PCIDevice parent_obj;
    /*< public >*/

    PAMMemoryRegion pam_regions[PAM_REGIONS_COUNT];
    MemoryRegion smram_region;
    MemoryRegion low_smram;
    uint64_t below_4g_mem_size;
    uint64_t above_4g_mem_size;
};

#endif
