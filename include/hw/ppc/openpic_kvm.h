#ifndef OPENPIC_KVM_H
#define OPENPIC_KVM_H

#include "hw/sysbus.h"
#include "system/memory.h"

#define TYPE_KVM_OPENPIC "kvm-openpic"
OBJECT_DECLARE_SIMPLE_TYPE(KVMOpenPICState, KVM_OPENPIC)

struct KVMOpenPICState {
    SysBusDevice parent_obj;

    MemoryRegion mem;
    MemoryListener mem_listener;
    uint32_t fd;
    uint32_t model;
    hwaddr mapped;
};

int kvm_openpic_connect_vcpu(DeviceState *d, CPUState *cs);

#endif /* OPENPIC_KVM_H */
