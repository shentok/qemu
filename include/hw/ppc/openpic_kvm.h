#ifndef OPENPIC_KVM_H
#define OPENPIC_KVM_H

#include "hw/sysbus.h"
#include "exec/memory.h"

#define TYPE_KVM_OPENPIC "kvm-openpic"
OBJECT_DECLARE_SIMPLE_TYPE(KVMOpenPICState, KVM_OPENPIC)

struct KVMOpenPICState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    MemoryRegion mem;
    MemoryListener mem_listener;
    uint32_t fd;
    uint32_t model;
    hwaddr mapped;
};

int kvm_openpic_connect_vcpu(DeviceState *d, CPUState *cs);

#endif /* OPENPIC_KVM_H */
