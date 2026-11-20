NXP i.MX 8M Plus and i.MX 8M Mini Evaluation Kits (``imx8mp-evk``, ``imx8mm-evk``)
==================================================================================

The ``imx8mp-evk`` and ``imx8mm-evk`` machine models the i.MX 8M Plus
and i.MX 8M Mini Evaluation Kits, based on i.MX 8M Plus and i.MX8M
Mini SoCs.

Supported devices
-----------------

The ``imx8mp-evk`` and ``imx8mm-evk`` machines implement the
following devices:

 * Up to 4 Cortex-A53 cores
 * Generic Interrupt Controller (GICv3)
 * 4 UARTs
 * 3 USDHC Storage Controllers
 * 1 Designware PCI Express Controller
 * 1 Ethernet Controller
 * 2 Designware USB 3 Controllers
 * 2 FlexCAN 3 CAN Controllers (``imx8mp-evk`` only)
 * 5 GPIO Controllers
 * 6 I2C Controllers
 * 3 SPI Controllers
 * 3 Watchdogs
 * 6 General Purpose Timers
 * Secure Non-Volatile Storage (SNVS) including an RTC
 * System Reset Controller
 * On-Chip OTP Controller (fuses)
 * Clock Tree
 * 1 System Counter

Boot options
------------

The ``imx8mp-evk`` and ``imx8mm-evk`` machines can start a Linux
kernel directly using the standard ``-kernel`` functionality. The
``imx8mm-evk`` machine can additionally start from a boot ROM image.

Direct Linux Kernel Boot
''''''''''''''''''''''''

Probably the easiest way to get started with a whole Linux system on the machine
is to generate an image with Buildroot. Version 2024.11.1 is tested at the time
of writing and involves two steps. First run the following commands in the
toplevel directory of the Buildroot source tree:

For i.MX 8M Plus EVK:

.. code-block:: bash

  $ make freescale_imx8mpevk_defconfig
  $ make

For i.MX 8M Mini EVK:

.. code-block:: bash

  $ make freescale_imx8mmevk_defconfig
  $ make

Once finished successfully there is an ``output/image`` subfolder. Navigate into
it and resize the SD card image to a power of two:

.. code-block:: bash

  $ qemu-img resize sdcard.img 256M

Now that everything is prepared the machine can be started as follows:

For i.MX 8M Plus EVK:

.. code-block:: bash

  $ qemu-system-aarch64 -M imx8mp-evk \
      -display none -serial null -serial stdio \
      -kernel Image \
      -dtb imx8mp-evk.dtb \
      -append "root=/dev/mmcblk2p2" \
      -drive file=sdcard.img,if=sd,bus=2,format=raw,id=mmcblk2

For i.MX 8M Mini EVK:

.. code-block:: bash

  $ qemu-system-aarch64 -M imx8mm-evk \
      -display none -serial null -serial stdio \
      -kernel Image \
      -dtb imx8mm-evk.dtb \
      -append "root=/dev/mmcblk2p2" \
      -drive file=sdcard.img,if=sd,bus=2,format=raw,id=mmcblk2

Boot ROM
''''''''

The ``imx8mm-evk`` machine can also start from a boot ROM image which is
passed using the ``-bios`` option. The image is loaded into the boot ROM
region at address 0, where the CPU starts executing in EL3 just like on real
hardware. This allows for running the complete boot chain, i.e. a boot ROM
which loads an i.MX boot image (``flash.bin``) containing U-Boot SPL, TF-A
and U-Boot from an SD card or eMMC.

The boot mode as determined by the ``BOOT_MODE`` pins on real hardware can be
set using the ``boot-mode`` machine property. USDHC2 is connected to an SD card
while USDHC3 is connected to an eMMC, like on the real board. For example, to
start a boot ROM which boots from an SD card attached to USDHC2:

.. code-block:: bash

  $ qemu-system-aarch64 -M imx8mm-evk \
      -display none -serial null -serial stdio \
      -bios imx8mm-boot.rom \
      -drive file=sdcard.img,if=sd,bus=1,format=raw,id=mmcblk1

Fuses
-----

The fuses of the On-Chip OTP Controller (OCOTP) are blank by default. They can
be backed by a raw file of 4 bytes per fuse word (1 KiB for ``imx8mm-evk``,
1.5 KiB for ``imx8mp-evk``) which makes programmed fuses persistent. Fuse word
``n`` of bank ``b`` is located at offset ``(b * 4 + n) * 4``. For example:

.. code-block:: bash

  $ truncate -s 1536 fuses.bin
  $ qemu-system-aarch64 -M imx8mp-evk ... \
      -drive if=none,id=fuses,file=fuses.bin,format=raw \
      -global imx8m-ocotp.drive=fuses

Like on real hardware, programmed fuses take effect in the shadow registers
after the next reset.

KVM Acceleration
----------------

To enable hardware-assisted acceleration via KVM, append
``-accel kvm`` to the command line. While this speeds up performance
significantly, be aware of the following limitations:

* The ``imx8mp-evk`` and ``imx8mm-evk`` machines are not included
  under the "virtualization use case" of :doc:`QEMU's security
  policy </system/security>`.  This means that you should not trust that
  it can contain malicious guests, whether it is run using TCG or KVM.
  If you don't trust your guests and you're relying on QEMU to be the
  security boundary, you want to choose another machine such as
  ``virt``.
* Rather than Cortex-A53 CPUs, the same CPU type as the host's will be used.
  This is a limitation of KVM and may not work with guests with a tight
  dependency on Cortex-A53.
* No EL2 and EL3 exception levels are available which is also a KVM limitation.
  Direct kernel boot should work but running U-Boot, TF-A, etc. won't succeed.
