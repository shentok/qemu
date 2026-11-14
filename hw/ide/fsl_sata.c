/*
 * QEMU Freescale SATA controller emulation
 *
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This is a minimal model of the Freescale SATA controller found on
 * e500v2 SoCs.  It implements the register interface needed by the
 * Linux sata_fsl driver to probe the controller and to perform a
 * successful hardreset/softreset.  Command processing is added in a
 * follow-up change.
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/core/sysbus.h"
#include "hw/ide/ide-bus.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "ide-internal.h"

#define TYPE_FSL_SATA "fsl,pq-sata-v2"
OBJECT_DECLARE_SIMPLE_TYPE(FslSataState, FSL_SATA)

#define FSL_SATA_MMIO_SIZE 0x1000

/* Host status register bits */
#define HSTATUS_ONLINE              (1u << 31)
#define HSTATUS_INT_ON_CMD_COMPLETE (1u << 0)
#define HSTATUS_IRQ_MASK            0x3fu
#define HSTATUS_PHYRDY_SIG          0x18u

/* Host control register bits */
#define HCONTROL_ONLINE_PHY_RST     (1u << 31)
#define HCONTROL_FORCE_OFFLINE      (1u << 30)

/* SATA superset register bits */
#define SSTATUS_DET_DEV_PRESENT     0x3u
#define SSTATUS_SPD_GEN1            0x1u
#define SSTATUS_IPM_ACTIVE          0x1u
#define SSTATUS_RESET_VALUE         ((SSTATUS_DET_DEV_PRESENT << 0) | \
                                     (SSTATUS_SPD_GEN1 << 4) |       \
                                     (SSTATUS_IPM_ACTIVE << 8))

/* ATA device signature (LBA mid = 0, LBA low = 1, sector count = 1) */
#define FSL_SATA_ATA_SIGNATURE      0x00000101u

enum {
    FSL_SATA_CQ        = 0x00,
    FSL_SATA_CA        = 0x08,
    FSL_SATA_CC        = 0x10,
    FSL_SATA_CE        = 0x18,
    FSL_SATA_DE        = 0x20,
    FSL_SATA_CHBA      = 0x24,
    FSL_SATA_HSTATUS   = 0x28,
    FSL_SATA_HCONTROL  = 0x2c,
    FSL_SATA_CQPMP     = 0x30,
    FSL_SATA_SIGNATURE = 0x34,
    FSL_SATA_ICC       = 0x38,

    FSL_SATA_SSTATUS   = 0x100,
    FSL_SATA_SERROR    = 0x104,
    FSL_SATA_SCONTROL  = 0x108,
    FSL_SATA_SNOTIFY   = 0x10c,

    FSL_SATA_TRANSCFG  = 0x140,
    FSL_SATA_CSR_END   = 0x180,
};

struct FslSataState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    qemu_irq irq;
    IDEBus bus;

    uint32_t cq;
    uint32_t ca;
    uint32_t cc;
    uint32_t ce;
    uint32_t de;
    uint32_t chba;
    uint32_t hstatus;
    uint32_t hcontrol;
    uint32_t cqpmp;
    uint32_t icc;

    uint32_t sstatus;
    uint32_t serror;
    uint32_t scontrol;
    uint32_t snotify;

    uint32_t csr[(FSL_SATA_CSR_END - FSL_SATA_TRANSCFG) / 4];
};

static void fsl_sata_update_irq(FslSataState *s)
{
    /*
     * Only command completion is delivered as an interrupt.  The PHY
     * ready/signature bits are polled by the driver during reset and
     * must not trigger the interrupt handler early.
     */
    qemu_set_irq(s->irq,
                 (s->hstatus & HSTATUS_INT_ON_CMD_COMPLETE) != 0);
}

static uint64_t fsl_sata_read(void *opaque, hwaddr addr, unsigned size)
{
    FslSataState *s = opaque;
    uint64_t ret = 0;

    switch (addr) {
    case FSL_SATA_CQ:
        ret = s->cq;
        break;
    case FSL_SATA_CA:
        ret = s->ca;
        break;
    case FSL_SATA_CC:
        ret = s->cc;
        break;
    case FSL_SATA_CE:
        ret = s->ce;
        break;
    case FSL_SATA_DE:
        ret = s->de;
        break;
    case FSL_SATA_CHBA:
        ret = s->chba;
        break;
    case FSL_SATA_HSTATUS:
        ret = s->hstatus;
        break;
    case FSL_SATA_HCONTROL:
        ret = s->hcontrol;
        break;
    case FSL_SATA_CQPMP:
        ret = s->cqpmp;
        break;
    case FSL_SATA_SIGNATURE:
        ret = FSL_SATA_ATA_SIGNATURE;
        /*
         * The driver polls the PHY ready/signature bits during reset and
         * then reads SIGNATURE to classify the device.  Once that read
         * has happened the reset is over, so drop the polled bits.
         */
        s->hstatus &= ~HSTATUS_PHYRDY_SIG;
        fsl_sata_update_irq(s);
        break;
    case FSL_SATA_ICC:
        ret = s->icc;
        break;
    case FSL_SATA_SSTATUS:
        ret = s->sstatus;
        break;
    case FSL_SATA_SERROR:
        ret = s->serror;
        break;
    case FSL_SATA_SCONTROL:
        ret = s->scontrol;
        break;
    case FSL_SATA_SNOTIFY:
        ret = s->snotify;
        break;
    default:
        if (addr >= FSL_SATA_TRANSCFG && addr < FSL_SATA_CSR_END) {
            ret = s->csr[(addr - FSL_SATA_TRANSCFG) / 4];
        } else {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: bad read at offset 0x%" HWADDR_PRIx "\n",
                          __func__, addr);
        }
        break;
    }

    return ret;
}

static void fsl_sata_write(void *opaque, hwaddr addr, uint64_t data,
                           unsigned size)
{
    FslSataState *s = opaque;

    switch (addr) {
    case FSL_SATA_CQ:
        /*
         * Writing a bit to CQ queues the corresponding command.  The
         * model completes it immediately by moving the bit to CC.
         */
        s->cq &= ~(uint32_t)data;
        s->cc |= (uint32_t)data;
        s->hstatus |= HSTATUS_INT_ON_CMD_COMPLETE;
        break;
    case FSL_SATA_CA:
        s->ca = data;
        break;
    case FSL_SATA_CC:
        s->cc &= ~(uint32_t)data;
        break;
    case FSL_SATA_CE:
        s->ce = data;
        break;
    case FSL_SATA_DE:
        s->de = data;
        break;
    case FSL_SATA_CHBA:
        s->chba = data;
        break;
    case FSL_SATA_HSTATUS:
        s->hstatus &= ~(uint32_t)data;
        break;
    case FSL_SATA_HCONTROL:
        s->hcontrol = data;
        if (data & HCONTROL_FORCE_OFFLINE) {
            s->hstatus &= ~(HSTATUS_ONLINE | HSTATUS_PHYRDY_SIG);
        } else if (data & HCONTROL_ONLINE_PHY_RST) {
            s->hstatus |= HSTATUS_ONLINE | HSTATUS_PHYRDY_SIG;
        } else {
            s->hstatus &= ~(HSTATUS_ONLINE | HSTATUS_PHYRDY_SIG);
        }
        break;
    case FSL_SATA_CQPMP:
        s->cqpmp = data;
        break;
    case FSL_SATA_SIGNATURE:
        /* read-only */
        break;
    case FSL_SATA_ICC:
        s->icc = data;
        break;
    case FSL_SATA_SSTATUS:
        /* read-only */
        break;
    case FSL_SATA_SERROR:
        s->serror = 0;
        break;
    case FSL_SATA_SCONTROL:
        s->scontrol = data;
        break;
    case FSL_SATA_SNOTIFY:
        s->snotify = data;
        break;
    default:
        if (addr >= FSL_SATA_TRANSCFG && addr < FSL_SATA_CSR_END) {
            s->csr[(addr - FSL_SATA_TRANSCFG) / 4] = data;
        } else {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: bad write at offset 0x%" HWADDR_PRIx
                          " value 0x%" PRIx64 "\n",
                          __func__, addr, data);
        }
        break;
    }

    fsl_sata_update_irq(s);
}

static const MemoryRegionOps fsl_sata_ops = {
    .read = fsl_sata_read,
    .write = fsl_sata_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void fsl_sata_reset_hold(Object *obj, ResetType type)
{
    FslSataState *s = FSL_SATA(obj);

    s->cq = 0;
    s->ca = 0;
    s->cc = 0;
    s->ce = 0;
    s->de = 0;
    s->chba = 0;
    s->hstatus = 0;
    s->hcontrol = 0;
    s->cqpmp = 0;
    s->icc = 0;

    s->sstatus = SSTATUS_RESET_VALUE;
    s->serror = 0;
    s->scontrol = 0;
    s->snotify = 0;

    memset(s->csr, 0, sizeof(s->csr));

    fsl_sata_update_irq(s);
}

static void fsl_sata_init(Object *obj)
{
    FslSataState *s = FSL_SATA(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &fsl_sata_ops, s,
                          "fsl-sata", FSL_SATA_MMIO_SIZE);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->irq);

    ide_bus_init(&s->bus, sizeof(s->bus), DEVICE(obj), 0, 1);
}

static void fsl_sata_realize(DeviceState *dev, Error **errp)
{
    FslSataState *s = FSL_SATA(dev);

    ide_bus_init_output_irq(&s->bus, s->irq);
}

static void fsl_sata_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = fsl_sata_realize;
    dc->desc = "Freescale SATA controller";
    rc->phases.hold = fsl_sata_reset_hold;
}

static const TypeInfo fsl_sata_types[] = {
    {
        .name          = TYPE_FSL_SATA,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FslSataState),
        .instance_init = fsl_sata_init,
        .class_init    = fsl_sata_class_init,
    },
    {
        .name          = "fsl,pq-sata",
        .parent        = TYPE_FSL_SATA,
    },
};

DEFINE_TYPES(fsl_sata_types)
