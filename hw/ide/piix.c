/*
 * QEMU IDE Emulation: PCI PIIX3/4 support.
 *
 * Copyright (c) 2003 Fabrice Bellard
 * Copyright (c) 2006 Openedhand Ltd.
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
#include "hw/pci/pci.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "sysemu/block-backend.h"
#include "sysemu/blockdev.h"
#include "sysemu/dma.h"

#include "hw/ide/pci.h"
#include "trace.h"

#define TYPE_PIIX_IDE "piix-ide"
OBJECT_DECLARE_SIMPLE_TYPE(PIIXIDEState, PIIX_IDE)

struct PIIXIDEState {
    PCIIDEState i;
};

static uint64_t bmdma_read(void *opaque, hwaddr addr, unsigned size)
{
    BMDMAState *bm = opaque;
    uint32_t val;

    if (size != 1) {
        return ((uint64_t)1 << (size * 8)) - 1;
    }

    switch(addr & 3) {
    case 0:
        val = bm->cmd;
        break;
    case 2:
        val = bm->status;
        break;
    default:
        val = 0xff;
        break;
    }

    trace_bmdma_read(addr, val);
    return val;
}

static void bmdma_write(void *opaque, hwaddr addr,
                        uint64_t val, unsigned size)
{
    BMDMAState *bm = opaque;

    if (size != 1) {
        return;
    }

    trace_bmdma_write(addr, val);

    switch(addr & 3) {
    case 0:
        bmdma_cmd_writeb(bm, val);
        break;
    case 2:
        bm->status = (val & 0x60) | (bm->status & 1) | (bm->status & ~val & 0x06);
        break;
    }
}

static const MemoryRegionOps piix_bmdma_ops = {
    .read = bmdma_read,
    .write = bmdma_write,
};

static void bmdma_setup_bar(PCIIDEState *d)
{
    int i;

    memory_region_init(&d->bmdma_bar, OBJECT(d), "piix-bmdma-container", 16);
    for(i = 0;i < 2; i++) {
        BMDMAState *bm = &d->bmdma[i];

        memory_region_init_io(&bm->extra_io, OBJECT(d), &piix_bmdma_ops, bm,
                              "piix-bmdma", 4);
        memory_region_add_subregion(&d->bmdma_bar, i * 8, &bm->extra_io);
        memory_region_init_io(&bm->addr_ioport, OBJECT(d),
                              &bmdma_addr_ioport_ops, bm, "bmdma", 4);
        memory_region_add_subregion(&d->bmdma_bar, i * 8 + 4, &bm->addr_ioport);
    }
}

static void piix_ide_reset(DeviceState *dev)
{
    PCIIDEState *d = PCI_IDE(dev);
    PCIDevice *pd = PCI_DEVICE(d);
    uint8_t *pci_conf = pd->config;
    int i;

    for (i = 0; i < 2; i++) {
        ide_bus_reset(&d->bus[i]);
    }

    /* TODO: this is the default. do not override. */
    pci_conf[PCI_COMMAND] = 0x00;
    /* TODO: this is the default. do not override. */
    pci_conf[PCI_COMMAND + 1] = 0x00;
    /* TODO: use pci_set_word */
    pci_conf[PCI_STATUS] = PCI_STATUS_FAST_BACK;
    pci_conf[PCI_STATUS + 1] = PCI_STATUS_DEVSEL_MEDIUM >> 8;
    pci_conf[0x20] = 0x01; /* BMIBA: 20-23h */
}

static void pci_piix_init_ports(PCIIDEState *d)
{
    DeviceState *dev = DEVICE(d);
    int i;

    for (i = 0; i < 2; i++) {
        ide_bus_init(&d->bus[i], sizeof(d->bus[i]), dev, i, 2);
        ide_init2(&d->bus[i], NULL);
        ide_register_restart_cb(&d->bus[i]);

        bmdma_init(&d->bus[i], &d->bmdma[i], d);
        qdev_init_gpio_out(dev, &d->bmdma[i].irq, 1);
        d->bmdma[i].bus = &d->bus[i];
    }
}

static void pci_piix_ide_realize(PCIDevice *dev, Error **errp)
{
    PCIIDEState *d = PCI_IDE(dev);
    uint8_t *pci_conf = dev->config;

    pci_conf[PCI_CLASS_PROG] = 0x80; // legacy ATA mode

    bmdma_setup_bar(d);
    pci_register_bar(dev, 4, PCI_BASE_ADDRESS_SPACE_IO, &d->bmdma_bar);

    vmstate_register(VMSTATE_IF(dev), 0, &vmstate_ide_pci, d);

    pci_piix_init_ports(d);
}

int pci_piix3_xen_ide_unplug(DeviceState *dev, bool aux)
{
    PCIIDEState *pci_ide;
    int i;
    IDEDevice *idedev;
    IDEBus *idebus;
    BlockBackend *blk;

    pci_ide = PCI_IDE(dev);

    for (i = aux ? 1 : 0; i < 4; i++) {
        idebus = &pci_ide->bus[i / 2];
        blk = idebus->ifs[i % 2].blk;

        if (blk && idebus->ifs[i % 2].drive_kind != IDE_CD) {
            if (!(i % 2)) {
                idedev = idebus->master;
            } else {
                idedev = idebus->slave;
            }

            blk_drain(blk);
            blk_flush(blk);

            blk_detach_dev(blk, DEVICE(idedev));
            idebus->ifs[i % 2].blk = NULL;
            idedev->conf.blk = NULL;
            monitor_remove_blk(blk);
            blk_unref(blk);
        }
    }
    qdev_reset_all(dev);
    return 0;
}

static void pci_piix_ide_exitfn(PCIDevice *dev)
{
    PCIIDEState *d = PCI_IDE(dev);
    unsigned i;

    for (i = 0; i < 2; ++i) {
        memory_region_del_subregion(&d->bmdma_bar, &d->bmdma[i].extra_io);
        memory_region_del_subregion(&d->bmdma_bar, &d->bmdma[i].addr_ioport);
    }
}

static void piix_ide_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *pd = PCI_DEVICE_CLASS(klass);

    pd->realize = pci_piix_ide_realize;
    pd->exit = pci_piix_ide_exitfn;
    pd->vendor_id = PCI_VENDOR_ID_INTEL;
    pd->class_id = PCI_CLASS_STORAGE_IDE;
    dc->reset = piix_ide_reset;
    set_bit(DEVICE_CATEGORY_STORAGE, dc->categories);
    dc->hotpluggable = false;
}

static const TypeInfo piix_ide_info = {
    .name = TYPE_PIIX_IDE,
    .parent = TYPE_PCI_IDE,
    .instance_size = sizeof(PIIXIDEState),
    .class_init = piix_ide_class_init,
    .abstract = true,
};

/* NOTE: for the PIIX3, the IRQs and IOports are hardcoded */
static void piix3_ide_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->device_id = PCI_DEVICE_ID_INTEL_82371SB_1;
}

static const TypeInfo piix3_ide_info = {
    .name          = "piix3-ide",
    .parent        = TYPE_PIIX_IDE,
    .class_init    = piix3_ide_class_init,
};

static const TypeInfo piix3_ide_xen_info = {
    .name          = "piix3-ide-xen",
    .parent        = TYPE_PIIX_IDE,
    .class_init    = piix3_ide_class_init,
};

/* NOTE: for the PIIX4, the IRQs and IOports are hardcoded */
static void piix4_ide_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->device_id = PCI_DEVICE_ID_INTEL_82371AB;
}

static const TypeInfo piix4_ide_info = {
    .name          = "piix4-ide",
    .parent        = TYPE_PIIX_IDE,
    .class_init    = piix4_ide_class_init,
};

static void piix_ide_register_types(void)
{
    type_register_static(&piix_ide_info);
    type_register_static(&piix3_ide_info);
    type_register_static(&piix3_ide_xen_info);
    type_register_static(&piix4_ide_info);
}

type_init(piix_ide_register_types)
