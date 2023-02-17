/*
 * QTest testcase for migration
 *
 * Copyright (c) 2016-2018 Red Hat, Inc. and/or its affiliates
 *   based on the vhost-user-test.c that is:
 *      Copyright (c) 2014 Virtual Open Systems Sarl.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"

#include "libqtest.h"
#include "qapi/error.h"
#include "qapi/qmp/qdict.h"
#include "qemu/module.h"
#include "qemu/sockets.h"
#include "qapi/qapi-visit-sockets.h"
#include "qapi/qobject-input-visitor.h"

#include "migration-helpers.h"
#include "tests/migration/migration-test.h"

/* TODO actually test the results and get rid of this */
#define qtest_qmp_discard_response(...) qobject_unref(qtest_qmp(__VA_ARGS__))

unsigned start_address;
unsigned end_address;

#if defined(__linux__)
#include <sys/syscall.h>
#include <sys/vfs.h>
#endif

static char *tmpfs;

/* The boot file modifies memory area in [start_address, end_address)
 * repeatedly. It outputs a 'B' at a fixed rate while it's still running.
 */
#include "tests/migration/i386/a-b-bootblock.h"
#include "tests/migration/aarch64/a-b-kernel.h"
#include "tests/migration/s390x/a-b-bios.h"

static void init_bootfile(const char *bootpath, void *content, size_t len)
{
    FILE *bootfile = fopen(bootpath, "wb");

    g_assert_cmpint(fwrite(content, len, 1, bootfile), ==, 1);
    fclose(bootfile);
}

/*
 * Wait for some output in the serial output file,
 * we get an 'A' followed by an endless string of 'B's
 * but on the destination we won't have the A.
 */
static void wait_for_serial(const char *side)
{
    g_autofree char *serialpath = g_strdup_printf("%s/%s", tmpfs, side);
    FILE *serialfile = fopen(serialpath, "r");
    const char *arch = qtest_get_arch();
    int started = (strcmp(side, "src_serial") == 0 &&
                   strcmp(arch, "ppc64") == 0) ? 0 : 1;

    do {
        int readvalue = fgetc(serialfile);

        if (!started) {
            /* SLOF prints its banner before starting test,
             * to ignore it, mark the start of the test with '_',
             * ignore all characters until this marker
             */
            switch (readvalue) {
            case '_':
                started = 1;
                break;
            case EOF:
                fseek(serialfile, 0, SEEK_SET);
                usleep(1000);
                break;
            }
            continue;
        }
        switch (readvalue) {
        case 'A':
            /* Fine */
            break;

        case 'B':
            /* It's alive! */
            fclose(serialfile);
            return;

        case EOF:
            started = (strcmp(side, "src_serial") == 0 &&
                       strcmp(arch, "ppc64") == 0) ? 0 : 1;
            fseek(serialfile, 0, SEEK_SET);
            usleep(1000);
            break;

        default:
            fprintf(stderr, "Unexpected %d on %s serial\n", readvalue, side);
            g_assert_not_reached();
        }
    } while (true);
}

/*
 * It's tricky to use qemu's migration event capability with qtest,
 * events suddenly appearing confuse the qmp()/hmp() responses.
 */

static int64_t read_ram_property_int(QTestState *who, const char *property)
{
    QDict *rsp_return, *rsp_ram;
    int64_t result;

    rsp_return = migrate_query_not_failed(who);
    if (!qdict_haskey(rsp_return, "ram")) {
        /* Still in setup */
        result = 0;
    } else {
        rsp_ram = qdict_get_qdict(rsp_return, "ram");
        result = qdict_get_try_int(rsp_ram, property, 0);
    }
    qobject_unref(rsp_return);
    return result;
}

static uint64_t get_migration_pass(QTestState *who)
{
    return read_ram_property_int(who, "dirty-sync-count");
}

static void wait_for_migration_pass(QTestState *who)
{
    uint64_t initial_pass = get_migration_pass(who);
    uint64_t pass;

    /* Wait for the 1st sync */
    while (!got_stop && !initial_pass) {
        usleep(1000);
        initial_pass = get_migration_pass(who);
    }

    do {
        usleep(1000);
        pass = get_migration_pass(who);
    } while (pass == initial_pass && !got_stop);
}

static void check_guests_ram(QTestState *who)
{
    /* Our ASM test will have been incrementing one byte from each page from
     * start_address to < end_address in order. This gives us a constraint
     * that any page's byte should be equal or less than the previous pages
     * byte (mod 256); and they should all be equal except for one transition
     * at the point where we meet the incrementer. (We're running this with
     * the guest stopped).
     */
    unsigned address;
    uint8_t first_byte;
    uint8_t last_byte;
    bool hit_edge = false;
    int bad = 0;

    qtest_memread(who, start_address, &first_byte, 1);
    last_byte = first_byte;

    for (address = start_address + TEST_MEM_PAGE_SIZE; address < end_address;
         address += TEST_MEM_PAGE_SIZE)
    {
        uint8_t b;
        qtest_memread(who, address, &b, 1);
        if (b != last_byte) {
            if (((b + 1) % 256) == last_byte && !hit_edge) {
                /* This is OK, the guest stopped at the point of
                 * incrementing the previous page but didn't get
                 * to us yet.
                 */
                hit_edge = true;
                last_byte = b;
            } else {
                bad++;
                if (bad <= 10) {
                    fprintf(stderr, "Memory content inconsistency at %x"
                            " first_byte = %x last_byte = %x current = %x"
                            " hit_edge = %x\n",
                            address, first_byte, last_byte, b, hit_edge);
                }
            }
        }
    }
    if (bad >= 10) {
        fprintf(stderr, "and in another %d pages", bad - 10);
    }
    g_assert(bad == 0);
}

static void cleanup(const char *filename)
{
    g_autofree char *path = g_strdup_printf("%s/%s", tmpfs, filename);

    unlink(path);
}

static char *migrate_get_socket_address(QTestState *who, const char *parameter)
{
    QDict *rsp;
    char *result;
    SocketAddressList *addrs;
    Visitor *iv = NULL;
    QObject *object;

    rsp = migrate_query(who);
    object = qdict_get(rsp, parameter);

    iv = qobject_input_visitor_new(object);
    visit_type_SocketAddressList(iv, NULL, &addrs, &error_abort);
    visit_free(iv);

    /* we are only using a single address */
    result = socket_uri(addrs->value);

    qapi_free_SocketAddressList(addrs);
    qobject_unref(rsp);
    return result;
}

static long long migrate_get_parameter_int(QTestState *who,
                                           const char *parameter)
{
    QDict *rsp;
    long long result;

    rsp = wait_command(who, "{ 'execute': 'query-migrate-parameters' }");
    result = qdict_get_int(rsp, parameter);
    qobject_unref(rsp);
    return result;
}

static void migrate_check_parameter_int(QTestState *who, const char *parameter,
                                        long long value)
{
    long long result;

    result = migrate_get_parameter_int(who, parameter);
    g_assert_cmpint(result, ==, value);
}

static void migrate_set_parameter_int(QTestState *who, const char *parameter,
                                      long long value)
{
    QDict *rsp;

    rsp = qtest_qmp(who,
                    "{ 'execute': 'migrate-set-parameters',"
                    "'arguments': { %s: %lld } }",
                    parameter, value);
    g_assert(qdict_haskey(rsp, "return"));
    qobject_unref(rsp);
    migrate_check_parameter_int(who, parameter, value);
}

static void migrate_ensure_non_converge(QTestState *who)
{
    /* Can't converge with 1ms downtime + 30 mbs bandwidth limit */
    migrate_set_parameter_int(who, "max-bandwidth", 30 * 1000 * 1000);
    migrate_set_parameter_int(who, "downtime-limit", 1);
}

static void migrate_ensure_converge(QTestState *who)
{
    /* Should converge with 30s downtime + 1 gbs bandwidth limit */
    migrate_set_parameter_int(who, "max-bandwidth", 1 * 1000 * 1000 * 1000);
    migrate_set_parameter_int(who, "downtime-limit", 30 * 1000);
}

typedef struct {
    /* Required: the URI for the dst QEMU to listen on */
    const char *listen_uri;

    /*
     * Optional: the URI for the src QEMU to connect to
     * If NULL, then it will query the dst QEMU for its actual
     * listening address and use that as the connect address.
     * This allows for dynamically picking a free TCP port.
     */
    const char *connect_uri;
} MigrateCommon;

static int test_migrate_start(QTestState **from, QTestState **to,
                              const char *uri)
{
    g_autofree gchar *arch_source = NULL;
    g_autofree gchar *arch_target = NULL;
    g_autofree gchar *cmd_source = NULL;
    g_autofree gchar *cmd_target = NULL;
    g_autofree char *bootpath = NULL;
    const char *arch = qtest_get_arch();
    const char *machine_opts = NULL;
    const char *memory_size;

    got_stop = false;
    bootpath = g_strdup_printf("%s/bootsect", tmpfs);
    if (strcmp(arch, "i386") == 0 || strcmp(arch, "x86_64") == 0) {
        /* the assembled x86 boot sector should be exactly one sector large */
        assert(sizeof(x86_bootsect) == 512);
        init_bootfile(bootpath, x86_bootsect, sizeof(x86_bootsect));
        memory_size = "150M";
        arch_source = g_strdup_printf("-drive file=%s,format=raw", bootpath);
        arch_target = g_strdup(arch_source);
        start_address = X86_TEST_MEM_START;
        end_address = X86_TEST_MEM_END;
    } else if (g_str_equal(arch, "s390x")) {
        init_bootfile(bootpath, s390x_elf, sizeof(s390x_elf));
        memory_size = "128M";
        arch_source = g_strdup_printf("-bios %s", bootpath);
        arch_target = g_strdup(arch_source);
        start_address = S390_TEST_MEM_START;
        end_address = S390_TEST_MEM_END;
    } else if (strcmp(arch, "ppc64") == 0) {
        machine_opts = "vsmt=8";
        memory_size = "256M";
        start_address = PPC_TEST_MEM_START;
        end_address = PPC_TEST_MEM_END;
        arch_source = g_strdup_printf("-nodefaults "
                                      "-prom-env 'use-nvramrc?=true' -prom-env "
                                      "'nvramrc=hex .\" _\" begin %x %x "
                                      "do i c@ 1 + i c! 1000 +loop .\" B\" 0 "
                                      "until'", end_address, start_address);
        arch_target = g_strdup("");
    } else if (strcmp(arch, "aarch64") == 0) {
        init_bootfile(bootpath, aarch64_kernel, sizeof(aarch64_kernel));
        machine_opts = "virt,gic-version=max";
        memory_size = "150M";
        arch_source = g_strdup_printf("-cpu max "
                                      "-kernel %s",
                                      bootpath);
        arch_target = g_strdup(arch_source);
        start_address = ARM_TEST_MEM_START;
        end_address = ARM_TEST_MEM_END;

        g_assert(sizeof(aarch64_kernel) <= ARM_TEST_MAX_KERNEL_SIZE);
    } else {
        g_assert_not_reached();
    }

    cmd_source = g_strdup_printf("-accel kvm -accel tcg%s%s "
                                 "-name source,debug-threads=on "
                                 "-m %s "
                                 "-serial file:%s/src_serial "
                                 "%s %s",
                                 machine_opts ? " -machine " : "",
                                 machine_opts ? machine_opts : "",
                                 memory_size, tmpfs,
                                 arch_source,
                                 "");
    *from = qtest_init(cmd_source);

    cmd_target = g_strdup_printf("-accel kvm -accel tcg%s%s "
                                 "-name target,debug-threads=on "
                                 "-m %s "
                                 "-serial file:%s/dest_serial "
                                 "-incoming %s "
                                 "%s %s",
                                 machine_opts ? " -machine " : "",
                                 machine_opts ? machine_opts : "",
                                 memory_size, tmpfs, uri,
                                 arch_target,
                                 "");
    *to = qtest_init(cmd_target);

    return 0;
}

static void test_migrate_end(QTestState *from, QTestState *to)
{
    unsigned char dest_byte_a, dest_byte_b, dest_byte_c, dest_byte_d;

    qtest_quit(from);

    qtest_memread(to, start_address, &dest_byte_a, 1);

    /* Destination still running, wait for a byte to change */
    do {
        qtest_memread(to, start_address, &dest_byte_b, 1);
        usleep(1000 * 10);
    } while (dest_byte_a == dest_byte_b);

    qtest_qmp_discard_response(to, "{ 'execute' : 'stop'}");

    /* With it stopped, check nothing changes */
    qtest_memread(to, start_address, &dest_byte_c, 1);
    usleep(1000 * 200);
    qtest_memread(to, start_address, &dest_byte_d, 1);
    g_assert_cmpint(dest_byte_c, ==, dest_byte_d);

    check_guests_ram(to);

    qtest_quit(to);

    cleanup("bootsect");
    cleanup("migsocket");
    cleanup("src_serial");
    cleanup("dest_serial");
}

static void test_precopy_common(MigrateCommon *args)
{
    QTestState *from = NULL, *to = NULL;

    if (test_migrate_start(&from, &to, args->listen_uri)) {
        return;
    }

    migrate_ensure_non_converge(from);

    /* Wait for the first serial output from the source */
    wait_for_serial("src_serial");

    if (!args->connect_uri) {
        g_autofree char *local_connect_uri =
            migrate_get_socket_address(to, "socket-address");
        migrate_qmp(from, local_connect_uri, "{}");
    } else {
        migrate_qmp(from, args->connect_uri, "{}");
    }

    wait_for_migration_pass(from);

    migrate_ensure_converge(from);

    /* We do this first, as it has a timeout to stop us
     * hanging forever if migration didn't converge */
    wait_for_migration_complete(from);

    if (!got_stop) {
        qtest_qmp_eventwait(from, "STOP");
    }

    qtest_qmp_eventwait(to, "RESUME");

    wait_for_serial("dest_serial");

    test_migrate_end(from, to);
}

static void test_precopy_unix_plain(void)
{
    g_autofree char *uri = g_strdup_printf("unix:%s/migsocket", tmpfs);
    MigrateCommon args = {
        .listen_uri = uri,
        .connect_uri = uri,
    };

    test_precopy_common(&args);
}

static void test_precopy_tcp_plain(void)
{
    MigrateCommon args = {
        .listen_uri = "tcp:127.0.0.1:0",
    };

    test_precopy_common(&args);
}

int main(int argc, char **argv)
{
    const bool has_kvm = qtest_has_accel("kvm");
    const char *arch = qtest_get_arch();
    g_autoptr(GError) err = NULL;
    int ret;

    g_test_init(&argc, &argv, NULL);

    /*
     * On ppc64, the test only works with kvm-hv, but not with kvm-pr and TCG
     * is touchy due to race conditions on dirty bits (especially on PPC for
     * some reason)
     */
    if (g_str_equal(arch, "ppc64") &&
        (!has_kvm || access("/sys/module/kvm_hv", F_OK))) {
        g_test_message("Skipping test: kvm_hv not available");
        return g_test_run();
    }

    /*
     * Similar to ppc64, s390x seems to be touchy with TCG, so disable it
     * there until the problems are resolved
     */
    if (g_str_equal(arch, "s390x") && !has_kvm) {
        g_test_message("Skipping test: s390x host with KVM is required");
        return g_test_run();
    }

    tmpfs = g_dir_make_tmp("migration-abi-test-XXXXXX", &err);
    if (!tmpfs) {
        g_test_message("Can't create temporary directory in %s: %s",
                       g_get_tmp_dir(), err->message);
    }
    g_assert(tmpfs);

    module_call_init(MODULE_INIT_QOM);

    qtest_add_func("/migration/abi/unix/plain", test_precopy_unix_plain);

    qtest_add_func("/migration/abi/tcp/plain", test_precopy_tcp_plain);

    ret = g_test_run();

    g_assert_cmpint(ret, ==, 0);

    ret = rmdir(tmpfs);
    if (ret != 0) {
        g_test_message("unable to rmdir: path (%s): %s",
                       tmpfs, strerror(errno));
    }
    g_free(tmpfs);

    return ret;
}
