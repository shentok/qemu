# Common mips*-softmmu CONFIG defines

# CONFIG_SEMIHOSTING is always required on this architecture
CONFIG_SEMIHOSTING=y

CONFIG_ISA_BUS=y
CONFIG_PCI=y
CONFIG_PCI_DEVICES=y
CONFIG_VGA_ISA=y
CONFIG_VGA_CIRRUS=y
CONFIG_VMWARE_VGA=y
CONFIG_MIPS_ITU=y
CONFIG_MALTA=y
CONFIG_MIPSSIM=y
CONFIG_TEST_DEVICES=y
