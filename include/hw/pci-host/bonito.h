/*
 * QEMU Bonito64 north bridge support
 *
 * Copyright (c) 2008 yajin (yajin@vm-kernel.org)
 * Copyright (c) 2010 Huacai Chen (zltjiangshi@gmail.com)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_PCI_HOST_BONITO_H
#define HW_PCI_HOST_BONITO_H

#include "qom/object.h"

/* ICU Pins */
#define ICU_PIN_MBOXx(x)        (0 + (x))
#define ICU_PIN_DMARDY          4
#define ICU_PIN_DMAEMPTY        5
#define ICU_PIN_COPYRDY         6
#define ICU_PIN_COPYEMPTY       7
#define ICU_PIN_COPYERR         8
#define ICU_PIN_PCIIRQ          9
#define ICU_PIN_MASTERERR       10
#define ICU_PIN_SYSTEMERR       11
#define ICU_PIN_DRAMPERR        12
#define ICU_PIN_RETRYERR        13
#define ICU_PIN_INTTIMER        14
#define ICU_PIN_GPIOx(x)        (16 + (x))
#define ICU_PIN_GPINx(x)        (25 + (x))

#define TYPE_BONITO_PCI_HOST_BRIDGE "Bonito-pcihost"
OBJECT_DECLARE_SIMPLE_TYPE(BonitoState, BONITO_PCI_HOST_BRIDGE)

#endif
