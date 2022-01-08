/*
 * isa bus support for qdev.
 *
 * Copyright (c) 2009 Gerd Hoffmann <kraxel@redhat.com>
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/sysbus.h"
#include "sysemu/sysemu.h"
#include "hw/isa/isa.h"

static char *isabus_get_fw_dev_path(DeviceState *dev);

static void isa_bus_class_init(ObjectClass *klass, void *data)
{
    BusClass *k = BUS_CLASS(klass);

    k->get_fw_dev_path = isabus_get_fw_dev_path;
}

static const TypeInfo isa_dma_info = {
    .name = TYPE_ISADMA,
    .parent = TYPE_INTERFACE,
    .class_size = sizeof(IsaDmaClass),
};

static const TypeInfo isa_bus_info = {
    .name = TYPE_ISA_BUS,
    .parent = TYPE_BUS,
    .instance_size = sizeof(ISABus),
    .class_init = isa_bus_class_init,
};

ISABus *isa_bus_new(DeviceState *dev, MemoryRegion* address_space,
                    MemoryRegion *address_space_io, Error **errp)
{
    ISABus *isabus;

    if (!dev) {
        dev = qdev_new("isabus-bridge");
        sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    }

    isabus = ISA_BUS(qbus_new(TYPE_ISA_BUS, dev, NULL));
    isabus->address_space = address_space;
    isabus->address_space_io = address_space_io;
    return isabus;
}

void isa_bus_irqs(ISABus *bus, qemu_irq *irqs)
{
    bus->irqs = irqs;
}

/*
 * isa_bus_get_irq() returns the corresponding qemu_irq entry for the i8259.
 *
 * This function is only for special cases such as the 'ferr', and
 * temporary use for normal devices until they are converted to qdev.
 */
qemu_irq isa_bus_get_irq(ISABus *bus, unsigned isairq)
{
    assert(bus);
    assert(isairq < ISA_NUM_IRQS);
    return bus->irqs[isairq];
}

void isa_bus_dma(ISABus *bus, IsaDma *dma8, IsaDma *dma16)
{
    assert(bus && dma8 && dma16);
    assert(!bus->dma[0] && !bus->dma[1]);
    bus->dma[0] = dma8;
    bus->dma[1] = dma16;
}

IsaDma *isa_get_dma(ISABus *bus, int nchan)
{
    assert(bus);
    return bus->dma[nchan > 3 ? 1 : 0];
}

static inline void isa_init_ioport(ISADevice *dev, uint16_t ioport)
{
    if (dev && (dev->ioport_id == 0 || ioport < dev->ioport_id)) {
        dev->ioport_id = ioport;
    }
}

void isa_register_ioport(ISADevice *dev, MemoryRegion *io, uint16_t start)
{
    ISABus *isabus = isa_bus_from_device(dev);
    memory_region_add_subregion(isabus->address_space_io, start, io);
    isa_init_ioport(dev, start);
}

int isa_register_portio_list(ISADevice *dev,
                             PortioList *piolist, uint16_t start,
                             const MemoryRegionPortio *pio_start,
                             void *opaque, const char *name)
{
    ISABus *isabus;
    assert(piolist && !piolist->owner);
    isabus = isa_bus_from_device(dev);

    if (!isabus) {
        return -ENODEV;
    }

    /* START is how we should treat DEV, regardless of the actual
       contents of the portio array.  This is how the old code
       actually handled e.g. the FDC device.  */
    isa_init_ioport(dev, start);

    portio_list_init(piolist, OBJECT(dev), pio_start, opaque, name);
    portio_list_add(piolist, isabus->address_space_io, start);

    return 0;
}

void isa_bus_register_portio_list(ISABus *isabus,
                                  PortioList *piolist, uint16_t start,
                                  const MemoryRegionPortio *pio_start,
                                  void *opaque, const char *name)
{
    assert(isabus);
    assert(piolist && !piolist->owner);

    portio_list_init(piolist, NULL, pio_start, opaque, name);
    portio_list_add(piolist, isabus->address_space_io, start);
}

ISADevice *isa_new(const char *name)
{
    return ISA_DEVICE(qdev_new(name));
}

ISADevice *isa_try_new(const char *name)
{
    return ISA_DEVICE(qdev_try_new(name));
}

ISADevice *isa_create_simple(ISABus *bus, const char *name)
{
    ISADevice *dev;

    dev = isa_new(name);
    isa_realize_and_unref(dev, bus, &error_fatal);
    return dev;
}

bool isa_realize_and_unref(ISADevice *dev, ISABus *bus, Error **errp)
{
    return qdev_realize_and_unref(&dev->parent_obj, &bus->parent_obj, errp);
}

ISADevice *isa_vga_init(ISABus *bus)
{
    vga_interface_created = true;
    switch (vga_interface_type) {
    case VGA_CIRRUS:
        return isa_create_simple(bus, "isa-cirrus-vga");
    case VGA_QXL:
        error_report("%s: qxl: no PCI bus", __func__);
        return NULL;
    case VGA_STD:
        return isa_create_simple(bus, "isa-vga");
    case VGA_VMWARE:
        error_report("%s: vmware_vga: no PCI bus", __func__);
        return NULL;
    case VGA_VIRTIO:
        error_report("%s: virtio-vga: no PCI bus", __func__);
        return NULL;
    case VGA_NONE:
    default:
        return NULL;
    }
}

void isa_build_aml(ISABus *bus, Aml *scope)
{
    BusChild *kid;
    ISADevice *dev;
    ISADeviceClass *dc;

    QTAILQ_FOREACH(kid, &bus->parent_obj.children, sibling) {
        dev = ISA_DEVICE(kid->child);
        dc = ISA_DEVICE_GET_CLASS(dev);
        if (dc->build_aml) {
            dc->build_aml(dev, scope);
        }
    }
}

static void isabus_bridge_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->fw_name = "isa";
}

static const TypeInfo isabus_bridge_info = {
    .name          = "isabus-bridge",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SysBusDevice),
    .class_init    = isabus_bridge_class_init,
};

static void isa_device_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);
    k->bus_type = TYPE_ISA_BUS;
}

static const TypeInfo isa_device_type_info = {
    .name = TYPE_ISA_DEVICE,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(ISADevice),
    .abstract = true,
    .class_size = sizeof(ISADeviceClass),
    .class_init = isa_device_class_init,
};

static void isabus_register_types(void)
{
    type_register_static(&isa_dma_info);
    type_register_static(&isa_bus_info);
    type_register_static(&isabus_bridge_info);
    type_register_static(&isa_device_type_info);
}

static char *isabus_get_fw_dev_path(DeviceState *dev)
{
    ISADevice *d = ISA_DEVICE(dev);
    char path[40];
    int off;

    off = snprintf(path, sizeof(path), "%s", qdev_fw_name(dev));
    if (d->ioport_id) {
        snprintf(path + off, sizeof(path) - off, "@%04x", d->ioport_id);
    }

    return g_strdup(path);
}

MemoryRegion *isa_address_space(ISADevice *dev)
{
    assert(dev);

    return isa_bus_from_device(dev)->address_space;
}

MemoryRegion *isa_address_space_io(ISADevice *dev)
{
    assert(dev);

    return isa_bus_from_device(dev)->address_space_io;
}

type_init(isabus_register_types)
