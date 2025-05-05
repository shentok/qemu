/*
 * IMX6 System Reset Controller
 *
 * Copyright (C) 2012 NICTA
 * Updated by Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef IMX6_SRC_H
#define IMX6_SRC_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define SRC_SCR 0
#define SRC_SBMR1 1
#define SRC_SRSR 2
#define SRC_SISR 5
#define SRC_SIMR 6
#define SRC_SBMR2 7
#define SRC_GPR1 8
#define SRC_GPR2 9
#define SRC_GPR3 10
#define SRC_GPR4 11
#define SRC_GPR5 12
#define SRC_GPR6 13
#define SRC_GPR7 14
#define SRC_GPR8 15
#define SRC_GPR9 16
#define SRC_GPR10 17
#define SRC_MAX 18

#define TYPE_IMX6_SRC "imx6.src"
OBJECT_DECLARE_SIMPLE_TYPE(IMX6SRCState, IMX6_SRC)

struct IMX6SRCState {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion iomem;

    uint32_t regs[SRC_MAX];

};

#endif /* IMX6_SRC_H */
