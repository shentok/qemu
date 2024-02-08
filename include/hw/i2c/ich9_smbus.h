/*
 * QEMU ICH9 SMBus emulation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef HW_I2C_ICH9_SMBUS_H
#define HW_I2C_ICH9_SMBUS_H

#include "qom/object.h"
#include "hw/pci/pci_device.h"
#include "hw/i2c/pm_smbus.h"

#define TYPE_ICH9_SMB_DEVICE "ICH9-SMB"

OBJECT_DECLARE_SIMPLE_TYPE(ICH9SMBState, ICH9_SMB_DEVICE)

struct ICH9SMBState {
    PCIDevice dev;

    bool irq_enabled;

    PMSMBus smb;
};

#endif
