/*
 * Copyright (c) 2018, Impinj, Inc.
 *
 * i.MX7 GPCv2 block emulation code
 *
 * Author: Andrey Smirnov <andrew.smirnov@gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/intc/imx_gpcv2.h"
#include "hw/registerfields.h"
#include "migration/vmstate.h"
#include "qemu/module.h"
#include "trace.h"

REG32(GPC_LPCR_A53_BSC, 0x000)
REG32(GPC_LPCR_A53_AD, 0x004)
REG32(GPC_LPCR_M7, 0x008)
REG32(GPC_SLPCR, 0x014)
REG32(GPC_MST_CPU_MAPPING, 0x018)
REG32(GPC_MLPCR, 0x020)
REG32(GPC_PGC_ACK_SEL_A53, 0x024)
REG32(GPC_PGC_ACK_SEL_M7, 0x028)
REG32(GPC_MISC, 0x02C)
REG32(GPC_IMR1_CORE0_A53, 0x030)
REG32(GPC_IMR2_CORE0_A53, 0x034)
REG32(GPC_IMR3_CORE0_A53, 0x038)
REG32(GPC_IMR4_CORE0_A53, 0x03C)
REG32(GPC_IMR5_CORE0_A53, 0x040)
REG32(GPC_IMR1_CORE1_A53, 0x044)
REG32(GPC_IMR2_CORE1_A53, 0x048)
REG32(GPC_IMR3_CORE1_A53, 0x04C)
REG32(GPC_IMR4_CORE1_A53, 0x050)
REG32(GPC_IMR5_CORE1_A53, 0x054)
REG32(GPC_IMR1_M7, 0x058)
REG32(GPC_IMR2_M7, 0x05C)
REG32(GPC_IMR3_M7, 0x060)
REG32(GPC_IMR4_M7, 0x064)
REG32(GPC_IMR5_M7, 0x068)
REG32(GPC_ISR1_A53, 0x080)
REG32(GPC_ISR2_A53, 0x084)
REG32(GPC_ISR3_A53, 0x088)
REG32(GPC_ISR4_A53, 0x08C)
REG32(GPC_ISR5_A53, 0x090)
REG32(GPC_ISR1_M7, 0x094)
REG32(GPC_ISR2_M7, 0x098)
REG32(GPC_ISR3_M7, 0x09C)
REG32(GPC_ISR4_M7, 0x0A0)
REG32(GPC_ISR5_M7, 0x0A4)
REG32(GPC_CPU_PGC_SW_PUP_REQ, 0x0D0)
REG32(GPC_MIX_PGC_SW_PUP_REQ, 0x0D4)
REG32(GPC_PU_PGC_SW_PUP_REQ, 0x0D8)
REG32(GPC_CPU_PGC_SW_PDN_REQ, 0x0DC)
REG32(GPC_MIX_PGC_SW_PDN_REQ, 0x0E0)
REG32(GPC_PU_PGC_SW_PDN_REQ, 0x0E4)
REG32(GPC_GPC_LPCR_CM4_AD, 0x0EC)
REG32(GPC_CPU_PGC_PUP_STATUS1, 0x108)
REG32(GPC_A53_MIX_PGC_PUP_STATUS0, 0x10C)
REG32(GPC_A53_MIX_PGC_PUP_STATUS1, 0x110)
REG32(GPC_A53_MIX_PGC_PUP_STATUS2, 0x114)
REG32(GPC_M7_MIX_PGC_PUP_STATUS0, 0x118)
REG32(GPC_M7_MIX_PGC_PUP_STATUS1, 0x11C)
REG32(GPC_M7_MIX_PGC_PUP_STATUS2, 0x120)
REG32(GPC_A53_PU_PGC_PUP_STATUS0, 0x124)
REG32(GPC_A53_PU_PGC_PUP_STATUS1, 0x128)
REG32(GPC_A53_PU_PGC_PUP_STATUS2, 0x12C)
REG32(GPC_M7_PU_PGC_PUP_STATUS0, 0x130)
REG32(GPC_M7_PU_PGC_PUP_STATUS1, 0x134)
REG32(GPC_M7_PU_PGC_PUP_STATUS2, 0x138)
REG32(GPC_CPU_PGC_PDN_STATUS1, 0x13C)
REG32(GPC_A53_MIX_PGC_PDN_STATUS0, 0x140)
REG32(GPC_A53_MIX_PGC_PDN_STATUS1, 0x144)
REG32(GPC_A53_MIX_PGC_PDN_STATUS2, 0x148)
REG32(GPC_M7_MIX_PGC_PDN_STATUS0, 0x14C)
REG32(GPC_M7_MIX_PGC_PDN_STATUS1, 0x150)
REG32(GPC_M7_MIX_PGC_PDN_STATUS2, 0x154)
REG32(GPC_A53_PU_PGC_PDN_STATUS0, 0x158)
REG32(GPC_A53_PU_PGC_PDN_STATUS1, 0x15C)
REG32(GPC_A53_PU_PGC_PDN_STATUS2, 0x160)
REG32(GPC_M7_PU_PGC_PDN_STATUS0, 0x164)
REG32(GPC_M7_PU_PGC_PDN_STATUS1, 0x168)
REG32(GPC_M7_PU_PGC_PDN_STATUS2, 0x16C)
REG32(GPC_A53_MIX_PDN_FLG, 0x170)
REG32(GPC_A53_PU_PDN_FLG, 0x174)
REG32(GPC_M7_MIX_PDN_FLG, 0x178)
REG32(GPC_M7_PU_PDN_FLG, 0x17C)
REG32(GPC_LPCR_A53_BSC2, 0x180)
REG32(GPC_PU_PWRHSK, 0x190)
REG32(GPC_IMR1_CORE2_A53, 0x194)
REG32(GPC_IMR2_CORE2_A53, 0x198)
REG32(GPC_IMR3_CORE2_A53, 0x19C)
REG32(GPC_IMR4_CORE2_A53, 0x1A0)
REG32(GPC_IMR5_CORE2_A53, 0x1A4)
REG32(GPC_IMR1_CORE3_A53, 0x1A8)
REG32(GPC_IMR2_CORE3_A53, 0x1AC)
REG32(GPC_IMR3_CORE3_A53, 0x1B0)
REG32(GPC_IMR4_CORE3_A53, 0x1B4)
REG32(GPC_IMR5_CORE3_A53, 0x1B8)
REG32(GPC_ACK_SEL_A53_PU, 0x1BC)
REG32(GPC_ACK_SEL_A53_PU1, 0x1C0)
REG32(GPC_ACK_SEL_M7_PU, 0x1C4)
REG32(GPC_ACK_SEL_M7_PU1, 0x1C8)
REG32(GPC_PGC_CPU_A53_MAPPING, 0x1CC)
REG32(GPC_PGC_CPU_M7_MAPPING, 0x1D0)
REG32(GPC_SLT0_CFG, 0x200)
REG32(GPC_SLT1_CFG, 0x204)
REG32(GPC_SLT2_CFG, 0x208)
REG32(GPC_SLT3_CFG, 0x20C)
REG32(GPC_SLT4_CFG, 0x210)
REG32(GPC_SLT5_CFG, 0x214)
REG32(GPC_SLT6_CFG, 0x218)
REG32(GPC_SLT7_CFG, 0x21C)
REG32(GPC_SLT8_CFG, 0x220)
REG32(GPC_SLT9_CFG, 0x224)
REG32(GPC_SLT10_CFG, 0x228)
REG32(GPC_SLT11_CFG, 0x22C)
REG32(GPC_SLT12_CFG, 0x230)
REG32(GPC_SLT13_CFG, 0x234)
REG32(GPC_SLT14_CFG, 0x238)
REG32(GPC_SLT15_CFG, 0x23C)
REG32(GPC_SLT16_CFG, 0x240)
REG32(GPC_SLT17_CFG, 0x244)
REG32(GPC_SLT18_CFG, 0x248)
REG32(GPC_SLT19_CFG, 0x24C)
REG32(GPC_SLT20_CFG, 0x250)
REG32(GPC_SLT21_CFG, 0x254)
REG32(GPC_SLT22_CFG, 0x258)
REG32(GPC_SLT23_CFG, 0x25C)
REG32(GPC_SLT24_CFG, 0x260)
REG32(GPC_SLT25_CFG, 0x264)
REG32(GPC_SLT26_CFG, 0x268)
REG32(GPC_SLT0_CFG_PU, 0x280)
REG32(GPC_SLT0_CFG_PU1, 0x284)
REG32(GPC_SLT1_CFG_PU, 0x288)
REG32(GPC_SLT1_CFG_PU1, 0x28C)
REG32(GPC_SLT2_CFG_PU, 0x290)
REG32(GPC_SLT2_CFG_PU1, 0x294)
REG32(GPC_SLT3_CFG_PU, 0x298)
REG32(GPC_SLT3_CFG_PU1, 0x29C)
REG32(GPC_SLT4_CFG_PU, 0x2A0)
REG32(GPC_SLT4_CFG_PU1, 0x2A4)
REG32(GPC_SLT5_CFG_PU, 0x2A8)
REG32(GPC_SLT5_CFG_PU1, 0x2AC)
REG32(GPC_SLT6_CFG_PU, 0x2B0)
REG32(GPC_SLT6_CFG_PU1, 0x2B4)
REG32(GPC_SLT7_CFG_PU, 0x2B8)
REG32(GPC_SLT7_CFG_PU1, 0x2BC)
REG32(GPC_SLT8_CFG_PU, 0x2C0)
REG32(GPC_SLT8_CFG_PU1, 0x2C4)
REG32(GPC_SLT9_CFG_PU, 0x2C8)
REG32(GPC_SLT9_CFG_PU1, 0x2CC)
REG32(GPC_SLT10_CFG_PU, 0x2D0)
REG32(GPC_SLT10_CFG_PU1, 0x2D4)
REG32(GPC_SLT11_CFG_PU, 0x2D8)
REG32(GPC_SLT11_CFG_PU1, 0x2DC)
REG32(GPC_SLT12_CFG_PU, 0x2E0)
REG32(GPC_SLT12_CFG_PU1, 0x2E4)
REG32(GPC_SLT13_CFG_PU, 0x2E8)
REG32(GPC_SLT13_CFG_PU1, 0x2EC)
REG32(GPC_SLT14_CFG_PU, 0x2F0)
REG32(GPC_SLT14_CFG_PU1, 0x2F4)
REG32(GPC_SLT15_CFG_PU, 0x2F8)
REG32(GPC_SLT15_CFG_PU1, 0x2FC)
REG32(GPC_SLT16_CFG_PU, 0x300)
REG32(GPC_SLT16_CFG_PU1, 0x304)
REG32(GPC_SLT17_CFG_PU, 0x308)
REG32(GPC_SLT17_CFG_PU1, 0x30C)
REG32(GPC_SLT18_CFG_PU, 0x310)
REG32(GPC_SLT18_CFG_PU1, 0x314)
REG32(GPC_SLT19_CFG_PU, 0x318)
REG32(GPC_SLT19_CFG_PU1, 0x31C)
REG32(GPC_SLT20_CFG_PU, 0x320)
REG32(GPC_SLT20_CFG_PU1, 0x324)
REG32(GPC_SLT21_CFG_PU, 0x328)
REG32(GPC_SLT21_CFG_PU1, 0x32C)
REG32(GPC_SLT22_CFG_PU, 0x330)
REG32(GPC_SLT22_CFG_PU1, 0x334)
REG32(GPC_SLT23_CFG_PU, 0x338)
REG32(GPC_SLT23_CFG_PU1, 0x33C)
REG32(GPC_SLT24_CFG_PU, 0x340)
REG32(GPC_SLT24_CFG_PU1, 0x344)
REG32(GPC_SLT25_CFG_PU, 0x348)
REG32(GPC_SLT25_CFG_PU1, 0x34C)
REG32(GPC_SLT26_CFG_PU, 0x350)
REG32(GPC_SLT26_CFG_PU1, 0x354)

static const char *imx_gpcv2_reg_name(uint32_t reg)
{
    switch (reg) {
    case R_GPC_LPCR_A53_BSC:
        return " (GPC_LPCR_A53_BSC)";
    case R_GPC_LPCR_A53_AD:
        return " (GPC_LPCR_A53_AD)";
    case R_GPC_LPCR_M7:
        return " (GPC_LPCR_M7)";
    case R_GPC_SLPCR:
        return " (GPC_SLPCR)";
    case R_GPC_MST_CPU_MAPPING:
        return " (GPC_MST_CPU_MAPPING)";
    case R_GPC_MLPCR:
        return " (GPC_MLPCR)";
    case R_GPC_PGC_ACK_SEL_A53:
        return " (GPC_PGC_ACK_SEL_A53)";
    case R_GPC_PGC_ACK_SEL_M7:
        return " (GPC_PGC_ACK_SEL_M7)";
    case R_GPC_MISC:
        return " (GPC_MISC)";
    case R_GPC_IMR1_CORE0_A53:
        return " (GPC_IMR1_CORE0_A53)";
    case R_GPC_IMR2_CORE0_A53:
        return " (GPC_IMR2_CORE0_A53)";
    case R_GPC_IMR3_CORE0_A53:
        return " (GPC_IMR3_CORE0_A53)";
    case R_GPC_IMR4_CORE0_A53:
        return " (GPC_IMR4_CORE0_A53)";
    case R_GPC_IMR5_CORE0_A53:
        return " (GPC_IMR5_CORE0_A53)";
    case R_GPC_IMR1_CORE1_A53:
        return " (GPC_IMR1_CORE1_A53)";
    case R_GPC_IMR2_CORE1_A53:
        return " (GPC_IMR2_CORE1_A53)";
    case R_GPC_IMR3_CORE1_A53:
        return " (GPC_IMR3_CORE1_A53)";
    case R_GPC_IMR4_CORE1_A53:
        return " (GPC_IMR4_CORE1_A53)";
    case R_GPC_IMR5_CORE1_A53:
        return " (GPC_IMR5_CORE1_A53)";
    case R_GPC_IMR1_M7:
        return " (GPC_IMR1_M7)";
    case R_GPC_IMR2_M7:
        return " (GPC_IMR2_M7)";
    case R_GPC_IMR3_M7:
        return " (GPC_IMR3_M7)";
    case R_GPC_IMR4_M7:
        return " (GPC_IMR4_M7)";
    case R_GPC_IMR5_M7:
        return " (GPC_IMR5_M7)";
    case R_GPC_ISR1_A53:
        return " (GPC_ISR1_A53)";
    case R_GPC_ISR2_A53:
        return " (GPC_ISR2_A53)";
    case R_GPC_ISR3_A53:
        return " (GPC_ISR3_A53)";
    case R_GPC_ISR4_A53:
        return " (GPC_ISR4_A53)";
    case R_GPC_ISR5_A53:
        return " (GPC_ISR5_A53)";
    case R_GPC_ISR1_M7:
        return " (GPC_ISR1_M7)";
    case R_GPC_ISR2_M7:
        return " (GPC_ISR2_M7)";
    case R_GPC_ISR3_M7:
        return " (GPC_ISR3_M7)";
    case R_GPC_ISR4_M7:
        return " (GPC_ISR4_M7)";
    case R_GPC_ISR5_M7:
        return " (GPC_ISR5_M7)";
    case R_GPC_CPU_PGC_SW_PUP_REQ:
        return " (GPC_CPU_PGC_SW_PUP_REQ)";
    case R_GPC_MIX_PGC_SW_PUP_REQ:
        return " (GPC_MIX_PGC_SW_PUP_REQ)";
    case R_GPC_PU_PGC_SW_PUP_REQ:
        return " (GPC_PU_PGC_SW_PUP_REQ)";
    case R_GPC_CPU_PGC_SW_PDN_REQ:
        return " (GPC_CPU_PGC_SW_PDN_REQ)";
    case R_GPC_MIX_PGC_SW_PDN_REQ:
        return " (GPC_MIX_PGC_SW_PDN_REQ)";
    case R_GPC_PU_PGC_SW_PDN_REQ:
        return " (GPC_PU_PGC_SW_PDN_REQ)";
    case R_GPC_GPC_LPCR_CM4_AD:
        return " (GPC_GPC_LPCR_CM4_AD)";
    case R_GPC_CPU_PGC_PUP_STATUS1:
        return " (GPC_CPU_PGC_PUP_STATUS1)";
    case R_GPC_A53_MIX_PGC_PUP_STATUS0:
        return " (GPC_A53_MIX_PGC_PUP_STATUS0)";
    case R_GPC_A53_MIX_PGC_PUP_STATUS1:
        return " (GPC_A53_MIX_PGC_PUP_STATUS1)";
    case R_GPC_A53_MIX_PGC_PUP_STATUS2:
        return " (GPC_A53_MIX_PGC_PUP_STATUS2)";
    case R_GPC_M7_MIX_PGC_PUP_STATUS0:
        return " (GPC_M7_MIX_PGC_PUP_STATUS0)";
    case R_GPC_M7_MIX_PGC_PUP_STATUS1:
        return " (GPC_M7_MIX_PGC_PUP_STATUS1)";
    case R_GPC_M7_MIX_PGC_PUP_STATUS2:
        return " (GPC_M7_MIX_PGC_PUP_STATUS2)";
    case R_GPC_A53_PU_PGC_PUP_STATUS0:
        return " (GPC_A53_PU_PGC_PUP_STATUS0)";
    case R_GPC_A53_PU_PGC_PUP_STATUS1:
        return " (GPC_A53_PU_PGC_PUP_STATUS1)";
    case R_GPC_A53_PU_PGC_PUP_STATUS2:
        return " (GPC_A53_PU_PGC_PUP_STATUS2)";
    case R_GPC_M7_PU_PGC_PUP_STATUS0:
        return " (GPC_M7_PU_PGC_PUP_STATUS0)";
    case R_GPC_M7_PU_PGC_PUP_STATUS1:
        return " (GPC_M7_PU_PGC_PUP_STATUS1)";
    case R_GPC_M7_PU_PGC_PUP_STATUS2:
        return " (GPC_M7_PU_PGC_PUP_STATUS2)";
    case R_GPC_CPU_PGC_PDN_STATUS1:
        return " (GPC_CPU_PGC_PDN_STATUS1)";
    case R_GPC_A53_MIX_PGC_PDN_STATUS0:
        return " (GPC_A53_MIX_PGC_PDN_STATUS0)";
    case R_GPC_A53_MIX_PGC_PDN_STATUS1:
        return " (GPC_A53_MIX_PGC_PDN_STATUS1)";
    case R_GPC_A53_MIX_PGC_PDN_STATUS2:
        return " (GPC_A53_MIX_PGC_PDN_STATUS2)";
    case R_GPC_M7_MIX_PGC_PDN_STATUS0:
        return " (GPC_M7_MIX_PGC_PDN_STATUS0)";
    case R_GPC_M7_MIX_PGC_PDN_STATUS1:
        return " (GPC_M7_MIX_PGC_PDN_STATUS1)";
    case R_GPC_M7_MIX_PGC_PDN_STATUS2:
        return " (GPC_M7_MIX_PGC_PDN_STATUS2)";
    case R_GPC_A53_PU_PGC_PDN_STATUS0:
        return " (GPC_A53_PU_PGC_PDN_STATUS0)";
    case R_GPC_A53_PU_PGC_PDN_STATUS1:
        return " (GPC_A53_PU_PGC_PDN_STATUS1)";
    case R_GPC_A53_PU_PGC_PDN_STATUS2:
        return " (GPC_A53_PU_PGC_PDN_STATUS2)";
    case R_GPC_M7_PU_PGC_PDN_STATUS0:
        return " (GPC_M7_PU_PGC_PDN_STATUS0)";
    case R_GPC_M7_PU_PGC_PDN_STATUS1:
        return " (GPC_M7_PU_PGC_PDN_STATUS1)";
    case R_GPC_M7_PU_PGC_PDN_STATUS2:
        return " (GPC_M7_PU_PGC_PDN_STATUS2)";
    case R_GPC_A53_MIX_PDN_FLG:
        return " (GPC_A53_MIX_PDN_FLG)";
    case R_GPC_A53_PU_PDN_FLG:
        return " (GPC_A53_PU_PDN_FLG)";
    case R_GPC_M7_MIX_PDN_FLG:
        return " (GPC_M7_MIX_PDN_FLG)";
    case R_GPC_M7_PU_PDN_FLG:
        return " (GPC_M7_PU_PDN_FLG)";
    case R_GPC_LPCR_A53_BSC2:
        return " (GPC_LPCR_A53_BSC2)";
    case R_GPC_PU_PWRHSK:
        return " (GPC_PU_PWRHSK)";
    case R_GPC_IMR1_CORE2_A53:
        return " (GPC_IMR1_CORE2_A53)";
    case R_GPC_IMR2_CORE2_A53:
        return " (GPC_IMR2_CORE2_A53)";
    case R_GPC_IMR3_CORE2_A53:
        return " (GPC_IMR3_CORE2_A53)";
    case R_GPC_IMR4_CORE2_A53:
        return " (GPC_IMR4_CORE2_A53)";
    case R_GPC_IMR5_CORE2_A53:
        return " (GPC_IMR5_CORE2_A53)";
    case R_GPC_IMR1_CORE3_A53:
        return " (GPC_IMR1_CORE3_A53)";
    case R_GPC_IMR2_CORE3_A53:
        return " (GPC_IMR2_CORE3_A53)";
    case R_GPC_IMR3_CORE3_A53:
        return " (GPC_IMR3_CORE3_A53)";
    case R_GPC_IMR4_CORE3_A53:
        return " (GPC_IMR4_CORE3_A53)";
    case R_GPC_IMR5_CORE3_A53:
        return " (GPC_IMR5_CORE3_A53)";
    case R_GPC_ACK_SEL_A53_PU:
        return " (GPC_ACK_SEL_A53_PU)";
    case R_GPC_ACK_SEL_A53_PU1:
        return " (GPC_ACK_SEL_A53_PU1)";
    case R_GPC_ACK_SEL_M7_PU:
        return " (GPC_ACK_SEL_M7_PU)";
    case R_GPC_ACK_SEL_M7_PU1:
        return " (GPC_ACK_SEL_M7_PU1)";
    case R_GPC_PGC_CPU_A53_MAPPING:
        return " (GPC_PGC_CPU_A53_MAPPING)";
    case R_GPC_PGC_CPU_M7_MAPPING:
        return " (GPC_PGC_CPU_M7_MAPPING)";
    case R_GPC_SLT0_CFG:
        return " (GPC_SLT0_CFG)";
    case R_GPC_SLT1_CFG:
        return " (GPC_SLT1_CFG)";
    case R_GPC_SLT2_CFG:
        return " (GPC_SLT2_CFG)";
    case R_GPC_SLT3_CFG:
        return " (GPC_SLT3_CFG)";
    case R_GPC_SLT4_CFG:
        return " (GPC_SLT4_CFG)";
    case R_GPC_SLT5_CFG:
        return " (GPC_SLT5_CFG)";
    case R_GPC_SLT6_CFG:
        return " (GPC_SLT6_CFG)";
    case R_GPC_SLT7_CFG:
        return " (GPC_SLT7_CFG)";
    case R_GPC_SLT8_CFG:
        return " (GPC_SLT8_CFG)";
    case R_GPC_SLT9_CFG:
        return " (GPC_SLT9_CFG)";
    case R_GPC_SLT10_CFG:
        return " (GPC_SLT10_CFG)";
    case R_GPC_SLT11_CFG:
        return " (GPC_SLT11_CFG)";
    case R_GPC_SLT12_CFG:
        return " (GPC_SLT12_CFG)";
    case R_GPC_SLT13_CFG:
        return " (GPC_SLT13_CFG)";
    case R_GPC_SLT14_CFG:
        return " (GPC_SLT14_CFG)";
    case R_GPC_SLT15_CFG:
        return " (GPC_SLT15_CFG)";
    case R_GPC_SLT16_CFG:
        return " (GPC_SLT16_CFG)";
    case R_GPC_SLT17_CFG:
        return " (GPC_SLT17_CFG)";
    case R_GPC_SLT18_CFG:
        return " (GPC_SLT18_CFG)";
    case R_GPC_SLT19_CFG:
        return " (GPC_SLT19_CFG)";
    case R_GPC_SLT20_CFG:
        return " (GPC_SLT20_CFG)";
    case R_GPC_SLT21_CFG:
        return " (GPC_SLT21_CFG)";
    case R_GPC_SLT22_CFG:
        return " (GPC_SLT22_CFG)";
    case R_GPC_SLT23_CFG:
        return " (GPC_SLT23_CFG)";
    case R_GPC_SLT24_CFG:
        return " (GPC_SLT24_CFG)";
    case R_GPC_SLT25_CFG:
        return " (GPC_SLT25_CFG)";
    case R_GPC_SLT26_CFG:
        return " (GPC_SLT26_CFG)";
    case R_GPC_SLT0_CFG_PU:
        return " (GPC_SLT0_CFG_PU)";
    case R_GPC_SLT0_CFG_PU1:
        return " (GPC_SLT0_CFG_PU1)";
    case R_GPC_SLT1_CFG_PU:
        return " (GPC_SLT1_CFG_PU)";
    case R_GPC_SLT1_CFG_PU1:
        return " (GPC_SLT1_CFG_PU1)";
    case R_GPC_SLT2_CFG_PU:
        return " (GPC_SLT2_CFG_PU)";
    case R_GPC_SLT2_CFG_PU1:
        return " (GPC_SLT2_CFG_PU1)";
    case R_GPC_SLT3_CFG_PU:
        return " (GPC_SLT3_CFG_PU)";
    case R_GPC_SLT3_CFG_PU1:
        return " (GPC_SLT3_CFG_PU1)";
    case R_GPC_SLT4_CFG_PU:
        return " (GPC_SLT4_CFG_PU)";
    case R_GPC_SLT4_CFG_PU1:
        return " (GPC_SLT4_CFG_PU1)";
    case R_GPC_SLT5_CFG_PU:
        return " (GPC_SLT5_CFG_PU)";
    case R_GPC_SLT5_CFG_PU1:
        return " (GPC_SLT5_CFG_PU1)";
    case R_GPC_SLT6_CFG_PU:
        return " (GPC_SLT6_CFG_PU)";
    case R_GPC_SLT6_CFG_PU1:
        return " (GPC_SLT6_CFG_PU1)";
    case R_GPC_SLT7_CFG_PU:
        return " (GPC_SLT7_CFG_PU)";
    case R_GPC_SLT7_CFG_PU1:
        return " (GPC_SLT7_CFG_PU1)";
    case R_GPC_SLT8_CFG_PU:
        return " (GPC_SLT8_CFG_PU)";
    case R_GPC_SLT8_CFG_PU1:
        return " (GPC_SLT8_CFG_PU1)";
    case R_GPC_SLT9_CFG_PU:
        return " (GPC_SLT9_CFG_PU)";
    case R_GPC_SLT9_CFG_PU1:
        return " (GPC_SLT9_CFG_PU1)";
    case R_GPC_SLT10_CFG_PU:
        return " (GPC_SLT10_CFG_PU)";
    case R_GPC_SLT10_CFG_PU1:
        return " (GPC_SLT10_CFG_PU1)";
    case R_GPC_SLT11_CFG_PU:
        return " (GPC_SLT11_CFG_PU)";
    case R_GPC_SLT11_CFG_PU1:
        return " (GPC_SLT11_CFG_PU1)";
    case R_GPC_SLT12_CFG_PU:
        return " (GPC_SLT12_CFG_PU)";
    case R_GPC_SLT12_CFG_PU1:
        return " (GPC_SLT12_CFG_PU1)";
    case R_GPC_SLT13_CFG_PU:
        return " (GPC_SLT13_CFG_PU)";
    case R_GPC_SLT13_CFG_PU1:
        return " (GPC_SLT13_CFG_PU1)";
    case R_GPC_SLT14_CFG_PU:
        return " (GPC_SLT14_CFG_PU)";
    case R_GPC_SLT14_CFG_PU1:
        return " (GPC_SLT14_CFG_PU1)";
    case R_GPC_SLT15_CFG_PU:
        return " (GPC_SLT15_CFG_PU)";
    case R_GPC_SLT15_CFG_PU1:
        return " (GPC_SLT15_CFG_PU1)";
    case R_GPC_SLT16_CFG_PU:
        return " (GPC_SLT16_CFG_PU)";
    case R_GPC_SLT16_CFG_PU1:
        return " (GPC_SLT16_CFG_PU1)";
    case R_GPC_SLT17_CFG_PU:
        return " (GPC_SLT17_CFG_PU)";
    case R_GPC_SLT17_CFG_PU1:
        return " (GPC_SLT17_CFG_PU1)";
    case R_GPC_SLT18_CFG_PU:
        return " (GPC_SLT18_CFG_PU)";
    case R_GPC_SLT18_CFG_PU1:
        return " (GPC_SLT18_CFG_PU1)";
    case R_GPC_SLT19_CFG_PU:
        return " (GPC_SLT19_CFG_PU)";
    case R_GPC_SLT19_CFG_PU1:
        return " (GPC_SLT19_CFG_PU1)";
    case R_GPC_SLT20_CFG_PU:
        return " (GPC_SLT20_CFG_PU)";
    case R_GPC_SLT20_CFG_PU1:
        return " (GPC_SLT20_CFG_PU1)";
    case R_GPC_SLT21_CFG_PU:
        return " (GPC_SLT21_CFG_PU)";
    case R_GPC_SLT21_CFG_PU1:
        return " (GPC_SLT21_CFG_PU1)";
    case R_GPC_SLT22_CFG_PU:
        return " (GPC_SLT22_CFG_PU)";
    case R_GPC_SLT22_CFG_PU1:
        return " (GPC_SLT22_CFG_PU1)";
    case R_GPC_SLT23_CFG_PU:
        return " (GPC_SLT23_CFG_PU)";
    case R_GPC_SLT23_CFG_PU1:
        return " (GPC_SLT23_CFG_PU1)";
    case R_GPC_SLT24_CFG_PU:
        return " (GPC_SLT24_CFG_PU)";
    case R_GPC_SLT24_CFG_PU1:
        return " (GPC_SLT24_CFG_PU1)";
    case R_GPC_SLT25_CFG_PU:
        return " (GPC_SLT25_CFG_PU)";
    case R_GPC_SLT25_CFG_PU1:
        return " (GPC_SLT25_CFG_PU1)";
    case R_GPC_SLT26_CFG_PU:
        return " (GPC_SLT26_CFG_PU)";
    case R_GPC_SLT26_CFG_PU1:
        return " (GPC_SLT26_CFG_PU1)";
    default:
        return " (reserved)";
    }
}

static void imx_gpcv2_reset(DeviceState *dev)
{
    IMXGPCv2State *s = IMX_GPCV2(dev);

    memset(s->regs, 0, sizeof(s->regs));

    s->regs[R_GPC_LPCR_A53_BSC] = 0x00003ff0;
    s->regs[R_GPC_LPCR_A53_AD] = 0x00000020;
    s->regs[R_GPC_LPCR_M7] = 0x00003ff0;
    s->regs[R_GPC_SLPCR] = 0xe000ff82;
    s->regs[R_GPC_MST_CPU_MAPPING] = 0x000000ff;
    s->regs[R_GPC_MLPCR] = 0x01010100;
    s->regs[R_GPC_PGC_ACK_SEL_A53] = 0x80008000;
    s->regs[R_GPC_PGC_ACK_SEL_M7] = 0x80008000;
    s->regs[R_GPC_MISC] = 0x00000021;
    s->regs[R_GPC_PU_PWRHSK] = 0x0000ffff;
}

static uint64_t imx_gpcv2_read(void *opaque, hwaddr offset,
                               unsigned size)
{
    IMXGPCv2State *s = opaque;
    const uint32_t idx = offset / sizeof(uint32_t);
    uint32_t value;

    switch (idx) {
    case R_GPC_PU_PWRHSK:
        value = 0x10000000;
        break;
    default:
        value = s->regs[idx];
        break;
    }

    trace_imx_gpcv2_read(offset, imx_gpcv2_reg_name(idx), value);

    return value;
}

static void imx_gpcv2_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
    IMXGPCv2State *s = opaque;
    const size_t idx = offset / sizeof(uint32_t);

    trace_imx_gpcv2_write(offset, imx_gpcv2_reg_name(idx), value);

    switch (idx) {
    case R_GPC_PU_PGC_SW_PUP_REQ:
    case R_GPC_PU_PGC_SW_PDN_REQ:
    case R_GPC_CPU_PGC_SW_PUP_REQ:
        /*
         * Real HW will clear those bits once as a way to indicate that
         * power up request is complete
         */
        s->regs[idx] = 0;
        break;
    default:
        s->regs[idx] = value;
        break;
    }
}

static const struct MemoryRegionOps imx_gpcv2_ops = {
    .read = imx_gpcv2_read,
    .write = imx_gpcv2_write,
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

static void imx_gpcv2_set_irq(void *opaque, int irq, int level)
{
    trace_imx_gpcv2_set_irq(irq, level);
}

static void imx_gpcv2_init(Object *obj)
{
    SysBusDevice *sd = SYS_BUS_DEVICE(obj);
    IMXGPCv2State *s = IMX_GPCV2(obj);

    memory_region_init_io(&s->iomem,
                          obj,
                          &imx_gpcv2_ops,
                          s,
                          TYPE_IMX_GPCV2 ".iomem",
                          sizeof(s->regs));
    sysbus_init_mmio(sd, &s->iomem);
    qdev_init_gpio_in(DEVICE(sd), imx_gpcv2_set_irq, 160);
}

static const VMStateDescription vmstate_imx_gpcv2 = {
    .name = TYPE_IMX_GPCV2,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, IMXGPCv2State, GPC_NUM),
        VMSTATE_END_OF_LIST()
    },
};

static void imx_gpcv2_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_legacy_reset(dc, imx_gpcv2_reset);
    dc->vmsd  = &vmstate_imx_gpcv2;
    dc->desc  = "i.MX GPCv2 Module";
}

static const TypeInfo imx_gpcv2_info = {
    .name          = TYPE_IMX_GPCV2,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(IMXGPCv2State),
    .instance_init = imx_gpcv2_init,
    .class_init    = imx_gpcv2_class_init,
};

static void imx_gpcv2_register_type(void)
{
    type_register_static(&imx_gpcv2_info);
}
type_init(imx_gpcv2_register_type)
