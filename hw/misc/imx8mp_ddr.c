/*
 * i.MX 8M Plus DDR Controller
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/imx8mp_ddr.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/units.h"

/* Register offsets used by the model, see the DWC DDR uMCTL2 documentation */
#define DDRC_STAT           0x004
#define DDRC_MRSTAT         0x018
#define DDRC_PWRCTL         0x030
#define DDRC_DFIMISC        0x1b0
#define DDRC_DFISTAT        0x1bc
#define DDRC_DBGCAM         0x308
#define DDRC_DBGSTAT        0x310
#define DDRC_SWCTL          0x320
#define DDRC_SWSTAT         0x324
#define DDRC_PSTAT          0x3fc

/* Performance monitor block, mapped at 0x3d800000 */
#define DDRC_PERF_MRR0_DAT  0x040

/* STAT.operating_mode value while the controller is in self-refresh */
#define DDRC_STAT_SELFREFRESH   0x23
#define DDRC_STAT_NORMAL        0x01

/* PWRCTL.selfref_sw requests a software self-refresh entry */
#define DDRC_PWRCTL_SELFREF_SW  BIT(5)
/* PWRCTL.stay_in_selfref keeps the controller in self-refresh */
#define DDRC_PWRCTL_STAY_SR     BIT(6)

/* DFIMISC.dfi_init_start triggers a DFI initialization handshake */
#define DDRC_DFIMISC_INIT_START BIT(5)

/* DBGCAM data pipeline empty bits, DDR4 uses 26:25, LPDDR4 uses 29:28 */
#define DDRC_DBGCAM_QUEUES_EMPTY 0x36000000

static void imx8mp_ddr_log_bad_access(const char *func, hwaddr offset)
{
    qemu_log_mask(LOG_UNIMP, "[%s]%s: Bad register at offset 0x%"
                  HWADDR_PRIx "\n", TYPE_IMX8MP_DDR, func, offset);
}

static uint32_t imx8mp_ddr_stat(FslImx8mpDdrState *s)
{
    uint32_t pwrctl = s->regs[DDRC_PWRCTL / sizeof(uint32_t)];
    uint32_t selfref_state;

    /*
     * STAT.selfref_state encodes the self-refresh sub-state selected by
     * PWRCTL.selfref_sw and PWRCTL.stay_in_selfref. TF-A sequences through
     * these states during an LPDDR4 frequency change.
     */
    if (pwrctl & DDRC_PWRCTL_SELFREF_SW) {
        selfref_state = pwrctl & DDRC_PWRCTL_STAY_SR ? 0x1 : 0x2;
    } else {
        selfref_state = pwrctl & DDRC_PWRCTL_STAY_SR ? 0x3 : 0x0;
    }

    return (s->regs[DDRC_STAT / sizeof(uint32_t)] & ~0x3ff) |
           (selfref_state << 8) |
           (pwrctl & (DDRC_PWRCTL_SELFREF_SW | DDRC_PWRCTL_STAY_SR) ?
            DDRC_STAT_SELFREFRESH : DDRC_STAT_NORMAL);
}

static uint64_t fsl_imx8mp_ddr_read(void *opaque, hwaddr offset,
                                    unsigned size)
{
    FslImx8mpDdrState *s = opaque;
    uint32_t value;

    if (offset >= sizeof_field(FslImx8mpDdrState, regs)) {
        imx8mp_ddr_log_bad_access(__func__, offset);
        return 0;
    }

    switch (offset) {
    case DDRC_STAT:
        value = imx8mp_ddr_stat(s);
        break;
    case DDRC_MRSTAT:
        /* Mode register accesses complete immediately */
        value = 0;
        break;
    case DDRC_DFISTAT:
        value = s->regs[offset / sizeof(uint32_t)];
        /*
         * A pending DFI initialization completes synchronously. Report the
         * handshake as still in progress for this read so that firmware
         * polling for dfi_init_complete to clear observes the transition,
         * and let the following read see the completed initialization.
         */
        s->regs[offset / sizeof(uint32_t)] = BIT(0);
        break;
    case DDRC_DBGCAM:
        /* Read and write data pipelines are always empty */
        value = DDRC_DBGCAM_QUEUES_EMPTY;
        break;
    case DDRC_DBGSTAT:
        /* Debug commands complete immediately */
        value = 0;
        break;
    case DDRC_SWSTAT:
        /* SWSTAT.sw_done_ack acknowledges SWCTL.sw_done */
        value = s->regs[DDRC_SWCTL / sizeof(uint32_t)] & BIT(0);
        break;
    case DDRC_PSTAT:
        /* All AXI ports are idle */
        value = 0;
        break;
    default:
        value = s->regs[offset / sizeof(uint32_t)];
        break;
    }

    return value;
}

static void fsl_imx8mp_ddr_write(void *opaque, hwaddr offset, uint64_t value,
                                 unsigned size)
{
    FslImx8mpDdrState *s = opaque;

    if (offset >= sizeof_field(FslImx8mpDdrState, regs)) {
        imx8mp_ddr_log_bad_access(__func__, offset);
        return;
    }

    s->regs[offset / sizeof(uint32_t)] = value;

    switch (offset) {
    case DDRC_DFIMISC:
        /*
         * Model the DFI initialization handshake: starting an
         * initialization clears dfi_init_complete, finishing it sets the
         * bit again. TF-A polls both transitions during a frequency change.
         */
        s->regs[DDRC_DFISTAT / sizeof(uint32_t)] =
            value & DDRC_DFIMISC_INIT_START ? 0 : BIT(0);
        break;
    }
}

static uint64_t fsl_imx8mp_ddr_perf_read(void *opaque, hwaddr offset,
                                         unsigned size)
{
    FslImx8mpDdrState *s = opaque;

    if (offset >= sizeof_field(FslImx8mpDdrState, perf_regs)) {
        imx8mp_ddr_log_bad_access(__func__, offset);
        return 0;
    }

    if (offset == DDRC_PERF_MRR0_DAT) {
        /* Mode register read data is always available */
        return s->perf_regs[offset / sizeof(uint32_t)] | BIT(3);
    }

    return s->perf_regs[offset / sizeof(uint32_t)];
}

static void fsl_imx8mp_ddr_perf_write(void *opaque, hwaddr offset,
                                      uint64_t value, unsigned size)
{
    FslImx8mpDdrState *s = opaque;

    if (offset >= sizeof_field(FslImx8mpDdrState, perf_regs)) {
        imx8mp_ddr_log_bad_access(__func__, offset);
        return;
    }

    s->perf_regs[offset / sizeof(uint32_t)] = value;
}

static const struct MemoryRegionOps imx8mp_ddr_ops = {
    .read = fsl_imx8mp_ddr_read,
    .write = fsl_imx8mp_ddr_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        /*
         * Our device would not work correctly if the guest was doing
         * unaligned access. This might not be a limitation on the real
         * device but in practice there is no reason for a guest to access
         * this device unaligned.
         */
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static const struct MemoryRegionOps imx8mp_ddr_perf_ops = {
    .read = fsl_imx8mp_ddr_perf_read,
    .write = fsl_imx8mp_ddr_perf_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void imx8mp_ddr_reset(DeviceState *dev)
{
    FslImx8mpDdrState *s = IMX8MP_DDR(dev);

    memset(s->regs, 0, sizeof(s->regs));
    memset(s->perf_regs, 0, sizeof(s->perf_regs));

    /* DFI initialization is complete after reset */
    s->regs[DDRC_DFISTAT / sizeof(uint32_t)] = BIT(0);
}

static void imx8mp_ddr_realize(DeviceState *dev, Error **errp)
{
    FslImx8mpDdrState *s = IMX8MP_DDR(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &imx8mp_ddr_ops, s,
                          TYPE_IMX8MP_DDR, 4 * MiB);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);

    memory_region_init_io(&s->iomem_perf, OBJECT(dev), &imx8mp_ddr_perf_ops,
                          s, TYPE_IMX8MP_DDR ".perf", 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem_perf);
}

static const VMStateDescription vmstate_imx8mp_ddr = {
    .name = TYPE_IMX8MP_DDR,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, FslImx8mpDdrState, IMX8MP_DDR_NUM_REGS),
        VMSTATE_UINT32_ARRAY(perf_regs, FslImx8mpDdrState,
                             IMX8MP_DDR_PERF_NUM_REGS),
        VMSTATE_END_OF_LIST()
    },
};

static void imx8mp_ddr_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, imx8mp_ddr_reset);
    dc->realize = imx8mp_ddr_realize;
    dc->vmsd = &vmstate_imx8mp_ddr;
    dc->desc = "i.MX 8M Plus DDR Controller";
}

static const TypeInfo imx8mp_ddr_types[] = {
    {
        .name          = TYPE_IMX8MP_DDR,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FslImx8mpDdrState),
        .class_init    = imx8mp_ddr_class_init,
    },
};

DEFINE_TYPES(imx8mp_ddr_types)
