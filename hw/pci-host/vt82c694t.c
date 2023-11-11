/*
 * QEMU VT82C694T PCI Bridge Emulation
 *
 * Copyright (c) 2006 Fabrice Bellard
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
#include "qemu/log.h"
#include "qemu/units.h"
#include "qemu/range.h"
#include "hw/i386/pc.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_bus.h"
#include "hw/pci/pci_host.h"
#include "hw/pci-host/vt82c694t.h"
#include "hw/qdev-properties.h"
#include "hw/resettable.h"
#include "hw/sysbus.h"
#include "qapi/error.h"
#include "migration/vmstate.h"
#include "qapi/visitor.h"
#include "qemu/error-report.h"
#include "qom/object.h"
#include "trace.h"

/*
 * VT82C694T chipset data sheet.
 */

#define TYPE_VT82C694T_PCI_BRIDGE "vt82c694t-pcibridge"

static void vt82c694t_pcibridge_reset_hold(Object *obj, ResetType type)
{
    PCIDevice *d = PCI_DEVICE(obj);

    memset(d->config + PCI_CONFIG_HEADER_SIZE, 0,
           PCI_CONFIG_SPACE_SIZE - PCI_CONFIG_HEADER_SIZE);
}

static void vt82c694t_pcibridge_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    k->vendor_id = PCI_VENDOR_ID_VIA;
    k->device_id = PCI_DEVICE_ID_VIA_82C694T_AGP;
    k->class_id = PCI_CLASS_BRIDGE_HOST;
    rc->phases.hold = vt82c694t_pcibridge_reset_hold;
    /*
     * PCI-facing part of the host bridge,
     * not usable without the host-facing part
     */
    dc->user_creatable = false;
}

OBJECT_DECLARE_SIMPLE_TYPE(VT82C694TState, VT82C694T_PCI_HOST_BRIDGE)

struct VT82C694TState {
    PCIHostState parent_obj;

    PCIBus pci_bus;
    MemoryRegion *system_memory;
    MemoryRegion *io_memory;
    MemoryRegion *pci_address_space;
    MemoryRegion *ram_memory;
    Range pci_hole;
    uint64_t below_4g_mem_size;
    uint64_t above_4g_mem_size;
    uint64_t pci_hole64_size;
    bool pci_hole64_fix;
};

#define VT82C694T_PAM      0x61
#define VT82C694T_PAM_SIZE 3
#define VT82C694T_SMRAM    0x63

#define VT82C694T_PMU_CONTROL_1 0x78

/* Keep it 2G to comply with older win32 guests */
#define VT82C694T_PCI_HOST_HOLE64_SIZE_DEFAULT (1ULL << 31)

/*
 * Older coreboot versions (4.0 and older) read a config register that doesn't
 * exist in real hardware, to get the RAM size from QEMU.
 */
#define VT82C694T_COREBOOT_RAM_SIZE 0x57

static void vt82c694t_pci_update_memory_mappings(VT82C694TPCIState *d)
{
    int i;
    PCIDevice *pd = PCI_DEVICE(d);

    memory_region_transaction_begin();
    for (i = 0; i < ARRAY_SIZE(d->pam_regions); i++) {
        int j = (i < ARRAY_SIZE(d->pam_regions) - 2) ? i : (i + 2);
        uint8_t val = (pd->config[VT82C694T_PAM + (j / 4)] >> (2 * (j % 4)))
                      & PAM_ATTR_MASK;

        if (val == 1) {
            val = 2;
        } else if (val == 2) {
            val = 1;
        }

        pam_update(&d->pam_regions[i], val);
    }

    switch (pd->config[VT82C694T_SMRAM] & 0x3) {
    case 0:
        memory_region_set_enabled(&d->smram_region, true);
        break;
    case 1:
    case 3: /* fall through */
        memory_region_set_enabled(&d->smram_region, false);
        break;
    case 2:
        qemu_log_mask(LOG_UNIMP, "%s: SMRAM mode 2\n", __func__);
        break;
    }
    memory_region_transaction_commit();
}

static void vt82c694t_pci_write_config(PCIDevice *dev, uint32_t address,
                                     uint32_t val, int len)
{
    VT82C694TPCIState *d = VT82C694T_PCI_DEVICE(dev);

    /* XXX: implement SMRAM.D_LOCK */
    pci_default_write_config(dev, address, val, len);
    if (ranges_overlap(address, len, VT82C694T_PAM, VT82C694T_PAM_SIZE) ||
        range_covers_byte(address, len, VT82C694T_SMRAM)) {
        vt82c694t_pci_update_memory_mappings(d);
    }

    if (ranges_overlap(address, len, VT82C694T_PMU_CONTROL_1, 1)) {
        bool enabled = PCI_DEVICE(d)->config[VT82C694T_PMU_CONTROL_1] & BIT(7);
        memory_region_set_enabled(&d->port22, enabled);
    }
}

static void vt82c694t_pci_port22_write(void *opaque, hwaddr addr, uint64_t data,
                                       unsigned size)
{
    VT82C694TPCIState *d = opaque;

    assert(addr == 0);

    trace_vt82c694t_pci_port22_write(data);

    d->port22_value = data;
}

static uint64_t vt82c694t_pci_port22_read(void *opaque, hwaddr addr,
                                          unsigned size)
{
    VT82C694TPCIState *d = opaque;

    assert(addr == 0);

    trace_vt82c694t_pci_port22_read(d->port22_value);

    return d->port22_value;
}

static const MemoryRegionOps vt82c694t_pci_port22_ops = {
    .read = vt82c694t_pci_port22_read,
    .write = vt82c694t_pci_port22_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void vt82c694t_pci_init(Object *obj)
{
    VT82C694TPCIState *d = VT82C694T_PCI_DEVICE(obj);

    memory_region_init_io(&d->port22, obj, &vt82c694t_pci_port22_ops, obj,
                          "port22", 1);
}

static void vt82c694t_pci_realize(PCIDevice *dev, Error **errp)
{
    ERRP_GUARD();
    VT82C694TPCIState *d = VT82C694T_PCI_DEVICE(dev);

    if (object_property_get_bool(qdev_get_machine(), "iommu", NULL)) {
        warn_report("VT82C694T doesn't support emulated iommu");
    }

    memory_region_add_subregion(pci_address_space_io(dev), 22, &d->port22);
}

static void vt82c694t_pci_reset_hold(Object *obj, ResetType type)
{
    VT82C694TPCIState *d = VT82C694T_PCI_DEVICE(obj);
    uint8_t *pci_conf = PCI_DEVICE(d)->config;

    memset(pci_conf + PCI_CONFIG_HEADER_SIZE, 0,
           PCI_CONFIG_SPACE_SIZE - PCI_CONFIG_HEADER_SIZE);

    pci_set_long(pci_conf + 0x10, 8);
    pci_set_long(pci_conf + 0x34, 0xa0);
    pci_set_byte(pci_conf + 0x53, 3);
    pci_set_byte(pci_conf + 0x56, 1);
    pci_set_byte(pci_conf + 0x57, 1);
    pci_set_word(pci_conf + 0x58, 0x40);
    pci_set_byte(pci_conf + 0x5a, 1);
    pci_set_byte(pci_conf + 0x5b, 1);
    pci_set_byte(pci_conf + 0x5c, 1);
    pci_set_byte(pci_conf + 0x5d, 1);
    pci_set_byte(pci_conf + 0x5e, 1);
    pci_set_byte(pci_conf + 0x5f, 1);
    pci_set_byte(pci_conf + VT82C694T_PAM, 0);
    pci_set_byte(pci_conf + VT82C694T_PAM + 1, 0);
    pci_set_byte(pci_conf + VT82C694T_PAM + 2, 0);
    pci_set_byte(pci_conf + 0x64, 0xfc);
    pci_set_byte(pci_conf + 0x65, 0xfc);
    pci_set_byte(pci_conf + 0x66, 0xfc);
    pci_set_byte(pci_conf + 0x67, 0xfc);
    pci_set_byte(pci_conf + 0x6b, 1);
    pci_set_byte(pci_conf + 0x7b, 2);

    vt82c694t_pci_update_memory_mappings(d);

    memory_region_set_enabled(&d->port22, false);
    d->port22_value = 0;
}

static int vt82c694t_pci_post_load(void *opaque, int version_id)
{
    VT82C694TPCIState *d = opaque;

    vt82c694t_pci_update_memory_mappings(d);
    return 0;
}

static const VMStateDescription vmstate_pci_vt82c694t = {
    .name = "VT82C694T-PCI",
    .version_id = 3,
    .minimum_version_id = 3,
    .post_load = vt82c694t_pci_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(parent_obj, VT82C694TPCIState),
        /*
         * Used to be smm_enabled, which was basically always zero because
         * SeaBIOS hardly uses SMM.  SMRAM is now handled by CPU code.
         */
        VMSTATE_UNUSED(1),
        VMSTATE_END_OF_LIST()
    }
};

static void vt82c694t_pci_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    k->realize = vt82c694t_pci_realize;
    k->config_write = vt82c694t_pci_write_config;
    k->vendor_id = PCI_VENDOR_ID_VIA;
    k->device_id = PCI_DEVICE_ID_VIA_82C694T_PCI;
    k->revision = 0x82;
    k->class_id = PCI_CLASS_BRIDGE_HOST;
    rc->phases.hold = vt82c694t_pci_reset_hold;
    dc->desc = "Host bridge";
    dc->vmsd = &vmstate_pci_vt82c694t;
    /*
     * PCI-facing part of the host bridge, not usable without the
     * host-facing part, which can't be device_add'ed, yet.
     */
    dc->user_creatable = false;
    dc->hotpluggable   = false;
}

static void vt82c694t_get_pci_hole_start(Object *obj, Visitor *v,
                                              const char *name, void *opaque,
                                              Error **errp)
{
    VT82C694TState *s = VT82C694T_PCI_HOST_BRIDGE(obj);
    uint64_t val64;
    uint32_t value;

    val64 = range_is_empty(&s->pci_hole) ? 0 : range_lob(&s->pci_hole);
    value = val64;
    assert(value == val64);
    visit_type_uint32(v, name, &value, errp);
}

static void vt82c694t_get_pci_hole_end(Object *obj, Visitor *v,
                                            const char *name, void *opaque,
                                            Error **errp)
{
    VT82C694TState *s = VT82C694T_PCI_HOST_BRIDGE(obj);
    uint64_t val64;
    uint32_t value;

    val64 = range_is_empty(&s->pci_hole) ? 0 : range_upb(&s->pci_hole) + 1;
    value = val64;
    assert(value == val64);
    visit_type_uint32(v, name, &value, errp);
}

/*
 * The 64bit PCI hole start is set by the Guest firmware
 * as the address of the first 64bit PCI MEM resource.
 * If no PCI device has resources on the 64bit area,
 * the 64bit PCI hole will start after "over 4G RAM" and the
 * reserved space for memory hotplug if any.
 */
static uint64_t vt82c694t_pcihost_get_pci_hole64_start_value(Object *obj)
{
    PCIHostState *h = PCI_HOST_BRIDGE(obj);
    VT82C694TState *s = VT82C694T_PCI_HOST_BRIDGE(obj);
    Range w64;
    uint64_t value;

    pci_bus_get_w64_range(h->bus, &w64);
    value = range_is_empty(&w64) ? 0 : range_lob(&w64);
    if (!value && s->pci_hole64_fix) {
        value = pc_pci_hole64_start();
    }
    return value;
}

static void vt82c694t_get_pci_hole64_start(Object *obj, Visitor *v,
                                           const char *name,
                                           void *opaque, Error **errp)
{
    uint64_t hole64_start = vt82c694t_pcihost_get_pci_hole64_start_value(obj);

    visit_type_uint64(v, name, &hole64_start, errp);
}

/*
 * The 64bit PCI hole end is set by the Guest firmware
 * as the address of the last 64bit PCI MEM resource.
 * Then it is expanded to the PCI_HOST_PROP_PCI_HOLE64_SIZE
 * that can be configured by the user.
 */
static void vt82c694t_get_pci_hole64_end(Object *obj, Visitor *v,
                                         const char *name, void *opaque,
                                         Error **errp)
{
    PCIHostState *h = PCI_HOST_BRIDGE(obj);
    VT82C694TState *s = VT82C694T_PCI_HOST_BRIDGE(obj);
    uint64_t hole64_start = vt82c694t_pcihost_get_pci_hole64_start_value(obj);
    Range w64;
    uint64_t value, hole64_end;

    pci_bus_get_w64_range(h->bus, &w64);
    value = range_is_empty(&w64) ? 0 : range_upb(&w64) + 1;
    hole64_end = ROUND_UP(hole64_start + s->pci_hole64_size, 1ULL << 30);
    if (s->pci_hole64_fix && value < hole64_end) {
        value = hole64_end;
    }
    visit_type_uint64(v, name, &value, errp);
}

static void vt82c694t_init(Object *obj)
{
    VT82C694TState *s = VT82C694T_PCI_HOST_BRIDGE(obj);
    PCIHostState *phb = PCI_HOST_BRIDGE(obj);

    memory_region_init_io(&phb->conf_mem, obj, &pci_host_conf_le_ops, phb,
                          "pci-conf-idx", 4);
    memory_region_init_io(&phb->data_mem, obj, &pci_host_data_le_ops, phb,
                          "pci-conf-data", 4);

    object_property_add_link(obj, PCI_HOST_PROP_RAM_MEM, TYPE_MEMORY_REGION,
                             (Object **) &s->ram_memory,
                             qdev_prop_allow_set_link_before_realize, 0);

    object_property_add_link(obj, PCI_HOST_PROP_PCI_MEM, TYPE_MEMORY_REGION,
                             (Object **) &s->pci_address_space,
                             qdev_prop_allow_set_link_before_realize, 0);

    object_property_add_link(obj, PCI_HOST_PROP_SYSTEM_MEM, TYPE_MEMORY_REGION,
                             (Object **) &s->system_memory,
                             qdev_prop_allow_set_link_before_realize, 0);

    object_property_add_link(obj, PCI_HOST_PROP_IO_MEM, TYPE_MEMORY_REGION,
                             (Object **) &s->io_memory,
                             qdev_prop_allow_set_link_before_realize, 0);
}

static void vt82c694t_realize(DeviceState *dev, Error **errp)
{
    ERRP_GUARD();
    VT82C694TState *s = VT82C694T_PCI_HOST_BRIDGE(dev);
    PCIHostState *phb = PCI_HOST_BRIDGE(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    PCIDevice *d;
    VT82C694TPCIState *f;
    unsigned i;

    memory_region_add_subregion(s->io_memory, 0xcf8, &phb->conf_mem);
    sysbus_init_ioports(sbd, 0xcf8, 4);

    memory_region_add_subregion(s->io_memory, 0xcfc, &phb->data_mem);
    sysbus_init_ioports(sbd, 0xcfc, 4);

    /* register VT82C694T 0xcf8 port as coalesced pio */
    memory_region_set_flush_coalesced(&phb->data_mem);
    memory_region_add_coalescing(&phb->conf_mem, 0, 4);

    range_set_bounds(&s->pci_hole, s->below_4g_mem_size,
                     IO_APIC_DEFAULT_ADDRESS - 1);

    /* setup pci memory mapping */
    pc_pci_as_mapping_init(s->system_memory, s->pci_address_space);

    pci_root_bus_init(&s->pci_bus, sizeof(s->pci_bus), dev, NULL,
                      s->pci_address_space, s->io_memory, PCI_DEVFN(7, 0),
                      TYPE_PCI_BUS);
    phb->bus = &s->pci_bus;

    d = pci_create_simple(&s->pci_bus, 0, TYPE_VT82C694T_PCI_DEVICE);
    f = VT82C694T_PCI_DEVICE(d);

    /* if *disabled* show SMRAM to all CPUs */
    memory_region_init_alias(&f->smram_region, OBJECT(d), "smram-region",
                             s->pci_address_space, SMRAM_C_BASE, SMRAM_C_SIZE);
    memory_region_add_subregion_overlap(s->system_memory, SMRAM_C_BASE,
                                        &f->smram_region, 1);
    memory_region_set_enabled(&f->smram_region, true);

    /* smram, as seen by SMM CPUs */
    memory_region_init(&f->smram, OBJECT(d), "smram", 4 * GiB);
    memory_region_set_enabled(&f->smram, true);
    memory_region_init_alias(&f->low_smram, OBJECT(d), "smram-low",
                             s->ram_memory, SMRAM_C_BASE, SMRAM_C_SIZE);
    memory_region_set_enabled(&f->low_smram, true);
    memory_region_add_subregion(&f->smram, SMRAM_C_BASE, &f->low_smram);
    object_property_add_const_link(qdev_get_machine(), "smram",
                                   OBJECT(&f->smram));

    for (i = 0; i < ARRAY_SIZE(f->pam_regions) - 2; ++i) {
        init_pam(&f->pam_regions[i], OBJECT(d), s->ram_memory,
                 s->system_memory, s->pci_address_space,
                 PAM_EXPAN_BASE + i * PAM_EXPAN_SIZE, PAM_EXPAN_SIZE);
    }
    init_pam(&f->pam_regions[8], OBJECT(d), s->ram_memory, s->system_memory,
             s->pci_address_space, PAM_BIOS_BASE, PAM_BIOS_SIZE);
    init_pam(&f->pam_regions[9], OBJECT(d), s->ram_memory, s->system_memory,
             s->pci_address_space, PAM_EXBIOS_BASE, PAM_BIOS_SIZE);

    ram_addr_t ram_size = s->below_4g_mem_size + s->above_4g_mem_size;
    ram_size = ram_size / 8 / 1024 / 1024;
    if (ram_size > 255) {
        ram_size = 255;
    }
    d->config[VT82C694T_COREBOOT_RAM_SIZE] = ram_size;

    pci_create_simple(phb->bus, PCI_DEVFN(PCI_SLOT(d->devfn) + 1, 0),
                      TYPE_VT82C694T_PCI_BRIDGE);
}

static const char *vt82c694t_root_bus_path(PCIHostState *host_bridge,
                                         PCIBus *rootbus)
{
    return "0000:00";
}

static const Property vt82c694t_props[] = {
    DEFINE_PROP_SIZE(PCI_HOST_PROP_PCI_HOLE64_SIZE, VT82C694TState,
                     pci_hole64_size, VT82C694T_PCI_HOST_HOLE64_SIZE_DEFAULT),
    DEFINE_PROP_SIZE(PCI_HOST_BELOW_4G_MEM_SIZE, VT82C694TState,
                     below_4g_mem_size, 0),
    DEFINE_PROP_SIZE(PCI_HOST_ABOVE_4G_MEM_SIZE, VT82C694TState,
                     above_4g_mem_size, 0),
    DEFINE_PROP_BOOL("x-pci-hole64-fix", VT82C694TState, pci_hole64_fix, true),
};

static void vt82c694t_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIHostBridgeClass *hc = PCI_HOST_BRIDGE_CLASS(klass);

    hc->root_bus_path = vt82c694t_root_bus_path;
    dc->realize = vt82c694t_realize;
    dc->fw_name = "pci";
    device_class_set_props(dc, vt82c694t_props);
    /* Reason: needs to be wired up by pc_init1 */
    dc->user_creatable = false;

    object_class_property_add(klass, PCI_HOST_PROP_PCI_HOLE_START, "uint32",
                              vt82c694t_get_pci_hole_start,
                              NULL, NULL, NULL);

    object_class_property_add(klass, PCI_HOST_PROP_PCI_HOLE_END, "uint32",
                              vt82c694t_get_pci_hole_end,
                              NULL, NULL, NULL);

    object_class_property_add(klass, PCI_HOST_PROP_PCI_HOLE64_START, "uint64",
                              vt82c694t_get_pci_hole64_start,
                              NULL, NULL, NULL);

    object_class_property_add(klass, PCI_HOST_PROP_PCI_HOLE64_END, "uint64",
                              vt82c694t_get_pci_hole64_end,
                              NULL, NULL, NULL);
}

static const TypeInfo types[] = {
    {
        .name          = TYPE_VT82C694T_PCI_BRIDGE,
        .parent        = TYPE_PCI_DEVICE,
        .instance_size = sizeof(PCIDevice),
        .class_init    = vt82c694t_pcibridge_class_init,
        .interfaces    = (InterfaceInfo[]) {
            { INTERFACE_CONVENTIONAL_PCI_DEVICE },
            { },
        },
    },
    {
        .name          = TYPE_VT82C694T_PCI_DEVICE,
        .parent        = TYPE_PCI_DEVICE,
        .instance_size = sizeof(VT82C694TPCIState),
        .instance_init = vt82c694t_pci_init,
        .class_init    = vt82c694t_pci_class_init,
        .interfaces    = (InterfaceInfo[]) {
            { INTERFACE_CONVENTIONAL_PCI_DEVICE },
            { },
        },
    },
    {
        .name          = TYPE_VT82C694T_PCI_HOST_BRIDGE,
        .parent        = TYPE_PCI_HOST_BRIDGE,
        .instance_size = sizeof(VT82C694TState),
        .instance_init = vt82c694t_init,
        .class_init    = vt82c694t_class_init,
    }
};

DEFINE_TYPES(types)
