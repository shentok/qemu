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
#include "monitor/hmp.h"
#include "qapi/qapi-commands-gpio.h"
#include "qobject/qdict.h"

void hmp_gpio_set(Monitor *mon, const QDict *qdict)
{
    const char *path = qdict_get_str(qdict, "path");
    const char *gpio = qdict_get_try_str(qdict, "gpio");
    int number = qdict_get_try_int(qdict, "number", 0);
    bool value = qdict_get_bool(qdict, "value");
    Error *err = NULL;

    qmp_gpio_set(path, gpio, true, number, value, &err);
    if (hmp_handle_error(mon, err)) {
        return;
    }
}
