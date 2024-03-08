/*
 * QEMU Smram/pam logic implementation
 *
 * Copyright (c) 2006 Fabrice Bellard
 * Copyright (c) 2011 Isaku Yamahata <yamahata at valinux co jp>
 *                    VA Linux Systems Japan K.K.
 * Copyright (c) 2012 Jason Baron <jbaron@redhat.com>
 *
 * Split out from piix.c
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/pci-host/pam.h"
#include "trace.h"

static MemTxResult pci_ram_ops_read(void *opaque, hwaddr addr, uint64_t *data,
                                    unsigned size, MemTxAttrs attrs)
{
    PAMMemoryRegion *pam = opaque;
    MemTxResult res = address_space_read(pam->pci_as, pam->pci_mr.alias_offset + addr, attrs, data, size);

    trace_pam_pci_ram_ops_read(pam->pci_mr.alias_offset + addr, *data);

    return res;
}

static MemTxResult pci_ram_ops_write(void *opaque, hwaddr addr, uint64_t data,
                                     unsigned size, MemTxAttrs attrs)
{
    PAMMemoryRegion *pam = opaque;

    trace_pam_pci_ram_ops_write(pam->ram_mr.alias_offset + addr, data);

    return address_space_write(pam->ram_as, pam->ram_mr.alias_offset + addr, attrs, &data, size);
}

static const MemoryRegionOps pci_ram_ops = {
    .read_with_attrs = pci_ram_ops_read,
    .write_with_attrs = pci_ram_ops_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

void init_pam(PAMMemoryRegion *mem, Object *owner, AddressSpace *ram_memory,
              MemoryRegion *system_memory, AddressSpace *pci_address_space,
              uint32_t start, uint32_t size)
{
    int i;

    mem->pci_as = pci_address_space;
    mem->ram_as = ram_memory;

    /* RAM */
    memory_region_init_alias(&mem->alias[3], owner, "pam-ram", ram_memory->root,
                             start, size);
    /* ROM (XXX: not quite correct) */
    memory_region_init_alias(&mem->alias[1], owner, "pam-rom", ram_memory->root,
                             start, size);
    memory_region_set_readonly(&mem->alias[1], true);

    memory_region_init_alias(&mem->alias[0], owner, "pam-pci", pci_address_space->root,
                             start, size);
    memory_region_init_io(&mem->alias[2], owner, &pci_ram_ops, mem, "pam-ram",
                          size);

    memory_region_init_alias(&mem->ram_mr, owner, "pam-ram-as", ram_memory->root, start,
                             size);

    memory_region_init_alias(&mem->pci_mr, owner, "pam-pci-as", pci_address_space->root,
                             start, size);

    memory_region_transaction_begin();
    for (i = 0; i < ARRAY_SIZE(mem->alias); ++i) {
        memory_region_set_enabled(&mem->alias[i], false);
        memory_region_add_subregion_overlap(system_memory, start,
                                            &mem->alias[i], 1);
    }
    memory_region_transaction_commit();
    mem->mode = 0;
}

void pam_update(PAMMemoryRegion *pam, uint8_t mode)
{
    g_assert(mode < ARRAY_SIZE(pam->alias));

    memory_region_set_enabled(&pam->alias[pam->mode], false);
    pam->mode = mode;
    memory_region_set_enabled(&pam->alias[pam->mode], true);
}
