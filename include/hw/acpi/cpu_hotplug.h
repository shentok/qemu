/*
 * QEMU ACPI hotplug utilities
 *
 * Copyright (C) 2013,2016 Red Hat Inc
 *
 * Authors:
 *   Igor Mammedov <imammedo@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef HW_ACPI_CPU_HOTPLUG_H
#define HW_ACPI_CPU_HOTPLUG_H

#include "hw/acpi/pc-hotplug.h"
#include "hw/acpi/aml-build.h"
#include "hw/hotplug.h"
#include "hw/qdev-core.h"
#include "hw/boards.h"
#include "qapi/qapi-types-acpi.h"
#include "exec/memory.h"


#define ACPI_CPU_HOTPLUG_REG_LEN 12
#define ACPI_CPU_FLAGS_OFFSET_RW 4

#define OVMF_CPUHP_SMI_CMD 4

enum {
    CPHP_GET_NEXT_CPU_WITH_EVENT_CMD = 0,
    CPHP_OST_EVENT_CMD = 1,
    CPHP_OST_STATUS_CMD = 2,
    CPHP_GET_CPU_ID_CMD = 3,
    CPHP_CMD_MAX
};

typedef struct AcpiCpuStatus {
    struct CPUState *cpu;
    uint64_t arch_id;
    bool is_inserting;
    bool is_removing;
    bool fw_remove;
    uint32_t ost_event;
    uint32_t ost_status;
} AcpiCpuStatus;

typedef struct CPUHotplugState {
    MemoryRegion ctrl_reg;
    uint32_t selector;
    uint8_t command;
    uint32_t dev_count;
    AcpiCpuStatus *devs;
} CPUHotplugState;

typedef struct AcpiCpuHotplug {
    Object *device;
    MemoryRegion io;
    uint8_t sts[ACPI_GPE_PROC_LEN];
} AcpiCpuHotplug;

void acpi_cpu_plug_cb(HotplugHandler *hotplug_dev,
                      CPUHotplugState *cpu_st, DeviceState *dev, Error **errp);

void acpi_cpu_unplug_request_cb(HotplugHandler *hotplug_dev,
                                CPUHotplugState *cpu_st,
                                DeviceState *dev, Error **errp);

void acpi_cpu_unplug_cb(CPUHotplugState *cpu_st,
                        DeviceState *dev, Error **errp);

typedef struct CPUHotplugFeatures {
    bool acpi_1_compatible;
    bool has_legacy_cphp;
    bool fw_unplugs_cpu;
    const char *smi_path;
} CPUHotplugFeatures;

void build_cpus_aml(Aml *table, const CPUArchIdList *arch_ids,
                    CPUHotplugFeatures opts, hwaddr io_base,
                    const char *res_root,
                    const char *event_handler_method);

void acpi_cpu_ospm_status(CPUHotplugState *cpu_st, ACPIOSTInfoList ***list);

void legacy_acpi_cpu_plug_cb(HotplugHandler *hotplug_dev,
                             AcpiCpuHotplug *g, DeviceState *dev, Error **errp);

void legacy_acpi_cpu_hotplug_init(MemoryRegion *parent, Object *owner,
                                  AcpiCpuHotplug *gpe_cpu, uint16_t base);

void acpi_switch_to_modern_cphp(AcpiCpuHotplug *gpe_cpu,
                                CPUHotplugState *cpuhp_state,
                                uint16_t io_port);

void build_legacy_cpu_hotplug_aml(Aml *ctx, const CPUArchIdList *apic_ids,
                                  unsigned apic_id_limit, uint16_t io_base);

extern const VMStateDescription vmstate_cpu_hotplug;
#define VMSTATE_CPU_HOTPLUG(cpuhp, state) \
    VMSTATE_STRUCT(cpuhp, state, 1, \
                   vmstate_cpu_hotplug, CPUHotplugState)
#endif
