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

#include "qemu/osdep.h"
#include "hw/usb/hcd-ehci-fsl.h"

#define PHY_CLK_VALID BIT(17)

static uint64_t fsl_ehci_read(void *opaque, hwaddr addr, unsigned size)
{
    switch (addr) {
    case 0x100:
        return PHY_CLK_VALID;
    }

    return 0;
}

static void fsl_ehci_write(void *opaque, hwaddr addr, uint64_t val,
                                unsigned size)
{
}

static const MemoryRegionOps fsl_ehci_mmio_control_ops = {
    .read = fsl_ehci_read,
    .write = fsl_ehci_write,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void fsl_ehci_init(Object *obj)
{
    FSLEHCIState *s = FSL_EHCI(obj);
    EHCISysBusState *i = SYS_BUS_EHCI(s);
    EHCIState *ehci = &i->ehci;

    memory_region_init_io(&s->control, OBJECT(s), &fsl_ehci_mmio_control_ops,
                          ehci, "control", 0x104);
    memory_region_add_subregion(&ehci->mem, 0x400, &s->control);
}

static const TypeInfo ehci_fsl_types[] = {
    {
        .name          = TYPE_FSL_EHCI,
        .parent        = TYPE_CHIPIDEA,
        .instance_init = fsl_ehci_init,
        .instance_size = sizeof(FSLEHCIState),
    },
    {
        .name          = "fsl-usb2-dr",
        .parent        = TYPE_FSL_EHCI,
    },
    {
        .name          = "fsl-usb2-dr-v1.6",
        .parent        = TYPE_FSL_EHCI,
    },
};

DEFINE_TYPES(ehci_fsl_types)
