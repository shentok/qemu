#!/usr/bin/env python3
#
# Functional test that boots a Linux kernel and checks the console
#
# SPDX-License-Identifier: GPL-2.0-or-later

from qemu_test import LinuxKernelTest, Asset
from qemu_test.utils import image_pow2ceil_expand


class Imx8mpEvkMachine(LinuxKernelTest):

    ASSET_KERNEL = Asset(
        ('https://ftp.debian.org/debian/dists/bookworm/main/installer-armhf/'
         '20230607/images/netboot/vmlinuz'),
        '5851e9c61465a26db810796db671c4729e66e81c365b3ebb1a4b7f772737605b')

    ASSET_INITRD = Asset(
        ('https://ftp.debian.org/debian/dists/bookworm/main/installer-armhf/'
         '20230607/images/netboot/initrd.gz'),
        'fe33f836232a9107716d34dc56d36dea81bab039243a2a42ea6583a50c9b92ca')

    ASSET_DTB = Asset(
        ('https://ftp.debian.org/debian/dists/bookworm/main/installer-armhf/'
         '20230607/images/device-tree/imx53-qsb.dtb'),
        '7af5f34e1424dbbb17d8472fdd38fc2c922baa1678301d8a1ac16d3743cee9b3')

    def setUp(self):
        super().setUp()

        self.kernel_path = self.ASSET_KERNEL.fetch()
        self.initrd_path = self.ASSET_INITRD.fetch()
        self.dtb_path = self.ASSET_DTB.fetch()

    def test_aarch64_imx8mp_evk_usdhc(self):
        self.require_accelerator("tcg")
        self.set_machine('imx53-qsb')
        self.vm.set_console(console_index=0)
        self.vm.add_args('-kernel', self.kernel_path,
                         '-initrd', self.initrd_path,
                         '-dtb', self.dtb_path)

        self.vm.launch()
        self.wait_for_console_pattern('Starting system log daemon:')

if __name__ == '__main__':
    LinuxKernelTest.main()
