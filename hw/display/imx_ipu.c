/*
 * i.MX Secure Real Time Clock
 *
 * This implementation is based on the following reference manual:
 * i.MX53 Multimedia Applications Processor Reference Manual
 * Document Number: iMX53RM, Rev. 2.1, 06/2012
 *
 * Copyright (c) 2026 Bernhard Beschow <shentey@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qemu/osdep.h"
#include "hw/display/imx_ipu.h"
#include "ui/surface.h"
#include "hw/display/framebuffer.h"
#include "hw/core/irq.h"
#include "hw/core/registerfields.h"
#include "migration/vmstate.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "system/address-spaces.h"
#include "ui/console.h"
#include "ui/pixel_ops.h"
#include "trace.h"

/* IPU Common */
REG32(IPU_CONF,               0x000000)
REG32(IPU_SISG_CTRL0,         0x000004)
REG32(IPU_SISG_CTRL1,         0x000008)
REG32(IPU_SISG_SET_I,         0x00000C)
REG32(IPU_SISG_CLR_I,         0x000024)
REG32(IPU_INT_CTRL_1,         0x00003C)
REG32(IPU_INT_CTRL_2,      0x000040)
REG32(IPU_INT_CTRL_3,      0x000044)
REG32(IPU_INT_CTRL_4,      0x000048)
REG32(IPU_INT_CTRL_5,      0x00004C)
REG32(IPU_INT_CTRL_6,      0x000050)
REG32(IPU_INT_CTRL_7,      0x000054)
REG32(IPU_INT_CTRL_8,      0x000058)
REG32(IPU_INT_CTRL_9,      0x00005C)
REG32(IPU_INT_CTRL_10,     0x000060)
REG32(IPU_INT_CTRL_11,     0x000064)
REG32(IPU_INT_CTRL_12,     0x000068)
REG32(IPU_INT_CTRL_13,     0x00006C)
REG32(IPU_INT_CTRL_14,     0x000070)
REG32(IPU_INT_CTRL_15,     0x000074)
REG32(IPU_SDMA_EVENT_1,    0x000078)
REG32(IPU_SDMA_EVENT_2,    0x00007C)
REG32(IPU_SDMA_EVENT_3,    0x000080)
REG32(IPU_SDMA_EVENT_4,    0x000084)
REG32(IPU_SDMA_EVENT_7,    0x000088)
REG32(IPU_SDMA_EVENT_8,    0x00008C)
REG32(IPU_SDMA_EVENT_11,    0x000090)
REG32(IPU_SDMA_EVENT_12,    0x000094)
REG32(IPU_SDMA_EVENT_13,    0x000098)
REG32(IPU_SDMA_EVENT_14,    0x00009C)
REG32(IPU_SRM_PRI1,         0x0000A0)
REG32(IPU_SRM_PRI2,         0x0000A4)
REG32(IPU_FS_PROC_FLOW1,    0x0000A8)
REG32(IPU_FS_PROC_FLOW2,    0x0000AC)
REG32(IPU_FS_PROC_FLOW3,    0x0000B0)
REG32(IPU_FS_DISP_FLOW1,    0x0000B4)
REG32(IPU_FS_DISP_FLOW2,    0x0000B8)
REG32(IPU_SKIP,             0x0000BC)
REG32(IPU_DISP_GEN,         0x0000C4)
REG32(IPU_DISP_ALT1,        0x0000C8)
REG32(IPU_DISP_ALT2,        0x0000CC)
REG32(IPU_DISP_ALT3,        0x0000D0)
REG32(IPU_DISP_ALT4,        0x0000D4)
REG32(IPU_SNOOP,            0x0000D8)
REG32(IPU_MEM_RST,          0x0000DC)
REG32(IPU_PM,               0x0000E0)
REG32(IPU_GPR,                  0x0000E4)
REG32(IPU_CH_DB_MODE_SEL0,      0x000150)
REG32(IPU_CH_DB_MODE_SEL1,      0x000154)
REG32(IPU_ALT_CH_DB_MODE_SEL0,  0x000168)
REG32(IPU_ALT_CH_DB_MODE_SEL1,  0x00016C)
REG32(IPU_ALT_CH_TRB_MODE_SEL0, 0x000178)
REG32(IPU_INT_STAT_1,           0x000200)
REG32(IPU_INT_STAT_2,           0x000204)
REG32(IPU_INT_STAT_3,           0x000208)
REG32(IPU_INT_STAT_4,           0x00020c)
REG32(IPU_INT_STAT_5,           0x000210)
REG32(IPU_INT_STAT_6,           0x000214)
REG32(IPU_INT_STAT_7,           0x000218)
REG32(IPU_INT_STAT_8,           0x00021C)
REG32(IPU_INT_STAT_9,           0x000220)
REG32(IPU_INT_STAT_10,          0x000224)
REG32(IPU_INT_STAT_11,          0x000228)
REG32(IPU_INT_STAT_12,          0x00022C)
REG32(IPU_INT_STAT_13,          0x000230)
REG32(IPU_INT_STAT_14,          0x000234)
REG32(IPU_INT_STAT_15,          0x000238)
REG32(IPU_CUR_BUF_0,             0x00023C)
REG32(IPU_CUR_BUF_1,             0x000240)
REG32(IPU_ALT_CUR_0,             0x000244)
REG32(IPU_ALT_CUR_1,             0x000248)
REG32(IPU_SRM_STAT,              0x00024C)
REG32(IPU_PROC_TASKS_STAT,       0x000250)
REG32(IPU_DISP_TASKS_STAT,       0x000254)
REG32(IPU_TRIPLE_CUR_BUF_0,      0x000258)
REG32(IPU_TRIPLE_CUR_BUF_1,      0x00025C)
REG32(IPU_CH_BUF0_RDY0,          0x000268)
REG32(IPU_CH_BUF0_RDY1,          0x00026C)
REG32(IPU_CH_BUF1_RDY0,          0x000270)
REG32(IPU_CH_BUF1_RDY1,          0x000274)
REG32(IPU_ALT_CH_BUF0_RDY0,      0x000278)
REG32(IPU_ALT_CH_BUF0_RDY1,      0x00027C)
REG32(IPU_ALT_CH_BUF1_RDY0,      0x000280)
REG32(IPU_ALT_CH_BUF1_RDY1,      0x000284)
REG32(IPU_CH_BUF2_RDY0,          0x000288)
REG32(IPU_CH_BUF2_RDY1,          0x00028C)

/* IDMAC */
REG32(IPU_IDMAC_CONF,          0x000000)
REG32(IPU_IDMAC_CH_EN_1,       0x000004)
REG32(IPU_IDMAC_CH_EN_2,       0x000008)
REG32(IPU_IDMAC_SEP_ALPHA,     0x00000C)
REG32(IPU_IDMAC_ALT_SEP_ALPHA, 0x000010)
REG32(IPU_IDMAC_CH_PRI_1,      0x000014)
REG32(IPU_IDMAC_CH_PRI_2,      0x000018)
REG32(IPU_IDMAC_WM_EN_1,       0x00001C)
REG32(IPU_IDMAC_WM_EN_2,       0x000020)
REG32(IPU_IDMAC_LOCK_EN_1,     0x000024)
REG32(IPU_IDMAC_SC_CORD_1,     0x00004C)
REG32(IPU_IDMAC_CH_BUSY_1,     0x000100)
REG32(IPU_IDMAC_CH_BUSY_2,     0x000104)

/* CPMEM */
REG32(IPU_CPMEM_CH0_W0,       0)
REG32(IPU_CPMEM_CH63_W15,     80 * 0x40 - 4)

#define IPU_CPMEM_WORD(word, ofs, size) ((word) * 0x100 + (ofs)), (size)

#define IPU_FIELD_UBO        IPU_CPMEM_WORD(0, 46, 22)
#define IPU_FIELD_VBO        IPU_CPMEM_WORD(0, 68, 22)
#define IPU_FIELD_IOX        IPU_CPMEM_WORD(0, 90, 4)
#define IPU_FIELD_RDRW       IPU_CPMEM_WORD(0, 94, 1)
#define IPU_FIELD_SO         IPU_CPMEM_WORD(0, 113, 1)
#define IPU_FIELD_SLY        IPU_CPMEM_WORD(1, 102, 14)
#define IPU_FIELD_SLUV       IPU_CPMEM_WORD(1, 128, 14)
#define IPU_FIELD_XV         IPU_CPMEM_WORD(0, 0, 10)
#define IPU_FIELD_YV         IPU_CPMEM_WORD(0, 10, 9)
#define IPU_FIELD_XB         IPU_CPMEM_WORD(0, 19, 13)
#define IPU_FIELD_YB         IPU_CPMEM_WORD(0, 32, 12)
#define IPU_FIELD_NSB_B      IPU_CPMEM_WORD(0, 44, 1)
#define IPU_FIELD_CF         IPU_CPMEM_WORD(0, 45, 1)
#define IPU_FIELD_SX         IPU_CPMEM_WORD(0, 46, 12)
#define IPU_FIELD_SY         IPU_CPMEM_WORD(0, 58, 11)
#define IPU_FIELD_NS         IPU_CPMEM_WORD(0, 69, 10)
#define IPU_FIELD_SDX        IPU_CPMEM_WORD(0, 79, 7)
#define IPU_FIELD_SM         IPU_CPMEM_WORD(0, 86, 10)
#define IPU_FIELD_SCC        IPU_CPMEM_WORD(0, 96, 1)
#define IPU_FIELD_SCE        IPU_CPMEM_WORD(0, 97, 1)
#define IPU_FIELD_SDY        IPU_CPMEM_WORD(0, 98, 7)
#define IPU_FIELD_SDRX       IPU_CPMEM_WORD(0, 105, 1)
#define IPU_FIELD_SDRY       IPU_CPMEM_WORD(0, 106, 1)
#define IPU_FIELD_BPP        IPU_CPMEM_WORD(0, 107, 3)
#define IPU_FIELD_DEC_SEL    IPU_CPMEM_WORD(0, 110, 2)
#define IPU_FIELD_DIM        IPU_CPMEM_WORD(0, 112, 1)
#define IPU_FIELD_BNDM       IPU_CPMEM_WORD(0, 114, 3)
#define IPU_FIELD_BM         IPU_CPMEM_WORD(0, 117, 2)
#define IPU_FIELD_ROT        IPU_CPMEM_WORD(0, 119, 1)
#define IPU_FIELD_ROT_HF_VF  IPU_CPMEM_WORD(0, 119, 3)
#define IPU_FIELD_HF         IPU_CPMEM_WORD(0, 120, 1)
#define IPU_FIELD_VF         IPU_CPMEM_WORD(0, 121, 1)
#define IPU_FIELD_THE        IPU_CPMEM_WORD(0, 122, 1)
#define IPU_FIELD_CAP        IPU_CPMEM_WORD(0, 123, 1)
#define IPU_FIELD_CAE        IPU_CPMEM_WORD(0, 124, 1)
#define IPU_FIELD_FW         IPU_CPMEM_WORD(0, 125, 13)
#define IPU_FIELD_FH         IPU_CPMEM_WORD(0, 138, 12)
#define IPU_FIELD_EBA0       IPU_CPMEM_WORD(1, 0, 29)
#define IPU_FIELD_EBA1       IPU_CPMEM_WORD(1, 29, 29)
#define IPU_FIELD_ILO        IPU_CPMEM_WORD(1, 58, 20)
#define IPU_FIELD_NPB        IPU_CPMEM_WORD(1, 78, 7)
#define IPU_FIELD_PFS        IPU_CPMEM_WORD(1, 85, 4)
#define IPU_FIELD_ALU        IPU_CPMEM_WORD(1, 89, 1)
#define IPU_FIELD_ALBM       IPU_CPMEM_WORD(1, 90, 3)
#define IPU_FIELD_ID         IPU_CPMEM_WORD(1, 93, 2)
#define IPU_FIELD_TH         IPU_CPMEM_WORD(1, 95, 7)
#define IPU_FIELD_SL         IPU_CPMEM_WORD(1, 102, 14)
#define IPU_FIELD_WID0       IPU_CPMEM_WORD(1, 116, 3)
#define IPU_FIELD_WID1       IPU_CPMEM_WORD(1, 119, 3)
#define IPU_FIELD_WID2       IPU_CPMEM_WORD(1, 122, 3)
#define IPU_FIELD_WID3       IPU_CPMEM_WORD(1, 125, 3)
#define IPU_FIELD_OFS0       IPU_CPMEM_WORD(1, 128, 5)
#define IPU_FIELD_OFS1       IPU_CPMEM_WORD(1, 133, 5)
#define IPU_FIELD_OFS2       IPU_CPMEM_WORD(1, 138, 5)
#define IPU_FIELD_OFS3       IPU_CPMEM_WORD(1, 143, 5)
#define IPU_FIELD_SXYS       IPU_CPMEM_WORD(1, 148, 1)
#define IPU_FIELD_CRE        IPU_CPMEM_WORD(1, 149, 1)
#define IPU_FIELD_DEC_SEL2   IPU_CPMEM_WORD(1, 150, 1)

static const char *imx_ipu_common_regname(uint32_t index)
{
    switch (index) {
    case R_IPU_CONF: return "IPU_CONF";
    case R_IPU_SISG_CTRL0: return "IPU_SISG_CTRL0";
    case R_IPU_SISG_CTRL1: return "IPU_SISG_CTRL1";
    case R_IPU_SISG_SET_I: return "IPU_SISG_SET_I";
    case R_IPU_SISG_CLR_I: return "IPU_SISG_CLR_I";
    case R_IPU_INT_CTRL_1: return "IPU_INT_CTRL_1";
    case R_IPU_INT_CTRL_2: return "IPU_INT_CTRL_2";
    case R_IPU_INT_CTRL_3: return "IPU_INT_CTRL_3";
    case R_IPU_INT_CTRL_4: return "IPU_INT_CTRL_4";
    case R_IPU_INT_CTRL_5: return "IPU_INT_CTRL_5";
    case R_IPU_INT_CTRL_6: return "IPU_INT_CTRL_6";
    case R_IPU_INT_CTRL_7: return "IPU_INT_CTRL_7";
    case R_IPU_INT_CTRL_8: return "IPU_INT_CTRL_8";
    case R_IPU_INT_CTRL_9: return "IPU_INT_CTRL_9";
    case R_IPU_INT_CTRL_10: return "IPU_INT_CTRL_10";
    case R_IPU_INT_CTRL_11: return "IPU_INT_CTRL_11";
    case R_IPU_INT_CTRL_12: return "IPU_INT_CTRL_12";
    case R_IPU_INT_CTRL_13: return "IPU_INT_CTRL_13";
    case R_IPU_INT_CTRL_14: return "IPU_INT_CTRL_14";
    case R_IPU_INT_CTRL_15: return "IPU_INT_CTRL_15";

    case R_IPU_SDMA_EVENT_1: return "IPU_SDMA_EVENT_1";
    case R_IPU_SDMA_EVENT_2: return "IPU_SDMA_EVENT_2";
    case R_IPU_SDMA_EVENT_3: return "IPU_SDMA_EVENT_3";
    case R_IPU_SDMA_EVENT_4: return "IPU_SDMA_EVENT_4";
    case R_IPU_SDMA_EVENT_7: return "IPU_SDMA_EVENT_7";
    case R_IPU_SDMA_EVENT_8: return "IPU_SDMA_EVENT_8";
    case R_IPU_SDMA_EVENT_11: return "IPU_SDMA_EVENT_11";
    case R_IPU_SDMA_EVENT_12: return "IPU_SDMA_EVENT_12";
    case R_IPU_SDMA_EVENT_13: return "IPU_SDMA_EVENT_13";
    case R_IPU_SDMA_EVENT_14: return "IPU_SDMA_EVENT_14";

    case R_IPU_SRM_PRI1: return "IPU_SRM_PRI1";
    case R_IPU_SRM_PRI2: return "IPU_SRM_PRI2";

    case R_IPU_FS_PROC_FLOW1: return "IPU_FS_PROC_FLOW1";
    case R_IPU_FS_PROC_FLOW2: return "IPU_FS_PROC_FLOW2";
    case R_IPU_FS_PROC_FLOW3: return "IPU_FS_PROC_FLOW3";

    case R_IPU_FS_DISP_FLOW1: return "IPU_FS_DISP_FLOW1";
    case R_IPU_FS_DISP_FLOW2: return "IPU_FS_DISP_FLOW2";

    case R_IPU_SKIP: return "IPU_SKIP";

    case R_IPU_DISP_GEN: return "IPU_DISP_GEN";
    case R_IPU_DISP_ALT1: return "IPU_DISP_ALT1";
    case R_IPU_DISP_ALT2: return "IPU_DISP_ALT2";
    case R_IPU_DISP_ALT3: return "IPU_DISP_ALT3";
    case R_IPU_DISP_ALT4: return "IPU_DISP_ALT4";

    case R_IPU_SNOOP: return "IPU_SNOOP";
    case R_IPU_MEM_RST: return "IPU_MEM_RST";
    case R_IPU_PM: return "IPU_PM";
    case R_IPU_GPR: return "IPU_GPR";

    case R_IPU_CH_DB_MODE_SEL0: return "IPU_CH_DB_MODE_SEL0";
    case R_IPU_CH_DB_MODE_SEL1: return "IPU_CH_DB_MODE_SEL1";

    case R_IPU_ALT_CH_DB_MODE_SEL0: return "IPU_ALT_CH_DB_MODE_SEL0";
    case R_IPU_ALT_CH_DB_MODE_SEL1: return "IPU_ALT_CH_DB_MODE_SEL1";

    case R_IPU_ALT_CH_TRB_MODE_SEL0: return "IPU_ALT_CH_TRB_MODE_SEL0";

    case R_IPU_INT_STAT_1: return "IPU_INT_STAT_1";
    case R_IPU_INT_STAT_2: return "IPU_INT_STAT_2";
    case R_IPU_INT_STAT_3: return "IPU_INT_STAT_3";
    case R_IPU_INT_STAT_4: return "IPU_INT_STAT_4";
    case R_IPU_INT_STAT_5: return "IPU_INT_STAT_5";
    case R_IPU_INT_STAT_6: return "IPU_INT_STAT_6";
    case R_IPU_INT_STAT_7: return "IPU_INT_STAT_7";
    case R_IPU_INT_STAT_8: return "IPU_INT_STAT_8";
    case R_IPU_INT_STAT_9: return "IPU_INT_STAT_9";
    case R_IPU_INT_STAT_10: return "IPU_INT_STAT_10";
    case R_IPU_INT_STAT_11: return "IPU_INT_STAT_11";
    case R_IPU_INT_STAT_12: return "IPU_INT_STAT_12";
    case R_IPU_INT_STAT_13: return "IPU_INT_STAT_13";
    case R_IPU_INT_STAT_14: return "IPU_INT_STAT_14";
    case R_IPU_INT_STAT_15: return "IPU_INT_STAT_15";
    case R_IPU_CUR_BUF_0: return "IPU_CUR_BUF_0";
    case R_IPU_CUR_BUF_1: return "IPU_CUR_BUF_1";
    case R_IPU_ALT_CUR_0: return "IPU_ALT_CUR_0";
    case R_IPU_ALT_CUR_1: return "IPU_ALT_CUR_1";
    case R_IPU_SRM_STAT: return "IPU_SRM_STAT";
    case R_IPU_PROC_TASKS_STAT: return "IPU_PROC_TASKS_STAT";
    case R_IPU_DISP_TASKS_STAT: return "IPU_DISP_TASKS_STAT";
    case R_IPU_TRIPLE_CUR_BUF_0: return "IPU_TRIPLE_CUR_BUF_0";
    case R_IPU_TRIPLE_CUR_BUF_1: return "IPU_TRIPLE_CUR_BUF_1";

    case R_IPU_CH_BUF0_RDY0: return "IPU_CH_BUF0_RDY0";
    case R_IPU_CH_BUF0_RDY1: return "IPU_CH_BUF0_RDY1";
    case R_IPU_CH_BUF1_RDY0: return "IPU_CH_BUF1_RDY0";
    case R_IPU_CH_BUF1_RDY1: return "IPU_CH_BUF1_RDY1";

    case R_IPU_ALT_CH_BUF0_RDY0: return "IPU_ALT_CH_BUF0_RDY0";
    case R_IPU_ALT_CH_BUF0_RDY1: return "IPU_ALT_CH_BUF0_RDY1";
    case R_IPU_ALT_CH_BUF1_RDY0: return "IPU_ALT_CH_BUF1_RDY0";
    case R_IPU_ALT_CH_BUF1_RDY1: return "IPU_ALT_CH_BUF1_RDY1";

    case R_IPU_CH_BUF2_RDY0: return "IPU_CH_BUF2_RDY0";
    case R_IPU_CH_BUF2_RDY1: return "IPU_CH_BUF2_RDY1";
    }

    return "unknown";
}

static const char *imx_ipu_idmac_regname(uint32_t index)
{
    switch (index) {
    case R_IPU_IDMAC_CONF: return "IPU_IDMAC_CONF";
    case R_IPU_IDMAC_CH_EN_1: return "IPU_IDMAC_CH_EN_1";
    case R_IPU_IDMAC_CH_EN_2: return "IPU_IDMAC_CH_EN_2";
    case R_IPU_IDMAC_SEP_ALPHA: return "IPU_IDMAC_SEP_ALPHA";
    case R_IPU_IDMAC_ALT_SEP_ALPHA: return "IPU_IDMAC_ALT_SEP_ALPHA";
    case R_IPU_IDMAC_CH_PRI_1: return "IPU_IDMAC_CH_PRI_1";
    case R_IPU_IDMAC_CH_PRI_2: return "IPU_IDMAC_CH_PRI_2";
    case R_IPU_IDMAC_WM_EN_1: return "IPU_IDMAC_WM_EN_1";
    case R_IPU_IDMAC_WM_EN_2: return "IPU_IDMAC_WM_EN_2";
    case R_IPU_IDMAC_LOCK_EN_1: return "IPU_IDMAC_LOCK_EN_1";
    case R_IPU_IDMAC_SC_CORD_1: return "IPU_IDMAC_SC_CORD_1";

    case R_IPU_IDMAC_CH_BUSY_1: return "IPU_IDMAC_CH_BUSY_1";
    case R_IPU_IDMAC_CH_BUSY_2: return "IPU_IDMAC_CH_BUSY_2";
    }

    return "unknown";
}

#if 0
static void ipu_ch_param_write_field(struct ipu_ch_param *channel, uint32_t wbs, uint32_t v)
{
    uint32_t bit = (wbs >> 8) % 160;
    uint32_t size = wbs & 0xff;
    uint32_t word = (wbs >> 8) / 160;
    uint32_t i = bit / 32;
    uint32_t ofs = bit % 32;
    uint32_t mask = (1 << size) - 1;
    uint32_t val;

    val = channel->word[word].data[i];
    val &= ~(mask << ofs);
    val |= v << ofs;
    channel->word[word].data[i] = val;
    if ((bit + size - 1) / 32 > i) {
        val = channel->word[word].data[i + 1];
        val &= ~(mask >> (ofs ? (32 - ofs) : 0));
        val |= v >> (ofs ? (32 - ofs) : 0);
        channel->word[word].data[i + 1] = val;
    }
}
#endif

static uint32_t ipu_ch_param_read_field(const ImxIpuState *s, int channel,
                                        uint16_t bit, uint8_t size)
{
    uint32_t i = bit / 32;
    uint32_t ofs = bit % 32;
    uint32_t mask = (1 << size) - 1;
    uint32_t val = 0;

    val = (s->cpmem[channel * 0x10 + i] >> ofs) & mask;
    if ((bit + size - 1) / 32 > i) {
        uint32_t tmp;
        tmp = s->cpmem[channel * 0x10 + i + 1];
        tmp &= mask >> (ofs ? (32 - ofs) : 0);
        val |= tmp << (ofs ? (32 - ofs) : 0);
    }
    return val;
}

static void imx_ipu_check_interrupts(ImxIpuState *s)
{
    bool is_set = false;

    for (int i = R_IPU_INT_STAT_1; i <= R_IPU_INT_STAT_15; i++) {
        is_set |= !!(s->common[i] & s->common[i - R_IPU_INT_STAT_1 + R_IPU_INT_CTRL_1]);
    }

    qemu_set_irq(s->irq_sync, is_set);
}

static void imx6ul_lcdif_draw_line_rgb565(void *opaque, uint8_t *dst,
    const uint8_t *src, int width,
    int dststep)
{
    uint32_t *dst32 = (uint32_t *)dst;
    int i;

    for (i = 0; i < width; i++) {
        uint16_t pixel = lduw_le_p(src);
        uint8_t r = ((pixel >> 11) & 0x1f) << 3;
        uint8_t g = ((pixel >> 5) & 0x3f) << 2;
        uint8_t b = (pixel & 0x1f) << 3;

        *dst32++ = rgb_to_pixel32(r, g, b);
        src += 2;
    }
}

static void imx6ul_lcdif_draw_line_xrgb8888(void *opaque, uint8_t *dst,
    const uint8_t *src, int width,
    int dststep)
{
    uint32_t *dst32 = (uint32_t *)dst;
    int i;

    for (i = 0; i < width; i++) {
        uint32_t pixel = ldl_le_p(src);
        uint8_t r = (pixel >> 16) & 0xff;
        uint8_t g = (pixel >> 8) & 0xff;
        uint8_t b = pixel & 0xff;

        *dst32++ = rgb_to_pixel32(r, g, b);
        src += 4;
    }
}

static uint8_t imx_ipu_bpp(const ImxIpuState *s, int channel)
{
    switch (ipu_ch_param_read_field(s, channel, IPU_FIELD_BPP)) {
    case 0:
        return 32;
    case 1:
        return 24;
    case 2:
        return 18;
    case 3:
        return 16;
    case 4:
        return 12;
    case 5:
        return 8;
    case 6:
        return 4;
    case 7:
        return 4; /* guest error */
    }

    g_assert_not_reached();
}

static uint32_t imx_ipu_frame_width(const ImxIpuState *s, int channel)
{
    return 1 + ipu_ch_param_read_field(s, channel, IPU_FIELD_FW);
}

static uint32_t imx_ipu_frame_height(const ImxIpuState *s, int channel)
{
    return 1 + ipu_ch_param_read_field(s, channel, IPU_FIELD_FH);
}

static uint32_t imx_ipu_extmem_buffer_0_address(const ImxIpuState *s, int channel)
{
    return ipu_ch_param_read_field(s, channel, IPU_FIELD_EBA0) << 3;
}

#if 0
static uint32_t imx_ipu_extmem_buffer_1_address(const ImxIpuState *s, int channel)
{
    return ((s->cpmem[channel *0x10 + 9] << 3) + (s->cpmem[channel * 0x10 + 8] >> 29)) << 3;
}
#endif

static bool imx_ipu_update_display(void *opaque)
{
    struct Channel *ch = opaque;
    ImxIpuState *s = ch->self;
    DisplaySurface *surface = qemu_console_surface(ch->con);
    uint32_t width = imx_ipu_frame_width(s, ch->index);
    uint32_t height = imx_ipu_frame_height(s, ch->index);
    uint32_t frame_base = imx_ipu_extmem_buffer_0_address(s, ch->index);
    uint8_t bpp = imx_ipu_bpp(s, ch->index);
    drawfn fn;
    int first = 0;
    int last = 0;
    int src_width;

    switch (bpp) {
    case 16:
        fn = imx6ul_lcdif_draw_line_rgb565;
        break;
    case 32:
        fn = imx6ul_lcdif_draw_line_xrgb8888;
        break;
    default:
        return true;
    }

    if (surface_width(surface) != width || surface_height(surface) != height) {
        qemu_console_resize(ch->con, width, height);
        surface = qemu_console_surface(ch->con);
        ch->invalidate = true;
    }

    src_width = (width * bpp) / 8;
    if (ch->invalidate || ch->fb_base != frame_base ||
        ch->src_width != src_width || ch->rows != height) {
        framebuffer_update_memory_section(&ch->fbsection, get_system_memory(),
                                          frame_base, height, src_width);
        ch->fb_base = frame_base;
        ch->src_width = src_width;
        ch->rows = height;

        s->common[R_IPU_INT_STAT_1] |= s->common[R_IPU_INT_CTRL_1];
        imx_ipu_check_interrupts(s);
    }

    framebuffer_update_display(surface, &ch->fbsection, width, height,
                               src_width, surface_stride(surface), 0,
                               ch->invalidate, fn, ch, &first, &last);
    if (first >= 0) {
        qemu_console_update(ch->con, 0, first, width, last - first + 1);
    }

    ch->invalidate = false;

    return true;
}

static void imx_ipu_invalidate_display(void *opaque)
{
    struct Channel *ch = opaque;

    ch->invalidate = true;
}

static const GraphicHwOps imx_ipu_graphic_ops = {
    .invalidate = imx_ipu_invalidate_display,
    .gfx_update = imx_ipu_update_display,
};

static uint64_t imx_ipu_read(void *opaque, hwaddr offset, unsigned size)
{
    uint64_t val = 0;

    trace_imx_ipu_read(offset, val);

    return val;
}

static uint64_t imx_ipu_common_read(void *opaque, hwaddr offset, unsigned size)
{
    ImxIpuState *s = opaque;
    const uint32_t reg = offset >> 2;
    uint64_t val = s->common[reg];

    trace_imx_ipu_common_read(offset, imx_ipu_common_regname(reg), val);

    return val;
}

static uint64_t imx_ipu_idmac_read(void *opaque, hwaddr offset, unsigned size)
{
    ImxIpuState *s = opaque;
    const uint32_t reg = offset >> 2;
    uint64_t val = s->idmac[reg];

    trace_imx_ipu_idmac_read(offset, imx_ipu_idmac_regname(reg), val);

    return val;
}

static uint64_t imx_ipu_cpmem_read(void *opaque, hwaddr offset, unsigned size)
{
    ImxIpuState *s = opaque;
    const uint32_t reg = offset >> 2;
    uint64_t val = s->cpmem[reg];

    trace_imx_ipu_cpmem_read(offset, val);

    return val;
}

static void imx_ipu_write(void *opaque, hwaddr offset,
                          uint64_t val, unsigned size)
{
    trace_imx_ipu_write(offset, val);
}

static void imx_ipu_common_write(void *opaque, hwaddr offset, uint64_t val,
                                 unsigned size)
{
    ImxIpuState *s = opaque;
    const uint32_t reg = offset >> 2;

    trace_imx_ipu_common_write(offset, imx_ipu_common_regname(reg), val);

    switch (reg) {
    case R_IPU_CONF:
        if (val & BIT(7)) {
            s->common[R_IPU_INT_STAT_1] |= s->common[R_IPU_INT_CTRL_1];
            for (int i = 0; i < ARRAY_SIZE(s->channels); i++) {
                s->channels[i].invalidate = true;
            }
        }
        QEMU_FALLTHROUGH;
    case R_IPU_SISG_CTRL0... R_IPU_SNOOP:
    case R_IPU_PM ... R_IPU_ALT_CH_TRB_MODE_SEL0:
    case R_IPU_CH_BUF0_RDY0 ... R_IPU_CH_BUF2_RDY1:
        s->common[reg] = val;
        for (int i = 0; i < ARRAY_SIZE(s->channels); i++) {
            s->channels[i].invalidate = true;
        }
        break;

    case R_IPU_MEM_RST:
    case R_IPU_INT_STAT_1 ... R_IPU_INT_STAT_15:
        s->common[reg] &= ~val;
        break;
    }

    imx_ipu_check_interrupts(s);
}

static void imx_ipu_idmac_write(void *opaque, hwaddr offset, uint64_t val,
                                unsigned size)
{
    ImxIpuState *s = opaque;
    const uint32_t reg = offset >> 2;

    trace_imx_ipu_idmac_write(offset, imx_ipu_idmac_regname(reg), val);

    switch (reg) {
    case R_IPU_IDMAC_CH_EN_1:
        for (int ch = 0; ch < 32; ch++) {
            if ((val & BIT(ch)) && !(s->idmac[reg] & BIT(ch))) {
                trace_imx_ipu_enable_channel(ch,
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_PFS),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_BPP),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_NPB),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_FW),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_FH),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_EBA0) << 3,
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_EBA1) << 3,
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_SL),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_SO),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_SLUV),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_UBO) << 3,
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_VBO) << 3,
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_WID0),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_WID1),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_WID2),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_WID3),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_OFS0),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_OFS1),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_OFS2),
                        ipu_ch_param_read_field(s, ch, IPU_FIELD_OFS3));
                s->channels[ch].con = qemu_graphic_console_create(DEVICE(s), 0,
                        &imx_ipu_graphic_ops, &s->channels[ch]);
            } else if (!(val & BIT(ch)) && (s->idmac[reg] & BIT(ch))) {
                if (s->channels[ch].con) {
                    qemu_graphic_console_close(s->channels[ch].con);
                    s->channels[ch].con = NULL;
                }
            }
        }
        break;
    }

    s->idmac[reg] = val;

    imx_ipu_check_interrupts(s);
}

static void imx_ipu_cpmem_write(void *opaque, hwaddr offset, uint64_t val,
                                unsigned size)
{
    ImxIpuState *s = opaque;
    const uint32_t reg = offset >> 2;

    trace_imx_ipu_cpmem_write(offset, val);

    s->cpmem[reg] = val;
}

static const MemoryRegionOps imx_ipu_ops = {
    .read = imx_ipu_read,
    .write = imx_ipu_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps imx_ipu_common_ops = {
    .read = imx_ipu_common_read,
    .write = imx_ipu_common_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps imx_ipu_idmac_ops = {
    .read = imx_ipu_idmac_read,
    .write = imx_ipu_idmac_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static const MemoryRegionOps imx_ipu_cpmem_ops = {
    .read = imx_ipu_cpmem_read,
    .write = imx_ipu_cpmem_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void imx_ipu_reset(DeviceState *dev)
{
    ImxIpuState *s = IMX_IPU(dev);

    memset(s->common, 0, sizeof(s->common));
    memset(s->idmac, 0, sizeof(s->idmac));
    memset(s->cpmem, 0, sizeof(s->cpmem));

    for (int i = 0; i < ARRAY_SIZE(s->channels); i++) {
        struct Channel *ch = &s->channels[i];

        if (ch->con) {
            qemu_graphic_console_close(ch->con);
            ch->con = NULL;
        }
        ch->fb_base = 0;
        ch->src_width = 0;
        ch->rows = 0;
        ch->invalidate = false;
    }

#if 0
    /* DC */
    s->regs[R_IPU_DC_GEN]          = 0x00000060;
    s->regs[R_IPU_DC_DISP_CONF1_0] = 0x00000042;
    s->regs[R_IPU_DC_DISP_CONF1_1] = 0x00000042;
    s->regs[R_IPU_DC_DISP_CONF1_2] = 0x00000042;
    s->regs[R_IPU_DC_DISP_CONF1_3] = 0x00000042;
    s->regs[R_IPU_DC_STAT]         = 0x000000AA;

    /* DMFC */
    s->regs[R_IPU_DMFC_RD_CHAN]         = 0x00000200;
    s->regs[R_IPU_DMFC_WR_CHAN_DEF]     = 0x20202020;
    s->regs[R_IPU_DMFC_DP_CHAN_DEF]     = 0x20202020;
    s->regs[R_IPU_DMFC_GENERAL_1]       = 0x00000003;
    s->regs[R_IPU_DMFC_IC_CTRL]         = 0x00000002;
    s->regs[R_IPU_DMFC_WR_CHAN_DEF_ALT] = 0x00002000;
    s->regs[R_IPU_DMFC_DP_CHAN_DEF_ALT] = 0x20200020;
    s->regs[R_IPU_DMFC_STAT]            = 0x02FFF000;
#endif
}

static void imx_ipu_realize(DeviceState *dev, Error **errp)
{
    ImxIpuState *s = IMX_IPU(dev);

    for (int i = 0; i < ARRAY_SIZE(s->channels); i++) {
        s->channels[i].self = s;
        s->channels[i].index = i;
    }
}

static const VMStateDescription vmstate_imx_ipu = {
    .name = TYPE_IMX_IPU,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(common, ImxIpuState, IMX_IPU_COMMON_SIZE),
        VMSTATE_UINT32_ARRAY(idmac, ImxIpuState, IMX_IPU_IDMAC_SIZE),
        VMSTATE_UINT32_ARRAY(cpmem, ImxIpuState, IMX_IPU_CPMEM_SIZE),
        VMSTATE_END_OF_LIST()
    },
};

static void imx_ipu_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = imx_ipu_realize;
    dc->vmsd = &vmstate_imx_ipu;
    device_class_set_legacy_reset(dc, imx_ipu_reset);
    dc->desc = "i.MX Advanced Vector Interrupt Controller";
}

static void imx_ipuv3m_init(Object *obj)
{
    ImxIpuState *s = IMX_IPU(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->io, obj, &imx_ipu_ops, s, TYPE_IMX_IPU,
                          128 * MiB);
    memory_region_init_io(&s->io_common, obj, &imx_ipu_common_ops, s,
                          TYPE_IMX_IPU ".common", sizeof(s->common));
    memory_region_init_io(&s->io_idmac, obj, &imx_ipu_idmac_ops, s,
                          TYPE_IMX_IPU ".idmac", sizeof(s->idmac));
    memory_region_init_io(&s->io_cpmem, obj, &imx_ipu_cpmem_ops, s,
                          TYPE_IMX_IPU ".cpmem", sizeof(s->cpmem));
    memory_region_add_subregion(&s->io, 0x6000000, &s->io_common);
    memory_region_add_subregion(&s->io, 0x6000000 + 0x8000, &s->io_idmac);
    memory_region_add_subregion(&s->io, 0x7000000, &s->io_cpmem);
    sysbus_init_mmio(sbd, &s->io);
    sysbus_init_irq(sbd, &s->irq_sync);
}

static void imx_ipuv3h_init(Object *obj)
{
    ImxIpuState *s = IMX_IPU(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->io, obj, &imx_ipu_ops, s, TYPE_IMX_IPU,
                          128 * MiB);
    memory_region_init_io(&s->io_common, obj, &imx_ipu_common_ops, s,
                          TYPE_IMX_IPU ".common", sizeof(s->common));
    memory_region_init_io(&s->io_idmac, obj, &imx_ipu_idmac_ops, s,
                          TYPE_IMX_IPU ".idmac", sizeof(s->idmac));
    memory_region_init_io(&s->io_cpmem, obj, &imx_ipu_cpmem_ops, s,
                          TYPE_IMX_IPU ".cpmem", sizeof(s->cpmem));
    memory_region_add_subregion(&s->io, 0x200000, &s->io_common);
    memory_region_add_subregion(&s->io, 0x200000 + 0x8000, &s->io_idmac);
    memory_region_add_subregion(&s->io, 0x300000, &s->io_cpmem);
    sysbus_init_mmio(sbd, &s->io);
    sysbus_init_irq(sbd, &s->irq_sync);
}

static const TypeInfo imx_ipu_types[] = {
    {
        .name = TYPE_IMX_IPU,
        .parent = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(ImxIpuState),
        .class_init = imx_ipu_class_init,
        .abstract = true,
    },
    {
        .name = TYPE_IMX_IPUV3M,
        .parent = TYPE_IMX_IPU,
        .instance_init = imx_ipuv3m_init,
    },
    {
        .name = TYPE_IMX_IPUV3H,
        .parent = TYPE_IMX_IPU,
        .instance_init = imx_ipuv3h_init,
    },
};

DEFINE_TYPES(imx_ipu_types)
