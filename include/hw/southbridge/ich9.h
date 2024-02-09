#ifndef HW_SOUTHBRIDGE_ICH9_H
#define HW_SOUTHBRIDGE_ICH9_H

#include "hw/isa/apm.h"
#include "hw/acpi/ich9.h"
#include "hw/intc/ioapic.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_device.h"
#include "hw/rtc/mc146818rtc.h"
#include "exec/memory.h"
#include "qemu/notify.h"
#include "qom/object.h"

#define TYPE_ICH9_SOUTHBRIDGE "ICH9-southbridge"
OBJECT_DECLARE_SIMPLE_TYPE(ICH9State, ICH9_SOUTHBRIDGE)

#define ICH9_CC_SIZE (16 * 1024) /* 16KB. Chipset configuration registers */

#define TYPE_ICH9_LPC_DEVICE "ICH9-LPC"
OBJECT_DECLARE_SIMPLE_TYPE(ICH9LPCState, ICH9_LPC_DEVICE)

struct ICH9LPCState {
    /* ICH9 LPC PCI to ISA bridge */
    PCIDevice d;

    /* (pci device, intx) -> pirq
     * In real chipset case, the unused slots are never used
     * as ICH9 supports only D25-D31 irq routing.
     * On the other hand in qemu case, any slot/function can be populated
     * via command line option.
     * So fallback interrupt routing for any devices in any slots is necessary.
    */
    uint8_t irr[PCI_SLOT_MAX][PCI_NUM_PINS];

    MC146818RtcState rtc;
    APMState apm;
    ICH9LPCPMRegs pm;
    uint32_t sci_level; /* track sci level */
    uint8_t sci_gsi;

    /* 2.24 Pin Straps */
    struct {
        bool spkr_hi;
    } pin_strap;

    /* 10.1 Chipset Configuration registers(Memory Space)
     which is pointed by RCBA */
    uint8_t chip_config[ICH9_CC_SIZE];

    /*
     * 13.7.5 RST_CNT---Reset Control Register (LPC I/F---D31:F0)
     *
     * register contents and IO memory region
     */
    uint8_t rst_cnt;
    MemoryRegion rst_cnt_mem;

    /* SMI feature negotiation via fw_cfg */
    uint64_t smi_host_features;       /* guest-invisible, host endian */
    uint8_t smi_host_features_le[8];  /* guest-visible, read-only, little
                                       * endian uint64_t */
    uint8_t smi_guest_features_le[8]; /* guest-visible, read-write, little
                                       * endian uint64_t */
    uint8_t smi_features_ok;          /* guest-visible, read-only; selecting it
                                       * triggers feature lockdown */
    uint64_t smi_negotiated_features; /* guest-invisible, host endian */

    MemoryRegion rcrb_mem; /* root complex register block */
    Notifier machine_ready;

    qemu_irq gsi[IOAPIC_NUM_PINS];
};

#define ICH9_MASK(bit, ms_bit, ls_bit) \
((uint##bit##_t)(((1ULL << ((ms_bit) + 1)) - 1) & ~((1ULL << ls_bit) - 1)))

/* ICH9: Chipset Configuration Registers */
#define ICH9_CC_ADDR_MASK                       (ICH9_CC_SIZE - 1)

#define ICH9_CC
#define ICH9_CC_D28IP                           0x310C
#define ICH9_CC_D28IP_SHIFT                     4
#define ICH9_CC_D28IP_MASK                      0xf
#define ICH9_CC_D28IP_DEFAULT                   0x00214321
#define ICH9_CC_D31IR                           0x3140
#define ICH9_CC_D30IR                           0x3142
#define ICH9_CC_D29IR                           0x3144
#define ICH9_CC_D28IR                           0x3146
#define ICH9_CC_D27IR                           0x3148
#define ICH9_CC_D26IR                           0x314C
#define ICH9_CC_D25IR                           0x3150
#define ICH9_CC_DIR_DEFAULT                     0x3210
#define ICH9_CC_D30IR_DEFAULT                   0x0
#define ICH9_CC_DIR_SHIFT                       4
#define ICH9_CC_DIR_MASK                        0x7
#define ICH9_CC_OIC                             0x31FF
#define ICH9_CC_OIC_AEN                         0x1
#define ICH9_CC_GCS                             0x3410
#define ICH9_CC_GCS_DEFAULT                     0x00000020
#define ICH9_CC_GCS_NO_REBOOT                   (1 << 5)

/* D28:F[0-5] */
#define ICH9_PCIE_DEV                           28
#define ICH9_PCIE_FUNC_MAX                      6

/* D31:F0 LPC Processor Interface */
#define ICH9_RST_CNT_IOPORT                     0xCF9

/* D31:F1 LPC controller */
#define ICH9_A2_LPC                             "ICH9 A2 LPC"
#define ICH9_A2_LPC_SAVEVM_VERSION              0

#define ICH9_LPC_DEV                            31
#define ICH9_LPC_FUNC                           0

#define ICH9_A2_LPC_REVISION                    0x2
#define ICH9_LPC_NB_PIRQS                       8       /* PCI A-H */

#define ICH9_LPC_PMBASE                         0x40
#define ICH9_LPC_PMBASE_BASE_ADDRESS_MASK       ICH9_MASK(32, 15, 7)
#define ICH9_LPC_PMBASE_RTE                     0x1
#define ICH9_LPC_PMBASE_DEFAULT                 0x1

#define ICH9_LPC_ACPI_CTRL                      0x44
#define ICH9_LPC_ACPI_CTRL_ACPI_EN              0x80
#define ICH9_LPC_ACPI_CTRL_SCI_IRQ_SEL_MASK     ICH9_MASK(8, 2, 0)
#define ICH9_LPC_ACPI_CTRL_9                    0x0
#define ICH9_LPC_ACPI_CTRL_10                   0x1
#define ICH9_LPC_ACPI_CTRL_11                   0x2
#define ICH9_LPC_ACPI_CTRL_20                   0x4
#define ICH9_LPC_ACPI_CTRL_21                   0x5
#define ICH9_LPC_ACPI_CTRL_DEFAULT              0x0

#define ICH9_LPC_PIRQA_ROUT                     0x60
#define ICH9_LPC_PIRQB_ROUT                     0x61
#define ICH9_LPC_PIRQC_ROUT                     0x62
#define ICH9_LPC_PIRQD_ROUT                     0x63

#define ICH9_LPC_PIRQE_ROUT                     0x68
#define ICH9_LPC_PIRQF_ROUT                     0x69
#define ICH9_LPC_PIRQG_ROUT                     0x6a
#define ICH9_LPC_PIRQH_ROUT                     0x6b

#define ICH9_LPC_PIRQ_ROUT_IRQEN                0x80
#define ICH9_LPC_PIRQ_ROUT_MASK                 ICH9_MASK(8, 3, 0)
#define ICH9_LPC_PIRQ_ROUT_DEFAULT              0x80

#define ICH9_LPC_GEN_PMCON_1                    0xa0
#define ICH9_LPC_GEN_PMCON_1_SMI_LOCK           (1 << 4)
#define ICH9_LPC_GEN_PMCON_2                    0xa2
#define ICH9_LPC_GEN_PMCON_3                    0xa4
#define ICH9_LPC_GEN_PMCON_LOCK                 0xa6

#define ICH9_LPC_RCBA                           0xf0
#define ICH9_LPC_RCBA_BA_MASK                   ICH9_MASK(32, 31, 14)
#define ICH9_LPC_RCBA_EN                        0x1
#define ICH9_LPC_RCBA_DEFAULT                   0x0

#define ICH9_LPC_PIC_NUM_PINS                   16
#define ICH9_LPC_IOAPIC_NUM_PINS                24

#define ICH9_GPIO_GSI "gsi"

/* D31:F0 power management I/O registers
   offset from the address ICH9_LPC_PMBASE */

/* FADT ACPI_ENABLE/ACPI_DISABLE */
#define ICH9_APM_ACPI_ENABLE                    0x2
#define ICH9_APM_ACPI_DISABLE                   0x3

#define ICH9_LPC_SMI_NEGOTIATED_FEAT_PROP "x-smi-negotiated-features"

/* bit positions used in fw_cfg SMI feature negotiation */
#define ICH9_LPC_SMI_F_BROADCAST_BIT            0
#define ICH9_LPC_SMI_F_CPU_HOTPLUG_BIT          1
#define ICH9_LPC_SMI_F_CPU_HOT_UNPLUG_BIT       2

#endif /* HW_SOUTHBRIDGE_ICH9_H */
