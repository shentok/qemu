#ifndef PPCE500_H
#define PPCE500_H

#include "hw/boards.h"
#include "hw/platform-bus.h"
#include "qom/object.h"

#define MPC8544_MPIC_REGS_OFFSET   0x40000ULL

struct boot_info {
    uint32_t dt_base;
    uint32_t dt_size;
    uint32_t entry;
};

struct PPCE500MachineState {
    MachineState parent_obj;

    /* points to instance of TYPE_PLATFORM_BUS_DEVICE if
     * board supports dynamic sysbus devices
     */
    PlatformBusDevice *pbus_dev;
    struct boot_info boot_info;
};

struct PPCE500MachineClass {
    MachineClass parent_class;

    /* required -- must at least add toplevel board compatible */
    void (*fixup_devtree)(void *fdt);

    int pci_first_slot;
    int pci_nr_slots;

    int mpic_version;
    bool has_mpc8xxx_gpio;
    bool has_esdhc;
    hwaddr platform_bus_base;
    hwaddr platform_bus_size;
    int platform_bus_first_irq;
    int platform_bus_num_irqs;
    hwaddr ccsrbar_base;
    hwaddr pci_pio_base;
    hwaddr pci_mmio_base;
    hwaddr pci_mmio_bus_base;
    hwaddr spin_base;
};

void ppce500_init(MachineState *machine);

#define TYPE_PPCE500_MACHINE      "ppce500-base-machine"
OBJECT_DECLARE_TYPE(PPCE500MachineState, PPCE500MachineClass, PPCE500_MACHINE)

#endif
