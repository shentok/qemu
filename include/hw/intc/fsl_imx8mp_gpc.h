#ifndef FSL_IMX8MP_GPC_H
#define FSL_IMX8MP_GPC_H

#include "hw/core/sysbus.h"
#include "system/memory.h"
#include "qom/object.h"

#include "hw/misc/imx8mp_src.h"

#define IMX8MP_GPC_REQUEST_WAKE_GIC "request-wake-gic"
#define IMX8MP_GPC_WFI "wfi"

enum FslImx8mpGpcRegisters {
    IMX8MP_GPC_NUM = 0x1000 / sizeof(uint32_t),
};

struct FslImx8mpGpcState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t regs[IMX8MP_GPC_NUM];
    struct {
        bool request_wake_gic;
        bool wfi;
    } cpu[4];
    FslImx8mpSrcState *src;
};

#define TYPE_IMX8MP_GPC "imx8mp-gpc"
OBJECT_DECLARE_SIMPLE_TYPE(FslImx8mpGpcState, IMX8MP_GPC)

#endif /* FSL_IMX8MP_GPC_H */
