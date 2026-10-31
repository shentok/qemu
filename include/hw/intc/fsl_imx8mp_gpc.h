#ifndef FSL_IMX8MP_GPC_H
#define FSL_IMX8MP_GPC_H

#include "hw/core/irq.h"
#include "hw/core/sysbus.h"
#include "system/memory.h"
#include "qom/object.h"

#define IMX8MP_GPC_REQUEST_WAKE_GIC "request-wake-gic"
#define IMX8MP_GPC_WFI "wfi"
#define IMX8MP_GPC_CPU_POWER "cpu-power"

enum FslImx8mpGpcRegisters {
    IMX8MP_GPC_NUM = 0x1000 / sizeof(uint32_t),
};

typedef struct FslImx8mpGpcCpuState {
    bool request_wake_gic;
    bool wfi;
    bool powered;
} FslImx8mpGpcCpuState;

struct FslImx8mpGpcState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t regs[IMX8MP_GPC_NUM];
    FslImx8mpGpcCpuState cpu[4];

    /* Per-core power state reported to the SRC */
    qemu_irq cpu_power[4];
};

#define TYPE_IMX8MP_GPC "imx8mp-gpc"
OBJECT_DECLARE_SIMPLE_TYPE(FslImx8mpGpcState, IMX8MP_GPC)

#endif /* FSL_IMX8MP_GPC_H */
