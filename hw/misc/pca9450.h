/*
 * NXP PCA9450 PMIC
 *
 * Copyright (C) 2025 Guenter Roeck
 * Copyright (C) 2008-2012 Andrzej Zaborowski <balrogg@gmail.com>
 */
#ifndef QEMU_PCA9450_H
#define QEMU_PCA9450_H

#include "hw/i2c/i2c.h"

typedef struct DeviceInfo {
    int model;
    const char *name;
} DeviceInfo;

struct PCA9450Class {
    I2CSlaveClass parent_class;
    const DeviceInfo *dev;
};

#define TYPE_PCA9450 "pca9450-generic"

OBJECT_DECLARE_TYPE(PCA9450State, PCA9450Class, PCA9450)

#define PCA9450_NUM_REGISTERS   0x30

/**
 * PCA9450State:
 */
typedef struct PCA9450State {
    /*< private >*/
    I2CSlave i2c;
    /*< public >*/

    uint8_t len;
    uint8_t buf;
    qemu_irq pin;

    uint8_t pointer;

    uint8_t regs[PCA9450_NUM_REGISTERS];
} PCA9450State;

#endif
