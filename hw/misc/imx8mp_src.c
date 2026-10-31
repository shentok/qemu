/*
 * i.MX 8M Plus System Reset Controller
 *
 * Copyright (c) 2025 Bernhard Beschow <shentey@gmail.com>
 *
 * Based on hw/misc/imx6_src.c
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/misc/imx8mp_src.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/registerfields.h"
#include "target/arm/arm-powerctl.h"
#include "target/arm/internals.h"
#include "migration/vmstate.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "trace.h"

REG32(SRC_SCR, 0x0000)

REG32(SRC_A53RCR0, 0x0004)
    FIELD(SRC_A53RCR0, CORE_RESET3, 7, 1)
    FIELD(SRC_A53RCR0, CORE_RESET2, 6, 1)
    FIELD(SRC_A53RCR0, CORE_RESET1, 5, 1)
    FIELD(SRC_A53RCR0, CORE_RESET0, 4, 1)

REG32(SRC_A53RCR1, 0x0008)
    FIELD(SRC_A53RCR1, CORE3_ENABLE, 3, 1)
    FIELD(SRC_A53RCR1, CORE2_ENABLE, 2, 1)
    FIELD(SRC_A53RCR1, CORE1_ENABLE, 1, 1)
    FIELD(SRC_A53RCR1, CORE0_ENABLE, 0, 1)

REG32(SRC_M7RCR, 0x000c)
REG32(SRC_SUPERMIX_RCR, 0x0018)
REG32(SRC_AUDIOMIX_RCR, 0x001c)
REG32(SRC_USBPHY1_RCR, 0x0020)
REG32(SRC_USBPHY2_RCR, 0x0024)
REG32(SRC_MLMIX_RCR, 0x0028)
REG32(SRC_PCIEPHY_RCR, 0x002c)
REG32(SRC_HDMI_RCR, 0x0030)
REG32(SRC_MEDIA_RCR, 0x0034)
REG32(SRC_GPU2D_RCR, 0x0038)
REG32(SRC_GPU3D_RCR, 0x003c)
REG32(SRC_GPU_RCR, 0x0040)
REG32(SRC_VPU_RCR, 0x0044)
REG32(SRC_VPU_G1_RCR, 0x0048)
REG32(SRC_VPU_G2_RCR, 0x004c)
REG32(SRC_VPUVC8KE_RCR, 0x0050)
REG32(SRC_NOC_RCR, 0x0054)
REG32(SRC_SBMR1, 0x0058)
REG32(SRC_SRSR, 0x005c)
REG32(SRC_SISR, 0x0068)
REG32(SRC_SIMR, 0x006c)

REG32(SRC_SBMR2, 0x0070)
    FIELD(SRC_SBMR2, IPP_BOOT_MODE, 24, 4)

REG32(SRC_GPR1, 0x0074)
REG32(SRC_GPR2, 0x0078)
REG32(SRC_GPR3, 0x007c)
REG32(SRC_GPR4, 0x0080)
REG32(SRC_GPR5, 0x0084)
REG32(SRC_GPR6, 0x0088)
REG32(SRC_GPR7, 0x008c)
REG32(SRC_GPR8, 0x0090)
REG32(SRC_GPR9, 0x0094)
REG32(SRC_GPR10, 0x0098)
REG32(SRC_DDRC_RCR, 0x1000)
REG32(SRC_HDMIPHY_RCR, 0x1008)
REG32(SRC_MIPIPHY1_RCR, 0x100c)
REG32(SRC_MIPIPHY2_RCR, 0x1010)
REG32(SRC_HSIO_RCR, 0x1014)
REG32(SRC_MEDIAISPDWP_RCR, 0x1018)

static const char *fsl_imx8mp_src_reg_name(uint32_t reg)
{
    switch (reg) {
    case R_SRC_SCR:
        return " (SRC_SCR)";
    case R_SRC_A53RCR0:
        return " (SRC_A53RCR0)";
    case R_SRC_A53RCR1:
        return " (SRC_A53RCR1)";
    case R_SRC_M7RCR:
        return " (SRC_M7RCR)";
    case R_SRC_SUPERMIX_RCR:
        return " (SRC_SUPERMIX_RCR)";
    case R_SRC_AUDIOMIX_RCR:
        return " (SRC_AUDIOMIX_RCR)";
    case R_SRC_USBPHY1_RCR:
        return " (SRC_USBPHY1_RCR)";
    case R_SRC_USBPHY2_RCR:
        return " (SRC_USBPHY2_RCR)";
    case R_SRC_MLMIX_RCR:
        return " (SRC_MLMIX_RCR)";
    case R_SRC_PCIEPHY_RCR:
        return " (SRC_PCIEPHY_RCR)";
    case R_SRC_HDMI_RCR:
        return " (SRC_HDMI_RCR)";
    case R_SRC_MEDIA_RCR:
        return " (SRC_MEDIA_RCR)";
    case R_SRC_GPU2D_RCR:
        return " (SRC_GPU2D_RCR)";
    case R_SRC_GPU3D_RCR:
        return " (SRC_GPU3D_RCR)";
    case R_SRC_GPU_RCR:
        return " (SRC_GPU_RCR)";
    case R_SRC_VPU_RCR:
        return " (SRC_VPU_RCR)";
    case R_SRC_VPU_G1_RCR:
        return " (SRC_VPU_G1_RCR)";
    case R_SRC_VPU_G2_RCR:
        return " (SRC_VPU_G2_RCR)";
    case R_SRC_VPUVC8KE_RCR:
        return " (SRC_VPUVC8KE_RCR)";
    case R_SRC_NOC_RCR:
        return " (SRC_NOC_RCR)";
    case R_SRC_SBMR1:
        return " (SRC_SBMR1)";
    case R_SRC_SRSR:
        return " (SRC_SRSR)";
    case R_SRC_SISR:
        return " (SRC_SISR)";
    case R_SRC_SIMR:
        return " (SRC_SIMR)";
    case R_SRC_SBMR2:
        return " (SRC_SBMR2)";
    case R_SRC_GPR1:
        return " (SRC_GPR1)";
    case R_SRC_GPR2:
        return " (SRC_GPR2)";
    case R_SRC_GPR3:
        return " (SRC_GPR3)";
    case R_SRC_GPR4:
        return " (SRC_GPR4)";
    case R_SRC_GPR5:
        return " (SRC_GPR5)";
    case R_SRC_GPR6:
        return " (SRC_GPR6)";
    case R_SRC_GPR7:
        return " (SRC_GPR7)";
    case R_SRC_GPR8:
        return " (SRC_GPR8)";
    case R_SRC_GPR9:
        return " (SRC_GPR9)";
    case R_SRC_GPR10:
        return " (SRC_GPR10)";
    case R_SRC_DDRC_RCR:
        return " (SRC_DDRC_RCR)";
    case R_SRC_HDMIPHY_RCR:
        return " (SRC_HDMIPHY_RCR)";
    case R_SRC_MIPIPHY1_RCR:
        return " (SRC_MIPIPHY1_RCR)";
    case R_SRC_MIPIPHY2_RCR:
        return " (SRC_MIPIPHY2_RCR)";
    case R_SRC_HSIO_RCR:
        return " (SRC_HSIO_RCR)";
    case R_SRC_MEDIAISPDWP_RCR:
        return " (SRC_MEDIAISPDWP_RCR)";
    default:
        return " (reserved)";
    }
}

static uint64_t fsl_imx8mp_src_read(void *opaque, hwaddr offset,
                                    unsigned size)
{
    FslImx8mpSrcState *s = opaque;
    const uint32_t reg = offset / 4;
    uint32_t value = 0;

    if (reg < FSL_IMX8MP_SRC_NUM_REGS) {
        value = s->regs[reg];
    }

    trace_fsl_imx8mp_src_read(offset, fsl_imx8mp_src_reg_name(reg), value);

    return value;
}

static uint64_t fsl_imx8mp_src_cpu_entry(FslImx8mpSrcState *s, int cpuid)
{
    /*
     * Each core has a 64-bit GPR pair holding the warm reset entry
     * address shifted right by two.  The upper word contributes bits
     * [39:24] and the lower word bits [23:2] of the address.
     */
    uint32_t hi = s->regs[R_SRC_GPR1 + 2 * cpuid];
    uint32_t lo = s->regs[R_SRC_GPR1 + 2 * cpuid + 1];

    return ((uint64_t)(hi & 0xffff) << 24) |
           ((uint64_t)(lo & 0x3fffff) << 2);
}

static void fsl_imx8mp_src_update_cpu(FslImx8mpSrcState *s, int cpuid)
{
    bool reset_asserted = !extract32(s->regs[R_SRC_A53RCR1], cpuid, 1);
    bool should_run = s->powered[cpuid] && !reset_asserted;

    if (should_run == s->running[cpuid]) {
        return;
    }
    s->running[cpuid] = should_run;

    if (should_run) {
        arm_set_cpu_on(cpuid, fsl_imx8mp_src_cpu_entry(s, cpuid), 0, 3, true);
    } else {
        CPUState *cs = arm_get_cpu_by_id(cpuid);

        if (cs) {
            arm_set_cpu_power_state(ARM_CPU(cs), PSCI_OFF);
        }
    }
}

static void fsl_imx8mp_src_cpu_powered(void *opaque, int irq, int level)
{
    FslImx8mpSrcState *s = opaque;

    trace_fsl_imx8mp_src_cpu_powered(irq, level);

    s->powered[irq] = level != 0;
    fsl_imx8mp_src_update_cpu(s, irq);
}

static void fsl_imx8mp_src_write(void *opaque, hwaddr offset, uint64_t value,
                                 unsigned size)
{
    FslImx8mpSrcState *s = opaque;
    const uint32_t reg = offset / 4;
    unsigned long change_mask;
    int i;

    change_mask = s->regs[reg] ^ (uint32_t)value;

    switch (reg) {
    case R_SRC_A53RCR0:
        /*
         * The A53RCR0 reset bits are level sensitive: writing 1 asserts
         * the reset, writing 0 releases it.  Unlike A53RCR1 they are not
         * self-clearing.  Bits 0-3 are the core power-on resets and
         * bits 4-7 the core resets; both reset the core, while power
         * sequencing is done by the GPC.
         */
        for (i = 0; i < 4; i++) {
            uint32_t mask = BIT(i) | BIT(i + 4);

            if (change_mask & (uint32_t)value & mask) {
                arm_reset_cpu(i);
            }
        }
        s->regs[reg] = value;
        break;
    case R_SRC_A53RCR1:
        /*
         * A53RCR1 bits are nCPUPORESET: 0 holds the core in reset and 1
         * releases it.  A core runs once its power domain is up (GPC)
         * and its reset is released (SRC), so re-evaluate it here.
         */
        s->regs[reg] = value;
        for (i = 0; i < 4; i++) {
            if (change_mask & BIT(i)) {
                fsl_imx8mp_src_update_cpu(s, i);
            }
        }
        break;
    default:
        if (reg < FSL_IMX8MP_SRC_NUM_REGS) {
            s->regs[reg] = value;
        }
        qemu_log_mask(LOG_UNIMP, "[%s]%s: Unimplemented register at offset 0x%"
                      HWADDR_PRIx "\n", TYPE_IMX8MP_SRC, __func__,
                      offset);
        break;
    }

    trace_fsl_imx8mp_src_write(offset, fsl_imx8mp_src_reg_name(reg), value);
}

static const struct MemoryRegionOps imx8mp_src_ops = {
    .read = fsl_imx8mp_src_read,
    .write = fsl_imx8mp_src_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        /*
         * Our device would not work correctly if the guest was doing
         * unaligned access. This might not be a limitation on the real
         * device but in practice there is no reason for a guest to access
         * this device unaligned.
         */
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void imx8mp_src_init(Object *obj)
{
    FslImx8mpSrcState *s = IMX8MP_SRC(obj);

    qdev_init_gpio_in_named(DEVICE(obj), fsl_imx8mp_src_cpu_powered,
                            IMX8MP_SRC_CPU_POWERED, ARRAY_SIZE(s->powered));
}

static void imx8mp_src_realize(DeviceState *dev, Error **errp)
{
    FslImx8mpSrcState *s = IMX8MP_SRC(dev);

    memory_region_init_io(&s->iomem, OBJECT(dev), &imx8mp_src_ops, s,
                          TYPE_IMX8MP_SRC, 0x1000);
    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void imx8mp_src_reset(DeviceState *dev)
{
    FslImx8mpSrcState *s = IMX8MP_SRC(dev);

    memset(s->regs, 0, sizeof(s->regs));
    memset(s->powered, 0, sizeof(s->powered));
    memset(s->running, 0, sizeof(s->running));

    /*
     * CPU 0 is the boot CPU: QEMU starts it directly and its power
     * domain is already up, so it is running from the outset.
     * Secondary cores stay off until the GPC powers them up.
     */
    s->powered[0] = true;
    s->running[0] = true;

    s->regs[R_SRC_SCR] = 0xa0;
    s->regs[R_SRC_A53RCR0] = 0xa0000;
    s->regs[R_SRC_A53RCR1] = 0x1;
    s->regs[R_SRC_M7RCR] = 0xa8;
    s->regs[R_SRC_PCIEPHY_RCR] = 0xa;
    s->regs[R_SRC_SRSR] = 0x1;
    s->regs[R_SRC_SIMR] = 0x3ff;
    s->regs[R_SRC_SBMR2] = FIELD_DP32(s->regs[R_SRC_SBMR2], SRC_SBMR2,
                                      IPP_BOOT_MODE, s->boot_mode);
}

static const VMStateDescription imx8mp_src_vmstate = {
    .name = TYPE_IMX8MP_SRC,
    .version_id = 2,
    .minimum_version_id = 2,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, FslImx8mpSrcState, FSL_IMX8MP_SRC_NUM_REGS),
        /*
         * The GPC's power levels and the run state last applied to the
         * vCPUs are migrated alongside the registers.  `running` makes
         * loading a no-op for the ARM power control layer instead of
         * restarting cores at their warm entry on the incoming side.
         */
        VMSTATE_BOOL_ARRAY(powered, FslImx8mpSrcState, 4),
        VMSTATE_BOOL_ARRAY(running, FslImx8mpSrcState, 4),
        VMSTATE_END_OF_LIST()
    },
};

static const Property imx8mp_src_properties[] = {
    DEFINE_PROP_UINT8("boot-mode", FslImx8mpSrcState, boot_mode, 0),
};

static void imx8mp_src_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = imx8mp_src_realize;
    device_class_set_legacy_reset(dc, imx8mp_src_reset);
    dc->vmsd = &imx8mp_src_vmstate;
    device_class_set_props(dc, imx8mp_src_properties);
    dc->desc = "i.MX 8M Plus System Reset Controller";
}

static const TypeInfo imx8mp_src_types[] = {
    {
        .name          = TYPE_IMX8MP_SRC,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FslImx8mpSrcState),
        .instance_init = imx8mp_src_init,
        .class_init    = imx8mp_src_class_init,
    },
};

DEFINE_TYPES(imx8mp_src_types)
