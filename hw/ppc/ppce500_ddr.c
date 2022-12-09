/*
 * QEMU PowerPC e500v2 Level 2 Cache Registers code
 *
 * Copyright (C) 2022 Bernhard Beschow <shentey@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/misc/unimp.h"
#include "qom/object.h"

#define MPC8XXX_DDR_REGS_SIZE     0x1000ULL

#define TYPE_E500_DDR "fsl,p1020-memory-controller"
OBJECT_DECLARE_SIMPLE_TYPE(PPCE500DDRState, E500_DDR)

struct PPCE500DDRState {
    UnimplementedDeviceState parent;

    qemu_irq irq;
};

static void ppce500_l2cache_init(Object *obj)
{
    PPCE500DDRState *s = E500_DDR(obj);
    DeviceState *dev = DEVICE(s);

    qdev_prop_set_string(dev, "name", "DDR control registers");
    qdev_prop_set_uint64(dev, "size", MPC8XXX_DDR_REGS_SIZE);

    sysbus_init_irq(SYS_BUS_DEVICE(dev), &s->irq);
}

static const TypeInfo types[] = {
    {
        .name          = TYPE_E500_DDR,
        .parent        = TYPE_UNIMPLEMENTED_DEVICE,
        .instance_init = ppce500_l2cache_init,
        .instance_size = sizeof(PPCE500DDRState)
    },
    {
        .name          = "fsl,p1022-memory-controller",
        .parent        = TYPE_E500_DDR,
    },
    {
        .name          = "fsl,mpc8572-memory-controller",
        .parent        = TYPE_E500_DDR,
    }
};

DEFINE_TYPES(types);
