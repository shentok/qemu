#ifndef E500_CCSR_H
#define E500_CCSR_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define MPC8544_CCSRBAR_SIZE       0x00100000ULL

struct PPCE500CCSRState {
    /*< private >*/
    SysBusDevice parent;
    /*< public >*/

    MemoryRegion ccsr_space;
};

#define TYPE_CCSR "e500-ccsr"
OBJECT_DECLARE_SIMPLE_TYPE(PPCE500CCSRState, CCSR)

#endif /* E500_CCSR_H */
