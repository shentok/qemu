/*
 * Internal memory management interfaces
 *
 * Copyright 2011 Red Hat, Inc. and/or its affiliates
 *
 * Authors:
 *  Avi Kivity <avi@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 */

#ifndef EXEC_ADDRESS_SPACES_H
#define EXEC_ADDRESS_SPACES_H

/*
 * Internal interfaces between memory.c/exec.c/vl.c.  Do not #include unless
 * you're one of them.
 */

#include "exec/memory.h"

#ifndef CONFIG_USER_ONLY

#include "hw/boards.h"

/**
 * Get the root memory region.  Prefer using SysBusState::system_memory
 * directly.
 */
inline MemoryRegion *get_system_memory(void)
{
    return &current_machine->main_system_bus.system_memory;
}

/**
 * Get the root I/O port region.  Prefer using SysBusState::system_io directly.
 */
inline MemoryRegion *get_system_io(void)
{
    return &current_machine->main_system_bus.system_io;
}

/**
 * Get the root memory address space.  Prefer using
 * SysBusState::address_space_memory directly.
 */
inline AddressSpace *get_address_space_memory(void)
{
    return &current_machine->main_system_bus.address_space_memory;
}

/**
 * Get the root I/O port address space.  Prefer using
 * SysBusState::address_space_io directly.
 */
inline AddressSpace *get_address_space_io(void)
{
    return &current_machine->main_system_bus.address_space_io;
}

#endif

#endif
