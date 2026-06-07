/*
 * IMX53 Clock Control Module
 *
 * Copyright (C) 2012 NICTA
 * Updated by Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef IMX53_CCM_H
#define IMX53_CCM_H

#include "hw/misc/imx_ccm.h"
#include "qom/object.h"

#define TYPE_IMX53_CCM "imx53.ccm"
OBJECT_DECLARE_SIMPLE_TYPE(Imx53CcmState, IMX53_CCM)

#define CCM_MAX ((0x88 / 4) + 1)
#define DPLLC_MAX (0x30 / 4)

struct Imx53CcmState {
    IMXCCMState parent_obj;

    MemoryRegion ioccm;
    MemoryRegion iodpllc[4];

    uint32_t ccm[CCM_MAX];
    uint32_t dpllc[4][DPLLC_MAX];
};

#endif /* IMX53_CCM_H */
