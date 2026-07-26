/*
 * PMIC
 *
 * This implementation is based on the following reference manual:
 * Power Management Integrated Circuit (PMIC) for i.MX50/53 Families
 * Document Number: MC34708, Rev. 11.0, 11/2013
 *
 * Copyright (c) 2026 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "hw/i2c/i2c.h"
#include "hw/core/irq.h"
#include "hw/core/qdev.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/registerfields.h"
#include "migration/vmstate.h"
#include "ui/input.h"
#include "trace.h"

#define TYPE_MC34708 "mc34708"
OBJECT_DECLARE_SIMPLE_TYPE(MC34708State, MC34708)

REG32(MC34708_IS0, 0)
    FIELD(MC34708_IS0, TSPENDET, 2, 1)
    FIELD(MC34708_IS0, TSDONEI, 1, 1)

REG32(MC34708_IM0, 1)
REG32(MC34708_IS1, 3)
REG32(MC34708_IM1, 4)

REG32(MC34708_IDENT, 7)

REG32(MC34708_TS_STATUS, 0x2a)
    FIELD(MC34708_TS_STATUS, PEN_DOWN, 0, 1)
    FIELD(MC34708_TS_STATUS, DATA_READY, 1, 1)

REG32(MC34708_ADC0, 43)
    FIELD(MC34708_ADC0, TSPENDETEN, 20, 1)
    FIELD(MC34708_ADC0, TSSTOP, 16, 3)
    FIELD(MC34708_ADC0, TSSTART, 13, 1)
    FIELD(MC34708_ADC0, TSEN, 12, 1)

REG32(MC34708_ADC1, 44)
    FIELD(MC34708_ADC1, VALUE, 0, 12)

REG32(MC34708_ADC2, 45)
    FIELD(MC34708_ADC2, VALUE, 0, 12)

REG32(MC34708_ADC3, 46)
REG32(MC34708_ADC4, 47)
REG32(MC34708_ADC5, 48)
REG32(MC34708_ADC6, 49)
REG32(MC34708_ADC7, 50)

struct MC34708State {
    I2CSlave parent_obj;

    QemuInputHandlerState *hs;
    qemu_irq irq;

    uint32_t regs[64];
    uint16_t axis[INPUT_AXIS__MAX];
    bool btns[INPUT_BUTTON__MAX];

    uint8_t reg;
    uint8_t byte;
    uint32_t val;
    bool expect_reg;
};

static const char *mc34708_reg_name(int reg)
{
    switch (reg) {
    case A_MC34708_IS0:
        return "Interrupt Status 0";
    case A_MC34708_IM0:
        return "Interrupt Mask 0";
    case A_MC34708_IS1:
        return "Interrupt Status 1";
    case A_MC34708_IM1:
        return "Interrupt Mask 1";
    case 15:
        return "Power Control 2";
    case A_MC34708_ADC0:
        return "ADC0";
    case A_MC34708_ADC1:
        return "ADC1";
    case A_MC34708_ADC2:
        return "ADC2";
    case A_MC34708_ADC3:
        return "ADC3";
    case A_MC34708_ADC4:
        return "ADC4";
    case A_MC34708_ADC5:
        return "ADC5";
    case A_MC34708_ADC6:
        return "ADC6";
    case A_MC34708_ADC7:
        return "ADC7";
    }

    return "unknown";
}

static void mc34708_update_irq(MC34708State *s)
{
    bool is_set = (s->regs[A_MC34708_IS0] & ~s->regs[A_MC34708_IM0]) ||
                  (s->regs[A_MC34708_IS1] & ~s->regs[A_MC34708_IM1]);

    qemu_set_irq(s->irq, is_set);
}

static int mc34708_send(I2CSlave *i2c, uint8_t data)
{
    MC34708State *s = MC34708(i2c);

    if (s->expect_reg) {
        s->reg = data;
        s->expect_reg = false;
        s->byte = 0;
        s->val = 0;
        return 0;
    }

    if (s->byte == 0) {
        s->val = 0;
    }

    switch (s->byte) {
    case 0:
        s->val |= (uint32_t)data << 16;
        break;
    case 1:
        s->val |= (uint32_t)data << 8;
        break;
    case 2:
        s->val |= data;
        break;
    }

    if (++s->byte == 3) {
        trace_mc34708_send(s->reg, mc34708_reg_name(s->reg), s->val);

        switch (s->reg) {
        case A_MC34708_IS0:
        case A_MC34708_IS1:
            s->regs[s->reg] &= ~s->val;
            break;
        case A_MC34708_ADC0:
            s->regs[s->reg] = s->val;
            if ((s->regs[A_MC34708_ADC0] & 0x3000) == 0x3000) {
                for (int i = 0;
                     i <= FIELD_EX32(s->regs[A_MC34708_ADC0], MC34708_ADC0, TSSTOP);
                     i++) {
                    uint16_t val;
                    switch ((s->regs[A_MC34708_ADC3] >> ((i * 2) + 8)) & 3) {
                    case 1:
                        val = ((s->axis[INPUT_AXIS_X] >> 3) + (rand() % 8 - 4))
                                / 4096.0 * 3696.0 + 200;
                        break;
                    case 2:
                        val = ((s->axis[INPUT_AXIS_Y] >> 3) + (rand() % 8 - 4))
                                / 4096.0 * 3696.0 + 200;
                        break;
                    case 3:
                        val = s->btns[INPUT_BUTTON_LEFT] ? 1000 : 0;
                        break;
                    default:
                        continue;
                    }
                    if (i % 2 == 0) {
                        s->regs[A_MC34708_ADC4 + (i / 2)] &= 0xfff000;
                        s->regs[A_MC34708_ADC4 + (i / 2)] |= val & 0xfff;
                    } else {
                        s->regs[A_MC34708_ADC4 + (i / 2)] &= 0xfff;
                        s->regs[A_MC34708_ADC4 + (i / 2)] |= (val & 0xfff) << 12;
                    }
                }
                s->regs[A_MC34708_IS0] |= BIT(R_MC34708_IS0_TSDONEI_SHIFT);
            }
            break;
        default:
            s->regs[s->reg] = s->val;
            break;
        }

        mc34708_update_irq(s);

        s->byte = 0;
        s->reg++;
    }

    return 0;
}

static uint8_t mc34708_recv(I2CSlave *i2c)
{
    MC34708State *s = MC34708(i2c);
    uint32_t val = s->regs[s->reg];
    uint8_t ret;

    switch (s->byte) {
    case 0:
        ret = (val >> 16) & 0xff;
        break;
    case 1:
        ret = (val >> 8) & 0xff;
        break;
    default:
        ret = val & 0xff;
        trace_mc34708_recv(s->reg, mc34708_reg_name(s->reg), s->regs[s->reg]);
        break;
    }

    if (++s->byte == 3) {
        s->byte = 0;
        s->reg++;
    }

    return ret;
}

static int mc34708_event(I2CSlave *i2c, enum i2c_event event)
{
    MC34708State *s = MC34708(i2c);

    trace_mc34708_event(event);

    switch (event) {
    case I2C_START_SEND:
        s->expect_reg = true;
        s->byte = 0;
        break;

    case I2C_START_RECV:
        s->byte = 0;
        break;

    default:
        break;
    }

    return 0;
}

static void mc34708_input_event(DeviceState *dev, QemuConsole *src,
    QemuInputEvent *evt)
{
    MC34708State *s = MC34708(dev);

    switch (evt->type) {
    case INPUT_EVENT_KIND_ABS:
        trace_mc34708_input_event_abs((int)evt->abs.axis, evt->abs.value);
        s->axis[evt->abs.axis] = evt->abs.value;
        break;

    case INPUT_EVENT_KIND_BTN:
        trace_mc34708_input_event_btn((int)evt->btn.button, (int)evt->btn.down);
        s->btns[evt->btn.button] = evt->btn.down;
        if (evt->btn.down) {
            s->regs[A_MC34708_IS0] |= BIT(R_MC34708_IS0_TSPENDET_SHIFT);
            mc34708_update_irq(s);
        }
        break;

    default:
        /* keep gcc happy */
        break;
    }
}

static void mc34708_input_sync(DeviceState *dev)
{
    trace_mc34708_input_sync();
#if 0
    MC34708State *s = MC34708(dev);

    if (tablet->send_events) {
        wctablet_queue_event(tablet);
    }
#endif
}

static const QemuInputHandler mc34708_input_handler = {
    .name  = "QEMU MC34708 Touchscreen Handler",
    .mask  = INPUT_EVENT_MASK_BTN | INPUT_EVENT_MASK_ABS,
    .event = mc34708_input_event,
    .sync  = mc34708_input_sync,
};

static void mc34708_realize(DeviceState *dev, Error **errp)
{
    MC34708State *s = MC34708(dev);

    s->hs = qemu_input_handler_register(dev, &mc34708_input_handler);
}

static void mc34708_unrealize(DeviceState *dev)
{
    MC34708State *s = MC34708(dev);

    if (s->hs) {
        qemu_input_handler_unregister(s->hs);
    }
}

static void mc34708_reset_hold(Object *obj, ResetType type)
{
    MC34708State *s = MC34708(obj);

    memset(s->regs, 0, sizeof(s->regs));

    s->regs[A_MC34708_IDENT] = 0x00000047;

    s->reg = 0;
    s->byte = 0;
    s->expect_reg = true;
}

static const VMStateDescription vmstate_mc34708 = {
    .name = TYPE_MC34708,
    .version_id = 1,
    .minimum_version_id = 1,
};

static void mc34708_init(Object *obj)
{
    MC34708State *s = MC34708(obj);

    qdev_prop_set_uint8(DEVICE(s), "address", 8);

    qdev_init_gpio_out(DEVICE(s), &s->irq, 1);
}

static void mc34708_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    I2CSlaveClass *ic = I2C_SLAVE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    dc->realize = mc34708_realize;
    dc->unrealize = mc34708_unrealize;
    dc->vmsd = &vmstate_mc34708;

    ic->event = mc34708_event;
    ic->send  = mc34708_send;
    ic->recv  = mc34708_recv;

    rc->phases.hold = mc34708_reset_hold;
}

static const TypeInfo mc34708_info = {
    .name          = TYPE_MC34708,
    .parent        = TYPE_I2C_SLAVE,
    .instance_size = sizeof(MC34708State),
    .instance_init = mc34708_init,
    .class_init    = mc34708_class_init,
};

static void mc34708_register_types(void)
{
    type_register_static(&mc34708_info);
}

type_init(mc34708_register_types);
