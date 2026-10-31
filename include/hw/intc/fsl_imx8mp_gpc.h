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

/*
 * The CPU power up/down trigger registers moved between SoC generations.
 * Everything else the CPU power state machine uses is common.
 */
struct FslImx8mpGpcClass {
    SysBusDeviceClass parent_class;

    uint32_t cpu_pgc_up_trg;
    uint32_t cpu_pgc_dn_trg;
};

#define TYPE_IMX8MP_GPC "imx8mp-gpc"
OBJECT_DECLARE_TYPE(FslImx8mpGpcState, FslImx8mpGpcClass, IMX8MP_GPC)

/*
 * i.MX8MM (and i.MX8MN/i.MX8MQ) use the same GPCv2 block as the i.MX8MP,
 * but the CPU power up/down trigger registers live at different offsets.
 */
#define TYPE_IMX8MM_GPC "imx8mm-gpc"

#endif /* FSL_IMX8MP_GPC_H */
