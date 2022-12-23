#ifndef E500_LAW_H
#define E500_LAW_H

#include "hw/sysbus.h"
#include "qemu/notify.h"
#include "qom/object.h"

#define MPC85XX_LAW_OFFSET         0x0ULL
#define MPC85XX_LAW_SIZE           0x1000ULL

typedef struct {
    uint32_t bar;
    uint32_t attributes;

    MemoryRegion mr;
} LawInfo;

struct PPCE500LAWState {
    /*< private >*/
    SysBusDevice parent;
    /*< public >*/

    MemoryRegion mmio;
    MemoryRegion law_ops;
    MemoryRegion *ccsr;
    MemoryRegion *system_memory;
    MemoryRegion *elbc;
    LawInfo law_info[12];

    Notifier ccsrbar_changed_notifier;
};

#define TYPE_E500_LAW "e500-law"
OBJECT_DECLARE_SIMPLE_TYPE(PPCE500LAWState, E500_LAW)

#endif /* E500_LAW_H */
