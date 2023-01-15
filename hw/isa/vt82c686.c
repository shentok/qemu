/*
 * VT82C686B south bridge support
 *
 * Copyright (c) 2008 yajin (yajin@vm-kernel.org)
 * Copyright (c) 2009 chenming (chenming@rdc.faw.com.cn)
 * Copyright (c) 2010 Huacai Chen (zltjiangshi@gmail.com)
 * This code is licensed under the GNU GPL v2.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 *
 * VT8231 south bridge support and general clean up to allow it
 * Copyright (c) 2018-2020 BALATON Zoltan
 */

#include "qemu/osdep.h"
#include "hw/isa/vt82c686.h"
#include "hw/block/fdc.h"
#include "hw/char/parallel-isa.h"
#include "hw/char/serial-isa.h"
#include "hw/pci/pci.h"
#include "hw/qdev-properties.h"
#include "hw/ide/pci.h"
#include "hw/isa/isa.h"
#include "hw/isa/superio.h"
#include "hw/intc/i8259.h"
#include "hw/irq.h"
#include "hw/dma/i8257.h"
#include "hw/usb/hcd-uhci.h"
#include "hw/timer/i8254.h"
#include "hw/rtc/mc146818rtc.h"
#include "hw/rtc/mc146818rtc_regs.h"
#include "migration/vmstate.h"
#include "hw/acpi/acpi.h"
#include "hw/acpi/acpi_aml_interface.h"
#include "hw/i2c/pm_smbus.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/notify.h"
#include "qemu/range.h"
#include "qemu/timer.h"
#include "system/runstate.h"
#include "trace.h"

OBJECT_DECLARE_SIMPLE_TYPE(ViaPMState, VIA_PM)

#define VIA_PM_IO_GBLSTS 0x28
#define VIA_PM_IO_GBLSTS_SW_SMI BIT(6)

#define VIA_PM_IO_GBLEN 0x2a
#define VIA_PM_IO_GBLEN_SW_SMI BIT(6)

#define VIA_PM_IO_GBLCTL 0x2c
#define VIA_PM_IO_GBLCTL_SMI_EN BIT(0)
#define VIA_PM_IO_GBLCTL_SMIIG BIT(4)
#define VIA_PM_IO_GBLCTL_INSMI BIT(8)

#define VIA_PM_IO_SMI_CMD 0x2f
#define ACPI_ENABLE 0xf1
#define ACPI_DISABLE 0xf0

#define QEMU_GPE_BASE 0xafe0
#define VIA_PM_GPE_LEN 4

#define VIA_PM_SCI_SELECT_OFS 0x42
#define VIA_PM_SCI_SELECT_MASK 0xf

struct ViaPMState {
    PCIDevice dev;

    MemoryRegion io;
    ACPIREGS ar;
    uint16_t gbl_sts;
    uint16_t gbl_en;
    uint16_t gbl_ctl;
    uint8_t smi_cmd;

    PMSMBus smb;

    MemoryRegion hw_io;

    MemoryRegion io_gpe_qemu;

    Notifier powerdown_notifier;

    qemu_irq sci_irq;
    qemu_irq smi_irq;
    bool smm_enabled;
};

static void pm_io_space_update(ViaPMState *s)
{
    uint32_t pmbase = pci_get_long(s->dev.config + 0x48) & 0xff80UL;

    memory_region_transaction_begin();
    memory_region_set_address(&s->io, pmbase);
    memory_region_set_enabled(&s->io, s->dev.config[0x41] & BIT(7));
    memory_region_transaction_commit();
}

static void pm_hw_io_space_update(ViaPMState *s)
{
    uint16_t hwbase = pci_get_word(s->dev.config + 0x70) & 0xff80UL;

    memory_region_transaction_begin();
    memory_region_set_address(&s->hw_io, hwbase);
    memory_region_set_enabled(&s->hw_io, s->dev.config[0x74] & BIT(0));
    memory_region_transaction_commit();
}

static void smb_io_space_update(ViaPMState *s)
{
    uint32_t smbase = pci_get_long(s->dev.config + 0x90) & 0xfff0UL;

    memory_region_transaction_begin();
    memory_region_set_address(&s->smb.io, smbase);
    memory_region_set_enabled(&s->smb.io, s->dev.config[0xd2] & BIT(0));
    memory_region_transaction_commit();
}

static int vmstate_acpi_post_load(void *opaque, int version_id)
{
    ViaPMState *s = opaque;

    pm_io_space_update(s);
    pm_hw_io_space_update(s);
    smb_io_space_update(s);
    return 0;
}

static const VMStateDescription vmstate_acpi = {
    .name = "vt82c686b_pm",
    .version_id = 2,
    .minimum_version_id = 1,
    .post_load = vmstate_acpi_post_load,
    .fields = (const VMStateField[]) {
        VMSTATE_PCI_DEVICE(dev, ViaPMState),
        VMSTATE_UINT16(ar.pm1.evt.sts, ViaPMState),
        VMSTATE_UINT16(ar.pm1.evt.en, ViaPMState),
        VMSTATE_UINT16(ar.pm1.cnt.cnt, ViaPMState),
        VMSTATE_TIMER_PTR(ar.tmr.timer, ViaPMState),
        VMSTATE_INT64(ar.tmr.overflow_time, ViaPMState),
        VMSTATE_UINT16(gbl_sts, ViaPMState),
        VMSTATE_UINT16(gbl_en, ViaPMState),
        VMSTATE_UINT16(gbl_ctl, ViaPMState),
        VMSTATE_UINT8(smi_cmd, ViaPMState),
        VMSTATE_END_OF_LIST()
    }
};

static void pm_write_config(PCIDevice *d, uint32_t addr, uint32_t val, int len)
{
    ViaPMState *s = VIA_PM(d);

    trace_via_pm_write(addr, val, len);
    pci_default_write_config(d, addr, val, len);
    if (ranges_overlap(addr, len, 0x48, 4)) {
        uint32_t v = pci_get_long(s->dev.config + 0x48);
        pci_set_long(s->dev.config + 0x48, (v & 0xff80UL) | 1);
    }
    if (range_covers_byte(addr, len, 0x41)) {
        pm_io_space_update(s);
    }
    if (range_covers_byte(addr, len, 0x74)) {
        pm_hw_io_space_update(s);
    }
    if (ranges_overlap(addr, len, 0x90, 4)) {
        uint32_t v = pci_get_long(s->dev.config + 0x90);
        pci_set_long(s->dev.config + 0x90, (v & 0xfff0UL) | 1);
    }
    if (range_covers_byte(addr, len, 0xd2)) {
        s->dev.config[0xd2] &= 0xf;
        smb_io_space_update(s);
    }
}

static void via_pm_trigger_sw_smi(ViaPMState *s, uint8_t val)
{
    s->smi_cmd = val;

    trace_via_pm_trigger_sw_smi(val);

    /* ACPI specs 3.0, 4.7.2.5 */
    acpi_pm1_cnt_update(&s->ar, val == ACPI_ENABLE, val == ACPI_DISABLE);
    if (val == ACPI_ENABLE || val == ACPI_DISABLE) {
        return;
    }

    if (s->gbl_en & VIA_PM_IO_GBLEN_SW_SMI
        && s->gbl_ctl & VIA_PM_IO_GBLCTL_SMI_EN
        && !(s->gbl_ctl & VIA_PM_IO_GBLCTL_SMIIG
             && s->gbl_ctl & VIA_PM_IO_GBLCTL_INSMI)) {
        s->gbl_ctl |= VIA_PM_IO_GBLCTL_INSMI;
        s->gbl_sts |= VIA_PM_IO_GBLSTS_SW_SMI;

        if (s->smi_irq) {
            qemu_irq_raise(s->smi_irq);
        }
    }
}

static void pm_io_write(void *op, hwaddr addr, uint64_t data, unsigned size)
{
    ViaPMState *s = op;

    trace_via_pm_io_write(addr, data, size);

    switch (addr) {
    case VIA_PM_IO_GBLSTS:
        s->gbl_sts &= ~(s->gbl_sts & data);
        break;
    case VIA_PM_IO_GBLEN:
        s->gbl_en = (s->gbl_en & 0xff00) | data;
        break;
    case VIA_PM_IO_GBLEN + 1:
        s->gbl_en = (s->gbl_en & 0x00ff) | (data << 8);
        break;
    case VIA_PM_IO_GBLCTL:
        s->gbl_ctl = (s->gbl_ctl & 0xff00) | data;
        break;
    case VIA_PM_IO_GBLCTL + 1:
        data <<= 8;
        if (data & VIA_PM_IO_GBLCTL_INSMI) {
            data &= ~VIA_PM_IO_GBLCTL_INSMI;
        }
        s->gbl_ctl = (s->gbl_ctl & 0x00ff) | data;
        break;
    case VIA_PM_IO_SMI_CMD:
        via_pm_trigger_sw_smi(s, data);
        break;
    }
}

static uint64_t pm_io_read(void *op, hwaddr addr, unsigned size)
{
    ViaPMState *s = op;
    uint64_t data = 0;

    switch (addr) {
    case VIA_PM_IO_GBLSTS:
        data = s->gbl_sts & 0xff;
        break;
    case VIA_PM_IO_GBLSTS + 1:
        data = s->gbl_sts >> 8;
        break;
    case VIA_PM_IO_GBLEN:
        data = s->gbl_en & 0xff;
        break;
    case VIA_PM_IO_GBLEN + 1:
        data = s->gbl_en >> 8;
        break;
    case VIA_PM_IO_GBLCTL:
        data = s->gbl_ctl & 0xff;
        break;
    case VIA_PM_IO_GBLCTL + 1:
        data = (s->gbl_ctl >> 8) & 0xd;
        break;
    case VIA_PM_IO_SMI_CMD:
        data = s->smi_cmd;
        break;
    }

    trace_via_pm_io_read(addr, data, size);

    return data;
}

static const MemoryRegionOps pm_io_ops = {
    .read = pm_io_read,
    .write = pm_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void pm_hw_io_write(void *op, hwaddr addr, uint64_t data, unsigned size)
{
    trace_via_pm_hw_io_write(addr, data, size);

    switch (addr) {
    default:
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented register: 0x%" PRIx64 "\n",
                      __func__, addr);
        break;
    }
}

static uint64_t pm_hw_io_read(void *op, hwaddr addr, unsigned size)
{
    uint64_t data = 0;

    switch (addr) {
    default:
        qemu_log_mask(LOG_UNIMP, "%s: unimplemented register: 0x%" PRIx64 "\n",
                      __func__, addr);
        break;
    }

    trace_via_pm_hw_io_read(addr, data, size);

    return data;
}

static const MemoryRegionOps pm_hw_io_ops = {
    .read = pm_hw_io_read,
    .write = pm_hw_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void via_pm_set_sci_irq(void *opaque, int n, int level)
{
    via_isa_set_irq(opaque, 0, level);
}

static void pm_tmr_timer(ACPIREGS *ar)
{
    ViaPMState *s = container_of(ar, ViaPMState, ar);
    acpi_update_sci(&s->ar, s->sci_irq);
}

static void via_pm_reset(DeviceState *d)
{
    ViaPMState *s = VIA_PM(d);

    memset(s->dev.config + PCI_CONFIG_HEADER_SIZE, 0,
           PCI_CONFIG_SPACE_SIZE - PCI_CONFIG_HEADER_SIZE);
    /* ACPI Interrupt Select */
    pci_set_byte(s->dev.config + 0x42, BIT(6));
    /* Power Management IO base */
    pci_set_long(s->dev.config + 0x48, 1);
    /* SMBus IO base */
    pci_set_long(s->dev.config + 0x90, 1);

    s->gbl_sts = 0;
    s->gbl_en = 0;
    s->gbl_ctl = VIA_PM_IO_GBLCTL_SMIIG;
    s->smi_cmd = 0;

    if (!s->smm_enabled) {
        /*
         * Mark SMM as already inited to prevent SMM from running. Some
         * virtualization technologies such as WHPX don't support SMM mode.
         */
        s->gbl_en |= VIA_PM_IO_GBLEN_SW_SMI;
    }

    acpi_pm1_evt_reset(&s->ar);
    acpi_pm1_cnt_reset(&s->ar);
    acpi_pm_tmr_reset(&s->ar);
    acpi_gpe_reset(&s->ar);
    acpi_update_sci(&s->ar, s->sci_irq);

    pm_io_space_update(s);
    pm_hw_io_space_update(s);
    smb_io_space_update(s);
}

static void via_pm_powerdown_req(Notifier *n, void *opaque)
{
    ViaPMState *s = container_of(n, ViaPMState, powerdown_notifier);

    acpi_pm1_evt_power_down(&s->ar);
}

static uint64_t via_pm_gpe_qemu_readb(void *opaque, hwaddr addr, unsigned width)
{
    ViaPMState *s = opaque;
    uint32_t val = acpi_gpe_ioport_readb(&s->ar, addr);

    return val;
}

static void via_pm_gpe_qemu_writeb(void *opaque, hwaddr addr, uint64_t val,
                                   unsigned width)
{
    ViaPMState *s = opaque;

    acpi_gpe_ioport_writeb(&s->ar, addr, val);
    acpi_update_sci(&s->ar, s->sci_irq);
}

static const MemoryRegionOps via_pm_gpe_qemu_ops = {
    .read = via_pm_gpe_qemu_readb,
    .write = via_pm_gpe_qemu_writeb,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void via_pm_get_prop_smi_cmd_port(Object *obj, Visitor *v,
                                         const char *name, void *opaque,
                                         Error **errp)
{
    ViaPMState *s = opaque;
    uint64_t value = s->io.addr + VIA_PM_IO_SMI_CMD;

    visit_type_uint64(v, name, &value, errp);
}

static void via_pm_add_properties(ViaPMState *s)
{
    static const uint8_t acpi_enable_cmd = ACPI_ENABLE;
    static const uint8_t acpi_disable_cmd = ACPI_DISABLE;
    static const uint16_t sci_int = 9;

    object_property_add_uint8_ptr(OBJECT(s), ACPI_PM_PROP_ACPI_ENABLE_CMD,
                                  &acpi_enable_cmd, OBJ_PROP_FLAG_READ);
    object_property_add_uint8_ptr(OBJECT(s), ACPI_PM_PROP_ACPI_DISABLE_CMD,
                                  &acpi_disable_cmd, OBJ_PROP_FLAG_READ);
    object_property_add_alias(OBJECT(s), ACPI_PM_PROP_GPE0_BLK,
                              OBJECT(&s->io_gpe_qemu), "addr");
    object_property_add_uint8_ptr(OBJECT(s), ACPI_PM_PROP_GPE0_BLK_LEN,
                                  &s->ar.gpe.len, OBJ_PROP_FLAG_READ);
    object_property_add(OBJECT(s), ACPI_PM_PROP_SMI_CMD_PORT, "uint64",
                        via_pm_get_prop_smi_cmd_port, NULL, NULL, s);
    object_property_add_uint16_ptr(OBJECT(s), ACPI_PM_PROP_SCI_INT,
                                   &sci_int, OBJ_PROP_FLAG_READ);
    object_property_add_alias(OBJECT(s), ACPI_PM_PROP_PM_IO_BASE,
                              OBJECT(&s->io), "addr");
}

static void via_pm_realize(PCIDevice *dev, Error **errp)
{
    ViaPMState *s = VIA_PM(dev);

    pci_set_word(dev->config + PCI_STATUS, PCI_STATUS_FAST_BACK |
                 PCI_STATUS_DEVSEL_MEDIUM);

    pm_smbus_init(DEVICE(s), &s->smb, true);
    memory_region_add_subregion(pci_address_space_io(dev), 0, &s->smb.io);
    memory_region_set_enabled(&s->smb.io, false);

    memory_region_init_io(&s->io, OBJECT(dev), &pm_io_ops, s, "via-pm", 128);
    memory_region_add_subregion(pci_address_space_io(dev), 0, &s->io);
    memory_region_set_enabled(&s->io, false);

    memory_region_init_io(&s->hw_io, OBJECT(dev), &pm_hw_io_ops, s, "via-pm-hw",
                          128);
    memory_region_add_subregion(pci_address_space_io(dev), 0, &s->hw_io);
    memory_region_set_enabled(&s->hw_io, false);

    memory_region_init_io(&s->io_gpe_qemu, OBJECT(s), &via_pm_gpe_qemu_ops, s,
                          "acpi-gpe-qemu", VIA_PM_GPE_LEN);
    memory_region_add_subregion(pci_address_space_io(dev), QEMU_GPE_BASE,
                                &s->io_gpe_qemu);

    acpi_pm_tmr_init(&s->ar, pm_tmr_timer, &s->io);
    acpi_pm1_evt_init(&s->ar, pm_tmr_timer, &s->io);
    acpi_pm1_cnt_init(&s->ar, &s->io, false, false, 2, !s->smm_enabled);
    acpi_gpe_init(&s->ar, VIA_PM_GPE_LEN);

    s->sci_irq = qemu_allocate_irq(via_pm_set_sci_irq, dev, 1);
    qdev_init_gpio_out_named(DEVICE(s), &s->smi_irq, "smi-irq", 1);

    s->powerdown_notifier.notify = via_pm_powerdown_req;
    qemu_register_powerdown_notifier(&s->powerdown_notifier);

    via_pm_add_properties(s);
}

typedef struct via_pm_init_info {
    uint16_t device_id;
} ViaPMInitInfo;

static const Property via_pm_properties[] = {
    DEFINE_PROP_BOOL("smm-enabled", ViaPMState, smm_enabled, false),
};

static void via_pm_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);
    const ViaPMInitInfo *info = data;

    k->realize = via_pm_realize;
    k->config_write = pm_write_config;
    k->vendor_id = PCI_VENDOR_ID_VIA;
    k->device_id = info->device_id;
    k->class_id = PCI_CLASS_BRIDGE_OTHER;
    k->revision = 0x40;
    device_class_set_legacy_reset(dc, via_pm_reset);
    /* Reason: part of VIA south bridge, does not exist stand alone */
    dc->user_creatable = false;
    dc->vmsd = &vmstate_acpi;
    device_class_set_props(dc, via_pm_properties);
}

static const TypeInfo via_pm_info = {
    .name          = TYPE_VIA_PM,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(ViaPMState),
    .abstract      = true,
    .interfaces = (const InterfaceInfo[]) {
        { TYPE_HOTPLUG_HANDLER },
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static const ViaPMInitInfo vt82c686b_pm_init_info = {
    .device_id = PCI_DEVICE_ID_VIA_82C686B_PM,
};

#define TYPE_VT82C686B_PM "vt82c686b-pm"

static const TypeInfo vt82c686b_pm_info = {
    .name          = TYPE_VT82C686B_PM,
    .parent        = TYPE_VIA_PM,
    .class_init    = via_pm_class_init,
    .class_data    = &vt82c686b_pm_init_info,
};

static const ViaPMInitInfo vt8231_pm_init_info = {
    .device_id = PCI_DEVICE_ID_VIA_8231_PM,
};

#define TYPE_VT8231_PM "vt8231-pm"

static const TypeInfo vt8231_pm_info = {
    .name          = TYPE_VT8231_PM,
    .parent        = TYPE_VIA_PM,
    .class_init    = via_pm_class_init,
    .class_data    = &vt8231_pm_init_info,
};


#define TYPE_VIA_SUPERIO "via-superio"
OBJECT_DECLARE_SIMPLE_TYPE(ViaSuperIOState, VIA_SUPERIO)

struct ViaSuperIOState {
    ISASuperIODevice superio;
    uint8_t regs[0x100];
    const MemoryRegionOps *io_ops;
    MemoryRegion io;
};

static inline void via_superio_io_enable(ViaSuperIOState *s, bool enable)
{
    memory_region_set_enabled(&s->io, enable);
}

static void via_superio_realize(DeviceState *d, Error **errp)
{
    ViaSuperIOState *s = VIA_SUPERIO(d);
    ISASuperIOClass *ic = ISA_SUPERIO_GET_CLASS(s);
    Error *local_err = NULL;

    assert(s->io_ops);
    ic->parent_realize(d, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }
    memory_region_init_io(&s->io, OBJECT(d), s->io_ops, s, "via-superio", 2);
    memory_region_set_enabled(&s->io, false);
    /* The floppy also uses 0x3f0 and 0x3f1 but this seems to work anyway */
    memory_region_add_subregion(isa_address_space_io(ISA_DEVICE(s)), 0x3f0,
                                &s->io);
}

static uint64_t via_superio_cfg_read(void *opaque, hwaddr addr, unsigned size)
{
    ViaSuperIOState *sc = opaque;
    uint8_t idx = sc->regs[0];
    uint8_t val = sc->regs[idx];

    if (addr == 0) {
        return idx;
    }
    if (addr == 1 && idx == 0) {
        val = 0; /* reading reg 0 where we store index value */
    }
    trace_via_superio_read(idx, val);
    return val;
}

static void via_superio_devices_enable(ViaSuperIOState *s, uint8_t data)
{
    ISASuperIOClass *ic = ISA_SUPERIO_GET_CLASS(s);

    isa_parallel_set_enabled(s->superio.parallel[0], (data & 0x3) != 3);
    for (int i = 0; i < ic->serial.count; i++) {
        isa_serial_set_enabled(s->superio.serial[i], data & BIT(i + 2));
    }
    isa_fdc_set_enabled(s->superio.floppy, data & BIT(4));
}

static void via_superio_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ISASuperIOClass *sc = ISA_SUPERIO_CLASS(klass);

    device_class_set_parent_realize(dc, via_superio_realize,
                                    &sc->parent_realize);
}

static const TypeInfo via_superio_info = {
    .name          = TYPE_VIA_SUPERIO,
    .parent        = TYPE_ISA_SUPERIO,
    .instance_size = sizeof(ViaSuperIOState),
    .class_size    = sizeof(ISASuperIOClass),
    .class_init    = via_superio_class_init,
    .abstract      = true,
};

#define TYPE_VT82C686B_SUPERIO "vt82c686b-superio"

static void vt82c686b_superio_cfg_write(void *opaque, hwaddr addr,
                                        uint64_t data, unsigned size)
{
    ViaSuperIOState *sc = opaque;
    uint8_t idx = sc->regs[0];

    if (addr == 0) { /* config index register */
        sc->regs[0] = data;
        return;
    }

    /* config data register */
    trace_via_superio_write(idx, data);
    switch (idx) {
    case 0x00 ... 0xdf:
    case 0xe4:
    case 0xe5:
    case 0xe9 ... 0xed:
    case 0xf3:
    case 0xf5:
    case 0xf7:
    case 0xf9 ... 0xfb:
    case 0xfd ... 0xff:
        /* ignore write to read only registers */
        return;
    case 0xe2:
        data &= 0x1f;
        via_superio_devices_enable(sc, data);
        break;
    case 0xe3:
        data &= 0xfc;
        isa_fdc_set_iobase(sc->superio.floppy, data << 2);
        break;
    case 0xe6:
        isa_parallel_set_iobase(sc->superio.parallel[0], data << 2);
        break;
    case 0xe7:
        data &= 0xfe;
        isa_serial_set_iobase(sc->superio.serial[0], data << 2);
        break;
    case 0xe8:
        data &= 0xfe;
        isa_serial_set_iobase(sc->superio.serial[1], data << 2);
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "via_superio_cfg: unimplemented register 0x%x\n", idx);
        break;
    }
    sc->regs[idx] = data;
}

static const MemoryRegionOps vt82c686b_superio_cfg_ops = {
    .read = via_superio_cfg_read,
    .write = vt82c686b_superio_cfg_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void vt82c686b_superio_reset(DeviceState *dev)
{
    ViaSuperIOState *s = VIA_SUPERIO(dev);

    memset(s->regs, 0, sizeof(s->regs));
    /* Device ID */
    vt82c686b_superio_cfg_write(s, 0, 0xe0, 1);
    vt82c686b_superio_cfg_write(s, 1, 0x3c, 1);
    /*
     * Function select - only serial enabled
     * Fuloong 2e's rescue-yl prints to the serial console w/o enabling it. This
     * suggests that the serial ports are enabled by default, so override the
     * datasheet.
     */
    vt82c686b_superio_cfg_write(s, 0, 0xe2, 1);
    vt82c686b_superio_cfg_write(s, 1, 0x0f, 1);
    /* Floppy ctrl base addr 0x3f0-7 */
    vt82c686b_superio_cfg_write(s, 0, 0xe3, 1);
    vt82c686b_superio_cfg_write(s, 1, 0xfc, 1);
    /* Parallel port base addr 0x378-f */
    vt82c686b_superio_cfg_write(s, 0, 0xe6, 1);
    vt82c686b_superio_cfg_write(s, 1, 0xde, 1);
    /* Serial port 1 base addr 0x3f8-f */
    vt82c686b_superio_cfg_write(s, 0, 0xe7, 1);
    vt82c686b_superio_cfg_write(s, 1, 0xfe, 1);
    /* Serial port 2 base addr 0x2f8-f */
    vt82c686b_superio_cfg_write(s, 0, 0xe8, 1);
    vt82c686b_superio_cfg_write(s, 1, 0xbe, 1);

    vt82c686b_superio_cfg_write(s, 0, 0, 1);
}

static void vt82c686b_superio_init(Object *obj)
{
    VIA_SUPERIO(obj)->io_ops = &vt82c686b_superio_cfg_ops;
}

static void vt82c686b_superio_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ISASuperIOClass *sc = ISA_SUPERIO_CLASS(klass);

    device_class_set_legacy_reset(dc, vt82c686b_superio_reset);
    sc->serial.count = 2;
    sc->parallel.count = 1;
    sc->ide.count = 0; /* emulated by via-ide */
    sc->floppy.count = 1;
}

static const TypeInfo vt82c686b_superio_info = {
    .name          = TYPE_VT82C686B_SUPERIO,
    .parent        = TYPE_VIA_SUPERIO,
    .instance_size = sizeof(ViaSuperIOState),
    .instance_init = vt82c686b_superio_init,
    .class_size    = sizeof(ISASuperIOClass),
    .class_init    = vt82c686b_superio_class_init,
};


#define TYPE_VT8231_SUPERIO "vt8231-superio"

static void vt8231_superio_cfg_write(void *opaque, hwaddr addr,
                                     uint64_t data, unsigned size)
{
    ViaSuperIOState *sc = opaque;
    uint8_t idx = sc->regs[0];

    if (addr == 0) { /* config index register */
        sc->regs[0] = data;
        return;
    }

    /* config data register */
    trace_via_superio_write(idx, data);
    switch (idx) {
    case 0x00 ... 0xdf:
    case 0xe7 ... 0xef:
    case 0xf0 ... 0xf1:
    case 0xf5:
    case 0xf8:
    case 0xfd:
        /* ignore write to read only registers */
        return;
    case 0xf2:
        data &= 0x17;
        via_superio_devices_enable(sc, data);
        break;
    case 0xf4:
        data &= 0xfe;
        isa_serial_set_iobase(sc->superio.serial[0], data << 2);
        break;
    case 0xf6:
        isa_parallel_set_iobase(sc->superio.parallel[0], data << 2);
        break;
    case 0xf7:
        data &= 0xfc;
        isa_fdc_set_iobase(sc->superio.floppy, data << 2);
        break;
    default:
        qemu_log_mask(LOG_UNIMP,
                      "via_superio_cfg: unimplemented register 0x%x\n", idx);
        break;
    }
    sc->regs[idx] = data;
}

static const MemoryRegionOps vt8231_superio_cfg_ops = {
    .read = via_superio_cfg_read,
    .write = vt8231_superio_cfg_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void vt8231_superio_reset(DeviceState *dev)
{
    ViaSuperIOState *s = VIA_SUPERIO(dev);

    memset(s->regs, 0, sizeof(s->regs));
    /* Device ID */
    s->regs[0xf0] = 0x3c;
    /* Device revision */
    s->regs[0xf1] = 0x01;
    /* Function select - all disabled */
    vt8231_superio_cfg_write(s, 0, 0xf2, 1);
    vt8231_superio_cfg_write(s, 1, 0x03, 1);
    /* Serial port base addr */
    vt8231_superio_cfg_write(s, 0, 0xf4, 1);
    vt8231_superio_cfg_write(s, 1, 0xfe, 1);
    /* Parallel port base addr */
    vt8231_superio_cfg_write(s, 0, 0xf6, 1);
    vt8231_superio_cfg_write(s, 1, 0xde, 1);
    /* Floppy ctrl base addr */
    vt8231_superio_cfg_write(s, 0, 0xf7, 1);
    vt8231_superio_cfg_write(s, 1, 0xfc, 1);

    vt8231_superio_cfg_write(s, 0, 0, 1);
}

static void vt8231_superio_init(Object *obj)
{
    VIA_SUPERIO(obj)->io_ops = &vt8231_superio_cfg_ops;
}

static void vt8231_superio_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ISASuperIOClass *sc = ISA_SUPERIO_CLASS(klass);

    device_class_set_legacy_reset(dc, vt8231_superio_reset);
    sc->serial.count = 1;
    sc->parallel.count = 1;
    sc->ide.count = 0; /* emulated by via-ide */
    sc->floppy.count = 1;
}

static const TypeInfo vt8231_superio_info = {
    .name          = TYPE_VT8231_SUPERIO,
    .parent        = TYPE_VIA_SUPERIO,
    .instance_size = sizeof(ViaSuperIOState),
    .instance_init = vt8231_superio_init,
    .class_size    = sizeof(ISASuperIOClass),
    .class_init    = vt8231_superio_class_init,
};


#define TYPE_VIA_ISA "via-isa"
OBJECT_DECLARE_SIMPLE_TYPE(ViaISAState, VIA_ISA)

struct ViaISAState {
    PCIDevice dev;

    IRQState i8259_irq;
    qemu_irq cpu_intr;
    qemu_irq isa_irqs_in[ISA_NUM_IRQS];
    uint16_t irq_state[ISA_NUM_IRQS];
    ViaSuperIOState via_sio;
    MC146818RtcState rtc;
    MemoryRegion rtc_io;
    MemoryRegion rtc_coalesced_io;
    uint8_t rtc_index;
    PCIIDEState ide;
    UHCIState uhci[2];
    ViaPMState pm;
    ViaAC97State ac97;
    PCIDevice mc97;

    bool has_acpi;
    bool has_pic;
    bool has_pit;
    bool has_usb;
    bool smm_enabled;
};

static const VMStateDescription vmstate_via = {
    .name = "via-isa",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_PCI_DEVICE(dev, ViaISAState),
        VMSTATE_END_OF_LIST()
    }
};

static void via_isa_init(Object *obj)
{
    ViaISAState *s = VIA_ISA(obj);
    DeviceState *dev = DEVICE(s);

    object_initialize_child(obj, "rtc", &s->rtc, TYPE_MC146818_RTC);
    object_initialize_child(obj, "ide", &s->ide, TYPE_VIA_IDE);
    object_initialize_child(obj, "uhci1", &s->uhci[0], TYPE_VT82C686B_USB_UHCI);
    object_initialize_child(obj, "uhci2", &s->uhci[1], TYPE_VT82C686B_USB_UHCI);
    object_initialize_child(obj, "ac97", &s->ac97, TYPE_VIA_AC97);
    object_initialize_child(obj, "mc97", &s->mc97, TYPE_VIA_MC97);

    qdev_init_gpio_out_named(dev, s->isa_irqs_in, "isa-irqs", ISA_NUM_IRQS);
}

static void build_pci_isa_aml(AcpiDevAmlIf *adev, Aml *scope)
{
    Aml *field;
    BusState *bus = qdev_get_child_bus(DEVICE(adev), "isa.0");

    /* PCI to ISA irq remapping */
    aml_append(scope, aml_operation_region("P40C", AML_PCI_CONFIG,
                                           aml_int(0x55), 0x03));

    field = aml_field("P40C", AML_BYTE_ACC, AML_NOLOCK, AML_PRESERVE);
    aml_append(field, aml_reserved_field(4));
    aml_append(field, aml_named_field("PRQ0", 4));
    aml_append(field, aml_named_field("PRQ1", 4));
    aml_append(field, aml_named_field("PRQ2", 4));
    aml_append(field, aml_reserved_field(4));
    aml_append(field, aml_named_field("PRQ3", 4));
    aml_append(scope, field);

    /* hack: put fields into _SB scope for LNKx to find them */
    aml_append(scope, aml_alias("PRQ0", "\\_SB.PRQ0"));
    aml_append(scope, aml_alias("PRQ1", "\\_SB.PRQ1"));
    aml_append(scope, aml_alias("PRQ2", "\\_SB.PRQ2"));
    aml_append(scope, aml_alias("PRQ3", "\\_SB.PRQ3"));

    qbus_build_aml(bus, scope);
}

static const Property via_isa_props[] = {
    DEFINE_PROP_BOOL("has-acpi", ViaISAState, has_acpi, true),
    DEFINE_PROP_BOOL("has-pic", ViaISAState, has_pic, true),
    DEFINE_PROP_BOOL("has-pit", ViaISAState, has_pit, true),
    DEFINE_PROP_BOOL("has-usb", ViaISAState, has_usb, true),
    DEFINE_PROP_BOOL("smm-enabled", ViaISAState, smm_enabled, false),
};

static void via_isa_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    AcpiDevAmlIfClass *adevc = ACPI_DEV_AML_IF_CLASS(klass);

    device_class_set_props(dc, via_isa_props);
    adevc->build_dev_aml = build_pci_isa_aml;
}

static const TypeInfo via_isa_info = {
    .name          = TYPE_VIA_ISA,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(ViaISAState),
    .instance_init = via_isa_init,
    .class_init    = via_isa_class_init,
    .abstract      = true,
    .interfaces    = (const InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { TYPE_ACPI_DEV_AML_IF },
        { },
    },
};

static PCIINTxRoute via_isa_get_pci_irq(void *opaque, int pin)
{
    const ViaISAState *s = opaque;
    PCIINTxRoute route;

    switch (pin) {
    case 0:
        route.irq = s->dev.config[0x55] >> 4;
        break;
    case 1:
        route.irq = s->dev.config[0x56] & 0xf;
        break;
    case 2:
        route.irq = s->dev.config[0x56] >> 4;
        break;
    case 3:
        route.irq = s->dev.config[0x57] >> 4;
        break;
    default:
        route.irq = 0;
        break;
    }

    route.mode = (route.irq == 0) ? PCI_INTX_DISABLED : PCI_INTX_ENABLED;

    return route;
}

void via_isa_set_irq(PCIDevice *d, int pin, int level)
{
    ViaISAState *s = VIA_ISA(pci_get_function_0(d));
    uint8_t irq = d->config[PCI_INTERRUPT_LINE], max_irq = 15;
    int f = PCI_FUNC(d->devfn);
    uint16_t mask;

    switch (f) {
    case 0: /* PIRQ/PINT inputs */
        irq = via_isa_get_pci_irq(s, pin).irq;
        f = 8 + pin; /* Use function 8-11 for PCI interrupt inputs */
        break;
    case 4: /* PM */
        irq = pci_get_byte(d->config + VIA_PM_SCI_SELECT_OFS)
              & VIA_PM_SCI_SELECT_MASK;
        break;
    case 2: /* USB ports 0-1 */
    case 3: /* USB ports 2-3 */
    case 5: /* AC97 audio */
        max_irq = 14;
        break;
    }

    /* Keep track of the state of all sources */
    mask = BIT(f);
    if (level) {
        s->irq_state[0] |= mask;
    } else {
        s->irq_state[0] &= ~mask;
    }
    if (irq == 0 || irq == 0xff) {
        return; /* disabled */
    }
    if (unlikely(irq > max_irq || irq == 2)) {
        qemu_log_mask(LOG_GUEST_ERROR, "Invalid ISA IRQ routing %d for %d",
                      irq, f);
        return;
    }
    /* Record source state at mapped IRQ */
    if (level) {
        s->irq_state[irq] |= mask;
    } else {
        s->irq_state[irq] &= ~mask;
    }
    /* Make sure there are no stuck bits if mapping has changed */
    s->irq_state[irq] &= s->irq_state[0];
    /* ISA IRQ level is the OR of all sources routed to it */
    qemu_set_irq(s->isa_irqs_in[irq], !!s->irq_state[irq]);
}

static void via_isa_pirq(void *opaque, int pin, int level)
{
    via_isa_set_irq(opaque, pin, level);
}

static void via_isa_request_i8259_irq(void *opaque, int irq, int level)
{
    ViaISAState *s = opaque;
    qemu_set_irq(s->cpu_intr, level);
}

static uint64_t via_rtc_read(void *opaque, hwaddr addr, unsigned size)
{
    ViaISAState *s = opaque;

    if ((addr & 1) == 0) {
        return 0xff;
    }

    return mc146818rtc_get_cmos_data(&s->rtc, s->rtc_index);
}

static void via_rtc_write(void *opaque, hwaddr addr, uint64_t data,
                          unsigned size)
{
    ViaISAState *s = opaque;

    if ((addr & 1) == 0) {
        s->rtc_index = data & (addr == 0 ? 0x7f : 0xff);
    } else if (s->rtc_index == RTC_REG_D) {
        PCIDevice *d = PCI_DEVICE(&s->pm);
        if (data & 0x80) {
            d->config[0x42] |= BIT(4);
        } else {
            d->config[0x42] &= ~BIT(4);
        }
    } else {
        mc146818rtc_set_cmos_data(&s->rtc, s->rtc_index, data);
    }
}

static const MemoryRegionOps via_rtc_ops = {
    .read = via_rtc_read,
    .write = via_rtc_write,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void via_isa_realize(PCIDevice *d, Error **errp)
{
    ViaISAState *s = VIA_ISA(d);
    DeviceState *dev = DEVICE(d);
    PCIBus *pci_bus = pci_get_bus(d);
    ISABus *isa_bus;
    int i;

    qdev_init_gpio_in_named(dev, via_isa_pirq, "pirq", PCI_NUM_PINS);
    isa_bus = isa_bus_new(dev, pci_address_space(d), pci_address_space_io(d),
                          errp);

    if (!isa_bus) {
        return;
    }

    /* PIC */
    if (s->has_pic) {
        qemu_init_irq(&s->i8259_irq, via_isa_request_i8259_irq, s, 0);
        qemu_irq *isa_irqs_in = i8259_init(isa_bus, &s->i8259_irq);

        for (i = 0; i < ISA_NUM_IRQS; i++) {
            s->isa_irqs_in[i] = isa_irqs_in[i];
        }

        g_free(isa_irqs_in);

        qdev_init_gpio_out_named(dev, &s->cpu_intr, "intr", 1);
    }

    isa_bus_register_input_irqs(isa_bus, s->isa_irqs_in);

    /* PIT */
    if (s->has_pit) {
        i8254_pit_init(isa_bus, 0x40, 0, NULL);
    }

    i8257_dma_init(OBJECT(d), isa_bus, 0);

    /* RTC */
    qdev_prop_set_int32(DEVICE(&s->rtc), "base_year", 2000);
    qdev_prop_set_bit(DEVICE(&s->rtc), "internal_io", false);
    if (!qdev_realize(DEVICE(&s->rtc), BUS(isa_bus), errp)) {
        return;
    }
    isa_connect_gpio_out(ISA_DEVICE(&s->rtc), 0, s->rtc.isairq);

    memory_region_init_io(&s->rtc_io, OBJECT(s), &via_rtc_ops, s, "rtc", 4);
    isa_register_ioport(ISA_DEVICE(&s->rtc), &s->rtc_io, s->rtc.io_base);

    /* register rtc 0x70 and 0x72 ports for coalesced_pio */
    memory_region_set_flush_coalesced(&s->rtc_io);
    memory_region_init_io(&s->rtc_coalesced_io, OBJECT(s), &via_rtc_ops,
                          s, "rtc-index", 1);
    memory_region_add_subregion(&s->rtc_io, 0, &s->rtc_coalesced_io);
    memory_region_add_coalescing(&s->rtc_coalesced_io, 0, 1);
    memory_region_add_coalescing(&s->rtc_coalesced_io, 2, 1);

    for (i = 0; i < PCI_CONFIG_HEADER_SIZE; i++) {
        if (i < PCI_COMMAND || i >= PCI_REVISION_ID) {
            d->wmask[i] = 0;
        }
    }

    /* Super I/O */
    if (!qdev_realize(DEVICE(&s->via_sio), BUS(isa_bus), errp)) {
        return;
    }

    /* Function 1: IDE */
    qdev_prop_set_int32(DEVICE(&s->ide), "addr", d->devfn + 1);
    if (!qdev_realize(DEVICE(&s->ide), BUS(pci_bus), errp)) {
        return;
    }
    for (i = 0; i < 2; i++) {
        qdev_connect_gpio_out_named(DEVICE(&s->ide), "isa-irq", i,
                                    s->isa_irqs_in[14 + i]);
    }

    /* Functions 2-3: USB Ports */
    for (i = 0; i < ARRAY_SIZE(s->uhci); i++) {
        if (s->has_usb) {
            qdev_prop_set_int32(DEVICE(&s->uhci[i]), "addr", d->devfn + 2 + i);
            if (!qdev_realize(DEVICE(&s->uhci[i]), BUS(pci_bus), errp)) {
                return;
            }
        } else {
            object_unparent(OBJECT(&s->uhci[i]));
        }
    }

    /* Function 4: Power Management */
    if (s->has_acpi) {
        qdev_prop_set_int32(DEVICE(&s->pm), "addr", d->devfn + 4);
        qdev_prop_set_bit(DEVICE(&s->pm), "smm-enabled", s->smm_enabled);
        if (!qdev_realize(DEVICE(&s->pm), BUS(pci_bus), errp)) {
            return;
        }
    } else {
        object_unparent(OBJECT(&s->pm));
    }

    /* Function 5: AC97 Audio */
    qdev_prop_set_int32(DEVICE(&s->ac97), "addr", d->devfn + 5);
    if (!qdev_realize(DEVICE(&s->ac97), BUS(pci_bus), errp)) {
        return;
    }

    /* Function 6: MC97 Modem */
    qdev_prop_set_int32(DEVICE(&s->mc97), "addr", d->devfn + 6);
    if (!qdev_realize(DEVICE(&s->mc97), BUS(pci_bus), errp)) {
        return;
    }

    pci_bus_irqs(pci_bus, via_isa_pirq, s, PCI_NUM_PINS);
    pci_bus_set_route_irq_fn(pci_bus, via_isa_get_pci_irq);
}

/* TYPE_VT82C686B_ISA */

static void vt82c686b_write_config(PCIDevice *d, uint32_t addr,
                                   uint32_t val, int len)
{
    ViaISAState *s = VIA_ISA(d);

    trace_via_isa_write(addr, val, len);

    if (range_covers_byte(addr, len, 0x47)) {
        if ((d->config[0x47] & BIT(7)) && (val & 1)) {
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        }
    }
    pci_default_write_config(d, addr, val, len);
    if (addr == 0x85) {
        /* BIT(1): enable or disable superio config io ports */
        via_superio_io_enable(&s->via_sio, val & BIT(1));
    } else if (ranges_overlap(addr, len, 0x55, 3)) {
        pci_bus_fire_intx_routing_notifier(pci_get_bus(d));
    }
}

static void vt82c686b_isa_reset(DeviceState *dev)
{
    ViaISAState *s = VIA_ISA(dev);
    uint8_t *pci_conf = s->dev.config;

    memset(pci_conf + PCI_CONFIG_HEADER_SIZE, 0,
           PCI_CONFIG_SPACE_SIZE - PCI_CONFIG_HEADER_SIZE);

    pci_set_long(pci_conf + PCI_CAPABILITY_LIST, 0x000000c0);
    pci_set_word(pci_conf + PCI_COMMAND, PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
                 PCI_COMMAND_MASTER | PCI_COMMAND_SPECIAL);
    pci_set_word(pci_conf + PCI_STATUS, PCI_STATUS_DEVSEL_MEDIUM);

    pci_conf[0x48] = 0x01; /* Miscellaneous Control 3 */
    pci_conf[0x4a] = 0x04; /* IDE interrupt Routing */
    pci_conf[0x4f] = 0x03; /* DMA/Master Mem Access Control 3 */
    pci_conf[0x50] = 0x2d; /* PnP DMA Request Control */
    pci_conf[0x59] = 0x04;
    pci_conf[0x5a] = 0x04; /* KBC/RTC Control*/
    pci_conf[0x5f] = 0x04;
    pci_conf[0x77] = 0x10; /* GPIO Control 1/2/3/4 */
}

static void vt82c686b_init(Object *obj)
{
    ViaISAState *s = VIA_ISA(obj);

    object_initialize_child(obj, "sio", &s->via_sio, TYPE_VT82C686B_SUPERIO);
    object_initialize_child(obj, "pm", &s->pm, TYPE_VT82C686B_PM);
}

static void vt82c686b_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = via_isa_realize;
    k->config_write = vt82c686b_write_config;
    k->vendor_id = PCI_VENDOR_ID_VIA;
    k->device_id = PCI_DEVICE_ID_VIA_82C686B_ISA;
    k->class_id = PCI_CLASS_BRIDGE_ISA;
    k->revision = 0x40;
    device_class_set_legacy_reset(dc, vt82c686b_isa_reset);
    dc->desc = "ISA bridge";
    dc->vmsd = &vmstate_via;
    /* Reason: part of VIA VT82C686 southbridge, needs to be wired up */
    dc->user_creatable = false;
}

static const TypeInfo vt82c686b_isa_info = {
    .name          = TYPE_VT82C686B_ISA,
    .parent        = TYPE_VIA_ISA,
    .instance_size = sizeof(ViaISAState),
    .instance_init = vt82c686b_init,
    .class_init    = vt82c686b_class_init,
};

/* TYPE_VT8231_ISA */

static void vt8231_write_config(PCIDevice *d, uint32_t addr,
                                uint32_t val, int len)
{
    ViaISAState *s = VIA_ISA(d);

    trace_via_isa_write(addr, val, len);
    pci_default_write_config(d, addr, val, len);
    if (addr == 0x50) {
        /* BIT(2): enable or disable superio config io ports */
        via_superio_io_enable(&s->via_sio, val & BIT(2));
    } else if (ranges_overlap(addr, len, 0x55, 3)) {
        pci_bus_fire_intx_routing_notifier(pci_get_bus(d));
    }
}

static void vt8231_isa_reset(DeviceState *dev)
{
    ViaISAState *s = VIA_ISA(dev);
    uint8_t *pci_conf = s->dev.config;

    memset(pci_conf + PCI_CONFIG_HEADER_SIZE, 0,
           PCI_CONFIG_SPACE_SIZE - PCI_CONFIG_HEADER_SIZE);

    pci_set_long(pci_conf + PCI_CAPABILITY_LIST, 0x000000c0);
    pci_set_word(pci_conf + PCI_COMMAND, PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
                 PCI_COMMAND_MASTER | PCI_COMMAND_SPECIAL);
    pci_set_word(pci_conf + PCI_STATUS, PCI_STATUS_DEVSEL_MEDIUM);

    pci_conf[0x4c] = 0x04; /* IDE interrupt Routing */
    pci_conf[0x58] = 0x40; /* Miscellaneous Control 0 */
    pci_conf[0x67] = 0x08; /* Fast IR Config */
    pci_conf[0x6b] = 0x01; /* Fast IR I/O Base */
}

static void vt8231_init(Object *obj)
{
    ViaISAState *s = VIA_ISA(obj);

    object_initialize_child(obj, "sio", &s->via_sio, TYPE_VT8231_SUPERIO);
    object_initialize_child(obj, "pm", &s->pm, TYPE_VT8231_PM);
}

static void vt8231_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = via_isa_realize;
    k->config_write = vt8231_write_config;
    k->vendor_id = PCI_VENDOR_ID_VIA;
    k->device_id = PCI_DEVICE_ID_VIA_8231_ISA;
    k->class_id = PCI_CLASS_BRIDGE_ISA;
    k->revision = 0x10;
    device_class_set_legacy_reset(dc, vt8231_isa_reset);
    dc->desc = "ISA bridge";
    dc->vmsd = &vmstate_via;
    /* Reason: part of VIA VT8231 southbridge, needs to be wired up */
    dc->user_creatable = false;
}

static const TypeInfo vt8231_isa_info = {
    .name          = TYPE_VT8231_ISA,
    .parent        = TYPE_VIA_ISA,
    .instance_size = sizeof(ViaISAState),
    .instance_init = vt8231_init,
    .class_init    = vt8231_class_init,
};


static void vt82c686b_register_types(void)
{
    type_register_static(&via_pm_info);
    type_register_static(&vt82c686b_pm_info);
    type_register_static(&vt8231_pm_info);
    type_register_static(&via_superio_info);
    type_register_static(&vt82c686b_superio_info);
    type_register_static(&vt8231_superio_info);
    type_register_static(&via_isa_info);
    type_register_static(&vt82c686b_isa_info);
    type_register_static(&vt8231_isa_info);
}

type_init(vt82c686b_register_types)
