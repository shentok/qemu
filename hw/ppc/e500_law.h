#ifndef E500_LAW_H
#define E500_LAW_H

#include "hw/sysbus.h"
#include "qemu/notify.h"
#include "qom/object.h"

#define MPC85XX_LAW_OFFSET         0x0ULL
#define MPC85XX_LAW_SIZE           0x1000ULL

struct PPCE500LAWState {
    /*< private >*/
    SysBusDevice parent;
    /*< public >*/

    MemoryRegion mmio;
    MemoryRegion law_ops;
    MemoryRegion *ccsr;

    Notifier ccsrbar_changed_notifier;
};

#define TYPE_E500_LAW "e500-law"
OBJECT_DECLARE_SIMPLE_TYPE(PPCE500LAWState, E500_LAW)

#endif /* E500_LAW_H */
