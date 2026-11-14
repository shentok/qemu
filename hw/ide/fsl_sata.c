/*
 * QEMU Freescale SATA controller emulation
 *
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This is a model of the Freescale SATA controller found on e500v2 SoCs.
 * It implements the register interface used by the Linux sata_fsl driver
 * and executes ATA commands through the generic IDE core.
 */

#include "qemu/osdep.h"
#include "hw/core/irq.h"
#include "hw/core/sysbus.h"
#include "hw/ide/ide-bus.h"
#include "qemu/bswap.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "system/dma.h"
#include "system/memory.h"
#include "ide-internal.h"

#define TYPE_FSL_SATA "fsl,pq-sata-v2"
OBJECT_DECLARE_SIMPLE_TYPE(FslSataState, FSL_SATA)

#define FSL_SATA_MMIO_SIZE 0x1000

/* Host status register bits */
#define HSTATUS_ONLINE              (1u << 31)
#define HSTATUS_INT_ON_CMD_COMPLETE (1u << 0)
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

/* Command queue layout */
#define FSL_SATA_MAX_TAGS           16
#define FSL_SATA_CMD_HDR_SIZE       16
#define FSL_SATA_DESC_SFIS_OFFSET   32
#define FSL_SATA_DESC_PRDT_OFFSET   96
#define FSL_SATA_MAX_PRD            63

/* Command descriptor flags */
#define CMD_DESC_FPDMA_QUEUED       (1u << 8)

/* PRD entry flags */
#define PRDE_EXT_INDIRECT           (1u << 31)
#define PRDE_LENGTH_MASK            0x03ffffffu

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
    qemu_irq bus_irq;
    IDEBus bus;
    IDEDMA dma;

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

    /* Currently executing command */
    uint32_t cur_cda;
    uint32_t cur_prde_fis_len;
    uint32_t cur_tag;
    bool cur_write;
    bool cur_active;
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

static void fsl_sata_bus_irq(void *opaque, int n, int level)
{
    /*
     * The IDE core raises its bus IRQ for every command completion.  The
     * controller reports completions through HSTATUS instead, so absorb
     * the bus IRQ here.
     */
}

static bool fsl_sata_cmd_is_write(uint8_t cmd)
{
    switch (cmd) {
    case WIN_WRITE:
    case WIN_WRITE_ONCE:
    case WIN_WRITE_EXT:
    case WIN_WRITE_VERIFY:
    case WIN_MULTWRITE:
    case WIN_MULTWRITE_EXT:
    case WIN_WRITEDMA:
    case WIN_WRITEDMA_ONCE:
    case WIN_WRITEDMA_EXT:
    case WIN_WRITE_BUFFER:
        return true;
    default:
        return false;
    }
}

/*
 * Translate the PRDT of the current command into an IDE scatter/gather
 * list.  io_buffer_offset bytes at the beginning of the PRDT are skipped
 * so that repeated calls for multi-sector PIO transfers walk forward.
 */
static int32_t fsl_sata_map_prdt(FslSataState *s, int32_t limit)
{
    IDEState *ide = &s->bus.ifs[0];
    uint8_t prdt[FSL_SATA_MAX_PRD * 16];
    uint32_t num_prde = (s->cur_prde_fis_len >> 16) & 0xff;
    uint32_t skip = ide->io_buffer_offset;
    uint32_t mapped = 0;
    int i;

    if (limit <= 0) {
        return 0;
    }
    if (num_prde == 0 || num_prde > FSL_SATA_MAX_PRD) {
        return -1;
    }

    if (dma_memory_read(&address_space_memory,
                        s->cur_cda + FSL_SATA_DESC_PRDT_OFFSET,
                        prdt, num_prde * 16,
                        MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
        return -1;
    }

    qemu_sglist_init(&ide->sg, DEVICE(s), num_prde, &address_space_memory);

    for (i = 0; i < num_prde && mapped < (uint32_t)limit; i++) {
        uint32_t dba = ldl_le_p(prdt + i * 16);
        uint32_t ddc = ldl_le_p(prdt + i * 16 + 12);
        uint32_t len = ddc & PRDE_LENGTH_MASK;

        if (ddc & PRDE_EXT_INDIRECT) {
            uint8_t *ind = g_malloc(len);
            uint32_t j;

            if (dma_memory_read(&address_space_memory, dba, ind, len,
                                MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
                g_free(ind);
                goto fail;
            }
            for (j = 0; j + 16 <= len && mapped < (uint32_t)limit; j += 16) {
                uint32_t idba = ldl_le_p(ind + j);
                uint32_t iddc = ldl_le_p(ind + j + 12);
                uint32_t ilen = iddc & PRDE_LENGTH_MASK;
                uint32_t avail;

                if (skip >= ilen) {
                    skip -= ilen;
                    continue;
                }
                avail = MIN(ilen - skip, (uint32_t)limit - mapped);
                if (avail) {
                    qemu_sglist_add(&ide->sg, idba + skip, avail);
                    mapped += avail;
                }
                skip = 0;
            }
            g_free(ind);
        } else {
            uint32_t avail;

            if (skip >= len) {
                skip -= len;
                continue;
            }
            avail = MIN(len - skip, (uint32_t)limit - mapped);
            if (avail) {
                qemu_sglist_add(&ide->sg, dba + skip, avail);
                mapped += avail;
            }
            skip = 0;
        }
    }

    if (mapped != (uint32_t)limit) {
        goto fail;
    }

    ide->io_buffer_size = ide->sg.size;
    return ide->sg.size;

fail:
    qemu_sglist_destroy(&ide->sg);
    return -1;
}

static void fsl_sata_start_dma(const IDEDMA *dma, IDEState *ide,
                               BlockCompletionFunc *cb)
{
    ide->io_buffer_offset = 0;
    cb(ide, 0);
}

static int32_t fsl_sata_prepare_buf(const IDEDMA *dma, int32_t limit)
{
    FslSataState *s = container_of(dma, FslSataState, dma);

    return fsl_sata_map_prdt(s, limit);
}

static bool fsl_sata_pio_transfer(const IDEDMA *dma)
{
    FslSataState *s = container_of(dma, FslSataState, dma);
    IDEState *ide = &s->bus.ifs[0];
    uint32_t size = ide->data_end - ide->data_ptr;

    if (!s->cur_active) {
        return false;
    }

    if (size) {
        if (fsl_sata_map_prdt(s, size) < 0) {
            return false;
        }
        if (s->cur_write) {
            dma_buf_write(ide->data_ptr, size, NULL, &ide->sg,
                          MEMTXATTRS_UNSPECIFIED);
        } else {
            dma_buf_read(ide->data_ptr, size, NULL, &ide->sg,
                         MEMTXATTRS_UNSPECIFIED);
        }
        ide_dma_buf_commit(ide, size);
    }

    ide->data_ptr = ide->data_end;
    return true;
}

static void fsl_sata_cmd_done(const IDEDMA *dma)
{
    FslSataState *s = container_of(dma, FslSataState, dma);
    IDEState *ide = &s->bus.ifs[0];
    uint8_t sfis[32] = { 0 };

    if (!s->cur_active) {
        return;
    }

    sfis[0] = 0x34;             /* register - device to host */
    sfis[1] = 0x40;             /* interrupt bit */
    sfis[2] = ide->status;
    sfis[3] = ide->error;
    sfis[4] = ide->sector;
    sfis[5] = ide->lcyl;
    sfis[6] = ide->hcyl;
    sfis[7] = ide->select;
    sfis[8] = ide->hob_sector;
    sfis[9] = ide->hob_lcyl;
    sfis[10] = ide->hob_hcyl;
    sfis[12] = ide->nsector & 0xff;
    sfis[13] = (ide->nsector >> 8) & 0xff;

    dma_memory_write(&address_space_memory,
                     s->cur_cda + FSL_SATA_DESC_SFIS_OFFSET,
                     sfis, sizeof(sfis), MEMTXATTRS_UNSPECIFIED);

    s->cc |= 1u << s->cur_tag;
    s->ca &= ~(1u << s->cur_tag);
    s->hstatus |= HSTATUS_INT_ON_CMD_COMPLETE;
    s->cur_active = false;

    fsl_sata_update_irq(s);
}

static const IDEDMAOps fsl_sata_dma_ops = {
    .start_dma = fsl_sata_start_dma,
    .pio_transfer = fsl_sata_pio_transfer,
    .prepare_buf = fsl_sata_prepare_buf,
    .cmd_done = fsl_sata_cmd_done,
};

static void fsl_sata_complete_cmd(FslSataState *s, uint32_t tag)
{
    s->cc |= 1u << tag;
    s->hstatus |= HSTATUS_INT_ON_CMD_COMPLETE;
    fsl_sata_update_irq(s);
}

static void fsl_sata_exec_cmd(FslSataState *s, uint32_t tag)
{
    IDEState *ide = &s->bus.ifs[0];
    uint8_t hdr[FSL_SATA_CMD_HDR_SIZE];
    uint8_t cfis[32];
    uint32_t cda, prde_fis_len, desc_info;

    if (dma_memory_read(&address_space_memory,
                        s->chba + tag * FSL_SATA_CMD_HDR_SIZE,
                        hdr, sizeof(hdr), MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
        fsl_sata_complete_cmd(s, tag);
        return;
    }

    cda = ldl_le_p(hdr);
    prde_fis_len = ldl_le_p(hdr + 4);
    desc_info = ldl_le_p(hdr + 12);

    if (dma_memory_read(&address_space_memory, cda, cfis, sizeof(cfis),
                        MEMTXATTRS_UNSPECIFIED) != MEMTX_OK) {
        fsl_sata_complete_cmd(s, tag);
        return;
    }

    s->cur_cda = cda;
    s->cur_prde_fis_len = prde_fis_len;
    s->cur_tag = tag;

    if (cfis[2] == 0) {
        /*
         * Control-only FIS (software reset).  There is no data transfer
         * and the driver polls CQ until the command is retired.
         */
        fsl_sata_complete_cmd(s, tag);
        return;
    }

    if (desc_info & CMD_DESC_FPDMA_QUEUED) {
        qemu_log_mask(LOG_UNIMP, "%s: NCQ command 0x%02x unsupported\n",
                      __func__, cfis[2]);
        fsl_sata_complete_cmd(s, tag);
        return;
    }

    s->cur_write = fsl_sata_cmd_is_write(cfis[2]);
    s->cur_active = true;
    s->ca |= 1u << tag;

    ide->feature = cfis[3];
    ide->sector = cfis[4];
    ide->lcyl = cfis[5];
    ide->hcyl = cfis[6];
    ide->select = cfis[7];
    ide->hob_sector = cfis[8];
    ide->hob_lcyl = cfis[9];
    ide->hob_hcyl = cfis[10];
    ide->hob_feature = cfis[11];
    ide->nsector = (int64_t)((cfis[13] << 8) | cfis[12]);
    ide->error = 0;

    ide_bus_exec_cmd(&s->bus, cfis[2]);
}

static void fsl_sata_process_cq(FslSataState *s, uint32_t cq)
{
    int tag;

    for (tag = 0; tag < FSL_SATA_MAX_TAGS; tag++) {
        if (cq & (1u << tag)) {
            s->cq &= ~(1u << tag);
            fsl_sata_exec_cmd(s, tag);
        }
    }
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
        /*
         * Only report a device signature when a drive is actually
         * attached.  Otherwise the driver would probe a phantom device
         * on an empty port.
         */
        ret = s->bus.master ? FSL_SATA_ATA_SIGNATURE : 0;
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
        ret = s->bus.master ? s->sstatus : 0;
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
        fsl_sata_process_cq(s, (uint32_t)data);
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
    case FSL_SATA_HCONTROL: {
        bool was_reset = s->hcontrol & HCONTROL_ONLINE_PHY_RST;

        s->hcontrol = data;
        if (data & HCONTROL_FORCE_OFFLINE) {
            s->hstatus &= ~(HSTATUS_ONLINE | HSTATUS_PHYRDY_SIG);
        } else if ((data & HCONTROL_ONLINE_PHY_RST) && !was_reset) {
            /*
             * A fresh online/PHY reset raises the PHY ready and
             * signature update events.  Subsequent writes that keep
             * the reset bit set (for example when enabling interrupts
             * in sata_fsl_thaw()) must not raise them again, otherwise
             * the driver mistakes them for error interrupts.
             */
            s->hstatus |= HSTATUS_ONLINE | HSTATUS_PHYRDY_SIG;
        } else if (!(data & HCONTROL_ONLINE_PHY_RST)) {
            s->hstatus &= ~(HSTATUS_ONLINE | HSTATUS_PHYRDY_SIG);
        }
        break;
    }
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

    s->cur_cda = 0;
    s->cur_prde_fis_len = 0;
    s->cur_tag = 0;
    s->cur_write = false;
    s->cur_active = false;

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

    s->bus_irq = qemu_allocate_irqs(fsl_sata_bus_irq, s, 1)[0];
    ide_bus_init_output_irq(&s->bus, s->bus_irq);

    s->bus.dma = &s->dma;
    s->dma.ops = &fsl_sata_dma_ops;

    ide_bus_register_restart_cb(&s->bus);
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
