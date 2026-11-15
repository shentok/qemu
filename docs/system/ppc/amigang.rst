=======================================================================
AmigaNG boards (``amigaone``, ``pegasos1``, ``pegasos2``, ``sam460ex``)
=======================================================================

These PowerPC machines emulate boards that are primarily used for
running Amiga like OSes (AmigaOS 4, MorphOS and AROS) but these can
also run Linux which is what this section documents.

Eyetech AmigaOne/Mai Logic Teron (``amigaone``)
===============================================

The ``amigaone`` machine emulates an AmigaOne XE mainboard by Eyetech
which is a rebranded Mai Logic Teron board with modified U-Boot
firmware to support AmigaOS 4.

Emulated devices
----------------

 * PowerPC 7457 CPU (can also use ``-cpu g3, 750cxe, 750fx`` or ``750gx``)
 * Articia S north bridge
 * VIA VT82C686B south bridge
 * PCI VGA compatible card (guests may need other card instead)
 * PS/2 keyboard and mouse
 * 4 KiB NVRAM (use ``-drive if=mtd,format=raw,file=nvram.bin`` to keep contents persistent)

Firmware
--------

A firmware binary is necessary for the boot process. It is a modified
U-Boot under GPL but its source is lost so it cannot be included in
QEMU. A binary is available at
https://www.hyperion-entertainment.com/index.php/downloads?view=files&parent=28.
The ROM image is in the last 512kB which can be extracted with the
following command:

.. code-block:: bash

  $ tail -c 524288 updater.image > u-boot-amigaone.bin

The BIOS emulator in the firmware is unable to run QEMU‘s standard
vgabios so ``VGABIOS-lgpl-latest.bin`` is needed instead which can be
downloaded from http://www.nongnu.org/vgabios.

Running Linux
-------------

There are some Linux images under the following link that work on the
``amigaone`` machine:
https://sourceforge.net/projects/amigaone-linux/files/debian-installer/.
To boot the system run:

.. code-block:: bash

  $ qemu-system-ppc -machine amigaone -bios u-boot-amigaone.bin \
                    -cdrom "A1 Linux Net Installer.iso" \
                    -device ati-vga,model=rv100,romfile=VGABIOS-lgpl-latest.bin

If a firmware menu appears, select ``Boot sequence`` → ``Amiga Multiboot Options``
and set ``Boot device 1`` to ``Onboard VIA IDE CDROM``. Then hit escape until
the main screen appears again, hit escape once more and from the exit menu that
appears select either ``Save settings and exit`` or ``Use settings for this
session only``. It may take a long time loading the kernel into memory but
eventually it boots and the installer becomes visible. The ``ati-vga`` RV100
emulation is not complete yet so only frame buffer works, DRM and 3D is not
available.

Genesi/bPlan Pegasos (``pegasos1``, ``pegasos2``)
=================================================

The ``pegasos1`` machine emulates the original Pegasos (later marked I) sold by
Genesi and designed by bPlan. It uses the same Articia S north bridge as the
``amigaone`` machine, otherwise it is mostly the same as the later Pegasos II.

The ``pegasos2`` machine emulates the Pegasos II which is a redesigned version
of Pegasos I to fix problems with its north bridge. Its schematics are available
at https://www.powerdeveloper.org/platforms/pegasos/schematics.

Emulated devices
----------------

 * PowerPC 7457 CPU (can also use ``-cpu g3`` or ``750cxe``)
 * Articia S north bridge (for ``pegasos1``)
 * Marvell MV64361 Discovery II north bridge (for ``pegasos2``)
 * VIA VT8231 south bridge
 * PCI VGA compatible card (guests may need other card instead)
 * PS/2 keyboard and mouse

Firmware
--------

The Pegasos boards have an Open Firmware compliant ROM based on
SmartFirmware with some changes that are not open-sourced therefore
the ROM binary cannot be included in QEMU. A Pegasos II updater was available
from bPlan, it can be found in the `Internet Archive
<http://web.archive.org/web/20071021223056/http://www.bplan-gmbh.de/up050404/up050404>`_.
The ROM image can be extracted from it with the following command:

.. code-block:: bash

  $ tail -c +85581 up050404 | head -c 524288 > pegasos2.rom

Running Linux
-------------

The PowerPC version of Debian 8.11 supported Pegasos II. The BIOS
emulator in the firmware binary is unable to run QEMU‘s standard
vgabios so it needs to be disabled. To boot the system run:

.. code-block:: bash

  $ qemu-system-ppc -machine pegasos2 -bios pegasos2.rom \
                    -cdrom debian-8.11.0-powerpc-netinst.iso \
                    -device VGA,romfile="" -serial stdio

At the firmware ``ok`` prompt enter ``boot cd install/pegasos``.

Alternatively, it is possible to boot the kernel directly without
firmware ROM using the QEMU built-in minimal Virtual Open Firmware
(VOF) emulation which is also supported on ``pegasos1`` and ``pegasos2``. For this,
extract the kernel ``install/powerpc/vmlinuz-chrp.initrd`` from the CD
image, then run:

.. code-block:: bash

  $ qemu-system-ppc -machine pegasos2 -serial stdio \
                    -kernel vmlinuz-chrp.initrd -append "---" \
                    -cdrom debian-8.11.0-powerpc-netinst.iso

aCube Sam460ex (``sam460ex``)
=============================

The ``sam460ex`` machine emulates the Sam460ex board by aCube which is
based on the AMCC PowerPC 460EX SoC (that despite its name has a
PPC440 CPU core).

Firmware
--------

The board has a firmware based on an older U-Boot version with
modifications to support booting AmigaOS 4. The firmware ROM is
included with QEMU.

Emulated devices
----------------

 * PowerPC 460EX SoC
 * M41T80 serial RTC chip
 * Silicon Motion SM501 display parts (identical to SM502 on real board)
 * Silicon Image SiI3112 2 port SATA controller
 * USB keyboard and mouse

Running Linux
-------------

The only Linux distro that supported Sam460ex out of box was CruxPPC
2.x. It can be booted by running:

.. code-block:: bash

  $ qemu-system-ppc -machine sam460ex -serial stdio \
                    -drive if=none,id=cd,format=raw,file=crux-ppc-2.7a.iso \
                    -device ide-cd,drive=cd,bus=ide.1

There are some other kernels and instructions for booting other
distros on aCube's product page at
https://www.acube-systems.biz/index.php?page=hardware&pid=5
but those are untested.

A-Eon A1222 (Varisys TABOR)
===========================

Running U-Boot
--------------

The SD card image from the firmware release archive has to be prepared
before QEMU can use it.  QEMU only accepts SD card images whose size is
a power of two, and the firmware's built in environment starts the
AmigaBoot menu, which needs a display.  Prepare a copy once:

.. code-block:: bash

  $ cp firmware-a1222.img firmware-a1222-qemu.img
  $ chmod u+w firmware-a1222-qemu.img
  $ qemu-img resize -f raw firmware-a1222-qemu.img 64M
  $ python3 - <<'EOF'
  p = 'firmware-a1222-qemu.img'
  d = bytearray(open(p, 'rb').read())
  i = d.find(b'preboot=ami\x00')
  assert i >= 0, 'preboot entry not found'
  d[i:i + 12] = b'preboot=;; \x00'
  open(p, 'wb').write(d)
  EOF

The last step replaces ``preboot=ami`` in the default environment, so
the firmware drops straight to the ``A1222>`` prompt instead of starting
AmigaBoot.  Without it the AmigaBoot screen appears first; press a key
there to return to the prompt.

.. code-block:: bash

  $ qemu-system-ppc -machine mpc8544ds -dtb tabor2.dtb -m 1G \
                    -drive id=mydrive,if=none,file=firmware-a1222-qemu.img,format=raw \
                    -device sd-card,drive=mydrive \
                    -drive id=tabor,if=none,file=Tabor_debian_8_powerpcspe_5.img,format=raw,snapshot=on \
                    -device ide-hd,drive=tabor,bus=ide.1 \
                    -device sparse-mem,baseaddr=0xffd00000,length=256000,priority=1 \
                    -serial stdio

The firmware image must be the first drive that is not attached to a
controller (``if=none``): QEMU reads the e500 boot block from the first
such drive to start the firmware, and the same drive is then used as the
SD card.

The SD card device is required as well.  The firmware's second program
loader reads ``u-boot.bin`` from the card, so without ``-device
sd-card`` the boot stops at ``Second program loader running in sram...``,
and a blank card of the right size fails at the same point.  The card
has to be backed by the prepared firmware image.

The ``tabor2.dtb`` supplied with QEMU describes the A1222 board.  The
GUTS model reads the ``fsl,pordevsr`` property from its
``global-utilities`` node to configure the SerDes protocol; the stock
``tabor2.dtb`` sets it to ``0x00240000`` (protocol 9, lane 0 is PCIe1),
which matches the board strapping.

Booting Linux with the firmware
-------------------------------

The firmware can boot Linux from the SATA disk.  The Debian 8 image for
the A1222 (``Tabor_debian_8_powerpcspe_5.img``) has a FAT boot partition
containing the kernel ``uImage.sdk17.2`` and the device tree, and an
ext4 root partition.  Attach it to the second emulated SATA port (the
first port is empty), as in the command above.  At the ``A1222>``
prompt load the kernel and device tree from the FAT partition and boot
with the root filesystem on the second partition:

.. code-block:: bash

  A1222> setenv bootargs root=/dev/sda2 rootdelay=10 console=ttyS0,115200
  A1222> sata init
  A1222> sata device 1
  A1222> fatload sata 1:1 1000000 uImage.sdk17.2
  A1222> fatload sata 1:1 2000000 tabor2.dtb
  A1222> bootm 1000000 - 2000000

Linux takes over the same serial port and, after a few minutes, prints
the login prompt:

.. code-block:: bash

  Debian GNU/Linux 8 TaborPowerPCSPE ttyS0
  TaborPowerPCSPE login:

Most of the boot time is spent in the network init script, which retries
while no network card is present.  Do not add ``console=tty0`` to the
command line: the DIU framebuffer console never initialises with the
stock device tree, and the extra console only makes the serial log
harder to read.

The device tree on the disk is the stock A1222 tree; the
``-dtb tabor2.dtb`` passed to QEMU is the tree used by the firmware.
For the DIU framebuffer console, replace the device tree on the disk
with the Linux-patched copy described below; otherwise Linux boots with
the serial console only.

The graphical AmigaBoot menu is not available.  The firmware's x86 BIOS
emulator keeps its own software model of the VGA registers and video
memory and never accesses the emulated card, and its own INT 10h handler
only implements a board specific ``AH=0x12``/``BL=0x32`` call instead of
forwarding to the card's BIOS.  Its VESA call therefore always fails,
whatever VGA BIOS is supplied, and booting stops with ``No VGA -
Required for AmigaBoot``.  Use ``-display none`` or the serial console.

Running Linux
-------------

The ``mpc8544ds`` machine can also boot Linux directly, without the
firmware.  The Debian 8 image ships everything that is needed in its FAT
boot partition: the kernel ``uImage.sdk17.2`` and the device tree
``tabor2.dtb``.  Copy both files out of the image and boot from the SATA
disk:

.. code-block:: bash

  $ qemu-system-ppc -machine mpc8544ds -m 1G -dtb tabor2.dtb \
                    -kernel uImage.sdk17.2 \
                    -append "root=/dev/sda2 console=ttyS0,115200" \
                    -drive id=tabor,if=none,file=Tabor_debian_8_powerpcspe_5.img,format=raw,snapshot=on \
                    -device ide-hd,drive=tabor,bus=ide.1 \
                    -serial stdio

The SATA disk has to be attached to the second emulated port; the first
port is empty on the real board and the firmware expects the disk on
``sata 1``.

The FAT partition starts at sector 2048 and is 81920 sectors long, so
the two files can be extracted with:

.. code-block:: bash

  $ dd if=Tabor_debian_8_powerpcspe_5.img of=boot.img bs=512 skip=2048 count=81920
  $ mcopy -i boot.img ::uImage.sdk17.2 ::tabor2.dtb .

On the real board the firmware relocates the CCSR from its reset address
``0xff700000`` to ``0xffe00000`` by writing ``CCSRBAR``, and the device
tree describes the relocated address.  A directly booted kernel never
writes ``CCSRBAR``, so when both ``-kernel`` and ``-dtb`` are given QEMU
keeps the CCSR at the address the device tree populated instead of
applying the reset address.  No extra options are needed for this.

The device tree also lacks the CPU clock properties that the firmware
adds before starting the kernel.  QEMU fills in ``timebase-frequency``,
``clock-frequency`` and ``bus-frequency`` when they are missing, so guest
time advances at the correct rate.  Booting to the login prompt still
takes several minutes, mostly because the network init script retries
while no network card is present.

The Integrated Display Unit (DIU) is emulated, so the Linux
``fsl-diu-fb`` driver can provide the framebuffer console in the QEMU
display window.  The stock kernel of the Debian image has no DIU
support; build an e500v2 kernel, for example with
``mpc85xx_smp_defconfig``, and make sure the following options are
enabled:

 * ``CONFIG_FB``
 * ``CONFIG_FB_FSL_DIU``
 * ``CONFIG_FRAMEBUFFER_CONSOLE``

Then build a uImage:

.. code-block:: bash

  $ export ARCH=powerpc
  $ export CROSS_COMPILE=powerpc-linux-
  $ make mpc85xx_smp_defconfig
  $ make uImage

The DIU driver only initialises if the ``p1022_ds`` platform code has
installed its clock and monitor callbacks, which requires the root
``compatible`` property to match ``fsl,p1022ds``. The stock
``tabor2.dtb`` identifies only as ``varisys,TABOR``, so patch a copy of
it. The ``fdtput`` tool is part of the device tree compiler package;
adjust the path to the installed DTB if QEMU is installed elsewhere:

.. code-block:: bash

  $ cp /usr/share/qemu/dtb/tabor2.dtb tabor2-linux.dtb
  $ fdtput -t s tabor2-linux.dtb / compatible "fsl,p1022ds" "varisys,TABOR"

QEMU patches the memory size, kernel command line and initrd location
into a device tree supplied with ``-dtb``, so no further changes are
needed. Boot the kernel and an initramfs with:

.. code-block:: bash

  $ qemu-system-ppc -M mpc8544ds -dtb tabor2-linux.dtb -m 1G \
                    -kernel arch/powerpc/boot/uImage -initrd /path/to/rootfs.cpio \
                    -append "console=ttyS0,115200 console=tty0" \
                    -serial stdio -vnc :1

Linux switches the console to the DIU framebuffer and shows the boot
log and login prompt in the QEMU window (or over VNC at display
``:1``). Only the primary plane is emulated; the additional overlay
planes registered by the driver are not rendered.
