/*
 * QTests for eMMC general purpose partitions
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "libqtest.h"
#include "libqtest-single.h"
#include "libqos/sdhci-cmd.h"

/* EXT_CSD field offsets, see hw/sd/sdmmc-internal.h */
#define EXT_CSD_GP_SIZE_MULT                143
#define EXT_CSD_PARTITION_SETTING_COMPLETED 155
#define EXT_CSD_PART_CONFIG                 179
#define EXT_CSD_SEC_CNT                     212

#define EMMC_MMC_BA         0xF0842000
#define EMMC_BLOCK_SIZE     512
#define EMMC_BOOT_SIZE      (128 * 1024)
#define EMMC_RPMB_SIZE      (128 * 1024)
#define EMMC_GP_SIZE        (512 * 1024)
#define EMMC_USER_SIZE      (1024 * 1024)

#define OFFSET_BOOT2        (EMMC_BOOT_SIZE)
#define OFFSET_RPMB         (2 * EMMC_BOOT_SIZE)
#define OFFSET_GP0          (OFFSET_RPMB + EMMC_RPMB_SIZE)
#define OFFSET_GP1          (OFFSET_GP0 + EMMC_GP_SIZE)
#define OFFSET_GP2          (OFFSET_GP1 + EMMC_GP_SIZE)
#define OFFSET_USER         (OFFSET_GP2 + EMMC_GP_SIZE)
#define EMMC_IMAGE_SIZE     (OFFSET_USER + EMMC_USER_SIZE)

#define BOOT1_MARKER        0x11
#define BOOT2_MARKER        0x22
#define RPMB_MARKER         0x33
#define GP0_MARKER          0x44
#define GP1_MARKER          0x55
#define GP2_MARKER          0x66
#define USER_MARKER         0x77

static char *emmc_path;

static void fifo_read(QTestState *qts, uint8_t *buf, size_t len)
{
    size_t i;

    g_assert_cmpuint(len % 4, ==, 0);
    for (i = 0; i < len; i += 4) {
        uint32_t val = qtest_readl(qts, EMMC_MMC_BA + SDHC_BDATA);

        buf[i] = val;
        buf[i + 1] = val >> 8;
        buf[i + 2] = val >> 16;
        buf[i + 3] = val >> 24;
    }
}

static void fifo_write(QTestState *qts, const uint8_t *buf, size_t len)
{
    size_t i;

    g_assert_cmpuint(len % 4, ==, 0);
    for (i = 0; i < len; i += 4) {
        uint32_t val = buf[i] | (buf[i + 1] << 8) |
                       (buf[i + 2] << 16) | ((uint32_t)buf[i + 3] << 24);

        qtest_writel(qts, EMMC_MMC_BA + SDHC_BDATA, val);
    }
}

static QTestState *setup_emmc(void)
{
    QTestState *qts = qtest_initf(
        "-machine kudo-bmc "
        "-device emmc,drive=drive0,"
        "boot-partition-size=%u,rpmb-partition-size=%u,"
        "gp0-partition-size=%u,gp1-partition-size=%u,gp2-partition-size=%u "
        "-drive id=drive0,if=none,file=%s,format=raw,auto-read-only=off",
        EMMC_BOOT_SIZE, EMMC_RPMB_SIZE, EMMC_GP_SIZE, EMMC_GP_SIZE,
        EMMC_GP_SIZE, emmc_path);

    qtest_writew(qts, EMMC_MMC_BA + SDHC_SWRST, SDHC_RESET_ALL);
    qtest_writew(qts, EMMC_MMC_BA + SDHC_CLKCON,
                 SDHC_CLOCK_SDCLK_EN | SDHC_CLOCK_INT_STABLE |
                 SDHC_CLOCK_INT_EN);

    /* CMD0 (GO_IDLE_STATE), CMD1 (SEND_OP_COND) */
    sdhci_cmd_regs(qts, EMMC_MMC_BA, 0, 0, 0, 0, 0);
    sdhci_cmd_regs(qts, EMMC_MMC_BA, 0, 0, 0x40ff8000, 0, (1 << 8));
    /* CMD2 (ALL_SEND_CID) */
    sdhci_cmd_regs(qts, EMMC_MMC_BA, 0, 0, 0, 0, (2 << 8));
    /* CMD3 (SET_RELATIVE_ADDR) and CMD7 (SELECT_CARD) with RCA 1 */
    sdhci_cmd_regs(qts, EMMC_MMC_BA, 0, 0, 1 << 16, 0,
                   (3 << 8) | SDHC_CMD_RESPONSE);
    sdhci_cmd_regs(qts, EMMC_MMC_BA, 0, 0, 1 << 16, 0,
                   (7 << 8) | SDHC_CMD_RESPONSE);

    return qts;
}

static void emmc_read_ext_csd(QTestState *qts, uint8_t *ext_csd)
{
    sdhci_cmd_regs(qts, EMMC_MMC_BA, EMMC_BLOCK_SIZE, 1, 0,
                   SDHC_TRNS_READ | SDHC_TRNS_BLK_CNT_EN,
                   (8 << 8) | SDHC_CMD_RESPONSE | SDHC_CMD_DATA_PRESENT);
    fifo_read(qts, ext_csd, EMMC_BLOCK_SIZE);
}

static void emmc_select_partition(QTestState *qts, uint8_t part)
{
    /* CMD6 (SWITCH), write byte in EXT_CSD_PART_CONFIG */
    uint32_t arg = (3 << 24) | (EXT_CSD_PART_CONFIG << 16) | (part << 8);

    sdhci_cmd_regs(qts, EMMC_MMC_BA, 0, 0, arg, 0,
                   (6 << 8) | SDHC_CMD_RESPONSE);
}

static void emmc_write_ext_csd(QTestState *qts, uint8_t index, uint8_t value)
{
    uint32_t arg = (3 << 24) | (index << 16) | (value << 8);

    sdhci_cmd_regs(qts, EMMC_MMC_BA, 0, 0, arg, 0,
                   (6 << 8) | SDHC_CMD_RESPONSE);
}

static void emmc_read_block(QTestState *qts, uint32_t addr, uint8_t *buf)
{
    sdhci_cmd_regs(qts, EMMC_MMC_BA, EMMC_BLOCK_SIZE, 1, addr,
                   SDHC_TRNS_READ | SDHC_TRNS_BLK_CNT_EN,
                   (17 << 8) | SDHC_CMD_RESPONSE | SDHC_CMD_DATA_PRESENT);
    fifo_read(qts, buf, EMMC_BLOCK_SIZE);
}

static void emmc_write_block(QTestState *qts, uint32_t addr, const uint8_t *buf)
{
    sdhci_cmd_regs(qts, EMMC_MMC_BA, EMMC_BLOCK_SIZE, 1, addr,
                   SDHC_TRNS_WRITE | SDHC_TRNS_BLK_CNT_EN,
                   (24 << 8) | SDHC_CMD_RESPONSE | SDHC_CMD_DATA_PRESENT);
    fifo_write(qts, buf, EMMC_BLOCK_SIZE);
}

static uint32_t get_gp_size_mult(const uint8_t *ext_csd, unsigned int gp)
{
    const uint8_t *p = &ext_csd[EXT_CSD_GP_SIZE_MULT + 3 * gp];

    return p[0] | (p[1] << 8) | (p[2] << 16);
}

static void check_block_marker(QTestState *qts, uint8_t part, uint8_t marker)
{
    uint8_t block[EMMC_BLOCK_SIZE];
    size_t i;

    emmc_select_partition(qts, part);
    emmc_read_block(qts, 0, block);
    for (i = 0; i < sizeof(block); i++) {
        g_assert_cmphex(block[i], ==, marker);
    }
}

static void test_ext_csd(void)
{
    QTestState *qts = setup_emmc();
    uint8_t ext_csd[EMMC_BLOCK_SIZE];
    unsigned int i;

    emmc_read_ext_csd(qts, ext_csd);

    for (i = 0; i < 3; i++) {
        g_assert_cmphex(get_gp_size_mult(ext_csd, i), ==,
                        EMMC_GP_SIZE / (512 * 1024));
    }
    g_assert_cmphex(get_gp_size_mult(ext_csd, 3), ==, 0);
    g_assert_cmphex(ext_csd[EXT_CSD_PARTITION_SETTING_COMPLETED], ==, 1);
    g_assert_cmphex(ext_csd[EXT_CSD_PART_CONFIG] & 0x7, ==, 0);
    g_assert_cmphex(ldl_le_p(&ext_csd[EXT_CSD_SEC_CNT]), ==,
                    EMMC_USER_SIZE / 512);

    qtest_quit(qts);
}

static void test_read_partitions(void)
{
    QTestState *qts = setup_emmc();

    check_block_marker(qts, 1, BOOT1_MARKER);
    check_block_marker(qts, 2, BOOT2_MARKER);
    check_block_marker(qts, 4, GP0_MARKER);
    check_block_marker(qts, 5, GP1_MARKER);
    check_block_marker(qts, 6, GP2_MARKER);
    check_block_marker(qts, 0, USER_MARKER);

    qtest_quit(qts);
}

static void test_unconfigured_gp(void)
{
    QTestState *qts = setup_emmc();
    uint8_t ext_csd[EMMC_BLOCK_SIZE];

    /* GP3 has no size configured, so switching to it must be rejected */
    emmc_select_partition(qts, 7);
    emmc_read_ext_csd(qts, ext_csd);
    g_assert_cmphex(ext_csd[EXT_CSD_PART_CONFIG] & 0x7, ==, 0);

    qtest_quit(qts);
}

static void test_fixed_gp_config(void)
{
    QTestState *qts = setup_emmc();
    uint8_t ext_csd[EMMC_BLOCK_SIZE];

    emmc_write_ext_csd(qts, EXT_CSD_GP_SIZE_MULT, 2);
    emmc_read_ext_csd(qts, ext_csd);
    g_assert_cmphex(get_gp_size_mult(ext_csd, 0), ==,
                    EMMC_GP_SIZE / (512 * 1024));
    g_assert_cmphex(ext_csd[EXT_CSD_PARTITION_SETTING_COMPLETED], ==, 1);

    emmc_write_ext_csd(qts, EXT_CSD_PARTITION_SETTING_COMPLETED, 0);
    emmc_read_ext_csd(qts, ext_csd);
    g_assert_cmphex(ext_csd[EXT_CSD_PARTITION_SETTING_COMPLETED], ==, 1);

    qtest_quit(qts);
}

static void check_file_block(int fd, off_t offset, uint8_t marker)
{
    uint8_t block[EMMC_BLOCK_SIZE];
    size_t i;

    g_assert_cmpint(lseek(fd, offset, SEEK_SET), ==, offset);
    g_assert_cmpint(read(fd, block, sizeof(block)), ==, sizeof(block));
    for (i = 0; i < sizeof(block); i++) {
        g_assert_cmphex(block[i], ==, marker);
    }
}

static void test_write_partitions(void)
{
    QTestState *qts = setup_emmc();
    uint8_t block[EMMC_BLOCK_SIZE];
    int fd;

    memset(block, 0xa0, sizeof(block));
    emmc_select_partition(qts, 4);
    emmc_write_block(qts, 0, block);

    memset(block, 0xb0, sizeof(block));
    emmc_select_partition(qts, 5);
    emmc_write_block(qts, 0, block);

    memset(block, 0xc0, sizeof(block));
    emmc_select_partition(qts, 0);
    emmc_write_block(qts, 0, block);

    qtest_quit(qts);

    fd = open(emmc_path, O_RDONLY);
    g_assert(fd >= 0);
    check_file_block(fd, OFFSET_GP0, 0xa0);
    check_file_block(fd, OFFSET_GP1, 0xb0);
    check_file_block(fd, OFFSET_USER, 0xc0);
    close(fd);
}

static void drive_create(void)
{
    GError *error = NULL;
    uint8_t *image;
    int fd;
    ssize_t ret;

    fd = g_file_open_tmp("emmc_XXXXXX", &emmc_path, &error);
    if (fd == -1) {
        fprintf(stderr, "unable to create eMMC image: %s\n", error->message);
    }
    g_assert(fd >= 0);
    g_assert(error == NULL);

    image = g_malloc0(EMMC_IMAGE_SIZE);
    memset(&image[0], BOOT1_MARKER, EMMC_BLOCK_SIZE);
    memset(&image[OFFSET_BOOT2], BOOT2_MARKER, EMMC_BLOCK_SIZE);
    memset(&image[OFFSET_RPMB], RPMB_MARKER, EMMC_BLOCK_SIZE);
    memset(&image[OFFSET_GP0], GP0_MARKER, EMMC_BLOCK_SIZE);
    memset(&image[OFFSET_GP1], GP1_MARKER, EMMC_BLOCK_SIZE);
    memset(&image[OFFSET_GP2], GP2_MARKER, EMMC_BLOCK_SIZE);
    memset(&image[OFFSET_USER], USER_MARKER, EMMC_BLOCK_SIZE);

    ret = write(fd, image, EMMC_IMAGE_SIZE);
    g_assert_cmpint(ret, ==, EMMC_IMAGE_SIZE);
    g_free(image);
    close(fd);
}

static void drive_destroy(void)
{
    unlink(emmc_path);
    g_free(emmc_path);
}

int main(int argc, char **argv)
{
    int ret;

    drive_create();

    g_test_init(&argc, &argv, NULL);

    qtest_add_func("emmc/ext_csd", test_ext_csd);
    qtest_add_func("emmc/read_partitions", test_read_partitions);
    qtest_add_func("emmc/unconfigured_gp", test_unconfigured_gp);
    qtest_add_func("emmc/fixed_gp_config", test_fixed_gp_config);
    qtest_add_func("emmc/write_partitions", test_write_partitions);

    ret = g_test_run();
    drive_destroy();
    return ret;
}
