/*
 * QEMU Intel ICH9 south bridge emulation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_SOUTHBRIDGE_ICH9_H
#define HW_SOUTHBRIDGE_ICH9_H

#include "qom/object.h"

#define TYPE_ICH9_SOUTHBRIDGE "ICH9-southbridge"
OBJECT_DECLARE_SIMPLE_TYPE(ICH9State, ICH9_SOUTHBRIDGE)

/* D28:F[0-5] */
#define ICH9_PCIE_DEV                           28
#define ICH9_PCIE_FUNC_MAX                      6

#define ICH9_GPIO_GSI "gsi"

#define ICH9_LPC_SMI_NEGOTIATED_FEAT_PROP "x-smi-negotiated-features"

/* bit positions used in fw_cfg SMI feature negotiation */
#define ICH9_LPC_SMI_F_BROADCAST_BIT            0
#define ICH9_LPC_SMI_F_CPU_HOTPLUG_BIT          1
#define ICH9_LPC_SMI_F_CPU_HOT_UNPLUG_BIT       2

#endif /* HW_SOUTHBRIDGE_ICH9_H */
