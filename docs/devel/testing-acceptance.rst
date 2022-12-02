.. _testing-acceptance:

Acceptance Tests
================

Machines
--------

The <span style="background:lightgreen;">green background color</span> means a test is already merged.

.. list-table:: Frozen Delights!
   :widths: 15 10 30 10 10 10
   :header-rows: 1
   * - Machine
     - Arch
     - CPU
     - Devices tested
     - Filename if merged URL of information if not merged
     - Code used
   * - style="background-color:lightgreen;" | clipper
     - style="text-align: center; background-color:lightgreen;" | alpha
     - style="text-align: center; background-color:lightgreen;" | ev67
     - style="text-align: center; background-color:lightgreen;" | tulip-nic
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py (see also https://www.mail-archive.com/qemu-devel@nongnu.org/msg604707.html )
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - mps2-an385
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-m3
     - style="text-align: center;" | lan9118-nic
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604690.html
     - style="text-align: center;" | Zephyr
   * - mps2-an505
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-m33
     - style="text-align: center;" | lan9118-nic
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604690.html
     - style="text-align: center;" | ARM-TFM, Zephyr
   * - mps2-an511
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-m3
     - style="text-align: center;" | lan9118-nic
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604690.html
     - style="text-align: center;" | Zephyr
   * - mps2-an521
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-m33
     - style="text-align: center;" | lan9118-nic
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604690.html
     - style="text-align: center;" | ARM-TFM, Zephyr
   * - musca-a
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-m33
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604690.html
     - style="text-align: center;" | Zephyr
   * - musca-b1
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-m33
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604690.html
     - style="text-align: center;" | ARM-TFM
   * - style="background-color:lightgreen;" | canon-a1100
     - style="text-align: center; background-color:lightgreen;" | arm
     - style="text-align: center; background-color:lightgreen;" | arm946
     - style="text-align: center; background-color:lightgreen;" | cfi02-pflash
     - style="background-color:lightgreen;" | tests/acceptance/machine_arm_canona1100.py
     - style="text-align: center; background-color:lightgreen;" | Barebox
   * - connex
     - style="text-align: center;" | arm
     - style="text-align: center;" | pxa255
     - style="text-align: center;" | cfi01-pflash
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604838.html
     - style="text-align: center;" |
   * - spitz (akita, borzoi, terrier)
     - style="text-align: center;" | arm
     - style="text-align: center;" | pxa270
     - style="text-align: center;" |
     - https://www.mail-archive.com/xen-devel@lists.xenproject.org/msg41162.html
     - style="text-align: center;" | ucLinux (Gentoo)
   * - z2
     - style="text-align: center;" | arm
     - style="text-align: center;" | pxa270-c5
     - style="text-align: center;" | wm8750-audio
     - https://www.mail-archive.com/xen-devel@lists.xenproject.org/msg41162.html
     - style="text-align: center;" |
   * - style="background-color:lightgreen;" | integratorcp
     - style="text-align: center; background-color:lightgreen;" | arm
     - style="text-align: center; background-color:lightgreen;" | arm926
     - style="text-align: center; background-color:lightgreen;" | smc91c111-nic, pl181-sdhci, pl110-lcd
     - style="background-color:lightgreen;" | tests/acceptance/machine_arm_integratorcp.py
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - musicpal
     - style="text-align: center;" | arm
     - style="text-align: center;" | arm926
     - style="text-align: center;" | cfi02-pflash, mv88w8618-nic, wm8750-audio
     - https://www.mail-archive.com/xen-devel@lists.xenproject.org/msg41162.html
     - style="text-align: center;" | U-Boot
   * - versatilepb
     - style="text-align: center;" | arm
     - style="text-align: center;" | arm926
     - style="text-align: center;" | ds1338-rtc, lsi53c895a
     -
     - style="text-align: center;" | Linux
   * - versatileab
     - style="text-align: center;" | arm
     - style="text-align: center;" | arm926
     - style="text-align: center;" | ds1338-rtc, lsi53c895a
     -
     - style="text-align: center;" | Linux
   * - style="background-color:lightgreen;" | vexpress-a9
     - style="text-align: center; background-color:lightgreen;" | arm
     - style="text-align: center; background-color:lightgreen;" | cortex-a9
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - vexpress-a15
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a15
     - style="text-align: center;" |
     -
     - style="text-align: center;" | Linux
   * - realview-eb
     - style="text-align: center;" | arm
     - style="text-align: center;" | arm926
     - style="text-align: center;" | smc91c111-nic, lsi53c895a
     -
     - style="text-align: center;" | Linux
   * - realview-eb-mpcore
     - style="text-align: center;" | arm
     - style="text-align: center;" | arm11mpcore
     - style="text-align: center;" | smc91c111-nic, lsi53c895a
     -
     - style="text-align: center;" | Linux
   * - realview-pb-a8
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a8
     - style="text-align: center;" | lan9118-nic
     -
     - style="text-align: center;" | Linux
   * - realview-pbx-a9
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a9
     - style="text-align: center;" | lan9118-nic
     -
     - style="text-align: center;" | Linux
   * - style="background-color:lightgreen;" | cubieboard
     - style="text-align: center; background-color:lightgreen;" | arm
     - style="text-align: center; background-color:lightgreen;" | cortex-a9
     - style="text-align: center; background-color:lightgreen;" | allwinner-a10-soc
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - style="background-color:lightgreen;" | emcraft-sf2
     - style="text-align: center; background-color:lightgreen;" | arm
     - style="text-align: center; background-color:lightgreen;" | cortex-m3
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | U-Boot
   * - mcimx7d-sabre
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a7
     - style="text-align: center;" | e1000e-nic
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg611067.html
     - style="text-align: center;" | Linux
   * - highbank
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a9
     - style="text-align: center;" | xgmac-nic
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604838.html
     - style="text-align: center;" |
   * - midway
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a15
     - style="text-align: center;" | xgmac-nic
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604838.html
     - style="text-align: center;" |
   * - netduino2
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-m3
     - style="text-align: center;" | stm32f2xx
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605206.html
     - style="text-align: center;" | FreeRTOS
   * - sabrelite
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a9
     - style="text-align: center;" | sst25vf016b-spi-flash
     - https://www.mail-archive.com/xen-devel@lists.xenproject.org/msg41162.html
     - style="text-align: center;" | Linux
   * - style="background-color:lightgreen;" | smdkc210
     - style="text-align: center; background-color:lightgreen;" | arm
     - style="text-align: center; background-color:lightgreen;" | cortex-a9
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Debian)
   * - style="background-color:lightgreen;" | orangepi-pc
     - style="text-align: center; background-color:lightgreen;" | arm
     - style="text-align: center; background-color:lightgreen;" | cortex-a7
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Debian)
   * - orangepi-pc
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a7
     - style="text-align: center;" | allwinner-h3-sdhost
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg665858.html
     - style="text-align: center;" | Linux (Debian)
   * - orangepi-pc
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a7
     - style="text-align: center;" | allwinner-h3-emac
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg665858.html
     - style="text-align: center;" | Linux (Ubuntu)
   * - ast2500-evb
     - style="text-align: center;" | arm
     - style="text-align: center;" | arm1176
     - style="text-align: center;" | w25q256-flash
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604793.html
     - style="text-align: center;" |
   * - palmetto-bmc
     - style="text-align: center;" | arm
     - style="text-align: center;" | arm926
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604793.html
     - style="text-align: center;" |
   * - romulus-bmc
     - style="text-align: center;" | arm
     - style="text-align: center;" | arm1176
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604793.html
     - style="text-align: center;" |
   * - swift-bmc
     - style="text-align: center;" | arm
     - style="text-align: center;" | arm1176
     - style="text-align: center;" |
     -
     - style="text-align: center;" |
   * - witherspoon-bmc
     - style="text-align: center;" | arm
     - style="text-align: center;" | arm1176
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604793.html
     - style="text-align: center;" |
   * - ast2600-evb
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a7
     - style="text-align: center;" |
     -
     - style="text-align: center;" |
   * - tacoma-bmc
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a7
     - style="text-align: center;" |
     -
     - style="text-align: center;" |
   * - kzm
     - style="text-align: center;" | arm
     - style="text-align: center;" | arm1136
     - style="text-align: center;" | lan9118-nic
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605210.html
     - style="text-align: center;" |
   * - lm3s811evb
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-m3
     - style="text-align: center;" |
     - http://roboticravings.blogspot.com/2018/07/freertos-on-cortex-m3-with-qemu.html
     - style="text-align: center;" |
   * - lm3s6965evb
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-m3
     - style="text-align: center;" |
     - https://rust-embedded.github.io/book/start/qemu.html
     - style="text-align: center;" |
   * - microbit
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-m0
     - style="text-align: center;" | nrf51-soc
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg606064.html
     - style="text-align: center;" | MicroPython
   * - style="background-color:lightgreen;" | n800 / n810
     - style="text-align: center; background-color:lightgreen;" | arm
     - style="text-align: center; background-color:lightgreen;" | arm1136-r2
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/machine_arm_n8x0.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Meego)
   * - style="background-color:lightgreen;" | raspi2
     - style="text-align: center; background-color:lightgreen;" | arm
     - style="text-align: center; background-color:lightgreen;" | cortex-a7
     - style="text-align: center; background-color:lightgreen;" | pl011-uart
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Debian)
   * - raspi2
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a7
     - style="text-align: center;" | uart8250
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg656332.html
     - style="text-align: center;" | Linux (Debian)
   * - raspi3
     - style="text-align: center;" | aarch64
     - style="text-align: center;" | cortex-a53
     - style="text-align: center;" | bcm2835-sdhost, bcm2835-thermal
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg656319.html
     - style="text-align: center;" | Linux
   * - raspi4
     - style="text-align: center;" | aarch64
     - style="text-align: center;" | cortex-a72
     - style="text-align: center;" | gic-v2
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg642257.html
     - style="text-align: center;" | Linux
   * - xilinx-zynq-a9
     - style="text-align: center;" | arm
     - style="text-align: center;" | cortex-a9
     - style="text-align: center;" | cfi02-pflash, sdcard
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605124.html
     - style="text-align: center;" |
   * - xlnx-zcu102
     - style="text-align: center;" | aarch64
     - style="text-align: center;" | cortex-a53, cortex-r5f
     - style="text-align: center;" | xlnx.ps7-qspi, sst25wf080
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605124.html
     - style="text-align: center;" |
   * - xlnx-versal-virt
     - style="text-align: center;" | aarch64
     - style="text-align: center;" | cortex-a72
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605124.html
     - style="text-align: center;" |
   * - style="background-color:lightgreen;" | virt
     - style="text-align: center; background-color:lightgreen;" | arm
     - style="text-align: center; background-color:lightgreen;" |
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Fedora)
   * - style="background-color:lightgreen;" | virt
     - style="text-align: center; background-color:lightgreen;" | aarch64
     - style="text-align: center; background-color:lightgreen;" |
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Fedora)
   * - arduino-mega-2560-v3
     - style="text-align: center;" | avr
     - style="text-align: center;" | atmega2560
     - style="text-align: center;" | avr-usart, avr-timer16
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg666326.html
     - style="text-align: center;" | FreeRTOS
   * - axis-dev88
     - style="text-align: center;" | cris
     - style="text-align: center;" | crisv32
     - style="text-align: center;" | etraxfs
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605124.html
     - style="text-align: center;" |
   * - milkymist
     - style="text-align: center;" | lm32
     - style="text-align: center;" | lm32-full
     - style="text-align: center;" | cfi01-pflash
     - http://milkymist.walle.cc/README.qemu
     - style="text-align: center;" | RTEMS
   * - hppa
     - style="text-align: center;" | hppa
     - style="text-align: center;" |
     - style="text-align: center;" | cdrom, lsi53c895a-scsi, pci
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg651012.html
     - style="text-align: center;" | HP-UX
   * - an5206
     - style="text-align: center;" | m68k
     - style="text-align: center;" | m5206
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605814.html
     - style="text-align: center;" |
   * - style="background-color:lightgreen;" | mcf5208evb
     - style="text-align: center; background-color:lightgreen;" | m68k
     - style="text-align: center; background-color:lightgreen;" | m5208
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - style="background-color:lightgreen;" | next-cube
     - style="text-align: center; background-color:lightgreen;" | m68k
     - style="text-align: center; background-color:lightgreen;" | m68040
     - style="text-align: center; background-color:lightgreen;" | next-framebuffer
     - style="background-color:lightgreen;" | tests/acceptance/machine_m68k_nextcube.py
     - style="text-align: center; background-color:lightgreen;" | NeXT firmware
   * - style="background-color:lightgreen;" | q800
     - style="text-align: center; background-color:lightgreen;" | m68k
     - style="text-align: center; background-color:lightgreen;" | m68040
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Debian)
   * - petalogix-ml605
     - style="text-align: center;" | microblaze
     - style="text-align: center;" | microblaze
     - style="text-align: center;" | cfi01-pflash, xlnx.xps-spi, xlnx.axi-ethernet
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605124.html
     - style="text-align: center;" |
   * - style="background-color:lightgreen;" | petalogix-s3adsp1800
     - style="text-align: center; background-color:lightgreen;" | microblaze
     - style="text-align: center; background-color:lightgreen;" | microblaze
     - style="text-align: center; background-color:lightgreen;" | cfi01-pflash
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py (see also https://www.mail-archive.com/qemu-devel@nongnu.org/msg605124.html)
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - xlnx-zynqmp-pmu
     - style="text-align: center;" | microblaze
     - style="text-align: center;" | microblaze
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605124.html
     - style="text-align: center;" |
   * - style="background-color:lightgreen;" | malta
     - style="text-align: center; background-color:lightgreen;" | mips
     - style="text-align: center; background-color:lightgreen;" | MIPS 24Kc
     - style="text-align: center; background-color:lightgreen;" | pcnet, piix4-ide
     - style="background-color:lightgreen;" | tests/acceptance/linux_ssh_mips_malta.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Debian)
   * - style="background-color:lightgreen;" | malta
     - style="text-align: center; background-color:lightgreen;" | mips
     - style="text-align: center; background-color:lightgreen;" | I7200
     - style="text-align: center; background-color:lightgreen;" | nanomips
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - mipssim
     - style="text-align: center;" | mips
     - style="text-align: center;" | 24Kf
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg606846.html
     - style="text-align: center;" |
   * - mipssim
     - style="text-align: center;" | mips64
     - style="text-align: center;" | 5Kf
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg606846.html
     - style="text-align: center;" |
   * - boston
     - style="text-align: center;" | mips64
     - style="text-align: center;" | I6400
     - style="text-align: center;" | ich9, xilinx_pcie
     - https://lists.gnu.org/archive/html/qemu-devel/2016-08/msg03419.html
     - style="text-align: center;" | Linux
   * - style="background-color:lightgreen;" | malta
     - style="text-align: center; background-color:lightgreen;" | mips64
     - style="text-align: center; background-color:lightgreen;" | MIPS 20Kc
     - style="text-align: center; background-color:lightgreen;" | pcnet, piix4-ide
     - style="background-color:lightgreen;" | tests/acceptance/linux_ssh_mips_malta.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Debian)
   * - fuloong2e
     - style="text-align: center;" | mips64
     - style="text-align: center;" | Loongson-2E
     - style="text-align: center;" | bonito, vt82c686b, r8139-nic
     -
     - style="text-align: center;" | Linux (Gentoo)
   * - style="background-color:lightgreen;" | 10m50
     - style="text-align: center; background-color:lightgreen;" | nios2
     - style="text-align: center; background-color:lightgreen;" | nios2
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - style="background-color:lightgreen;" | or1k-sim
     - style="text-align: center; background-color:lightgreen;" | openrisc
     - style="text-align: center; background-color:lightgreen;" | or1200
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py (see also [[Documentation/Platforms/OpenRISC]])
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - bamboo
     - style="text-align: center;" | ppc
     - style="text-align: center;" | PowerPC 440epb
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg606064.html
     - style="text-align: center;" | Linux
   * - style="background-color:lightgreen;" | e500
     - style="text-align: center; background-color:lightgreen;" | ppc
     - style="text-align: center; background-color:lightgreen;" | e5500
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - style="background-color:lightgreen;" | g3beige
     - style="text-align: center; background-color:lightgreen;" | ppc
     - style="text-align: center; background-color:lightgreen;" | PowerPC 750
     - style="text-align: center; background-color:lightgreen;" | macio
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py (see also https://www.mail-archive.com/qemu-devel@nongnu.org/msg605247.html)
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - style="background-color:lightgreen;" | mac99
     - style="text-align: center; background-color:lightgreen;" | ppc
     - style="text-align: center; background-color:lightgreen;" | PowerPC 7400
     - style="text-align: center; background-color:lightgreen;" | via-cuda, via-pmu, macio, sungem-nic
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py (see also https://www.mail-archive.com/qemu-devel@nongnu.org/msg605247.html)
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - mac99
     - style="text-align: center;" | ppc64
     - style="text-align: center;" | PowerPC 970fx
     - style="text-align: center;" | via-cuda, via-pmu, macio, sungem-nic
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605247.html
     - style="text-align: center;" | Linux (Debian)
   * - style="background-color:lightgreen;" | prep/40p
     - style="text-align: center; background-color:lightgreen;" | ppc
     - style="text-align: center; background-color:lightgreen;" | PowerPC 604
     - style="text-align: center; background-color:lightgreen;" | floppy disk
     - style="background-color:lightgreen;" | tests/acceptance/ppc_prep_40p.py
     - style="text-align: center; background-color:lightgreen;" | NetBSD
   * - style="background-color:lightgreen;" | prep/40p
     - style="text-align: center; background-color:lightgreen;" | ppc
     - style="text-align: center; background-color:lightgreen;" | PowerPC 604
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/ppc_prep_40p.py
     - style="text-align: center; background-color:lightgreen;" | OpenBIOS
   * - style="background-color:lightgreen;" | prep/40p
     - style="text-align: center; background-color:lightgreen;" | ppc
     - style="text-align: center; background-color:lightgreen;" | PowerPC 604
     - style="text-align: center; background-color:lightgreen;" | cdrom
     - style="background-color:lightgreen;" | tests/acceptance/ppc_prep_40p.py
     - style="text-align: center; background-color:lightgreen;" | NetBSD
   * - sam460ex
     - style="text-align: center;" | ppc
     - style="text-align: center;" | PowerPC 460exb
     - style="text-align: center;" | cfi01-pflash, sm501
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605088.html
     - style="text-align: center;" |
   * - powernv
     - style="text-align: center;" | ppc64
     - style="text-align: center;" | POWER8
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg604793.html
     - style="text-align: center;" |
   * - style="background-color:lightgreen;" | pseries
     - style="text-align: center; background-color:lightgreen;" | ppc64
     - style="text-align: center; background-color:lightgreen;" |
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Fedora)
   * - virt
     - style="text-align: center;" | riscv64
     - style="text-align: center;" | rv64gcsu-v1.10.0
     - style="text-align: center;" | cfi01-pflash, gpex-pcihost
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg633013.html
     - style="text-align: center;" | Linux
   * - sifive_u
     - style="text-align: center;" | riscv64
     - style="text-align: center;" | sifive-u54
     - style="text-align: center;" |
     - https://github.com/riscv/meta-riscv/blob/master/conf/machine/freedom-u540.conf
     - style="text-align: center;" | Linux
   * - virt
     - style="text-align: center;" | riscv32
     - style="text-align: center;" | rv32gcsu-v1.10.0
     - style="text-align: center;" | cfi01-pflash, gpex-pcihost
     - https://github.com/riscv/meta-riscv/blob/master/conf/machine/qemuriscv32.conf
     - style="text-align: center;" | Linux
   * - sifie_e
     - style="text-align: center;" | riscv32
     - style="text-align: center;" | sifive-e51
     - style="text-align: center;" |
     - https://github.com/tock/tock/tree/master/boards/hifive1
     - style="text-align: center;" | Tock
   * - virt
     - style="text-align: center;" | rx
     - style="text-align: center;" |
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg652023.html
     - style="text-align: center;" | U-Boot
   * - virt
     - style="text-align: center;" | rx
     - style="text-align: center;" |
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg652023.html
     - style="text-align: center;" | Linux
   * - style="background-color:lightgreen;" | s390-ccw-virtio
     - style="text-align: center; background-color:lightgreen;" | s390x
     - style="text-align: center; background-color:lightgreen;" |
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Fedora)
   * - style="background-color:lightgreen;" |r2d
     - style="text-align: center; background-color:lightgreen;" | sh4
     - style="text-align: center; background-color:lightgreen;" | sh7751r
     - style="text-align: center; background-color:lightgreen;" | cfi02-pflash, rtl8139-nic (untested)
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py (see also: https://lists.gnu.org/archive/html/qemu-devel/2018-10/msg02748.html )
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - shix
     - style="text-align: center;" | sh4
     - style="text-align: center;" |
     - style="text-align: center;" |
     -
     - style="text-align: center;" |
   * - style="background-color:lightgreen;" | leon3
     - style="text-align: center; background-color:lightgreen;" | sparc
     - style="text-align: center; background-color:lightgreen;" | LEON3
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/machine_sparc_leon3.py
     - style="text-align: center; background-color:lightgreen;" | HelenOS
   * - niagara
     - style="text-align: center;" | sparc64
     - style="text-align: center;" | Sun-UltraSparc-T1
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg605247.html
     - style="text-align: center;" |
   * - style="background-color:lightgreen;" | sun4u
     - style="text-align: center; background-color:lightgreen;" | sparc64
     - style="text-align: center; background-color:lightgreen;" | TI-UltraSparc-IIi
     - style="text-align: center; background-color:lightgreen;" | pci-cmd646-ide
     - style="background-color:lightgreen;" | tests/acceptance/machine_sparc64_sun4u.py
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - SS-5
     - style="text-align: center;" | sparc
     - style="text-align: center;" | Fujitsu-MB86904
     - style="text-align: center;" | esp-scsi, tcx-display
     - https://people.debian.org/~aurel32/qemu/sparc/
     - style="text-align: center;" | Linux (Debian)
   * - SS-10
     - style="text-align: center;" | sparc
     - style="text-align: center;" | TI-SuperSparc-II
     - style="text-align: center;" | esp-scsi, tcx-display
     - https://people.debian.org/~aurel32/qemu/sparc/
     - style="text-align: center;" | Linux (Debian)
   * - style="background-color:lightgreen;" | SS-20
     - style="text-align: center; background-color:lightgreen;" | sparc
     - style="text-align: center; background-color:lightgreen;" | TI-SuperSparc-II
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux
   * - testboard
     - style="text-align: center;" | tricore
     - style="text-align: center;" | tc1796
     - style="text-align: center;" |
     - https://lists.gnu.org/archive/html/qemu-devel/2018-05/msg00074.html
     - style="text-align: center;" |
   * - puv3
     - style="text-align: center;" | unicore32
     - style="text-align: center;" | UniCore-II
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg608054.html
     - style="text-align: center;" |
   * - style="background-color:lightgreen;" | pc-i440fx-piix
     - style="text-align: center; background-color:lightgreen;" | x86_64
     - style="text-align: center; background-color:lightgreen;" |
     - style="text-align: center; background-color:lightgreen;" | i440fx, piix3
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py
     - style="text-align: center; background-color:lightgreen;" | Linux (Fedora)
   * - pc-q35
     - style="text-align: center;" | x86_64
     - style="text-align: center;" |
     - style="text-align: center;" | q35, ich9
     -
     - style="text-align: center;" |
   * - xenpv
     - style="text-align: center;" | x86_64
     - style="text-align: center;" |
     - style="text-align: center;" |
     - https://www.mail-archive.com/qemu-devel@nongnu.org/msg606480.html
     - style="text-align: center;" |
   * - style="background-color:lightgreen;" | lx60
     - style="text-align: center; background-color:lightgreen;" | xtensa
     - style="text-align: center; background-color:lightgreen;" | dc232c
     - style="text-align: center; background-color:lightgreen;" |
     - style="background-color:lightgreen;" | tests/acceptance/boot_linux_console.py (see also https://www.mail-archive.com/qemu-devel@nongnu.org/msg604723.html)
     - style="text-align: center; background-color:lightgreen;" | Linux

