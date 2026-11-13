/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Freescale Integrated Display Unit (DIU)
 *
 * Emulates the display controller found on Freescale/NXP e500 SoCs such as
 * the P1022 used by the A-Eon A1222 (Varisys TABOR).  Only the register file
 * and plane 0 (the main framebuffer) are modelled; that is what the Linux
 * fsl-diu-fb driver uses for its primary framebuffer.
 */

#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "hw/display/fsl_diu.h"
#include "hw/core/irq.h"
#include "hw/display/framebuffer.h"
#include "system/address-spaces.h"
#include "system/memory.h"
#include "qemu/module.h"
#include "qemu/units.h"
#include "ui/pixel_ops.h"

/* Register offsets (all registers are 32-bit, big endian) */
#define DIU_DESC0          0x00
#define DIU_DESC1          0x04
#define DIU_DESC2          0x08
#define DIU_GAMMA          0x0c
#define DIU_PALETTE        0x10
#define DIU_CURSOR         0x14
#define DIU_CURS_POS       0x18
#define DIU_MODE           0x1c
#define DIU_BGND           0x20
#define DIU_BGND_WB        0x24
#define DIU_DISP_SIZE      0x28
#define DIU_WB_SIZE        0x2c
#define DIU_WB_MEM_ADDR    0x30
#define DIU_HSYN_PARA      0x34
#define DIU_VSYN_PARA      0x38
#define DIU_SYN_POL        0x3c
#define DIU_THRESHOLDS     0x40
#define DIU_INT_STATUS     0x44
#define DIU_INT_MASK       0x48
#define DIU_COLORBAR       0x4c
#define DIU_FILLING        0x6c
#define DIU_PLUT           0x70

#define DIU_MODE_OFF       0

/* Area descriptor, as laid out in guest memory by the driver */
struct FslDiuAd {
    uint32_t pix_fmt;           /* little endian */
    uint32_t addr;              /* little endian */
    uint32_t src_size_g_alpha;  /* little endian */
    uint32_t aoi_size;          /* little endian */
    uint32_t offset_xyi;        /* little endian */
    uint32_t offset_xyd;        /* little endian */
    uint8_t ckmax_r;
    uint8_t ckmax_g;
    uint8_t ckmax_b;
    uint8_t res9;
    uint8_t ckmin_r;
    uint8_t ckmin_g;
    uint8_t ckmin_b;
    uint8_t res10;
    uint32_t next_ad;           /* little endian */
    uint32_t paddr;
} QEMU_PACKED;

typedef struct FslDiuPlane {
    uint32_t base;
    uint32_t stride;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t red_off;
    uint8_t red_len;
    uint8_t green_off;
    uint8_t green_len;
    uint8_t blue_off;
    uint8_t blue_len;
} FslDiuPlane;

static uint8_t fsl_diu_scale_component(uint32_t pixel, uint8_t off,
                                       uint8_t len)
{
    uint32_t mask;
    uint32_t value;

    if (!len || len > 16) {
        return 0;
    }

    mask = (1u << len) - 1;
    value = (pixel >> off) & mask;

    if (len >= 8) {
        return value >> (len - 8);
    }

    return (value << (8 - len)) | (value >> (2 * len - 8));
}

static void fsl_diu_draw_line(void *opaque, uint8_t *dst, const uint8_t *src,
                              int width, int dststep)
{
    FslDiuState *s = opaque;
    uint32_t *dst32 = (uint32_t *)dst;
    int i;

    for (i = 0; i < width; i++) {
        uint32_t pixel;
        uint8_t r, g, b;

        switch (s->src_bpp) {
        case 4:
            pixel = ldl_be_p(src);
            break;
        case 3:
            pixel = ((uint32_t)src[0] << 16) |
                    ((uint32_t)src[1] << 8) | src[2];
            break;
        case 2:
            pixel = lduw_be_p(src);
            break;
        default:
            pixel = *src;
            break;
        }

        r = fsl_diu_scale_component(pixel, s->red_off, s->red_len);
        g = fsl_diu_scale_component(pixel, s->green_off, s->green_len);
        b = fsl_diu_scale_component(pixel, s->blue_off, s->blue_len);

        *dst32++ = rgb_to_pixel32(r, g, b);
        src += s->src_bpp;
    }
}

static bool fsl_diu_decode_plane(FslDiuState *s, FslDiuPlane *plane)
{
    struct FslDiuAd ad;
    uint32_t desc = s->regs[DIU_DESC0 / 4];
    uint32_t pix_fmt, src_size, aoi, offset;
    uint32_t size[4], bit_off[4];
    uint32_t red_idx, green_idx, blue_idx;
    uint32_t bpp = 0;
    uint32_t offset_x, offset_y;
    int i;

    if (s->regs[DIU_MODE / 4] == DIU_MODE_OFF || desc == 0) {
        return false;
    }

    if (address_space_read(&address_space_memory, desc,
                           MEMTXATTRS_UNSPECIFIED, &ad, sizeof(ad)) !=
        MEMTX_OK) {
        return false;
    }

    pix_fmt = le32_to_cpu(ad.pix_fmt);
    src_size = le32_to_cpu(ad.src_size_g_alpha);
    aoi = le32_to_cpu(ad.aoi_size);
    offset = le32_to_cpu(ad.offset_xyi);

    for (i = 0; i < 4; i++) {
        size[i] = (pix_fmt >> (4 * i)) & 0xf;
        bpp += size[i];
    }

    if (bpp != 16 && bpp != 24 && bpp != 32) {
        return false;
    }

    red_idx = (pix_fmt >> 19) & 0x3;
    green_idx = (pix_fmt >> 21) & 0x3;
    blue_idx = (pix_fmt >> 23) & 0x3;

    for (i = 0, bpp = 0; i < 4; i++) {
        bit_off[i] = bpp;
        bpp += size[i];
    }

    plane->width = aoi & 0x7ff;
    plane->height = (aoi >> 16) & 0x7ff;
    if (!plane->width || !plane->height ||
        plane->width > 4096 || plane->height > 4096) {
        return false;
    }

    plane->bpp = bpp / 8;
    /*
     * SRC_SIZE_G_ALPHA packs the source width into bits [11:0] (bits
     * [23:12] hold the source height).  The framebuffer pitch is the
     * source width in pixels, not the visible AOI width.
     */
    plane->stride = (src_size & 0xfff) * plane->bpp;
    if (!plane->stride) {
        return false;
    }

    offset_x = offset & 0xffff;
    offset_y = (offset >> 16) & 0xffff;
    plane->base = le32_to_cpu(ad.addr) + offset_y * plane->stride +
                  offset_x * plane->bpp;

    plane->red_off = bit_off[red_idx];
    plane->red_len = size[red_idx];
    plane->green_off = bit_off[green_idx];
    plane->green_len = size[green_idx];
    plane->blue_off = bit_off[blue_idx];
    plane->blue_len = size[blue_idx];

    return true;
}

static bool fsl_diu_update_display(void *opaque)
{
    FslDiuState *s = opaque;
    FslDiuPlane plane;
    DisplaySurface *surface;
    int first = 0, last = 0;

    if (!s->con) {
        return true;
    }

    if (!fsl_diu_decode_plane(s, &plane)) {
        return true;
    }

    surface = qemu_console_surface(s->con);
    if (surface_width(surface) != plane.width ||
        surface_height(surface) != plane.height) {
        qemu_console_resize(s->con, plane.width, plane.height);
        surface = qemu_console_surface(s->con);
        s->invalidate = true;
    }

    if (s->invalidate || s->fb_base != plane.base ||
        s->fb_stride != plane.stride || s->width != plane.width ||
        s->height != plane.height || s->src_bpp != plane.bpp) {
        framebuffer_update_memory_section(&s->fbsection, get_system_memory(),
                                          plane.base, plane.height,
                                          plane.stride);
        s->fb_base = plane.base;
        s->fb_stride = plane.stride;
        s->width = plane.width;
        s->height = plane.height;
        s->src_bpp = plane.bpp;
        s->red_off = plane.red_off;
        s->red_len = plane.red_len;
        s->green_off = plane.green_off;
        s->green_len = plane.green_len;
        s->blue_off = plane.blue_off;
        s->blue_len = plane.blue_len;
        s->invalidate = true;
    }

    framebuffer_update_display(surface, &s->fbsection, plane.width,
                               plane.height, plane.stride,
                               surface_stride(surface), 0, s->invalidate,
                               fsl_diu_draw_line, s, &first, &last);
    if (first >= 0) {
        qemu_console_update(s->con, 0, first, plane.width,
                            last - first + 1);
    }

    s->invalidate = false;
    return true;
}

static void fsl_diu_invalidate_display(void *opaque)
{
    FslDiuState *s = opaque;

    s->invalidate = true;
}

static const GraphicHwOps fsl_diu_graphic_ops = {
    .invalidate = fsl_diu_invalidate_display,
    .gfx_update = fsl_diu_update_display,
};

static uint64_t fsl_diu_read(void *opaque, hwaddr offset, unsigned size)
{
    FslDiuState *s = opaque;
    unsigned idx = offset >> 2;

    if (idx >= FSL_DIU_NUM_REGS) {
        return 0;
    }

    return s->regs[idx];
}

static void fsl_diu_write(void *opaque, hwaddr offset, uint64_t value,
                          unsigned size)
{
    FslDiuState *s = opaque;
    unsigned idx = offset >> 2;

    if (idx >= FSL_DIU_NUM_REGS) {
        return;
    }

    s->regs[idx] = value;

    switch (offset) {
    case DIU_DESC0:
    case DIU_DESC1:
    case DIU_DESC2:
    case DIU_MODE:
    case DIU_DISP_SIZE:
        s->invalidate = true;
        if (s->con) {
            qemu_console_hw_invalidate(s->con);
        }
        break;
    default:
        break;
    }
}

static const MemoryRegionOps fsl_diu_ops = {
    .read = fsl_diu_read,
    .write = fsl_diu_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
        .unaligned = false,
    },
};

static void fsl_diu_reset(DeviceState *dev)
{
    FslDiuState *s = FSL_DIU(dev);

    memset(s->regs, 0, sizeof(s->regs));
    s->regs[DIU_INT_MASK / 4] = 0xffffffff;
    s->invalidate = true;
}

static void fsl_diu_realize(DeviceState *dev, Error **errp)
{
    FslDiuState *s = FSL_DIU(dev);

    s->invalidate = true;
    s->con = qemu_graphic_console_create(dev, 0, &fsl_diu_graphic_ops, s);
}

static void fsl_diu_unrealize(DeviceState *dev)
{
    FslDiuState *s = FSL_DIU(dev);

    if (s->con) {
        qemu_graphic_console_close(s->con);
        s->con = NULL;
    }
}

static void fsl_diu_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = fsl_diu_realize;
    dc->unrealize = fsl_diu_unrealize;
    dc->desc = "Freescale Integrated Display Unit";
    device_class_set_legacy_reset(dc, fsl_diu_reset);
}

static void fsl_diu_init(Object *obj)
{
    FslDiuState *s = FSL_DIU(obj);

    memory_region_init_io(&s->iomem, obj, &fsl_diu_ops, s, TYPE_FSL_DIU,
                          FSL_DIU_MMIO_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->iomem);
    sysbus_init_irq(SYS_BUS_DEVICE(s), &s->irq);
}

static const TypeInfo fsl_diu_types[] = {
    {
        .name          = TYPE_FSL_DIU,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(FslDiuState),
        .instance_init = fsl_diu_init,
        .class_init    = fsl_diu_class_init,
    },
    {
        .name          = "fsl,p1022-diu",
        .parent        = TYPE_FSL_DIU,
    },
};

DEFINE_TYPES(fsl_diu_types)
