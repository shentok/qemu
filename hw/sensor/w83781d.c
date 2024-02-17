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
#include "hw/i2c/i2c.h"
#include "hw/i2c/smbus_slave.h"
#include "hw/resettable.h"
#include "hw/sensor/tmp105.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qom/object.h"
#include "trace.h"

#define TYPE_W83781D "w83781d"

OBJECT_DECLARE_SIMPLE_TYPE(W83781DState, W83781D)

#define W83781D_SIZE 256

struct W83781DState {
    SMBusDevice parent_obj;

    TMP105State lm75[2];
    uint8_t regs[0x20];
    uint8_t data[W83781D_SIZE];
    uint16_t vendor;
    uint8_t offset;
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

static const VMStateDescription vmstate_w83781d = {
    .name = "w83781d",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_SMBUS_DEVICE(parent_obj, W83781DState),
        VMSTATE_UINT8_ARRAY(data, W83781DState, W83781D_SIZE),
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
    regs[0x47] = 0x50;
    regs[0x48] = i2c->address;
    regs[0x49] = 0x10;
    regs[0x4a] = 1;
    regs[0x4b] = 0x44;
    regs[0x4c] = 1;
    regs[0x4d] = 0x15;
    regs[0x4e] = 0x80;

    s->offset = 0;
}

static void w83781d_realize(DeviceState *dev, Error **errp)
{
    W83781DState *s = W83781D(dev);

    for (int i = 0; i < ARRAY_SIZE(s->lm75); i++) {
        if (!qdev_realize_and_unref(DEVICE(&s->lm75[i]), dev->parent_bus, errp)) {
            return;
        }
    }
}

static void w83781d_init(Object *obj)
{
    W83781DState *s = W83781D(obj);

    qdev_prop_set_uint8(DEVICE(s), "address", 0x2d);

    for (int i = 0; i < ARRAY_SIZE(s->lm75); i++) {
        object_initialize(&s->lm75[i], sizeof(s->lm75[i]), TYPE_TMP105);
        qdev_prop_set_uint8(DEVICE(&s->lm75[i]), "address", 0x48 + i);
    }
}

static const Property w83781d_props[] = {
    DEFINE_PROP_UINT16("vendor", W83781DState, vendor, 0x5ca3),
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

static void w83781d_register_types(void)
{
    type_register_static(&w83781d_info);
}

type_init(w83781d_register_types)
