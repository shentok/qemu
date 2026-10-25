.. SPDX-License-Identifier: GPL-2.0-or-later

==============
eMMC Emulation
==============

Besides SD card emulation, QEMU also offers an eMMC model as found on many
embedded boards. An eMMC, just like an SD card, is connected to the machine
via an SDHCI controller.

Create eMMC Images
==================

A recent eMMC consists of up to 8 partitions: 2 boot partitions, 1 Replay
protected Memory Block (RPMB), up to 4 general purpose (GP) partitions, and the
user data area. QEMU expects backing images for the eMMC to contain those
partitions concatenated in exactly that order. However, the boot partitions,
the RPMB, and the GP partitions might be absent if their sizes are configured
to zero.

The eMMC specification defines alignment constraints for the partitions. The
two boot partitions must be of the same size. Furthermore, boot and RPMB
partitions must be multiples of 128 KB with a maximum of 32640 KB for each
boot partition and 16384K for the RPMB partition. GP partitions must be
multiples of 512 KB.

The alignment constraint of the user data area depends on its size. Up to 2
GByte, the size must be a power of 2. From 2 GByte onward, the size has to be
multiples of 512 byte.

QEMU is enforcing those alignment rules before instantiating the device.
Therefore, the provided image has to strictly follow them as well. The helper
script ``scripts/mkemmc.sh`` can be used to create compliant images, with or
without pre-filled partitions. E.g., to create an eMMC image from a firmware
image and an OS image with an empty 2 MByte RPMB and an empty 512 MByte first
GP partition, use the following command:

.. code-block:: console

    scripts/mkemmc.sh -b firmware.img -r /dev/zero:2M -g /dev/zero:512M os.img emmc.img

This will take care of rounding up the partition sizes to the next valid value
and will leave the RPMB, the GP partition, and the second boot partition empty
(zeroed). The ``-g`` option can be repeated up to four times to add further GP
partitions.

Adding eMMC Devices
===================

An eMMC is either automatically created by a machine model (e.g. Aspeed boards)
or can be user-created when using a PCI-attached SDHCI controller. To
instantiate the eMMC image from the example above in a machine without other
SDHCI controllers while assuming that the firmware needs a boot partitions of
1 MB, use the following options:

.. code-block:: console

    -drive file=emmc.img,if=none,format=raw,id=emmc-img
    -device sdhci-pci
    -device emmc,drive=emmc-img,boot-partition-size=1048576,rpmb-partition-size=2097152,gp0-partition-size=536870912

The GP partition sizes are set via the ``gp0-partition-size`` to
``gp3-partition-size`` properties. As soon as at least one GP partition has a
non-zero size, QEMU reports the partitioning as completed to the guest. Linux
requires this before it exposes the GP partitions as ``/dev/mmcblk0gp0`` etc.
The partitioning is fixed at device creation. Guests attempting to change the
GP partition sizes or to clear the partitioning completed flag receive a switch
error.

RPMB Authentication Key
=======================

A private shared key is used for authenticating requests of the host to the
RPMB. A real eMMC stores this persistently and permits no reprogramming once it
is set. QEMU emulates key programming but does not persist the key state
across restarts. To emulate the state "key is set", the eMMC can be created
with a user-provided key via the ``auth-key`` property:

.. code-block:: console

    -device emmc,[...],auth-key=D3EB3EC36E334C9F988CE2C0B85954610D2BCF8664844DF2AB56E6C61BB701E4

This sets the well-known test key of OP-TEE on emmc device creation. In case an
eMMC is instantiated by the machine model already:

.. code-block:: console

    -global emmc.auth-key=D3EB3EC36E334C9F988CE2C0B85954610D2BCF8664844DF2AB56E6C61BB701E4

A key always consists of 32 bytes that have to be encoded as hex numbers,
left-padding with zeros as needed.
