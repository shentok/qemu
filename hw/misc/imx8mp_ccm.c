/*
 * Copyright (c) 2018, Impinj, Inc.
 *
 * i.MX8MP CCM, PMU and ANALOG IP blocks emulation code
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"

#include "hw/misc/imx8mp_ccm.h"
#include "migration/vmstate.h"

#include "trace.h"

#define CKIH_FREQ 24000000 /* 24MHz crystal input */

#define ANALOG_PLL_LOCK BIT(31)

static void imx8mp_analog_reset(DeviceState *dev)
{
    IMX8MPAnalogState *s = IMX8MP_ANALOG(dev);

    memset(s->analog, 0, sizeof(s->analog));

    s->analog[ANALOG_AUDIO_PLL1_GEN_CTRL] = 0x00002010;
    s->analog[ANALOG_AUDIO_PLL1_FDIV_CTL0] = 0x00145032;
    s->analog[ANALOG_AUDIO_PLL1_FDIV_CTL1] = 0x00000000;
    s->analog[ANALOG_AUDIO_PLL1_SSCG_CTRL] = 0x00000000;
    s->analog[ANALOG_AUDIO_PLL1_MNIT_CTRL] = 0x00100103;
    s->analog[ANALOG_AUDIO_PLL2_GEN_CTRL] = 0x00002010;
    s->analog[ANALOG_AUDIO_PLL2_FDIV_CTL0] = 0x00145032;
    s->analog[ANALOG_AUDIO_PLL2_FDIV_CTL1] = 0x00000000;
    s->analog[ANALOG_AUDIO_PLL2_SSCG_CTRL] = 0x00000000;
    s->analog[ANALOG_AUDIO_PLL2_MNIT_CTRL] = 0x00100103;
    s->analog[ANALOG_VIDEO_PLL1_GEN_CTRL] = 0x00002010;
    s->analog[ANALOG_VIDEO_PLL1_FDIV_CTL0] = 0x00145032;
    s->analog[ANALOG_VIDEO_PLL1_FDIV_CTL1] = 0x00000000;
    s->analog[ANALOG_VIDEO_PLL1_SSCG_CTRL] = 0x00000000;
    s->analog[ANALOG_VIDEO_PLL1_MNIT_CTRL] = 0x00100103;
    s->analog[ANALOG_DRAM_PLL_GEN_CTRL] = 0x00002010;
    s->analog[ANALOG_DRAM_PLL_FDIV_CTL0] = 0x0012C032;
    s->analog[ANALOG_DRAM_PLL_FDIV_CTL1] = 0x00000000;
    s->analog[ANALOG_DRAM_PLL_SSCG_CTRL] = 0x00000000;
    s->analog[ANALOG_DRAM_PLL_MNIT_CTRL] = 0x00100103;
    s->analog[ANALOG_GPU_PLL_GEN_CTRL] = 0x00000810;
    s->analog[ANALOG_GPU_PLL_FDIV_CTL0] = 0x000C8031;
    s->analog[ANALOG_GPU_PLL_LOCKD_CTRL] = 0x0010003F;
    s->analog[ANALOG_GPU_PLL_MNIT_CTRL] = 0x00280081;
    s->analog[ANALOG_VPU_PLL_GEN_CTRL] = 0x00000810;
    s->analog[ANALOG_VPU_PLL_FDIV_CTL0] = 0x0012C032;
    s->analog[ANALOG_VPU_PLL_LOCKD_CTRL] = 0x0010003F;
    s->analog[ANALOG_VPU_PLL_MNIT_CTRL] = 0x00280081;
    s->analog[ANALOG_ARM_PLL_GEN_CTRL] = 0x00000810;
    s->analog[ANALOG_ARM_PLL_FDIV_CTL0] = 0x000FA031;
    s->analog[ANALOG_ARM_PLL_LOCKD_CTRL] = 0x0010003F;
    s->analog[ANALOG_ARM_PLL_MNIT_CTRL] = 0x00280081;
    s->analog[ANALOG_SYS_PLL1_GEN_CTRL] = 0x0AAAA810;
    s->analog[ANALOG_SYS_PLL1_FDIV_CTL0] = 0x00190032;
    s->analog[ANALOG_SYS_PLL1_LOCKD_CTRL] = 0x0010003F;
    s->analog[ANALOG_SYS_PLL1_MNIT_CTRL] = 0x00280081;
    s->analog[ANALOG_SYS_PLL2_GEN_CTRL] = 0x0AAAA810;
    s->analog[ANALOG_SYS_PLL2_FDIV_CTL0] = 0x000FA031;
    s->analog[ANALOG_SYS_PLL2_LOCKD_CTRL] = 0x0010003F;
    s->analog[ANALOG_SYS_PLL2_MNIT_CTRL] = 0x00280081;
    s->analog[ANALOG_SYS_PLL3_GEN_CTRL] = 0x00000810;
    s->analog[ANALOG_SYS_PLL3_FDIV_CTL0] = 0x000FA031;
    s->analog[ANALOG_SYS_PLL3_LOCKD_CTRL] = 0x0010003F;
    s->analog[ANALOG_SYS_PLL3_MNIT_CTRL] = 0x00280081;
    s->analog[ANALOG_OSC_MISC_CFG] = 0x00000000;
    s->analog[ANALOG_ANAMIX_PLL_MNIT_CTL] = 0x00000000;
    s->analog[ANALOG_DIGPROG] = 0x00824010;

    /* all PLLs need to be locked */
    s->analog[ANALOG_AUDIO_PLL1_GEN_CTRL] |= ANALOG_PLL_LOCK;
    s->analog[ANALOG_AUDIO_PLL2_GEN_CTRL] |= ANALOG_PLL_LOCK;
    s->analog[ANALOG_VIDEO_PLL1_GEN_CTRL] |= ANALOG_PLL_LOCK;
    s->analog[ANALOG_DRAM_PLL_GEN_CTRL] |= ANALOG_PLL_LOCK;
    s->analog[ANALOG_GPU_PLL_GEN_CTRL] |= ANALOG_PLL_LOCK;
    s->analog[ANALOG_VPU_PLL_GEN_CTRL] |= ANALOG_PLL_LOCK;
    s->analog[ANALOG_ARM_PLL_GEN_CTRL] |= ANALOG_PLL_LOCK;
    s->analog[ANALOG_SYS_PLL1_GEN_CTRL] |= ANALOG_PLL_LOCK;
    s->analog[ANALOG_SYS_PLL2_GEN_CTRL] |= ANALOG_PLL_LOCK;
    s->analog[ANALOG_SYS_PLL3_GEN_CTRL] |= ANALOG_PLL_LOCK;
}

static void imx8mp_ccm_reset(DeviceState *dev)
{
    IMX8MPCCMState *s = IMX8MP_CCM(dev);

    memset(s->ccm, 0, sizeof(s->ccm));

    for (int i = 0x800; i < 0xa70; i += 4) {
        s->ccm[i / 4] = 2;
    }

    for (int i = 0x4000; i < 0x4c00; i += 4) {
        s->ccm[i / 4] = 2;
    }

    s->ccm[0x8000 / 4] = 0x10000000; /* CCM_TARGET_ROOT0 */
    s->ccm[0x8030 / 4] = 0x10000000; /* CCM_PRE0 */
    s->ccm[0x8080 / 4] = 0x10000000; /* CCM_TARGET_ROOT1 */
    s->ccm[0x80B0 / 4] = 0x10000000; /* CCM_PRE1 */
    s->ccm[0x8100 / 4] = 0x10000000; /* CCM_TARGET_ROOT2 */
    s->ccm[0x8130 / 4] = 0x10000000; /* CCM_PRE2 */
    s->ccm[0x8180 / 4] = 0x10000000; /* CCM_TARGET_ROOT3 */
    s->ccm[0x81B0 / 4] = 0x10000000; /* CCM_PRE3 */
    s->ccm[0x8200 / 4] = 0x10000000; /* CCM_TARGET_ROOT4 */
    s->ccm[0x8230 / 4] = 0x10000000; /* CCM_PRE4 */
    s->ccm[0x8280 / 4] = 0x10000000; /* CCM_TARGET_ROOT5 */
    s->ccm[0x82B0 / 4] = 0x10000000; /* CCM_PRE5 */
    s->ccm[0x8300 / 4] = 0x10000000; /* CCM_TARGET_ROOT6 */
    s->ccm[0x8330 / 4] = 0x10000000; /* CCM_PRE6 */
    s->ccm[0x8380 / 4] = 0x10000000; /* CCM_TARGET_ROOT7 */
    s->ccm[0x83B0 / 4] = 0x10000000; /* CCM_PRE7 */
    s->ccm[0x8400 / 4] = 0x10000000; /* CCM_TARGET_ROOT8 */
    s->ccm[0x8430 / 4] = 0x10000000; /* CCM_PRE8 */
    s->ccm[0x8480 / 4] = 0x10000000; /* CCM_TARGET_ROOT9 */
    s->ccm[0x84B0 / 4] = 0x10000000; /* CCM_PRE9 */
    s->ccm[0x8500 / 4] = 0x10000000; /* CCM_TARGET_ROOT10 */
    s->ccm[0x8530 / 4] = 0x10000000; /* CCM_PRE10 */
    s->ccm[0x8580 / 4] = 0x10000000; /* CCM_TARGET_ROOT11 */
    s->ccm[0x85B0 / 4] = 0x10000000; /* CCM_PRE11 */
    s->ccm[0x8600 / 4] = 0x10000000; /* CCM_TARGET_ROOT12 */
    s->ccm[0x8630 / 4] = 0x10000000; /* CCM_PRE12 */
    s->ccm[0x8680 / 4] = 0x10000000; /* CCM_TARGET_ROOT13 */
    s->ccm[0x86B0 / 4] = 0x10000000; /* CCM_PRE13 */
    s->ccm[0x8700 / 4] = 0x10000000; /* CCM_TARGET_ROOT14 */
    s->ccm[0x8730 / 4] = 0x10000000; /* CCM_PRE14 */
    s->ccm[0x8780 / 4] = 0x10000000; /* CCM_TARGET_ROOT15 */
    s->ccm[0x87B0 / 4] = 0x10000000; /* CCM_PRE15 */
    s->ccm[0x8800 / 4] = 0x10000000; /* CCM_TARGET_ROOT16 */
    s->ccm[0x8830 / 4] = 0x10000000; /* CCM_PRE16 */
    s->ccm[0x8880 / 4] = 0x10000000; /* CCM_TARGET_ROOT17 */
    s->ccm[0x88B0 / 4] = 0x10000000; /* CCM_PRE17 */
    s->ccm[0x8900 / 4] = 0x10000000; /* CCM_TARGET_ROOT18 */
    s->ccm[0x8930 / 4] = 0x10000000; /* CCM_PRE18 */
    s->ccm[0x8980 / 4] = 0x10000000; /* CCM_TARGET_ROOT19 */
    s->ccm[0x89B0 / 4] = 0x10000000; /* CCM_PRE19 */
    s->ccm[0x8A00 / 4] = 0x10000000; /* CCM_TARGET_ROOT20 */
    s->ccm[0x8A30 / 4] = 0x10000000; /* CCM_PRE20 */
    s->ccm[0x8A80 / 4] = 0x10000000; /* CCM_TARGET_ROOT21 */
    s->ccm[0x8AB0 / 4] = 0x10000000; /* CCM_PRE21 */
    s->ccm[0x8B00 / 4] = 0x10000000; /* CCM_TARGET_ROOT22 */
    s->ccm[0x8B30 / 4] = 0x10000000; /* CCM_PRE22 */
    s->ccm[0x8B80 / 4] = 0x10000000; /* CCM_TARGET_ROOT23 */
    s->ccm[0x8BB0 / 4] = 0x10000000; /* CCM_PRE23 */
    s->ccm[0x8C00 / 4] = 0x10000000; /* CCM_TARGET_ROOT24 */
    s->ccm[0x8C30 / 4] = 0x10000000; /* CCM_PRE24 */
    s->ccm[0x8C80 / 4] = 0x10000000; /* CCM_TARGET_ROOT25 */
    s->ccm[0x8CB0 / 4] = 0x10000000; /* CCM_PRE25 */
    s->ccm[0x8D00 / 4] = 0x10000000; /* CCM_TARGET_ROOT26 */
    s->ccm[0x8D30 / 4] = 0x10000000; /* CCM_PRE26 */
    s->ccm[0x8D80 / 4] = 0x10000000; /* CCM_TARGET_ROOT27 */
    s->ccm[0x8DB0 / 4] = 0x10000000; /* CCM_PRE27 */
    s->ccm[0x8E00 / 4] = 0x10000000; /* CCM_TARGET_ROOT28 */
    s->ccm[0x8E30 / 4] = 0x10000000; /* CCM_PRE28 */
    s->ccm[0x8E80 / 4] = 0x10000000; /* CCM_TARGET_ROOT29 */
    s->ccm[0x8EB0 / 4] = 0x10000000; /* CCM_PRE29 */
    s->ccm[0x8F00 / 4] = 0x10000000; /* CCM_TARGET_ROOT30 */
    s->ccm[0x8F30 / 4] = 0x10000000; /* CCM_PRE30 */
    s->ccm[0x8F80 / 4] = 0x10000000; /* CCM_TARGET_ROOT31 */
    s->ccm[0x8FB0 / 4] = 0x10000000; /* CCM_PRE31 */
    s->ccm[0x9000 / 4] = 0x10000000; /* CCM_TARGET_ROOT32 */
    s->ccm[0x9030 / 4] = 0x10000000; /* CCM_PRE32 */
    s->ccm[0x9080 / 4] = 0x10000000; /* CCM_TARGET_ROOT33 */
    s->ccm[0x90B0 / 4] = 0x10000000; /* CCM_PRE33 */
    s->ccm[0x9100 / 4] = 0x10000000; /* CCM_TARGET_ROOT34 */
    s->ccm[0x9130 / 4] = 0x10000000; /* CCM_PRE34 */
    s->ccm[0x9180 / 4] = 0x10000000; /* CCM_TARGET_ROOT35 */
    s->ccm[0x91B0 / 4] = 0x10000000; /* CCM_PRE35 */
    s->ccm[0x9200 / 4] = 0x10000000; /* CCM_TARGET_ROOT36 */
    s->ccm[0x9230 / 4] = 0x10000000; /* CCM_PRE36 */
    s->ccm[0x9280 / 4] = 0x10000000; /* CCM_TARGET_ROOT37 */
    s->ccm[0x92B0 / 4] = 0x10000000; /* CCM_PRE37 */
    s->ccm[0x9300 / 4] = 0x10000000; /* CCM_TARGET_ROOT38 */
    s->ccm[0x9330 / 4] = 0x10000000; /* CCM_PRE38 */
    s->ccm[0x9380 / 4] = 0x10000000; /* CCM_TARGET_ROOT39 */
    s->ccm[0x93B0 / 4] = 0x10000000; /* CCM_PRE39 */
    s->ccm[0x9400 / 4] = 0x10000000; /* CCM_TARGET_ROOT40 */
    s->ccm[0x9430 / 4] = 0x10000000; /* CCM_PRE40 */
    s->ccm[0x9480 / 4] = 0x10000000; /* CCM_TARGET_ROOT41 */
    s->ccm[0x94B0 / 4] = 0x10000000; /* CCM_PRE41 */
    s->ccm[0x9500 / 4] = 0x10000000; /* CCM_TARGET_ROOT42 */
    s->ccm[0x9530 / 4] = 0x10000000; /* CCM_PRE42 */
    s->ccm[0x9580 / 4] = 0x10000000; /* CCM_TARGET_ROOT43 */
    s->ccm[0x95B0 / 4] = 0x10000000; /* CCM_PRE43 */
    s->ccm[0x9600 / 4] = 0x10000000; /* CCM_TARGET_ROOT44 */
    s->ccm[0x9630 / 4] = 0x10000000; /* CCM_PRE44 */
    s->ccm[0x9680 / 4] = 0x10000000; /* CCM_TARGET_ROOT45 */
    s->ccm[0x96B0 / 4] = 0x10000000; /* CCM_PRE45 */
    s->ccm[0x9700 / 4] = 0x10000000; /* CCM_TARGET_ROOT46 */
    s->ccm[0x9730 / 4] = 0x10000000; /* CCM_PRE46 */
    s->ccm[0x9780 / 4] = 0x10000000; /* CCM_TARGET_ROOT47 */
    s->ccm[0x97B0 / 4] = 0x10000000; /* CCM_PRE47 */
    s->ccm[0x9800 / 4] = 0x10000000; /* CCM_TARGET_ROOT48 */
    s->ccm[0x9830 / 4] = 0x10000000; /* CCM_PRE48 */
    s->ccm[0x9880 / 4] = 0x10000000; /* CCM_TARGET_ROOT49 */
    s->ccm[0x98B0 / 4] = 0x10000000; /* CCM_PRE49 */
    s->ccm[0x9900 / 4] = 0x10000000; /* CCM_TARGET_ROOT50 */
    s->ccm[0x9930 / 4] = 0x10000000; /* CCM_PRE50 */
    s->ccm[0x9980 / 4] = 0x10000000; /* CCM_TARGET_ROOT51 */
    s->ccm[0x99B0 / 4] = 0x10000000; /* CCM_PRE51 */
    s->ccm[0x9A00 / 4] = 0x10000000; /* CCM_TARGET_ROOT52 */
    s->ccm[0x9A30 / 4] = 0x10000000; /* CCM_PRE52 */
    s->ccm[0x9A80 / 4] = 0x10000000; /* CCM_TARGET_ROOT53 */
    s->ccm[0x9AB0 / 4] = 0x10000000; /* CCM_PRE53 */
    s->ccm[0x9B00 / 4] = 0x10000000; /* CCM_TARGET_ROOT54 */
    s->ccm[0x9B30 / 4] = 0x10000000; /* CCM_PRE54 */
    s->ccm[0x9B80 / 4] = 0x10000000; /* CCM_TARGET_ROOT55 */
    s->ccm[0x9BB0 / 4] = 0x10000000; /* CCM_PRE55 */
    s->ccm[0x9C00 / 4] = 0x10000000; /* CCM_TARGET_ROOT56 */
    s->ccm[0x9C30 / 4] = 0x10000000; /* CCM_PRE56 */
    s->ccm[0x9C80 / 4] = 0x10000000; /* CCM_TARGET_ROOT57 */
    s->ccm[0x9CB0 / 4] = 0x10000000; /* CCM_PRE57 */
    s->ccm[0x9D00 / 4] = 0x10000000; /* CCM_TARGET_ROOT58 */
    s->ccm[0x9D30 / 4] = 0x10000000; /* CCM_PRE58 */
    s->ccm[0x9D80 / 4] = 0x10000000; /* CCM_TARGET_ROOT59 */
    s->ccm[0x9DB0 / 4] = 0x10000000; /* CCM_PRE59 */
    s->ccm[0x9E00 / 4] = 0x10000000; /* CCM_TARGET_ROOT60 */
    s->ccm[0x9E30 / 4] = 0x10000000; /* CCM_PRE60 */
    s->ccm[0x9E80 / 4] = 0x10000000; /* CCM_TARGET_ROOT61 */
    s->ccm[0x9EB0 / 4] = 0x10000000; /* CCM_PRE61 */
    s->ccm[0x9F00 / 4] = 0x10000000; /* CCM_TARGET_ROOT62 */
    s->ccm[0x9F30 / 4] = 0x10000000; /* CCM_PRE62 */
    s->ccm[0x9F80 / 4] = 0x10000000; /* CCM_TARGET_ROOT63 */
    s->ccm[0x9FB0 / 4] = 0x10000000; /* CCM_PRE63 */
    s->ccm[0xA000 / 4] = 0x10000000; /* CCM_TARGET_ROOT64 */
    s->ccm[0xA030 / 4] = 0x10000000; /* CCM_PRE64 */
    s->ccm[0xA080 / 4] = 0x10000000; /* CCM_TARGET_ROOT65 */
    s->ccm[0xA0B0 / 4] = 0x10000000; /* CCM_PRE65 */
    s->ccm[0xA100 / 4] = 0x10000000; /* CCM_TARGET_ROOT66 */
    s->ccm[0xA130 / 4] = 0x10000000; /* CCM_PRE66 */
    s->ccm[0xA180 / 4] = 0x10000000; /* CCM_TARGET_ROOT67 */
    s->ccm[0xA1B0 / 4] = 0x10000000; /* CCM_PRE67 */
    s->ccm[0xA200 / 4] = 0x10000000; /* CCM_TARGET_ROOT68 */
    s->ccm[0xA230 / 4] = 0x10000000; /* CCM_PRE68 */
    s->ccm[0xA280 / 4] = 0x10000000; /* CCM_TARGET_ROOT69 */
    s->ccm[0xA2B0 / 4] = 0x10000000; /* CCM_PRE69 */
    s->ccm[0xA300 / 4] = 0x10000000; /* CCM_TARGET_ROOT70 */
    s->ccm[0xA330 / 4] = 0x10000000; /* CCM_PRE70 */
    s->ccm[0xA380 / 4] = 0x10000000; /* CCM_TARGET_ROOT71 */
    s->ccm[0xA3B0 / 4] = 0x10000000; /* CCM_PRE71 */
    s->ccm[0xA400 / 4] = 0x10000000; /* CCM_TARGET_ROOT72 */
    s->ccm[0xA430 / 4] = 0x10000000; /* CCM_PRE72 */
    s->ccm[0xA480 / 4] = 0x10000000; /* CCM_TARGET_ROOT73 */
    s->ccm[0xA4B0 / 4] = 0x10000000; /* CCM_PRE73 */
    s->ccm[0xA500 / 4] = 0x10000000; /* CCM_TARGET_ROOT74 */
    s->ccm[0xA530 / 4] = 0x10000000; /* CCM_PRE74 */
    s->ccm[0xA580 / 4] = 0x10000000; /* CCM_TARGET_ROOT75 */
    s->ccm[0xA5B0 / 4] = 0x10000000; /* CCM_PRE75 */
    s->ccm[0xA600 / 4] = 0x10000000; /* CCM_TARGET_ROOT76 */
    s->ccm[0xA630 / 4] = 0x10000000; /* CCM_PRE76 */
    s->ccm[0xA680 / 4] = 0x10000000; /* CCM_TARGET_ROOT77 */
    s->ccm[0xA6B0 / 4] = 0x10000000; /* CCM_PRE77 */
    s->ccm[0xA700 / 4] = 0x10000000; /* CCM_TARGET_ROOT78 */
    s->ccm[0xA730 / 4] = 0x10000000; /* CCM_PRE78 */
    s->ccm[0xA780 / 4] = 0x10000000; /* CCM_TARGET_ROOT79 */
    s->ccm[0xA7B0 / 4] = 0x10000000; /* CCM_PRE79 */
    s->ccm[0xA800 / 4] = 0x10000000; /* CCM_TARGET_ROOT80 */
    s->ccm[0xA830 / 4] = 0x10000000; /* CCM_PRE80 */
    s->ccm[0xA880 / 4] = 0x10000000; /* CCM_TARGET_ROOT81 */
    s->ccm[0xA8B0 / 4] = 0x10000000; /* CCM_PRE81 */
    s->ccm[0xA900 / 4] = 0x10000000; /* CCM_TARGET_ROOT82 */
    s->ccm[0xA930 / 4] = 0x10000000; /* CCM_PRE82 */
    s->ccm[0xA980 / 4] = 0x10000000; /* CCM_TARGET_ROOT83 */
    s->ccm[0xA9B0 / 4] = 0x10000000; /* CCM_PRE83 */
    s->ccm[0xAA00 / 4] = 0x10000000; /* CCM_TARGET_ROOT84 */
    s->ccm[0xAA30 / 4] = 0x10000000; /* CCM_PRE84 */
    s->ccm[0xAA80 / 4] = 0x10000000; /* CCM_TARGET_ROOT85 */
    s->ccm[0xAAB0 / 4] = 0x10000000; /* CCM_PRE85 */
    s->ccm[0xAB00 / 4] = 0x10000000; /* CCM_TARGET_ROOT86 */
    s->ccm[0xAB30 / 4] = 0x10000000; /* CCM_PRE86 */
    s->ccm[0xAB80 / 4] = 0x10000000; /* CCM_TARGET_ROOT87 */
    s->ccm[0xABB0 / 4] = 0x10000000; /* CCM_PRE87 */
    s->ccm[0xAC00 / 4] = 0x10000000; /* CCM_TARGET_ROOT88 */
    s->ccm[0xAC30 / 4] = 0x10000000; /* CCM_PRE88 */
    s->ccm[0xAC80 / 4] = 0x10000000; /* CCM_TARGET_ROOT89 */
    s->ccm[0xACB0 / 4] = 0x10000000; /* CCM_PRE89 */
    s->ccm[0xAD00 / 4] = 0x10000000; /* CCM_TARGET_ROOT90 */
    s->ccm[0xAD30 / 4] = 0x10000000; /* CCM_PRE90 */
    s->ccm[0xAD80 / 4] = 0x10000000; /* CCM_TARGET_ROOT91 */
    s->ccm[0xADB0 / 4] = 0x10000000; /* CCM_PRE91 */
    s->ccm[0xAE00 / 4] = 0x10000000; /* CCM_TARGET_ROOT92 */
    s->ccm[0xAE30 / 4] = 0x10000000; /* CCM_PRE92 */
    s->ccm[0xAE80 / 4] = 0x10000000; /* CCM_TARGET_ROOT93 */
    s->ccm[0xAEB0 / 4] = 0x10000000; /* CCM_PRE93 */
    s->ccm[0xAF00 / 4] = 0x10000000; /* CCM_TARGET_ROOT94 */
    s->ccm[0xAF30 / 4] = 0x10000000; /* CCM_PRE94 */
    s->ccm[0xAF80 / 4] = 0x10000000; /* CCM_TARGET_ROOT95 */
    s->ccm[0xAFB0 / 4] = 0x10000000; /* CCM_PRE95 */
    s->ccm[0xB000 / 4] = 0x10000000; /* CCM_TARGET_ROOT96 */
    s->ccm[0xB030 / 4] = 0x10000000; /* CCM_PRE96 */
    s->ccm[0xB080 / 4] = 0x10000000; /* CCM_TARGET_ROOT97 */
    s->ccm[0xB0B0 / 4] = 0x10000000; /* CCM_PRE97 */
    s->ccm[0xB100 / 4] = 0x10000000; /* CCM_TARGET_ROOT98 */
    s->ccm[0xB130 / 4] = 0x10000000; /* CCM_PRE98 */
    s->ccm[0xB180 / 4] = 0x10000000; /* CCM_TARGET_ROOT99 */
    s->ccm[0xB1B0 / 4] = 0x10000000; /* CCM_PRE99 */
    s->ccm[0xB200 / 4] = 0x10000000; /* CCM_TARGET_ROOT100 */
    s->ccm[0xB230 / 4] = 0x10000000; /* CCM_PRE100 */
    s->ccm[0xB280 / 4] = 0x10000000; /* CCM_TARGET_ROOT101 */
    s->ccm[0xB2B0 / 4] = 0x10000000; /* CCM_PRE101 */
    s->ccm[0xB300 / 4] = 0x10000000; /* CCM_TARGET_ROOT102 */
    s->ccm[0xB330 / 4] = 0x10000000; /* CCM_PRE102 */
    s->ccm[0xB380 / 4] = 0x10000000; /* CCM_TARGET_ROOT103 */
    s->ccm[0xB3B0 / 4] = 0x10000000; /* CCM_PRE103 */
    s->ccm[0xB400 / 4] = 0x10000000; /* CCM_TARGET_ROOT104 */
    s->ccm[0xB430 / 4] = 0x10000000; /* CCM_PRE104 */
    s->ccm[0xB480 / 4] = 0x10000000; /* CCM_TARGET_ROOT105 */
    s->ccm[0xB4B0 / 4] = 0x10000000; /* CCM_PRE105 */
    s->ccm[0xB500 / 4] = 0x10000000; /* CCM_TARGET_ROOT106 */
    s->ccm[0xB530 / 4] = 0x10000000; /* CCM_PRE106 */
    s->ccm[0xB580 / 4] = 0x10000000; /* CCM_TARGET_ROOT107 */
    s->ccm[0xB5B0 / 4] = 0x10000000; /* CCM_PRE107 */
    s->ccm[0xB600 / 4] = 0x10000000; /* CCM_TARGET_ROOT108 */
    s->ccm[0xB630 / 4] = 0x10000000; /* CCM_PRE108 */
    s->ccm[0xB680 / 4] = 0x10000000; /* CCM_TARGET_ROOT109 */
    s->ccm[0xB6B0 / 4] = 0x10000000; /* CCM_PRE109 */
    s->ccm[0xB700 / 4] = 0x10000000; /* CCM_TARGET_ROOT110 */
    s->ccm[0xB730 / 4] = 0x10000000; /* CCM_PRE110 */
    s->ccm[0xB780 / 4] = 0x10000000; /* CCM_TARGET_ROOT111 */
    s->ccm[0xB7B0 / 4] = 0x10000000; /* CCM_PRE111 */
    s->ccm[0xB800 / 4] = 0x10000000; /* CCM_TARGET_ROOT112 */
    s->ccm[0xB830 / 4] = 0x10000000; /* CCM_PRE112 */
    s->ccm[0xB880 / 4] = 0x10000000; /* CCM_TARGET_ROOT113 */
    s->ccm[0xB8B0 / 4] = 0x10000000; /* CCM_PRE113 */
    s->ccm[0xB900 / 4] = 0x10000000; /* CCM_TARGET_ROOT114 */
    s->ccm[0xB930 / 4] = 0x10000000; /* CCM_PRE114 */
    s->ccm[0xB980 / 4] = 0x10000000; /* CCM_TARGET_ROOT115 */
    s->ccm[0xB9B0 / 4] = 0x10000000; /* CCM_PRE115 */
    s->ccm[0xBA00 / 4] = 0x10000000; /* CCM_TARGET_ROOT116 */
    s->ccm[0xBA30 / 4] = 0x10000000; /* CCM_PRE116 */
    s->ccm[0xBA80 / 4] = 0x10000000; /* CCM_TARGET_ROOT117 */
    s->ccm[0xBAB0 / 4] = 0x10000000; /* CCM_PRE117 */
    s->ccm[0xBB00 / 4] = 0x10000000; /* CCM_TARGET_ROOT118 */
    s->ccm[0xBB30 / 4] = 0x10000000; /* CCM_PRE118 */
    s->ccm[0xBB80 / 4] = 0x10000000; /* CCM_TARGET_ROOT119 */
    s->ccm[0xBBB0 / 4] = 0x10000000; /* CCM_PRE119 */
    s->ccm[0xBC00 / 4] = 0x10000000; /* CCM_TARGET_ROOT120 */
    s->ccm[0xBC30 / 4] = 0x10000000; /* CCM_PRE120 */
    s->ccm[0xBC80 / 4] = 0x10000000; /* CCM_TARGET_ROOT121 */
    s->ccm[0xBCB0 / 4] = 0x10000000; /* CCM_PRE121 */
    s->ccm[0xBD00 / 4] = 0x10000000; /* CCM_TARGET_ROOT122 */
    s->ccm[0xBD30 / 4] = 0x10000000; /* CCM_PRE122 */
    s->ccm[0xBD80 / 4] = 0x10000000; /* CCM_TARGET_ROOT123 */
    s->ccm[0xBDB0 / 4] = 0x10000000; /* CCM_PRE123 */
    s->ccm[0xBE00 / 4] = 0x10000000; /* CCM_TARGET_ROOT124 */
    s->ccm[0xBE30 / 4] = 0x10000000; /* CCM_PRE124 */
    s->ccm[0xBE80 / 4] = 0x10000000; /* CCM_TARGET_ROOT125 */
    s->ccm[0xBEB0 / 4] = 0x10000000; /* CCM_PRE125 */
    s->ccm[0xBF00 / 4] = 0x10000000; /* CCM_TARGET_ROOT126 */
    s->ccm[0xBF30 / 4] = 0x10000000; /* CCM_PRE126 */
    s->ccm[0xBF80 / 4] = 0x10000000; /* CCM_TARGET_ROOT127 */
    s->ccm[0xBFB0 / 4] = 0x10000000; /* CCM_PRE127 */
    s->ccm[0xC000 / 4] = 0x10000000; /* CCM_TARGET_ROOT128 */
    s->ccm[0xC030 / 4] = 0x10000000; /* CCM_PRE128 */
    s->ccm[0xC080 / 4] = 0x10000000; /* CCM_TARGET_ROOT129 */
    s->ccm[0xC0B0 / 4] = 0x10000000; /* CCM_PRE129 */
    s->ccm[0xC100 / 4] = 0x10000000; /* CCM_TARGET_ROOT130 */
    s->ccm[0xC130 / 4] = 0x10000000; /* CCM_PRE130 */
    s->ccm[0xC180 / 4] = 0x10000000; /* CCM_TARGET_ROOT131 */
    s->ccm[0xC1B0 / 4] = 0x10000000; /* CCM_PRE131 */
    s->ccm[0xC200 / 4] = 0x10000000; /* CCM_TARGET_ROOT132 */
    s->ccm[0xC230 / 4] = 0x10000000; /* CCM_PRE132 */
    s->ccm[0xC280 / 4] = 0x10000000; /* CCM_TARGET_ROOT133 */
    s->ccm[0xC2B0 / 4] = 0x10000000; /* CCM_PRE133 */
    s->ccm[0xC300 / 4] = 0x10000000; /* CCM_TARGET_ROOT134 */
    s->ccm[0xC330 / 4] = 0x10000000; /* CCM_PRE134 */
    s->ccm[0xC380 / 4] = 0x10000000; /* CCM_TARGET_ROOT135 */
    s->ccm[0xC3B0 / 4] = 0x10000000; /* CCM_PRE135 */
    s->ccm[0xC400 / 4] = 0x10000000; /* CCM_TARGET_ROOT136 */
    s->ccm[0xC430 / 4] = 0x10000000; /* CCM_PRE136 */
    s->ccm[0xC480 / 4] = 0x10000000; /* CCM_TARGET_ROOT137 */
    s->ccm[0xC4B0 / 4] = 0x10000000; /* CCM_PRE137 */
    s->ccm[0xC500 / 4] = 0x10000000; /* CCM_TARGET_ROOT138 */
    s->ccm[0xC530 / 4] = 0x10000000; /* CCM_PRE138 */
    s->ccm[0xC580 / 4] = 0x10000000; /* CCM_TARGET_ROOT139 */
    s->ccm[0xC5B0 / 4] = 0x10000000; /* CCM_PRE139 */
    s->ccm[0xC600 / 4] = 0x10000000; /* CCM_TARGET_ROOT140 */
    s->ccm[0xC630 / 4] = 0x10000000; /* CCM_PRE140 */
    s->ccm[0xC680 / 4] = 0x10000000; /* CCM_TARGET_ROOT141 */
    s->ccm[0xC6B0 / 4] = 0x10000000; /* CCM_PRE141 */
}

#define CCM_INDEX(offset)   (((offset) & ~(hwaddr)0xF) / sizeof(uint32_t))
#define CCM_BITOP(offset)   ((offset) & (hwaddr)0xF)

enum {
    CCM_BITOP_NONE = 0x00,
    CCM_BITOP_SET  = 0x04,
    CCM_BITOP_CLR  = 0x08,
    CCM_BITOP_TOG  = 0x0C,
};

static uint64_t imx8mp_set_clr_tog_read(void *opaque, hwaddr offset,
                                        unsigned size)
{
    const uint32_t *mmio = opaque;

    return mmio[CCM_INDEX(offset)];
}

static void imx8mp_set_clr_tog_write(void *opaque, hwaddr offset,
                                     uint64_t value, unsigned size)
{
    const uint8_t  bitop = CCM_BITOP(offset);
    const uint32_t index = CCM_INDEX(offset);
    uint32_t *mmio = opaque;

    switch (bitop) {
    case CCM_BITOP_NONE:
        mmio[index]  = value;
        break;
    case CCM_BITOP_SET:
        mmio[index] |= value;
        break;
    case CCM_BITOP_CLR:
        mmio[index] &= ~value;
        break;
    case CCM_BITOP_TOG:
        mmio[index] ^= value;
        break;
    };
}

static const struct MemoryRegionOps imx8mp_set_clr_tog_ops = {
    .read = imx8mp_set_clr_tog_read,
    .write = imx8mp_set_clr_tog_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
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

static uint64_t imx8mp_analog_read(void *opaque, hwaddr offset, unsigned size)
{
    IMX8MPAnalogState *s = opaque;

    return s->analog[offset >> 2];
}

static void imx8mp_analog_write(void *opaque, hwaddr offset,
                                uint64_t value, unsigned size)
{
    IMX8MPAnalogState *s = opaque;

    if (offset >> 2 == ANALOG_DIGPROG) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "Guest write to read-only ANALOG_DIGPROG register\n");
    } else {
        s->analog[offset >> 2] = value;
    }
}

static const struct MemoryRegionOps imx8mp_analog_ops = {
    .read = imx8mp_analog_read,
    .write = imx8mp_analog_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl = {
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

static void imx8mp_ccm_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    IMX8MPCCMState *s = IMX8MP_CCM(obj);

    memory_region_init_io(&s->iomem,
                          obj,
                          &imx8mp_set_clr_tog_ops,
                          s->ccm,
                          TYPE_IMX8MP_CCM ".ccm",
                          sizeof(s->ccm));

    sysbus_init_mmio(sd, &s->iomem);
}

static void imx8mp_analog_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    IMX8MPAnalogState *s = IMX8MP_ANALOG(obj);

    memory_region_init(&s->mmio.container, obj, TYPE_IMX8MP_ANALOG, 0x10000);

    memory_region_init_io(&s->mmio.analog, obj, &imx8mp_analog_ops, s,
                          TYPE_IMX8MP_ANALOG, sizeof(s->analog));
    memory_region_add_subregion(&s->mmio.container, 0, &s->mmio.analog);

    sysbus_init_mmio(sd, &s->mmio.container);
}

static const VMStateDescription vmstate_imx8mp_ccm = {
    .name = TYPE_IMX8MP_CCM,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(ccm, IMX8MPCCMState, CCM_MAX),
        VMSTATE_END_OF_LIST()
    },
};

static uint32_t imx8mp_ccm_get_clock_frequency(IMXCCMState *dev, IMXClk clock)
{
    /*
     * This function is "consumed" by GPT emulation code. Some clocks
     * have fixed frequencies and we can provide requested frequency
     * easily. However for CCM provided clocks (like IPG) each GPT
     * timer can have its own clock root.
     * This means we need additional information when calling this
     * function to know the requester's identity.
     */
    uint32_t freq = 0;

    switch (clock) {
    case CLK_NONE:
        break;
    case CLK_32k:
        freq = CKIL_FREQ;
        break;
    case CLK_HIGH:
        freq = CKIH_FREQ;
        break;
    case CLK_IPG:
    case CLK_IPG_HIGH:
        /*
         * For now we don't have a way to figure out the device this
         * function is called for. Until then the IPG derived clocks
         * are left unimplemented.
         */
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: Clock %d Not implemented\n",
                      TYPE_IMX8MP_CCM, __func__, clock);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "[%s]%s: unsupported clock %d\n",
                      TYPE_IMX8MP_CCM, __func__, clock);
        break;
    }

    trace_ccm_clock_freq(clock, freq);

    return freq;
}

static void imx8mp_ccm_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    IMXCCMClass *ccm = IMX_CCM_CLASS(klass);

    device_class_set_legacy_reset(dc, imx8mp_ccm_reset);
    dc->vmsd  = &vmstate_imx8mp_ccm;
    dc->desc  = "i.MX8MP Clock Control Module";

    ccm->get_clock_frequency = imx8mp_ccm_get_clock_frequency;
}

static const VMStateDescription vmstate_imx8mp_analog = {
    .name = TYPE_IMX8MP_ANALOG,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(analog, IMX8MPAnalogState, ANALOG_MAX),
        VMSTATE_END_OF_LIST()
    },
};

static void imx8mp_analog_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, imx8mp_analog_reset);
    dc->vmsd  = &vmstate_imx8mp_analog;
    dc->desc  = "i.MX8MP Analog Module";
}

static const TypeInfo imx8mp_ccm_types[] = {
    {
        .name          = TYPE_IMX8MP_CCM,
        .parent        = TYPE_IMX_CCM,
        .instance_size = sizeof(IMX8MPCCMState),
        .instance_init = imx8mp_ccm_init,
        .class_init    = imx8mp_ccm_class_init,
    },
    {
        .name          = TYPE_IMX8MP_ANALOG,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(IMX8MPAnalogState),
        .instance_init = imx8mp_analog_init,
        .class_init    = imx8mp_analog_class_init,
    }
};

DEFINE_TYPES(imx8mp_ccm_types);
