#ifndef HW_VT82C686_H
#define HW_VT82C686_H

#include "hw/pci/pci_device.h"
#include "qemu/audio.h"

#define TYPE_VT82C686B_ISA "vt82c686b-isa"
#define TYPE_VT82C686B_USB_UHCI "vt82c686b-usb-uhci"
#define TYPE_VT8231_ISA "vt8231-isa"
#define TYPE_VIA_AC97 "via-ac97"
#define TYPE_VIA_IDE "via-ide"
#define TYPE_VIA_MC97 "via-mc97"
#define TYPE_VIA_PM "via-pm"

typedef struct {
    uint8_t stat;
    uint8_t type;
    uint32_t base;
    uint32_t curr;
    uint32_t addr;
    uint32_t clen;
    bool restart_pending;
    /*
     * The SGD engine is clocked by the codec sample rate, not by the
     * audio backend callback.  clock_ns is the virtual time at which the
     * engine position was zeroed and clock_frames the number of frontend
     * frames fetched since then.
     */
    bool clock_valid;
    int64_t clock_ns;
    uint64_t clock_frames;
} ViaAC97SGDChannel;

OBJECT_DECLARE_SIMPLE_TYPE(ViaAC97State, VIA_AC97);

struct ViaAC97State {
    PCIDevice dev;

    AudioBackend *audio_be;
    MemoryRegion sgd;
    MemoryRegion fm;
    MemoryRegion midi;
    SWVoiceOut *vo;
    ViaAC97SGDChannel aur;
    QEMUTimer *pump_timer;
    uint16_t codec_regs[128];
    uint32_t ac97_cmd;
};

void via_isa_set_irq(PCIDevice *d, int n, int level);

#endif
