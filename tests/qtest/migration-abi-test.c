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
#include "qemu/module.h"
#include "qemu/sockets.h"
#include "io/channel-socket.h"

#include "migration-helpers.h"

static char *tmpfs;

static void cleanup(const char *filename)
{
    g_autofree char *path = g_strdup_printf("%s/%s", tmpfs, filename);

    unlink(path);
}

typedef struct {
    /*
     * Optional: the URI for the src QEMU to connect to
     * If NULL, then it will query the dst QEMU for its actual
     * listening address and use that as the connect address.
     * This allows for dynamically picking a free TCP port.
     */
    const char *connect_uri;
} MigrateCommon;

static void tpm_emu_close_ioc(void *ioc)
{
    qio_channel_close(ioc, NULL);
}

static void test_precopy_common(MigrateCommon *args)
{
    QTestState *from = NULL;
    SocketAddress *sa = socket_parse(args->connect_uri, &error_fatal);
    QIOChannelSocket *lioc = qio_channel_socket_new();
    QIOChannel *ioc;

    qio_channel_socket_listen_sync(lioc, sa, 1, &error_abort);

    got_stop = false;

    from = qtest_init("-S");

    migrate_qmp(from, args->connect_uri, "{}");

    qio_channel_wait(QIO_CHANNEL(lioc), G_IO_IN);
    ioc = QIO_CHANNEL(qio_channel_socket_accept(lioc, &error_abort));
    g_assert(ioc);
    qtest_add_abrt_handler(tpm_emu_close_ioc, ioc);
    while (true) {
        char cmd[1024];
        ssize_t ret;

        ret = qio_channel_read(ioc, cmd, sizeof(cmd), &error_abort);
        if (ret <= 0) {
            break;
        }
    }

    qtest_quit(from);

    cleanup("migsocket");
}

static void test_precopy_unix_plain(void)
{
    g_autofree char *uri = g_strdup_printf("unix:%s/migsocket", tmpfs);
    MigrateCommon args = {
        .connect_uri = uri,
    };

    test_precopy_common(&args);
}

int main(int argc, char **argv)
{
    g_autoptr(GError) err = NULL;
    int ret;

    g_test_init(&argc, &argv, NULL);

    tmpfs = g_dir_make_tmp("migration-abi-test-XXXXXX", &err);
    if (!tmpfs) {
        g_test_message("Can't create temporary directory in %s: %s",
                       g_get_tmp_dir(), err->message);
    }
    g_assert(tmpfs);

    module_call_init(MODULE_INIT_QOM);

    qtest_add_func("/migration/abi/unix/plain", test_precopy_unix_plain);

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
