/*
 * QEMU Winbond W83781D device
 *
 * Copyright (c) 2024 Bernhard Beschow <shentey@gmail.com>
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
#include "qapi/error.h"
#include "hw/i2c/i2c.h"
#include "hw/i2c/smbus_slave.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/resettable.h"
#include "hw/sensor/tmp105.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qom/object.h"
#include "system/reset.h"
#include "system/runstate.h"
#include "trace.h"

#define TYPE_W83781D "w83781d"
#define TYPE_W83781D_LM75 "w83781d-lm75"

OBJECT_DECLARE_SIMPLE_TYPE(W83781DState, W83781D)
OBJECT_DECLARE_SIMPLE_TYPE(W83781DLM75State, W83781D_LM75)

#define W83781D_SIZE 256
#define W83781D_AS99127F_SIZE 0x80

struct W83781DState {
    SMBusDevice parent_obj;

    DeviceState *lm75[2];
    uint8_t regs[0x20];
    uint8_t data[W83781D_SIZE];
    /*
     * AS99127F-specific registers. They live behind the LM75 at 0x48 and are
     * selected by setting bit 7 of the register index.
     */
    uint8_t as99127f[W83781D_AS99127F_SIZE];
    uint16_t vendor;
    uint8_t offset;
};

/*
 * LM75 thermometer of the AS99127F. Accesses to the extended register range
 * are forwarded to the parent AS99127F.
 */
struct W83781DLM75State {
    SMBusDevice parent_obj;

    W83781DState *w83781d;
    uint8_t pointer;
};

static uint8_t w83781d_get_bank(W83781DState *s)
{
    return s->data[0x4e];
}

static uint8_t w83781d_receive_byte(SMBusDevice *dev)
{
    W83781DState *s = W83781D(dev);
    I2CSlave *i2c = I2C_SLAVE(s);
    uint8_t offset;
    uint8_t val;

    if (0x40 <= s->offset && s->offset < 0x60) {
        offset = s->offset;
        switch (offset) {
        case 0x4f:
            val = (w83781d_get_bank(s) & 0x80) ? s->vendor >> 8
                                               : s->vendor & 0xff;
            break;
        default:
            val = s->regs[s->offset - 0x40];
            break;
        }
    } else {
        offset = s->offset++;
        val = s->data[offset];
    }

    trace_w83781d_receive_byte(i2c->address, offset, val);

    return val;
}

static int w83781d_write_data(SMBusDevice *dev, uint8_t *buf, uint8_t len)
{
    W83781DState *s = W83781D(dev);
    I2CSlave *i2c = I2C_SLAVE(s);

    /* len is guaranteed to be > 0 */
    s->offset = buf[0];

    for (int i = 1; i < len; i++) {
        s->data[s->offset] = buf[i];

        trace_w83781d_write_byte(i2c->address, s->offset, s->data[s->offset]);

        switch (s->offset) {
        case 0x48:
            i2c_slave_set_address(i2c, buf[i]);
            break;
        case 0x4e:
            break;
        default:
            qemu_log_mask(LOG_UNIMP, "%s: unimplemented offset 0x%" PRIx8 "\n",
                          __func__, s->offset);
            break;
        }

        s->offset = (s->offset + 1) % W83781D_SIZE;
    }

    return 0;
}

static uint8_t w83781d_as99127f_read(W83781DState *s, uint8_t reg)
{
    reg &= 0x7f;

    trace_w83781d_receive_byte(I2C_SLAVE(s)->address, reg | 0x80,
                               s->as99127f[reg]);

    return s->as99127f[reg];
}

static void w83781d_as99127f_write(W83781DState *s, uint8_t reg, uint8_t val)
{
    reg &= 0x7f;

    trace_w83781d_write_byte(I2C_SLAVE(s)->address, reg | 0x80, val);

    switch (reg) {
    case 0x01:
        /*
         * CLKRST: clearing bit 4 resets the machine. The BIOS uses this to
         * reboot after settings have been saved.
         */
        if (!(val & 0x10)) {
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        }
        break;
    case 0x07:
        /* GPO15 is wired to the reset line on some boards. */
        if (val & 0x01) {
            qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        }
        break;
    default:
        break;
    }

    s->as99127f[reg] = val;
}

static uint8_t w83781d_lm75_receive_byte(SMBusDevice *dev)
{
    W83781DLM75State *s = W83781D_LM75(dev);
    uint8_t reg = s->pointer++;

    if (reg & 0x80) {
        return w83781d_as99127f_read(s->w83781d, reg);
    }

    /* The temperature registers are not modeled. */
    return 0;
}

static int w83781d_lm75_write_data(SMBusDevice *dev, uint8_t *buf, uint8_t len)
{
    W83781DLM75State *s = W83781D_LM75(dev);
    uint8_t reg = buf[0];

    for (int i = 1; i < len; i++) {
        if (reg & 0x80) {
            w83781d_as99127f_write(s->w83781d, reg, buf[i]);
        }
        reg++;
    }

    s->pointer = reg;

    return 0;
}

static const VMStateDescription vmstate_w83781d_lm75 = {
    .name = "w83781d-lm75",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_SMBUS_DEVICE(parent_obj, W83781DLM75State),
        VMSTATE_UINT8(pointer, W83781DLM75State),
        VMSTATE_END_OF_LIST()
    }
};

static void w83781d_lm75_reset_hold(Object *obj, ResetType type)
{
    W83781DLM75State *s = W83781D_LM75(obj);

    s->pointer = 0;
}

static const VMStateDescription vmstate_w83781d = {
    .name = "w83781d",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_SMBUS_DEVICE(parent_obj, W83781DState),
        VMSTATE_UINT8_ARRAY(data, W83781DState, W83781D_SIZE),
        VMSTATE_UINT8_ARRAY(as99127f, W83781DState, W83781D_AS99127F_SIZE),
        VMSTATE_UINT8(offset, W83781DState),
        VMSTATE_END_OF_LIST()
    }
};

static void w83781d_reset_hold(Object *obj, ResetType type)
{
    W83781DState *s = W83781D(obj);
    I2CSlave *i2c = I2C_SLAVE(s);
    uint8_t *regs = s->regs - 0x40;

    memset(s->regs, 0, sizeof(s->regs));

    regs[0x40] = 8;
    regs[0x46] = 0x40;
    regs[0x47] = 0x50;
    regs[0x48] = i2c->address;
    regs[0x49] = 0x20;
    regs[0x4a] = 1;
    regs[0x4b] = 0x44;
    regs[0x4c] = 0;
    regs[0x4d] = 0x15;
    regs[0x4e] = 0x80;
    regs[0x56] = 0xff;
    regs[0x57] = 0xff;
    regs[0x58] = 0x31;
    regs[0x59] = 0x8f;
    regs[0x5a] = 0x8f;
    regs[0x5b] = 0x2a;
    regs[0x5c] = 0xe0;
    regs[0x5d] = 0x48;
    regs[0x5f] = 0x1f;

    /*
     * Nominal readings of the LM78-compatible register file. Voltages are
     * reported as 16 mV per LSB and temperatures in degrees Celsius, as
     * defined by the W83781D datasheet. The values represent a board with
     * all rails in spec so that a BIOS POST hardware-monitor self test
     * passes on a machine without real sensors attached.
     */
    memset(s->data, 0, sizeof(s->data));
    s->data[0x20] = 0x80; /* Vcore: 2.048 V */
    s->data[0x21] = 0x00; /* Vtt/Vio: unused */
    s->data[0x22] = 0xce; /* +3.3 V */
    s->data[0x23] = 0xb9; /* +5 V behind the resistor divider */
    s->data[0x24] = 0xc5; /* +12 V behind the resistor divider */
    s->data[0x25] = 0xbd; /* -12 V behind the resistor divider */
    s->data[0x26] = 0xcf; /* -5 V behind the resistor divider */
    s->data[0x27] = 30;   /* temperature 1 */
    s->data[0x28] = 0xe1; /* fan 1: 3000 rpm */
    s->data[0x29] = 0xe1; /* fan 2: 3000 rpm */
    s->data[0x2a] = 0xff; /* fan 3 */

    memset(s->as99127f, 0, sizeof(s->as99127f));
    s->as99127f[0x00] = 0x88;
    s->as99127f[0x01] = 0x10;
    s->as99127f[0x03] = 0x02;
    s->as99127f[0x04] = 0x01;
    s->as99127f[0x05] = 0x1f;
    s->as99127f[0x06] = 0x2f;

    s->offset = 0;
}

static void w83781d_realize(DeviceState *dev, Error **errp)
{
    W83781DState *s = W83781D(dev);
    I2CBus *bus = I2C_BUS(dev->parent_bus);
    I2CSlave *lm75;

    lm75 = i2c_slave_new(TYPE_W83781D_LM75, 0x48);
    object_property_set_link(OBJECT(lm75), "w83781d", OBJECT(s), &error_abort);
    i2c_slave_realize_and_unref(lm75, bus, &error_abort);
    s->lm75[0] = DEVICE(lm75);

    s->lm75[1] = DEVICE(i2c_slave_create_simple(bus, TYPE_TMP105, 0x49));
}

static void w83781d_init(Object *obj)
{
    W83781DState *s = W83781D(obj);

    qdev_prop_set_uint8(DEVICE(s), "address", 0x2d);
}

static const Property w83781d_props[] = {
    DEFINE_PROP_UINT16("vendor", W83781DState, vendor, 0x12c3),
};

static void w83781d_class_init(ObjectClass *klass, const void *data)
{
    SMBusDeviceClass *sc = SMBUS_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    rc->phases.hold = w83781d_reset_hold;
    sc->receive_byte = w83781d_receive_byte;
    sc->write_data = w83781d_write_data;
    dc->realize = w83781d_realize;
    dc->vmsd = &vmstate_w83781d;
    device_class_set_props(dc, w83781d_props);
}

static const TypeInfo w83781d_info = {
    .name          = TYPE_W83781D,
    .parent        = TYPE_SMBUS_DEVICE,
    .instance_size = sizeof(W83781DState),
    .instance_init = w83781d_init,
    .class_init    = w83781d_class_init,
};

static const Property w83781d_lm75_props[] = {
    DEFINE_PROP_LINK("w83781d", W83781DLM75State, w83781d,
                     TYPE_W83781D, W83781DState *),
};

static void w83781d_lm75_class_init(ObjectClass *klass, const void *data)
{
    SMBusDeviceClass *sc = SMBUS_DEVICE_CLASS(klass);
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    rc->phases.hold = w83781d_lm75_reset_hold;
    sc->receive_byte = w83781d_lm75_receive_byte;
    sc->write_data = w83781d_lm75_write_data;
    dc->vmsd = &vmstate_w83781d_lm75;
    device_class_set_props(dc, w83781d_lm75_props);
}

static const TypeInfo w83781d_lm75_info = {
    .name          = TYPE_W83781D_LM75,
    .parent        = TYPE_SMBUS_DEVICE,
    .instance_size = sizeof(W83781DLM75State),
    .class_init    = w83781d_lm75_class_init,
};

static void w83781d_register_types(void)
{
    type_register_static(&w83781d_info);
    type_register_static(&w83781d_lm75_info);
}

type_init(w83781d_register_types)
