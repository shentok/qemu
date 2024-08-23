/*
 * MPC eSPI Controller
 *
 * Copyright (c) 2022 Bernhard Beschow <shentey@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "hw/irq.h"
#include "migration/vmstate.h"
#include "qemu/bitops.h"
#include "qemu/fifo8.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qom/object.h"

#include "trace.h"

/* eSPI Controller registers */
enum ESPIRegister {
    ESPI_SPMODE  =  0, /* eSPI mode register */
    ESPI_SPIE    =  1, /* eSPI event register */
    ESPI_SPIM    =  2, /* eSPI mask register */
    ESPI_SPCOM   =  3, /* eSPI command register */
    ESPI_SPITF   =  4, /* eSPI transmit FIFO access register */
    ESPI_SPIRF   =  5, /* eSPI receive FIFO access register */
    ESPI_SPMODE0 =  8, /* eSPI cs0 mode register */
    ESPI_SPMODE1 =  9, /* eSPI cs1 mode register */
    ESPI_SPMODE2 = 10, /* eSPI cs2 mode register */
    ESPI_SPMODE3 = 11  /* eSPI cs3 mode register */
};

#define ESPI_REGS_SIZE 12

/* SPMODE register definitions */
#define ESPI_SPMODE_ENABLE BIT(31)
#define ESPI_SPMODE_LOOP BIT(30)
#define ESPI_SPMODE_OD BIT(29)
#define ESPI_SPMODE_TXTHR_SHIFT 8
#define ESPI_SPMODE_TXTHR_LENGTH 6
#define ESPI_SPMODE_RXTHR_SHIFT 0
#define ESPI_SPMODE_RXTHR_LENGTH 5

/* SPIE register values */
#define ESPI_SPIE_RXCNT_SHIFT 24
#define ESPI_SPIE_RXCNT_LENGTH 6
#define ESPI_SPIE_TXCNT_SHIFT 16
#define ESPI_SPIE_TXCNT_LENGTH 6
#define ESPI_SPIE_TXE BIT(15)    /* TX FIFO empty */
#define ESPI_SPIE_DON BIT(14)    /* TX done */
#define ESPI_SPIE_RXT BIT(13)    /* RX FIFO threshold */
#define ESPI_SPIE_RXF BIT(12)    /* RX FIFO full */
#define ESPI_SPIE_TXT BIT(11)    /* TX FIFO threshold*/
#define ESPI_SPIE_RNE BIT(9)     /* RX FIFO not empty */
#define ESPI_SPIE_TNF BIT(8)     /* TX FIFO not full */

/* SPIM register values */
#define ESPI_SPIM_TXE BIT(15)    /* TX FIFO empty */
#define ESPI_SPIM_DON BIT(14)    /* TX done */
#define ESPI_SPIM_RXT BIT(13)    /* RX FIFO threshold */
#define ESPI_SPIM_RXF BIT(12)    /* RX FIFO full */
#define ESPI_SPIM_TXT BIT(11)    /* TX FIFO threshold*/
#define ESPI_SPIM_RNE BIT(9)     /* RX FIFO not empty */
#define ESPI_SPIM_TNF BIT(8)     /* TX FIFO not full */

/* SPCOM register values */
#define SPCOM_CS_SHIFT 30
#define SPCOM_CS_LENGTH 2
#define SPCOM_DO BIT(28)         /* Dual output */
#define SPCOM_TO BIT(27)         /* TX only */
#define SPCOM_RXSKIP_SHIFT 16
#define SPCOM_RXSKIP_LENGTH 8
#define SPCOM_TRANLEN_SHIFT 0
#define SPCOM_TRANLEN_LENGTH 16

/* CS mode register definitions */
#define CSMODE_CI_INACTIVEHIGH BIT(31)
#define CSMODE_REV BIT(29)
#define CSMODE_ODD BIT(23)
#define CSMODE_POL_1 BIT(20)
#define CSMODE_LEN_SHIFT 16
#define CSMODE_LEN_LENGTH 4

#define FSL_ESPI_FIFO_SIZE 32

#define EXTRACT(value, name) extract32(value, name##_SHIFT, name##_LENGTH)

#define TYPE_MPC_ESPI "mpc-espi"
OBJECT_DECLARE_SIMPLE_TYPE(MPCESPIState, MPC_ESPI)

struct MPCESPIState {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion iomem;

    qemu_irq irq;

    qemu_irq cs_lines[4];

    SSIBus *bus;

    uint32_t regs[ESPI_REGS_SIZE];

    Fifo8 rx_fifo;
    Fifo8 tx_fifo;

    int16_t burst_length;
    uint8_t rx_skip;
};

static const char *mpc_espi_reg_name(uint32_t reg)
{
    static char unknown[20];

    switch (reg) {
    case ESPI_SPMODE:
        return  "SPMODE";
    case ESPI_SPIE:
        return  "SPIE";
    case ESPI_SPIM:
        return  "SPIM";
    case ESPI_SPCOM:
        return  "SPCOM";
    case ESPI_SPITF:
        return  "SPITF";
    case ESPI_SPIRF:
        return  "SPIRF";
    case ESPI_SPMODE0:
        return  "SPMODE0";
    case ESPI_SPMODE1:
        return  "SPMODE1";
    case ESPI_SPMODE2:
        return  "SPMODE2";
    case ESPI_SPMODE3:
        return  "SPMODE3";
    default:
        sprintf(unknown, "%u ?", reg);
        return unknown;
    }
}

static const VMStateDescription vmstate_mpc_espi = {
    .name = TYPE_MPC_ESPI,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_FIFO8(tx_fifo, MPCESPIState),
        VMSTATE_FIFO8(rx_fifo, MPCESPIState),
        VMSTATE_INT16(burst_length, MPCESPIState),
        VMSTATE_UINT8(rx_skip, MPCESPIState),
        VMSTATE_UINT32_ARRAY(regs, MPCESPIState, ESPI_REGS_SIZE),
        VMSTATE_END_OF_LIST()
    },
};

static void mpc_espi_tx_fifo_push(MPCESPIState *s, uint32_t value)
{
    const uint32_t txthr = EXTRACT(s->regs[ESPI_SPMODE], ESPI_SPMODE_TXTHR);

    fifo8_push(&s->tx_fifo, value);

    s->regs[ESPI_SPIE] &= ~ESPI_SPIE_TXE;

    if (fifo8_is_full(&s->tx_fifo)) {
        s->regs[ESPI_SPIE] &= ~ESPI_SPIE_TNF;
    }

    if (fifo8_num_used(&s->tx_fifo) >= txthr) {
        s->regs[ESPI_SPIE] &= ~ESPI_SPIE_TXT;
    }
}

static uint32_t mpc_espi_tx_fifo_pop(MPCESPIState *s)
{
    const uint32_t txthr = EXTRACT(s->regs[ESPI_SPMODE], ESPI_SPMODE_TXTHR);
    const uint32_t old_events = s->regs[ESPI_SPIE] & s->regs[ESPI_SPIM];
    const uint32_t old_num_used = fifo8_num_used(&s->tx_fifo);
    uint32_t val;
    const bool was_empty = fifo8_is_empty(&s->tx_fifo);
    const bool was_full = fifo8_is_full(&s->tx_fifo);

    val = fifo8_pop(&s->tx_fifo);

    if (!was_empty && fifo8_is_empty(&s->tx_fifo)) {
        s->regs[ESPI_SPIE] |= ESPI_SPIE_TXE;
    }

    if (was_full && !fifo8_is_full(&s->tx_fifo)) {
        s->regs[ESPI_SPIE] |= ESPI_SPIE_TNF;
    }

    if (old_num_used >= txthr && fifo8_num_used(&s->tx_fifo) < txthr) {
        s->regs[ESPI_SPIE] |= ESPI_SPIE_TXT;
    }

    if (old_events != (s->regs[ESPI_SPIE] & s->regs[ESPI_SPIM])) {
        qemu_set_irq(s->irq, 1);
    }

    return val;
}

static void mpc_espi_rx_fifo_push(MPCESPIState *s, uint32_t value)
{
    const uint32_t old_events = s->regs[ESPI_SPIE] & s->regs[ESPI_SPIM];
    const uint32_t rxthr = EXTRACT(s->regs[ESPI_SPMODE], ESPI_SPMODE_RXTHR);
    const uint32_t old_num_used = fifo8_num_used(&s->rx_fifo);
    const bool was_empty = fifo8_is_empty(&s->rx_fifo);
    const bool was_full = fifo8_is_full(&s->rx_fifo);

    fifo8_push(&s->rx_fifo, value);

    if (was_empty && !fifo8_is_empty(&s->rx_fifo)) {
        s->regs[ESPI_SPIE] |= ESPI_SPIE_RNE;
    }

    if (!was_full && fifo8_is_full(&s->rx_fifo)) {
        s->regs[ESPI_SPIE] |= ESPI_SPIE_RXF;
    }

    if (old_num_used <= rxthr && fifo8_num_used(&s->rx_fifo) > rxthr) {
        s->regs[ESPI_SPIE] |= ESPI_SPIE_RXT;
    }

    if (old_events != (s->regs[ESPI_SPIE] & s->regs[ESPI_SPIM])) {
        qemu_set_irq(s->irq, 1);
    }
}

static uint32_t mpc_espi_rx_fifo_pop(MPCESPIState *s)
{
    const uint32_t rxthr = EXTRACT(s->regs[ESPI_SPMODE], ESPI_SPMODE_RXTHR);
    uint32_t val;

    val = fifo8_pop(&s->rx_fifo);

    if (fifo8_is_empty(&s->rx_fifo)) {
        s->regs[ESPI_SPIE] &= ~ESPI_SPIE_RNE;
    }

    s->regs[ESPI_SPIE] &= ~ESPI_SPIE_RXF;

    if (fifo8_num_used(&s->rx_fifo) <= rxthr) {
        s->regs[ESPI_SPIE] &= ~ESPI_SPIE_RXT;
    }

    return val;
}

static bool mpc_espi_is_enabled(MPCESPIState *s)
{
    return s->regs[ESPI_SPMODE] & ESPI_SPMODE_ENABLE;
}

static void mpc_espi_flush_txfifo(MPCESPIState *s)
{
    uint32_t tx;
    uint32_t rx;

    while (!fifo8_is_empty(&s->tx_fifo) && s->burst_length > 0) {
        tx = mpc_espi_tx_fifo_pop(s);

        /* We need to write one byte at a time */
        rx = ssi_transfer(s->bus, tx);

        if (s->regs[ESPI_SPMODE] & ESPI_SPMODE_LOOP) {
            rx = tx;
        }

        if (s->rx_skip == 0) {
            mpc_espi_rx_fifo_push(s, rx);
        }

        /* Remove 8 bits from the actual burst */
        s->burst_length--;
        if (s->rx_skip > 0) {
            s->rx_skip--;
        }

        if (s->burst_length <= 0 && (!(s->regs[ESPI_SPIE] & ESPI_SPIE_DON))) {
            s->regs[ESPI_SPIE] |= ESPI_SPIE_DON;
            qemu_set_irq(s->irq, 1);
        }
    }
}

static void mpc_espi_common_reset(MPCESPIState *s)
{
    s->regs[ESPI_SPMODE]  = 0x0000100f;
    s->regs[ESPI_SPIE]    = 0x00208900;
    s->regs[ESPI_SPIM]    = 0;
    s->regs[ESPI_SPCOM]   = 0;
    s->regs[ESPI_SPITF]   = 0;
    s->regs[ESPI_SPIRF]   = 0;
    s->regs[ESPI_SPMODE0] = 0x00100000;
    s->regs[ESPI_SPMODE1] = 0x00100000;
    s->regs[ESPI_SPMODE2] = 0x00100000;
    s->regs[ESPI_SPMODE3] = 0x00100000;

    fifo8_reset(&s->tx_fifo);
    fifo8_reset(&s->rx_fifo);

    s->burst_length = 0;
    s->rx_skip = 0;
}

static void mpc_espi_soft_reset(MPCESPIState *s)
{
    int i;

    mpc_espi_common_reset(s);

    qemu_set_irq(s->irq, 0);

    for (i = 0; i < ARRAY_SIZE(s->cs_lines); i++) {
        qemu_set_irq(s->cs_lines[i], 1);
    }
}

static void mpc_espi_reset(DeviceState *dev)
{
    MPCESPIState *s = MPC_ESPI(dev);

    mpc_espi_common_reset(s);
}

static uint64_t mpc_espi_read(void *opaque, hwaddr offset, unsigned size)
{
    uint32_t value = 0;
    MPCESPIState *s = opaque;
    uint32_t index = offset >> 2;

    if (index >= ESPI_REGS_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_MPC_ESPI, __func__, offset);
        return 0;
    }

    value = 0;

    switch (index) {
    case ESPI_SPIRF:
        if (mpc_espi_is_enabled(s)) {
            for (int i = 0; i < size; ++i) {
                if (fifo8_is_empty(&s->rx_fifo)) {
                    /* value is undefined */
                    value = 0xdeadbeef;
                } else {
                    /* read from the RX FIFO */
                    value = (value << 8) | mpc_espi_rx_fifo_pop(s);
                }
            }
        } else {
            value = 0xdeadbeef;
        }

        break;
    case ESPI_SPITF:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "[%s]%s: Trying to read from TX FIFO\n",
                      TYPE_MPC_ESPI, __func__);

        /* Reading from TXDATA gives 0 */
        break;
    case ESPI_SPIE:
        value = s->regs[ESPI_SPIE] & 0xc0c0ffff;
        value |= fifo8_num_used(&s->rx_fifo) << ESPI_SPIE_RXCNT_SHIFT;
        value |= fifo8_num_free(&s->tx_fifo) << ESPI_SPIE_TXCNT_SHIFT;

        break;
    default:
        value = s->regs[index];
        break;
    }

    trace_mpc_espi_read(s->iomem.name, mpc_espi_reg_name(index), size, value);

    return (uint64_t)value;
}

static void mpc_espi_write(void *opaque, hwaddr offset, uint64_t value,
                          unsigned size)
{
    MPCESPIState *s = opaque;
    uint32_t index = offset >> 2;
    int i;

    if (index >= ESPI_REGS_SIZE) {
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Bad register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_MPC_ESPI, __func__, offset);
        return;
    }

    trace_mpc_espi_write(s->iomem.name, mpc_espi_reg_name(index), size, value);

    switch (index) {
    case ESPI_SPIRF:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Trying to write to RX FIFO\n",
                      TYPE_MPC_ESPI, __func__);
        break;
    case ESPI_SPITF:
        if (mpc_espi_is_enabled(s)) {
            for (i = 0; i < size; ++i) {
                mpc_espi_tx_fifo_push(s, (value >> (i * 8)) & 0xff);
                mpc_espi_flush_txfifo(s);
            }
        }

        break;
    case ESPI_SPIE:
        s->regs[ESPI_SPIE] &= ~value;

        if (!(s->regs[ESPI_SPIE] & s->regs[ESPI_SPIM])) {
            qemu_set_irq(s->irq, 0);
        }

        break;
    case ESPI_SPMODE:
        s->regs[ESPI_SPMODE] = value;

        if (!mpc_espi_is_enabled(s)) {
            /* device is disabled, so this is a soft reset */
            mpc_espi_soft_reset(s);

            return;
        }

        break;
    case ESPI_SPCOM:
        s->regs[ESPI_SPCOM] = value;
        s->burst_length = EXTRACT(s->regs[ESPI_SPCOM], SPCOM_TRANLEN) + 1;
        s->rx_skip = EXTRACT(s->regs[ESPI_SPCOM], SPCOM_RXSKIP);

        trace_mpc_espi_spcom(s->iomem.name,
                            EXTRACT(s->regs[ESPI_SPCOM], SPCOM_CS),
                            s->regs[ESPI_SPCOM] & SPCOM_DO,
                            s->regs[ESPI_SPCOM] & SPCOM_TO,
                            EXTRACT(s->regs[ESPI_SPCOM], SPCOM_TRANLEN) + 1,
                            EXTRACT(s->regs[ESPI_SPCOM], SPCOM_RXSKIP));

        for (i = 0; i < ARRAY_SIZE(s->cs_lines); i++) {
            qemu_set_irq(s->cs_lines[i],
                         i == EXTRACT(s->regs[ESPI_SPCOM], SPCOM_CS) ? 0 : 1);
        }

        break;
    case ESPI_SPMODE0:
    case ESPI_SPMODE1:
    case ESPI_SPMODE2:
    case ESPI_SPMODE3:
        s->regs[index] = value;

        trace_mpc_espi_spmode(s->iomem.name,
                             index - ESPI_SPMODE0,
                             s->regs[index] & CSMODE_REV,
                             s->regs[index] & CSMODE_ODD,
                             s->regs[index] & CSMODE_POL_1,
                             EXTRACT(s->regs[index], CSMODE_LEN) + 1);

        break;
    default:
        s->regs[index] = value;

        break;
    }
}

static const struct MemoryRegionOps mpc_espi_ops = {
    .read = mpc_espi_read,
    .write = mpc_espi_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void mpc_espi_realize(DeviceState *dev, Error **errp)
{
    MPCESPIState *s = MPC_ESPI(dev);
    int i;

    s->bus = ssi_create_bus(dev, "spi");

    memory_region_init_io(&s->iomem, OBJECT(dev), &mpc_espi_ops, s,
                          TYPE_MPC_ESPI, 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);

    for (i = 0; i < ARRAY_SIZE(s->cs_lines); ++i) {
        sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->cs_lines[i]);
    }

    fifo8_create(&s->tx_fifo, FSL_ESPI_FIFO_SIZE);
    fifo8_create(&s->rx_fifo, FSL_ESPI_FIFO_SIZE);
}

static void mpc_espi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = mpc_espi_realize;
    dc->vmsd = &vmstate_mpc_espi;
    dc->reset = mpc_espi_reset;
    dc->desc = "Freescale eSPI Controller";
}

static const TypeInfo mpc_espi_info = {
    .name          = TYPE_MPC_ESPI,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(MPCESPIState),
    .class_init    = mpc_espi_class_init,
};

static void mpc_espi_register_types(void)
{
    type_register_static(&mpc_espi_info);
}

type_init(mpc_espi_register_types)
