#ifndef E500_ELBC_H
#define E500_ELBC_H

#include "hw/sysbus.h"
#include "hw/qdev-core.h"
#include "exec/memory.h"
#include "qom/object.h"

#define MPC85XX_ELBC_OFFSET        0x5000UL

typedef struct {
    uint32_t base;
    uint32_t options;

    MemoryRegion mr;
    MemoryRegion *dev;
} ElbcChipSelect;

struct PPCE500ELbcState {
    /*< private >*/
    SysBusDevice parent;
    /*< public >*/

    MemoryRegion boot_page;
    MemoryRegion ops;
    ElbcChipSelect chip_selects[8];
};

#define TYPE_E500_ELBC "e500-elbc"
OBJECT_DECLARE_SIMPLE_TYPE(PPCE500ELbcState, E500_ELBC)

#endif /* E500_ELBC_H */
