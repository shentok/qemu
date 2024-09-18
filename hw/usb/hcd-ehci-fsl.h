/*
 * QEMU USB EHCI Emulation
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_USB_HCD_EHCI_FSL_H
#define HW_USB_HCD_EHCI_FSL_H

#include "hw/usb/chipidea.h"
#include "system/memory.h"


#define TYPE_FSL_EHCI "fsl-ehci-usb"

OBJECT_DECLARE_SIMPLE_TYPE(FSLEHCIState, FSL_EHCI)

struct FSLEHCIState {
    ChipideaState parent_obj;

    MemoryRegion control;
};

#endif
