/*
 * NXP PCA9450 PMIC
 *
 * Copyright (C) 2025 Guenter Roeck
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/irq.h"
#include "hw/i2c/i2c.h"
#include "migration/vmstate.h"
#include "pca9450.h"
#include "qapi/error.h"
#include "qapi/visitor.h"
#include "trace.h"

#define PCA9450AA_ID    0x11
#define PCA9450B_ID     0x31

static const DeviceInfo devices[] = {
    { PCA9450AA_ID, "pca9450aa" },
    { PCA9450B_ID, "pca9450b" },
    { PCA9450B_ID, "pca9450c" },
};

enum PCA9450_REGS {
    REG_DEVICE_ID = 0x00,
    REG_INT1,
    REG_INT1_MSK,
    REG_STATUS1,
    REG_STATUS2,
    REG_PWRON_STAT,
    REG_SW_RST,
    REG_PWR_CTRL,
    REG_RESET_CTRL,
    REG_CONFIG1,
    REG_CONFIG2,
    REG_BUCK123_DVS = 0x0c,
    REG_BUCK1OUT_LIMIT,
    REG_BUCK2OUT_LIMIT,
    REG_BUCK3OUT_LIMIT,
    REG_BUCK1CTRL,
    REG_BUCK1OUT_DVS0,
    REG_BUCK1OUT_DVS1,
    REG_BUCK2CTRL,
    REG_BUCK2OUT_DVS0,
    REG_BUCK2OUT_DVS1,
    REG_BUCK3CTRL,
    REG_BUCK3OUT_DVS0,
    REG_BUCK3OUT_DVS1,
    REG_BUCK4CTRL,
    REG_BUCK4OUT,
    REG_BUCK5CTRL,
    REG_BUCK5OUT,
    REG_BUCK6CTRL,
    REG_BUCK6OUT,
    REG_LDO_AD_CTRL = 0x20,
    REG_LDO1CTRL,
    REG_LDO2CTRL,
    REG_LDO3CTRL,
    REG_LDO4CTRL,
    REG_LDO5CTRL_L,
    REG_LDO5CTRL_H,
    REG_RSVD1,
    REG_RSVD2,
    REG_RSVD3,
    REG_LOADSW_CTRL,
    REG_VRFLT1_STS,
    REG_VRFLT2_STS,
    REG_VRFLT1_MASK,
    REG_VRFLT2_MASK,
};

static const uint8_t pca9450_regs[PCA9450_NUM_REGISTERS] = {
    [REG_DEVICE_ID] = 0x00,
    [REG_INT1] = 0x00,
    [REG_INT1_MSK] = 0xff,
    [REG_STATUS1] = 0x00,
    [REG_STATUS2] = 0x00,
    [REG_PWRON_STAT] = 0x00,
    [REG_SW_RST] = 0x00,
    [REG_PWR_CTRL] = 0x4c,
    [REG_RESET_CTRL] = 0x21,
    [REG_CONFIG1] = 0x50,
    [REG_CONFIG2] = 0x00,
    [REG_BUCK123_DVS] = 0xa9,
    [REG_BUCK1OUT_LIMIT] = 0x1c,
    [REG_BUCK2OUT_LIMIT] = 0x20,
    [REG_BUCK3OUT_LIMIT] = 0x1c,
    [REG_BUCK1CTRL] = 0x49,
    [REG_BUCK1OUT_DVS0] = 0x14,
    [REG_BUCK1OUT_DVS1] = 0x14,
    [REG_BUCK2CTRL] = 0x4a,
    [REG_BUCK2OUT_DVS0] = 0x14,
    [REG_BUCK2OUT_DVS1] = 0x14,
    [REG_BUCK3CTRL] = 0x49,
    [REG_BUCK3OUT_DVS0] = 0x14,
    [REG_BUCK3OUT_DVS1] = 0x14,
    [REG_BUCK4CTRL] = 0x09,
    [REG_BUCK4OUT] = 0x6c,
    [REG_BUCK5CTRL] = 0x09,
    [REG_BUCK5OUT] = 0x30,
    [REG_BUCK6CTRL] = 0x09,
    [REG_BUCK6OUT] = 0x14,
    [REG_LDO_AD_CTRL] = 0xf8,
    [REG_LDO1CTRL] = 0xc2,
    [REG_LDO2CTRL] = 0xc1,
    [REG_LDO3CTRL] = 0x4a,
    [REG_LDO4CTRL] = 0x41,
    [REG_LDO5CTRL_L] = 0x4f,
    [REG_LDO5CTRL_H] = 0x00,
    [REG_RSVD1] = 0x00,
    [REG_RSVD2] = 0x00,
    [REG_RSVD3] = 0x00,
    [REG_LOADSW_CTRL] = 0x85,
    [REG_VRFLT1_STS] = 0x00,
    [REG_VRFLT2_STS] = 0x00,
    [REG_VRFLT1_MASK] = 0x3f,
    [REG_VRFLT2_MASK] = 0x1f,
};

static void pca9450_interrupt_update(PCA9450State *s)
{
    qemu_set_irq(s->pin, s->regs[REG_INT1] & ~s->regs[REG_INT1_MSK]);
}

static void pca9450_read(PCA9450State *s)
{
    s->len = 1;

    switch (s->pointer) {
    case REG_INT1:
        s->buf = s->regs[REG_INT1];
        s->regs[REG_INT1] = 0;
        pca9450_interrupt_update(s);
        break;
    case REG_PWRON_STAT:
        s->buf = s->regs[s->pointer];
        s->regs[s->pointer] = 0;
        break;
    default:
        if (s->pointer < PCA9450_NUM_REGISTERS) {
                s->buf = s->regs[s->pointer];
        } else {
                s->buf = 0xff;
        }
        break;
    }
}

static void pca9450_reset(I2CSlave *i2c);

static void pca9450_write(PCA9450State *s)
{
    switch (s->pointer) {
    case REG_INT1_MSK:
        s->regs[REG_INT1_MSK] = s->buf;
        pca9450_interrupt_update(s);
        break;
    case REG_DEVICE_ID: /* read-only registers */
    case REG_INT1:
    case REG_STATUS1:
    case REG_PWRON_STAT:
    case REG_STATUS2:
        break;
    case REG_SW_RST:
        pca9450_reset(I2C_SLAVE(s));
        break;
    default:
        if (s->pointer < PCA9450_NUM_REGISTERS) {
                s->regs[s->pointer] = s->buf;
        }
        break;
    }
}

static uint8_t pca9450_rx(I2CSlave *i2c)
{
    PCA9450State *s = PCA9450(i2c);

    trace_pca9450_recv(DEVICE(s)->canonical_path,
                       s->len < 2 ? s->buf : 0xff);

    if (s->len < 2) {
        return s->buf;
    } else {
        return 0xff;
    }
}

static int pca9450_tx(I2CSlave *i2c, uint8_t data)
{
    PCA9450State *s = PCA9450(i2c);

    trace_pca9450_send(DEVICE(s)->canonical_path, data);

    if (s->len == 0) {
        s->pointer = data;
        s->len++;
    } else if (s->len == 1) {
        pca9450_write(s);
    }
    return 0;
}

static int pca9450_event(I2CSlave *i2c, enum i2c_event event)
{
    PCA9450State *s = PCA9450(i2c);

    if (event == I2C_START_RECV) {
        trace_pca9450_event(DEVICE(s)->canonical_path, "I2C_START_RECV");
        pca9450_read(s);
    }

    s->len = 0;

    return 0;
}

static int pca9450_post_load(void *opaque, int version_id)
{
    PCA9450State *s = opaque;

    pca9450_interrupt_update(s);
    return 0;
}

static const VMStateDescription vmstate_pca9450 = {
    .name = "PCA9450",
    .version_id = 0,
    .minimum_version_id = 0,
    .post_load = pca9450_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8(len, PCA9450State),
        VMSTATE_UINT8(buf, PCA9450State),
        VMSTATE_UINT8(pointer, PCA9450State),
        VMSTATE_UINT8_ARRAY(regs, PCA9450State, PCA9450_NUM_REGISTERS),
        VMSTATE_I2C_SLAVE(i2c, PCA9450State),
        VMSTATE_END_OF_LIST()
    }
};

static void pca9450_reset(I2CSlave *i2c)
{
    PCA9450State *s = PCA9450(i2c);
    PCA9450Class *sc = PCA9450_GET_CLASS(s);

    memcpy(s->regs, pca9450_regs, sizeof(pca9450_regs));
    s->regs[REG_DEVICE_ID] = sc->dev->model;

    pca9450_interrupt_update(s);
}

static void pca9450_realize(DeviceState *dev, Error **errp)
{
    PCA9450State *s = PCA9450(dev);
    I2CSlave *i2c = I2C_SLAVE(dev);

    qdev_init_gpio_out(&i2c->qdev, &s->pin, 1);
    pca9450_reset(i2c);
}

static void pca9450_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *k = I2C_SLAVE_CLASS(klass);
    PCA9450Class *sc = PCA9450_CLASS(klass);

    k->event = pca9450_event;
    k->recv = pca9450_rx;
    k->send = pca9450_tx;
    dc->realize = pca9450_realize;
    dc->vmsd = &vmstate_pca9450;

    sc->dev = (DeviceInfo *) data;
}

static const TypeInfo pca9450_info = {
    .name          = TYPE_PCA9450,
    .parent        = TYPE_I2C_SLAVE,
    .instance_size = sizeof(PCA9450State),
    .class_size    = sizeof(PCA9450Class),
    .abstract      = true,
};

static void pca9450_register_types(void)
{
    int i;

    type_register_static(&pca9450_info);

    for (i = 0; i < ARRAY_SIZE(devices); ++i) {
        const TypeInfo ti = {
            .name       = devices[i].name,
            .parent     = TYPE_PCA9450,
            .class_init = pca9450_class_init,
            .class_data = (void *) &devices[i],
        };
        type_register_static(&ti);
    }
}

type_init(pca9450_register_types)
