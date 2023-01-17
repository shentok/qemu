/*
 * QEMU ACPI hotplug utilities
 *
 * Copyright (C) 2013 Red Hat Inc
 *
 * Authors:
 *   Igor Mammedov <imammedo@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "hw/acpi/cpu_hotplug.h"
#include "hw/acpi/acpi_cpu_interface.h"
#include "hw/acpi/acpi_dev_interface.h"
#include "qapi/error.h"
#include "qapi/qapi-events-acpi.h"
#include "hw/core/cpu.h"
#include "hw/i386/pc.h"
#include "hw/pci/pci.h"
#include "migration/vmstate.h"
#include "qemu/error-report.h"
#include "sysemu/numa.h"
#include "trace.h"

#define ACPI_CPU_SELECTOR_OFFSET_WR 0
#define ACPI_CPU_CMD_OFFSET_WR 5
#define ACPI_CPU_CMD_DATA_OFFSET_RW 8
#define ACPI_CPU_CMD_DATA2_OFFSET_R 0

static ACPIOSTInfo *acpi_cpu_device_status(int idx, AcpiCpuStatus *cdev)
{
    ACPIOSTInfo *info = g_new0(ACPIOSTInfo, 1);

    info->slot_type = ACPI_SLOT_TYPE_CPU;
    info->slot = g_strdup_printf("%d", idx);
    info->source = cdev->ost_event;
    info->status = cdev->ost_status;
    if (cdev->cpu) {
        DeviceState *dev = DEVICE(cdev->cpu);
        if (dev->id) {
            info->device = g_strdup(dev->id);
        }
    }
    return info;
}

void acpi_cpu_ospm_status(CPUHotplugState *cpu_st, ACPIOSTInfoList ***list)
{
    ACPIOSTInfoList ***tail = list;
    int i;

    for (i = 0; i < cpu_st->dev_count; i++) {
        QAPI_LIST_APPEND(*tail, acpi_cpu_device_status(i, &cpu_st->devs[i]));
    }
}

static uint64_t cpu_hotplug_rd(void *opaque, hwaddr addr, unsigned size)
{
    uint64_t val = 0;
    CPUHotplugState *cpu_st = opaque;
    AcpiCpuStatus *cdev;

    if (cpu_st->selector >= cpu_st->dev_count) {
        return val;
    }

    cdev = &cpu_st->devs[cpu_st->selector];
    switch (addr) {
    case ACPI_CPU_FLAGS_OFFSET_RW: /* pack and return is_* fields */
        val |= cdev->cpu ? 1 : 0;
        val |= cdev->is_inserting ? 2 : 0;
        val |= cdev->is_removing  ? 4 : 0;
        val |= cdev->fw_remove  ? 16 : 0;
        trace_cpuhp_acpi_read_flags(cpu_st->selector, val);
        break;
    case ACPI_CPU_CMD_DATA_OFFSET_RW:
        switch (cpu_st->command) {
        case CPHP_GET_NEXT_CPU_WITH_EVENT_CMD:
           val = cpu_st->selector;
           break;
        case CPHP_GET_CPU_ID_CMD:
           val = cdev->arch_id & 0xFFFFFFFF;
           break;
        default:
           break;
        }
        trace_cpuhp_acpi_read_cmd_data(cpu_st->selector, val);
        break;
    case ACPI_CPU_CMD_DATA2_OFFSET_R:
        switch (cpu_st->command) {
        case CPHP_GET_NEXT_CPU_WITH_EVENT_CMD:
           val = 0;
           break;
        case CPHP_GET_CPU_ID_CMD:
           val = cdev->arch_id >> 32;
           break;
        default:
           break;
        }
        trace_cpuhp_acpi_read_cmd_data2(cpu_st->selector, val);
        break;
    default:
        break;
    }
    return val;
}

static void cpu_hotplug_wr(void *opaque, hwaddr addr, uint64_t data,
                           unsigned int size)
{
    CPUHotplugState *cpu_st = opaque;
    AcpiCpuStatus *cdev;
    ACPIOSTInfo *info;

    assert(cpu_st->dev_count);

    if (addr) {
        if (cpu_st->selector >= cpu_st->dev_count) {
            trace_cpuhp_acpi_invalid_idx_selected(cpu_st->selector);
            return;
        }
    }

    switch (addr) {
    case ACPI_CPU_SELECTOR_OFFSET_WR: /* current CPU selector */
        cpu_st->selector = data;
        trace_cpuhp_acpi_write_idx(cpu_st->selector);
        break;
    case ACPI_CPU_FLAGS_OFFSET_RW: /* set is_* fields  */
        cdev = &cpu_st->devs[cpu_st->selector];
        if (data & 2) { /* clear insert event */
            cdev->is_inserting = false;
            trace_cpuhp_acpi_clear_inserting_evt(cpu_st->selector);
        } else if (data & 4) { /* clear remove event */
            cdev->is_removing = false;
            trace_cpuhp_acpi_clear_remove_evt(cpu_st->selector);
        } else if (data & 8) {
            DeviceState *dev = NULL;
            HotplugHandler *hotplug_ctrl = NULL;

            if (!cdev->cpu || cdev->cpu == first_cpu) {
                trace_cpuhp_acpi_ejecting_invalid_cpu(cpu_st->selector);
                break;
            }

            trace_cpuhp_acpi_ejecting_cpu(cpu_st->selector);
            dev = DEVICE(cdev->cpu);
            hotplug_ctrl = qdev_get_hotplug_handler(dev);
            hotplug_handler_unplug(hotplug_ctrl, dev, NULL);
            object_unparent(OBJECT(dev));
            cdev->fw_remove = false;
        } else if (data & 16) {
            if (!cdev->cpu || cdev->cpu == first_cpu) {
                trace_cpuhp_acpi_fw_remove_invalid_cpu(cpu_st->selector);
                break;
            }
            trace_cpuhp_acpi_fw_remove_cpu(cpu_st->selector);
            cdev->fw_remove = true;
        }
        break;
    case ACPI_CPU_CMD_OFFSET_WR:
        trace_cpuhp_acpi_write_cmd(cpu_st->selector, data);
        if (data < CPHP_CMD_MAX) {
            cpu_st->command = data;
            if (cpu_st->command == CPHP_GET_NEXT_CPU_WITH_EVENT_CMD) {
                uint32_t iter = cpu_st->selector;

                do {
                    cdev = &cpu_st->devs[iter];
                    if (cdev->is_inserting || cdev->is_removing ||
                        cdev->fw_remove) {
                        cpu_st->selector = iter;
                        trace_cpuhp_acpi_cpu_has_events(cpu_st->selector,
                            cdev->is_inserting, cdev->is_removing);
                        break;
                    }
                    iter = iter + 1 < cpu_st->dev_count ? iter + 1 : 0;
                } while (iter != cpu_st->selector);
            }
        }
        break;
    case ACPI_CPU_CMD_DATA_OFFSET_RW:
        switch (cpu_st->command) {
        case CPHP_OST_EVENT_CMD: {
           cdev = &cpu_st->devs[cpu_st->selector];
           cdev->ost_event = data;
           trace_cpuhp_acpi_write_ost_ev(cpu_st->selector, cdev->ost_event);
           break;
        }
        case CPHP_OST_STATUS_CMD: {
           cdev = &cpu_st->devs[cpu_st->selector];
           cdev->ost_status = data;
           info = acpi_cpu_device_status(cpu_st->selector, cdev);
           qapi_event_send_acpi_device_ost(info);
           qapi_free_ACPIOSTInfo(info);
           trace_cpuhp_acpi_write_ost_status(cpu_st->selector,
                                             cdev->ost_status);
           break;
        }
        default:
           break;
        }
        break;
    default:
        break;
    }
}

static const MemoryRegionOps cpu_hotplug_ops = {
    .read = cpu_hotplug_rd,
    .write = cpu_hotplug_wr,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
};

static void cpu_hotplug_hw_init(MemoryRegion *as, Object *owner,
                                CPUHotplugState *state, hwaddr base_addr)
{
    MachineState *machine = MACHINE(qdev_get_machine());
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    const CPUArchIdList *id_list;
    int i;

    assert(mc->possible_cpu_arch_ids);
    id_list = mc->possible_cpu_arch_ids(machine);
    state->dev_count = id_list->len;
    state->devs = g_new0(typeof(*state->devs), state->dev_count);
    for (i = 0; i < id_list->len; i++) {
        state->devs[i].cpu =  CPU(id_list->cpus[i].cpu);
        state->devs[i].arch_id = id_list->cpus[i].arch_id;
    }
    memory_region_init_io(&state->ctrl_reg, owner, &cpu_hotplug_ops, state,
                          "acpi-cpu-hotplug", ACPI_CPU_HOTPLUG_REG_LEN);
    memory_region_add_subregion(as, base_addr, &state->ctrl_reg);
}

static AcpiCpuStatus *get_cpu_status(CPUHotplugState *cpu_st, DeviceState *dev)
{
    CPUClass *k = CPU_GET_CLASS(dev);
    uint64_t cpu_arch_id = k->get_arch_id(CPU(dev));
    int i;

    for (i = 0; i < cpu_st->dev_count; i++) {
        if (cpu_arch_id == cpu_st->devs[i].arch_id) {
            return &cpu_st->devs[i];
        }
    }
    return NULL;
}

void acpi_cpu_plug_cb(HotplugHandler *hotplug_dev,
                      CPUHotplugState *cpu_st, DeviceState *dev, Error **errp)
{
    AcpiCpuStatus *cdev;

    cdev = get_cpu_status(cpu_st, dev);
    if (!cdev) {
        return;
    }

    cdev->cpu = CPU(dev);
    if (dev->hotplugged) {
        cdev->is_inserting = true;
        acpi_send_event(DEVICE(hotplug_dev), ACPI_CPU_HOTPLUG_STATUS);
    }
}

void acpi_cpu_unplug_request_cb(HotplugHandler *hotplug_dev,
                                CPUHotplugState *cpu_st,
                                DeviceState *dev, Error **errp)
{
    AcpiCpuStatus *cdev;

    cdev = get_cpu_status(cpu_st, dev);
    if (!cdev) {
        return;
    }

    cdev->is_removing = true;
    acpi_send_event(DEVICE(hotplug_dev), ACPI_CPU_HOTPLUG_STATUS);
}

void acpi_cpu_unplug_cb(CPUHotplugState *cpu_st,
                        DeviceState *dev, Error **errp)
{
    AcpiCpuStatus *cdev;

    cdev = get_cpu_status(cpu_st, dev);
    if (!cdev) {
        return;
    }

    cdev->cpu = NULL;
}

static const VMStateDescription vmstate_cpuhp_sts = {
    .name = "CPU hotplug device state",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_BOOL(is_inserting, AcpiCpuStatus),
        VMSTATE_BOOL(is_removing, AcpiCpuStatus),
        VMSTATE_UINT32(ost_event, AcpiCpuStatus),
        VMSTATE_UINT32(ost_status, AcpiCpuStatus),
        VMSTATE_END_OF_LIST()
    }
};

const VMStateDescription vmstate_cpu_hotplug = {
    .name = "CPU hotplug state",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32(selector, CPUHotplugState),
        VMSTATE_UINT8(command, CPUHotplugState),
        VMSTATE_STRUCT_VARRAY_POINTER_UINT32(devs, CPUHotplugState, dev_count,
                                             vmstate_cpuhp_sts, AcpiCpuStatus),
        VMSTATE_END_OF_LIST()
    }
};

#define CPU_NAME_FMT      "C%.03X"
#define CPUHP_RES_DEVICE  "PRES"
#define CPU_LOCK          "CPLK"
#define CPU_STS_METHOD    "CSTA"
#define CPU_SCAN_METHOD   "CSCN"
#define CPU_NOTIFY_METHOD "CTFY"
#define CPU_EJECT_METHOD  "CEJ0"
#define CPU_OST_METHOD    "COST"
#define CPU_ADDED_LIST    "CNEW"

#define CPU_ENABLED       "CPEN"
#define CPU_SELECTOR      "CSEL"
#define CPU_COMMAND       "CCMD"
#define CPU_DATA          "CDAT"
#define CPU_INSERT_EVENT  "CINS"
#define CPU_REMOVE_EVENT  "CRMV"
#define CPU_EJECT_EVENT   "CEJ0"
#define CPU_FW_EJECT_EVENT "CEJF"

void build_cpus_aml(Aml *table, MachineState *machine, CPUHotplugFeatures opts,
                    hwaddr io_base,
                    const char *res_root,
                    const char *event_handler_method)
{
    Aml *ifctx;
    Aml *field;
    Aml *method;
    Aml *cpu_ctrl_dev;
    Aml *cpus_dev;
    Aml *zero = aml_int(0);
    Aml *one = aml_int(1);
    Aml *sb_scope = aml_scope("_SB");
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    const CPUArchIdList *arch_ids = mc->possible_cpu_arch_ids(machine);
    char *cphp_res_path = g_strdup_printf("%s." CPUHP_RES_DEVICE, res_root);

    cpu_ctrl_dev = aml_device("%s", cphp_res_path);
    {
        Aml *crs;

        aml_append(cpu_ctrl_dev,
            aml_name_decl("_HID", aml_eisaid("PNP0A06")));
        aml_append(cpu_ctrl_dev,
            aml_name_decl("_UID", aml_string("CPU Hotplug resources")));
        aml_append(cpu_ctrl_dev, aml_mutex(CPU_LOCK, 0));

        crs = aml_resource_template();
        aml_append(crs, aml_io(AML_DECODE16, io_base, io_base, 1,
                               ACPI_CPU_HOTPLUG_REG_LEN));
        aml_append(cpu_ctrl_dev, aml_name_decl("_CRS", crs));

        /* declare CPU hotplug MMIO region with related access fields */
        aml_append(cpu_ctrl_dev,
            aml_operation_region("PRST", AML_SYSTEM_IO, aml_int(io_base),
                                 ACPI_CPU_HOTPLUG_REG_LEN));

        field = aml_field("PRST", AML_BYTE_ACC, AML_NOLOCK,
                          AML_WRITE_AS_ZEROS);
        aml_append(field, aml_reserved_field(ACPI_CPU_FLAGS_OFFSET_RW * 8));
        /* 1 if enabled, read only */
        aml_append(field, aml_named_field(CPU_ENABLED, 1));
        /* (read) 1 if has a insert event. (write) 1 to clear event */
        aml_append(field, aml_named_field(CPU_INSERT_EVENT, 1));
        /* (read) 1 if has a remove event. (write) 1 to clear event */
        aml_append(field, aml_named_field(CPU_REMOVE_EVENT, 1));
        /* initiates device eject, write only */
        aml_append(field, aml_named_field(CPU_EJECT_EVENT, 1));
        /* tell firmware to do device eject, write only */
        aml_append(field, aml_named_field(CPU_FW_EJECT_EVENT, 1));
        aml_append(field, aml_reserved_field(3));
        aml_append(field, aml_named_field(CPU_COMMAND, 8));
        aml_append(cpu_ctrl_dev, field);

        field = aml_field("PRST", AML_DWORD_ACC, AML_NOLOCK, AML_PRESERVE);
        /* CPU selector, write only */
        aml_append(field, aml_named_field(CPU_SELECTOR, 32));
        /* flags + cmd + 2byte align */
        aml_append(field, aml_reserved_field(4 * 8));
        aml_append(field, aml_named_field(CPU_DATA, 32));
        aml_append(cpu_ctrl_dev, field);

        if (opts.has_legacy_cphp) {
            method = aml_method("_INI", 0, AML_SERIALIZED);
            /* switch off legacy CPU hotplug HW and use new one,
             * on reboot system is in new mode and writing 0
             * in CPU_SELECTOR selects BSP, which is NOP at
             * the time _INI is called */
            aml_append(method, aml_store(zero, aml_name(CPU_SELECTOR)));
            aml_append(cpu_ctrl_dev, method);
        }
    }
    aml_append(sb_scope, cpu_ctrl_dev);

    cpus_dev = aml_device("\\_SB.CPUS");
    {
        int i;
        Aml *ctrl_lock = aml_name("%s.%s", cphp_res_path, CPU_LOCK);
        Aml *cpu_selector = aml_name("%s.%s", cphp_res_path, CPU_SELECTOR);
        Aml *is_enabled = aml_name("%s.%s", cphp_res_path, CPU_ENABLED);
        Aml *cpu_cmd = aml_name("%s.%s", cphp_res_path, CPU_COMMAND);
        Aml *cpu_data = aml_name("%s.%s", cphp_res_path, CPU_DATA);
        Aml *ins_evt = aml_name("%s.%s", cphp_res_path, CPU_INSERT_EVENT);
        Aml *rm_evt = aml_name("%s.%s", cphp_res_path, CPU_REMOVE_EVENT);
        Aml *ej_evt = aml_name("%s.%s", cphp_res_path, CPU_EJECT_EVENT);
        Aml *fw_ej_evt = aml_name("%s.%s", cphp_res_path, CPU_FW_EJECT_EVENT);

        aml_append(cpus_dev, aml_name_decl("_HID", aml_string("ACPI0010")));
        aml_append(cpus_dev, aml_name_decl("_CID", aml_eisaid("PNP0A05")));

        method = aml_method(CPU_NOTIFY_METHOD, 2, AML_NOTSERIALIZED);
        for (i = 0; i < arch_ids->len; i++) {
            Aml *cpu = aml_name(CPU_NAME_FMT, i);
            Aml *uid = aml_arg(0);
            Aml *event = aml_arg(1);

            ifctx = aml_if(aml_equal(uid, aml_int(i)));
            {
                aml_append(ifctx, aml_notify(cpu, event));
            }
            aml_append(method, ifctx);
        }
        aml_append(cpus_dev, method);

        method = aml_method(CPU_STS_METHOD, 1, AML_SERIALIZED);
        {
            Aml *idx = aml_arg(0);
            Aml *sta = aml_local(0);

            aml_append(method, aml_acquire(ctrl_lock, 0xFFFF));
            aml_append(method, aml_store(idx, cpu_selector));
            aml_append(method, aml_store(zero, sta));
            ifctx = aml_if(aml_equal(is_enabled, one));
            {
                aml_append(ifctx, aml_store(aml_int(0xF), sta));
            }
            aml_append(method, ifctx);
            aml_append(method, aml_release(ctrl_lock));
            aml_append(method, aml_return(sta));
        }
        aml_append(cpus_dev, method);

        method = aml_method(CPU_EJECT_METHOD, 1, AML_SERIALIZED);
        {
            Aml *idx = aml_arg(0);

            aml_append(method, aml_acquire(ctrl_lock, 0xFFFF));
            aml_append(method, aml_store(idx, cpu_selector));
            if (opts.fw_unplugs_cpu) {
                aml_append(method, aml_store(one, fw_ej_evt));
                aml_append(method, aml_store(aml_int(OVMF_CPUHP_SMI_CMD),
                           aml_name("%s", opts.smi_path)));
            } else {
                aml_append(method, aml_store(one, ej_evt));
            }
            aml_append(method, aml_release(ctrl_lock));
        }
        aml_append(cpus_dev, method);

        method = aml_method(CPU_SCAN_METHOD, 0, AML_SERIALIZED);
        {
            const uint8_t max_cpus_per_pass = 255;
            Aml *else_ctx;
            Aml *while_ctx, *while_ctx2;
            Aml *has_event = aml_local(0);
            Aml *dev_chk = aml_int(1);
            Aml *eject_req = aml_int(3);
            Aml *next_cpu_cmd = aml_int(CPHP_GET_NEXT_CPU_WITH_EVENT_CMD);
            Aml *num_added_cpus = aml_local(1);
            Aml *cpu_idx = aml_local(2);
            Aml *uid = aml_local(3);
            Aml *has_job = aml_local(4);
            Aml *new_cpus = aml_name(CPU_ADDED_LIST);

            aml_append(method, aml_acquire(ctrl_lock, 0xFFFF));

            /*
             * Windows versions newer than XP (including Windows 10/Windows
             * Server 2019), do support* VarPackageOp but, it is cripled to hold
             * the same elements number as old PackageOp.
             * For compatibility with Windows XP (so it won't crash) use ACPI1.0
             * PackageOp which can hold max 255 elements.
             *
             * use named package as old Windows don't support it in local var
             */
            aml_append(method, aml_name_decl(CPU_ADDED_LIST,
                                             aml_package(max_cpus_per_pass)));

            aml_append(method, aml_store(zero, uid));
            aml_append(method, aml_store(one, has_job));
            /*
             * CPU_ADDED_LIST can hold limited number of elements, outer loop
             * allows to process CPUs in batches which let us to handle more
             * CPUs than CPU_ADDED_LIST can hold.
             */
            while_ctx2 = aml_while(aml_equal(has_job, one));
            {
                aml_append(while_ctx2, aml_store(zero, has_job));

                aml_append(while_ctx2, aml_store(one, has_event));
                aml_append(while_ctx2, aml_store(zero, num_added_cpus));

                /*
                 * Scan CPUs, till there are CPUs with events or
                 * CPU_ADDED_LIST capacity is exhausted
                 */
                while_ctx = aml_while(aml_land(aml_equal(has_event, one),
                                      aml_lless(uid, aml_int(arch_ids->len))));
                {
                     /*
                      * clear loop exit condition, ins_evt/rm_evt checks will
                      * set it to 1 while next_cpu_cmd returns a CPU with events
                      */
                     aml_append(while_ctx, aml_store(zero, has_event));

                     aml_append(while_ctx, aml_store(uid, cpu_selector));
                     aml_append(while_ctx, aml_store(next_cpu_cmd, cpu_cmd));

                     /*
                      * wrap around case, scan is complete, exit loop.
                      * It happens since events are not cleared in scan loop,
                      * so next_cpu_cmd continues to find already processed CPUs
                      */
                     ifctx = aml_if(aml_lless(cpu_data, uid));
                     {
                         aml_append(ifctx, aml_break());
                     }
                     aml_append(while_ctx, ifctx);

                     /*
                      * if CPU_ADDED_LIST is full, exit inner loop and process
                      * collected CPUs
                      */
                     ifctx = aml_if(
                         aml_equal(num_added_cpus, aml_int(max_cpus_per_pass)));
                     {
                         aml_append(ifctx, aml_store(one, has_job));
                         aml_append(ifctx, aml_break());
                     }
                     aml_append(while_ctx, ifctx);

                     aml_append(while_ctx, aml_store(cpu_data, uid));
                     ifctx = aml_if(aml_equal(ins_evt, one));
                     {
                         /* cache added CPUs to Notify/Wakeup later */
                         aml_append(ifctx, aml_store(uid,
                             aml_index(new_cpus, num_added_cpus)));
                         aml_append(ifctx, aml_increment(num_added_cpus));
                         aml_append(ifctx, aml_store(one, has_event));
                     }
                     aml_append(while_ctx, ifctx);
                     else_ctx = aml_else();
                     ifctx = aml_if(aml_equal(rm_evt, one));
                     {
                         aml_append(ifctx,
                             aml_call2(CPU_NOTIFY_METHOD, uid, eject_req));
                         aml_append(ifctx, aml_store(one, rm_evt));
                         aml_append(ifctx, aml_store(one, has_event));
                     }
                     aml_append(else_ctx, ifctx);
                     aml_append(while_ctx, else_ctx);
                     aml_append(while_ctx, aml_increment(uid));
                }
                aml_append(while_ctx2, while_ctx);

                /*
                 * in case FW negotiated ICH9_LPC_SMI_F_CPU_HOTPLUG_BIT,
                 * make upcall to FW, so it can pull in new CPUs before
                 * OS is notified and wakes them up
                 */
                if (opts.smi_path) {
                    ifctx = aml_if(aml_lgreater(num_added_cpus, zero));
                    {
                        aml_append(ifctx, aml_store(aml_int(OVMF_CPUHP_SMI_CMD),
                            aml_name("%s", opts.smi_path)));
                    }
                    aml_append(while_ctx2, ifctx);
                }

                /* Notify OSPM about new CPUs and clear insert events */
                aml_append(while_ctx2, aml_store(zero, cpu_idx));
                while_ctx = aml_while(aml_lless(cpu_idx, num_added_cpus));
                {
                    aml_append(while_ctx,
                        aml_store(aml_derefof(aml_index(new_cpus, cpu_idx)),
                                  uid));
                    aml_append(while_ctx,
                        aml_call2(CPU_NOTIFY_METHOD, uid, dev_chk));
                    aml_append(while_ctx, aml_store(uid, aml_debug()));
                    aml_append(while_ctx, aml_store(uid, cpu_selector));
                    aml_append(while_ctx, aml_store(one, ins_evt));
                    aml_append(while_ctx, aml_increment(cpu_idx));
                }
                aml_append(while_ctx2, while_ctx);
                /*
                 * If another batch is needed, then it will resume scanning
                 * exactly at -- and not after -- the last CPU that's currently
                 * in CPU_ADDED_LIST. In other words, the last CPU in
                 * CPU_ADDED_LIST is going to be re-checked. That's OK: we've
                 * just cleared the insert event for *all* CPUs in
                 * CPU_ADDED_LIST, including the last one. So the scan will
                 * simply seek past it.
                 */
            }
            aml_append(method, while_ctx2);
            aml_append(method, aml_release(ctrl_lock));
        }
        aml_append(cpus_dev, method);

        method = aml_method(CPU_OST_METHOD, 4, AML_SERIALIZED);
        {
            Aml *uid = aml_arg(0);
            Aml *ev_cmd = aml_int(CPHP_OST_EVENT_CMD);
            Aml *st_cmd = aml_int(CPHP_OST_STATUS_CMD);

            aml_append(method, aml_acquire(ctrl_lock, 0xFFFF));
            aml_append(method, aml_store(uid, cpu_selector));
            aml_append(method, aml_store(ev_cmd, cpu_cmd));
            aml_append(method, aml_store(aml_arg(1), cpu_data));
            aml_append(method, aml_store(st_cmd, cpu_cmd));
            aml_append(method, aml_store(aml_arg(2), cpu_data));
            aml_append(method, aml_release(ctrl_lock));
        }
        aml_append(cpus_dev, method);

        /* build Processor object for each processor */
        for (i = 0; i < arch_ids->len; i++) {
            Aml *dev;
            Aml *uid = aml_int(i);
            ObjectClass *oc = object_class_by_name(arch_ids->cpus[i].type);
            AcpiCpuAmlIfClass *acpuac = ACPI_CPU_AML_IF_CLASS(oc);
            GArray *madt_buf = g_array_new(0, 1, 1);
            int arch_id = arch_ids->cpus[i].arch_id;

            if (opts.acpi_1_compatible && arch_id < 255) {
                dev = aml_processor(i, 0, 0, CPU_NAME_FMT, i);
            } else {
                dev = aml_device(CPU_NAME_FMT, i);
                aml_append(dev, aml_name_decl("_HID", aml_string("ACPI0007")));
                aml_append(dev, aml_name_decl("_UID", uid));
            }

            method = aml_method("_STA", 0, AML_SERIALIZED);
            aml_append(method, aml_return(aml_call1(CPU_STS_METHOD, uid)));
            aml_append(dev, method);

            /* build _MAT object */
            assert(acpuac && acpuac->madt_cpu);
            acpuac->madt_cpu(i, arch_ids, madt_buf,
                             true); /* set enabled flag */
            aml_append(dev, aml_name_decl("_MAT",
                aml_buffer(madt_buf->len, (uint8_t *)madt_buf->data)));
            g_array_free(madt_buf, true);

            if (CPU(arch_ids->cpus[i].cpu) != first_cpu) {
                method = aml_method("_EJ0", 1, AML_NOTSERIALIZED);
                aml_append(method, aml_call1(CPU_EJECT_METHOD, uid));
                aml_append(dev, method);
            }

            method = aml_method("_OST", 3, AML_SERIALIZED);
            aml_append(method,
                aml_call4(CPU_OST_METHOD, uid, aml_arg(0),
                          aml_arg(1), aml_arg(2))
            );
            aml_append(dev, method);

            /* Linux guests discard SRAT info for non-present CPUs
             * as a result _PXM is required for all CPUs which might
             * be hot-plugged. For simplicity, add it for all CPUs.
             */
            if (arch_ids->cpus[i].props.has_node_id) {
                aml_append(dev, aml_name_decl("_PXM",
                           aml_int(arch_ids->cpus[i].props.node_id)));
            }

            aml_append(cpus_dev, dev);
        }
    }
    aml_append(sb_scope, cpus_dev);
    aml_append(table, sb_scope);

    method = aml_method(event_handler_method, 0, AML_NOTSERIALIZED);
    aml_append(method, aml_call0("\\_SB.CPUS." CPU_SCAN_METHOD));
    aml_append(table, method);

    g_free(cphp_res_path);
}

#define CPU_EJECT_METHOD_LEGACY "CPEJ"
#define CPU_MAT_METHOD "CPMA"
#define CPU_ON_BITMAP "CPON"
#define CPU_STATUS_METHOD "CPST"
#define CPU_STATUS_MAP "PRS"
#define CPU_SCAN_METHOD_LEGACY "PRSC"

static uint64_t cpu_status_read(void *opaque, hwaddr addr, unsigned int size)
{
    AcpiCpuHotplug *cpus = opaque;
    uint64_t val = cpus->sts[addr];

    return val;
}

static void cpu_status_write(void *opaque, hwaddr addr, uint64_t data,
                             unsigned int size)
{
    /* firmware never used to write in CPU present bitmap so use
       this fact as means to switch QEMU into modern CPU hotplug
       mode by writing 0 at the beginning of legacy CPU bitmap
     */
    if (addr == 0 && data == 0) {
        AcpiCpuHotplug *cpus = opaque;
        object_property_set_bool(cpus->device, "cpu-hotplug-legacy", false,
                                 &error_abort);
    }
}

static const MemoryRegionOps AcpiCpuHotplug_ops = {
    .read = cpu_status_read,
    .write = cpu_status_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    },
    .impl = {
        .max_access_size = 1,
    },
};

static void acpi_set_cpu_present_bit(AcpiCpuHotplug *g, CPUState *cpu)
{
    CPUClass *k = CPU_GET_CLASS(cpu);
    int64_t cpu_id;

    cpu_id = k->get_arch_id(cpu);
    if ((cpu_id / 8) >= ACPI_GPE_PROC_LEN) {
        object_property_set_bool(g->device, "cpu-hotplug-legacy", false,
                                 &error_abort);
        return;
    }

    g->sts[cpu_id / 8] |= (1 << (cpu_id % 8));
}

void legacy_acpi_cpu_plug_cb(HotplugHandler *hotplug_dev,
                             AcpiCpuHotplug *g, DeviceState *dev, Error **errp)
{
    acpi_set_cpu_present_bit(g, CPU(dev));
    acpi_send_event(DEVICE(hotplug_dev), ACPI_CPU_HOTPLUG_STATUS);
}

void legacy_acpi_cpu_hotplug_init(MemoryRegion *parent, Object *owner,
                                  AcpiCpuHotplug *gpe_cpu, uint16_t base)
{
    CPUState *cpu;

    memory_region_init_io(&gpe_cpu->io, owner, &AcpiCpuHotplug_ops,
                          gpe_cpu, "acpi-cpu-hotplug", ACPI_GPE_PROC_LEN);
    memory_region_add_subregion(parent, base, &gpe_cpu->io);
    gpe_cpu->device = owner;

    CPU_FOREACH(cpu) {
        acpi_set_cpu_present_bit(gpe_cpu, cpu);
    }
}

void acpi_switch_to_modern_cphp(AcpiCpuHotplug *gpe_cpu,
                                CPUHotplugState *cpuhp_state,
                                uint16_t io_port)
{
    MemoryRegion *parent = pci_address_space_io(PCI_DEVICE(gpe_cpu->device));

    memory_region_del_subregion(parent, &gpe_cpu->io);
    cpu_hotplug_hw_init(parent, gpe_cpu->device, cpuhp_state, io_port);
}

void build_legacy_cpu_hotplug_aml(Aml *ctx,const CPUArchIdList *apic_ids,
                                  unsigned apic_id_limit, uint16_t io_base)
{
    Aml *dev;
    Aml *crs;
    Aml *pkg;
    Aml *field;
    Aml *method;
    Aml *if_ctx;
    Aml *else_ctx;
    int i, apic_idx;
    Aml *sb_scope = aml_scope("_SB");
    uint8_t madt_tmpl[8] = {0x00, 0x08, 0x00, 0x00, 0x00, 0, 0, 0};
    Aml *cpu_id = aml_arg(1);
    Aml *apic_id = aml_arg(0);
    Aml *cpu_on = aml_local(0);
    Aml *madt = aml_local(1);
    Aml *cpus_map = aml_name(CPU_ON_BITMAP);
    Aml *zero = aml_int(0);
    Aml *one = aml_int(1);

    /*
     * _MAT method - creates an madt apic buffer
     * apic_id = Arg0 = Local APIC ID
     * cpu_id  = Arg1 = Processor ID
     * cpu_on = Local0 = CPON flag for this cpu
     * madt = Local1 = Buffer (in madt apic form) to return
     */
    method = aml_method(CPU_MAT_METHOD, 2, AML_NOTSERIALIZED);
    aml_append(method,
        aml_store(aml_derefof(aml_index(cpus_map, apic_id)), cpu_on));
    aml_append(method,
        aml_store(aml_buffer(sizeof(madt_tmpl), madt_tmpl), madt));
    /* Update the processor id, lapic id, and enable/disable status */
    aml_append(method, aml_store(cpu_id, aml_index(madt, aml_int(2))));
    aml_append(method, aml_store(apic_id, aml_index(madt, aml_int(3))));
    aml_append(method, aml_store(cpu_on, aml_index(madt, aml_int(4))));
    aml_append(method, aml_return(madt));
    aml_append(sb_scope, method);

    /*
     * _STA method - return ON status of cpu
     * apic_id = Arg0 = Local APIC ID
     * cpu_on = Local0 = CPON flag for this cpu
     */
    method = aml_method(CPU_STATUS_METHOD, 1, AML_NOTSERIALIZED);
    aml_append(method,
        aml_store(aml_derefof(aml_index(cpus_map, apic_id)), cpu_on));
    if_ctx = aml_if(cpu_on);
    {
        aml_append(if_ctx, aml_return(aml_int(0xF)));
    }
    aml_append(method, if_ctx);
    else_ctx = aml_else();
    {
        aml_append(else_ctx, aml_return(zero));
    }
    aml_append(method, else_ctx);
    aml_append(sb_scope, method);

    method = aml_method(CPU_EJECT_METHOD_LEGACY, 2, AML_NOTSERIALIZED);
    aml_append(method, aml_sleep(200));
    aml_append(sb_scope, method);

    method = aml_method(CPU_SCAN_METHOD_LEGACY, 0, AML_NOTSERIALIZED);
    {
        Aml *while_ctx, *if_ctx2, *else_ctx2;
        Aml *bus_check_evt = aml_int(1);
        Aml *remove_evt = aml_int(3);
        Aml *status_map = aml_local(5); /* Local5 = active cpu bitmap */
        Aml *byte = aml_local(2); /* Local2 = last read byte from bitmap */
        Aml *idx = aml_local(0); /* Processor ID / APIC ID iterator */
        Aml *is_cpu_on = aml_local(1); /* Local1 = CPON flag for cpu */
        Aml *status = aml_local(3); /* Local3 = active state for cpu */

        aml_append(method, aml_store(aml_name(CPU_STATUS_MAP), status_map));
        aml_append(method, aml_store(zero, byte));
        aml_append(method, aml_store(zero, idx));

        /* While (idx < SizeOf(CPON)) */
        while_ctx = aml_while(aml_lless(idx, aml_sizeof(cpus_map)));
        aml_append(while_ctx,
            aml_store(aml_derefof(aml_index(cpus_map, idx)), is_cpu_on));

        if_ctx = aml_if(aml_and(idx, aml_int(0x07), NULL));
        {
            /* Shift down previously read bitmap byte */
            aml_append(if_ctx, aml_shiftright(byte, one, byte));
        }
        aml_append(while_ctx, if_ctx);

        else_ctx = aml_else();
        {
            /* Read next byte from cpu bitmap */
            aml_append(else_ctx, aml_store(aml_derefof(aml_index(status_map,
                       aml_shiftright(idx, aml_int(3), NULL))), byte));
        }
        aml_append(while_ctx, else_ctx);

        aml_append(while_ctx, aml_store(aml_and(byte, one, NULL), status));
        if_ctx = aml_if(aml_lnot(aml_equal(is_cpu_on, status)));
        {
            /* State change - update CPON with new state */
            aml_append(if_ctx, aml_store(status, aml_index(cpus_map, idx)));
            if_ctx2 = aml_if(aml_equal(status, one));
            {
                aml_append(if_ctx2,
                    aml_call2(AML_NOTIFY_METHOD, idx, bus_check_evt));
            }
            aml_append(if_ctx, if_ctx2);
            else_ctx2 = aml_else();
            {
                aml_append(else_ctx2,
                    aml_call2(AML_NOTIFY_METHOD, idx, remove_evt));
            }
        }
        aml_append(if_ctx, else_ctx2);
        aml_append(while_ctx, if_ctx);

        aml_append(while_ctx, aml_increment(idx)); /* go to next cpu */
        aml_append(method, while_ctx);
    }
    aml_append(sb_scope, method);

    /* The current AML generator can cover the APIC ID range [0..255],
     * inclusive, for VCPU hotplug. */
    QEMU_BUILD_BUG_ON(ACPI_CPU_HOTPLUG_ID_LIMIT > 256);
    if (apic_id_limit > ACPI_CPU_HOTPLUG_ID_LIMIT) {
        error_report("max_cpus is too large. APIC ID of last CPU is %u",
                     apic_id_limit - 1);
        exit(1);
    }

    /* create PCI0.PRES device and its _CRS to reserve CPU hotplug MMIO */
    dev = aml_device("PCI0." stringify(CPU_HOTPLUG_RESOURCE_DEVICE));
    aml_append(dev, aml_name_decl("_HID", aml_eisaid("PNP0A06")));
    aml_append(dev,
        aml_name_decl("_UID", aml_string("CPU Hotplug resources"))
    );
    /* device present, functioning, decoding, not shown in UI */
    aml_append(dev, aml_name_decl("_STA", aml_int(0xB)));
    crs = aml_resource_template();
    aml_append(crs,
        aml_io(AML_DECODE16, io_base, io_base, 1, ACPI_GPE_PROC_LEN)
    );
    aml_append(dev, aml_name_decl("_CRS", crs));
    aml_append(sb_scope, dev);
    /* declare CPU hotplug MMIO region and PRS field to access it */
    aml_append(sb_scope, aml_operation_region(
        "PRST", AML_SYSTEM_IO, aml_int(io_base), ACPI_GPE_PROC_LEN));
    field = aml_field("PRST", AML_BYTE_ACC, AML_NOLOCK, AML_PRESERVE);
    aml_append(field, aml_named_field("PRS", 256));
    aml_append(sb_scope, field);

    /* build Processor object for each processor */
    for (i = 0; i < apic_ids->len; i++) {
        int apic_id = apic_ids->cpus[i].arch_id;

        assert(apic_id < ACPI_CPU_HOTPLUG_ID_LIMIT);

        dev = aml_processor(i, 0, 0, "CP%.02X", apic_id);

        method = aml_method("_MAT", 0, AML_NOTSERIALIZED);
        aml_append(method,
            aml_return(aml_call2(CPU_MAT_METHOD, aml_int(apic_id), aml_int(i))
        ));
        aml_append(dev, method);

        method = aml_method("_STA", 0, AML_NOTSERIALIZED);
        aml_append(method,
            aml_return(aml_call1(CPU_STATUS_METHOD, aml_int(apic_id))));
        aml_append(dev, method);

        method = aml_method("_EJ0", 1, AML_NOTSERIALIZED);
        aml_append(method,
            aml_return(aml_call2(CPU_EJECT_METHOD_LEGACY, aml_int(apic_id),
                aml_arg(0)))
        );
        aml_append(dev, method);

        aml_append(sb_scope, dev);
    }

    /* build this code:
     *   Method(NTFY, 2) {If (LEqual(Arg0, 0x00)) {Notify(CP00, Arg1)} ...}
     */
    /* Arg0 = APIC ID */
    method = aml_method(AML_NOTIFY_METHOD, 2, AML_NOTSERIALIZED);
    for (i = 0; i < apic_ids->len; i++) {
        int apic_id = apic_ids->cpus[i].arch_id;

        if_ctx = aml_if(aml_equal(aml_arg(0), aml_int(apic_id)));
        aml_append(if_ctx,
            aml_notify(aml_name("CP%.02X", apic_id), aml_arg(1))
        );
        aml_append(method, if_ctx);
    }
    aml_append(sb_scope, method);

    /* build "Name(CPON, Package() { One, One, ..., Zero, Zero, ... })"
     *
     * Note: The ability to create variable-sized packages was first
     * introduced in ACPI 2.0. ACPI 1.0 only allowed fixed-size packages
     * ith up to 255 elements. Windows guests up to win2k8 fail when
     * VarPackageOp is used.
     */
    pkg = apic_id_limit <= 255 ? aml_package(apic_id_limit) :
                                 aml_varpackage(apic_id_limit);

    for (i = 0, apic_idx = 0; i < apic_ids->len; i++) {
        int apic_id = apic_ids->cpus[i].arch_id;

        for (; apic_idx < apic_id; apic_idx++) {
            aml_append(pkg, aml_int(0));
        }
        aml_append(pkg, aml_int(apic_ids->cpus[i].cpu ? 1 : 0));
        apic_idx = apic_id + 1;
    }
    aml_append(sb_scope, aml_name_decl(CPU_ON_BITMAP, pkg));
    aml_append(ctx, sb_scope);

    method = aml_method("\\_GPE._E02", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_call0("\\_SB." CPU_SCAN_METHOD_LEGACY));
    aml_append(ctx, method);
}
