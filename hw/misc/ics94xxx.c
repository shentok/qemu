/*
 * QEMU ICS94XXX device
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
#include "migration/vmstate.h"
#include "qom/object.h"
#include "trace.h"

#define TYPE_ICS94XXX "ics94xxx"

OBJECT_DECLARE_SIMPLE_TYPE(ICS94XXXState, ICS94XXX)

#define ICS94XXX_SIZE 20

struct ICS94XXXState {
    SMBusDevice smbusdev;

    uint8_t data[ICS94XXX_SIZE];
    int8_t offset;
};

static uint8_t ics94xxx_receive_byte(SMBusDevice *dev)
{
    ICS94XXXState *s = ICS94XXX(dev);
    uint8_t val;

    if (0 <= s->offset && s->offset < ICS94XXX_SIZE) {
        val = s->data[s->offset];
        trace_ics94xxx_receive_byte(dev->i2c.address, s->offset, val);
    } else {
        val = s->data[8];
    }

    s->offset = (s->offset + 1) % ICS94XXX_SIZE;

    return val;
}

static int ics94xxx_write_data(SMBusDevice *dev, uint8_t *buf, uint8_t len)
{
    ICS94XXXState *s = ICS94XXX(dev);
    trace_ics94xxx_write_data(dev->i2c.address, dev->data_buf[0], 0);

    switch (len) {
    case 0:
        s->offset = -2;
        break;
    case 1:
        s->offset = -1;
        break;
    default:
        s->offset = 0;
        break;
    }

    for (int i = 2; i < len; ++i) {
        s->data[s->offset] = buf[i];
        trace_ics94xxx_write_byte(dev->i2c.address, s->offset, buf[i]);
        s->offset = (s->offset + 1) % ICS94XXX_SIZE;
    }

    return 0;
}

static const VMStateDescription vmstate_ics94xxx = {
    .name = "ics94xxx",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_SMBUS_DEVICE(smbusdev, ICS94XXXState),
        VMSTATE_UINT8_ARRAY(data, ICS94XXXState, ICS94XXX_SIZE),
        VMSTATE_END_OF_LIST()
    }
};

static void ics94xxx_reset_hold(Object *obj, ResetType type)
{
    ICS94XXXState *s = ICS94XXX(obj);

    trace_ics94xxx_reset(s->smbusdev.i2c.address);

    s->offset = -2;
    s->data[0] = 0x32;
    s->data[1] = 0xff;
    s->data[2] = 0xff;
    s->data[3] = 0xff;
    s->data[4] = 0x6d;
    s->data[5] = 0xbf;
    s->data[8] = 8;
}

static void ics94xxx_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    SMBusDeviceClass *sc = SMBUS_DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    rc->phases.hold = ics94xxx_reset_hold;
    sc->receive_byte = ics94xxx_receive_byte;
    sc->write_data = ics94xxx_write_data;
    dc->vmsd = &vmstate_ics94xxx;
}

static const TypeInfo ics94xxx_info = {
    .name          = TYPE_ICS94XXX,
    .parent        = TYPE_SMBUS_DEVICE,
    .instance_size = sizeof(ICS94XXXState),
    .class_init    = ics94xxx_class_init,
};

static void ics94xxx_register_types(void)
{
    type_register_static(&ics94xxx_info);
}

type_init(ics94xxx_register_types)
