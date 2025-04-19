/*
 * i.MX processors GPIO emulation.
 *
 * Copyright (C) 2015 Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qapi/qapi-commands-gpio.h"
#include "qapi/error.h"
#include "hw/irq.h"
#include "hw/qdev-core.h"
#include "qom/object.h"

void qmp_gpio_set(const char *path, const char *gpio, bool has_number,
                  int64_t number, bool value, Error **errp)
{
    DeviceState *dev;
    qemu_irq irq;

    dev = DEVICE(object_resolve_path(path, NULL));
    if (!dev) {
        error_set(errp, ERROR_CLASS_DEVICE_NOT_FOUND,
                  "Cannot find device '%s'", path);
        return;
    }

    if (!has_number) {
        number = 0;
    }
    irq = qdev_get_gpio_in_named(dev, gpio, number);
    if (!irq) {
        error_set(errp, ERROR_CLASS_GENERIC_ERROR,
                  "GPIO input '%s[%"PRId64"]' does not exist",
                  gpio ? gpio : "unnamed", number);
        return;
    }

    qemu_set_irq(irq, value);
}
