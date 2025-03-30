// Copyright 2024, Linaro Limited
// Author(s): Manos Pitsidianakis <manos.pitsidianakis@linaro.org>
// SPDX-License-Identifier: GPL-2.0-or-later

use std::{
    os::raw::{c_int, c_void},
    ptr::NonNull,
};

use qemu_api::{
    bindings::{qdev_prop_bool, qdev_prop_chr},
    c_str,
    prelude::*,
    vmstate::VMStateDescription,
    vmstate_clock, vmstate_fields, vmstate_of, vmstate_struct, vmstate_subsections, vmstate_unused,
    zeroable::Zeroable,
};

use crate::device::{SerialRegisters, SerialState};

extern "C" fn serial_clock_needed(opaque: *mut c_void) -> bool {
    let state = NonNull::new(opaque).unwrap().cast::<SerialState>();
    unsafe { state.as_ref().migrate_clock }
}

/// Migration subsection for [`SerialState`] clock.
static VMSTATE_SERIAL_CLOCK: VMStateDescription = VMStateDescription {
    name: c_str!("serial/clock").as_ptr(),
    version_id: 1,
    minimum_version_id: 1,
    needed: Some(serial_clock_needed),
    fields: vmstate_fields! {
        vmstate_clock!(SerialState, clock),
    },
    ..Zeroable::ZERO
};

extern "C" fn serial_post_load(opaque: *mut c_void, version_id: c_int) -> c_int {
    let state = NonNull::new(opaque).unwrap().cast::<SerialState>();
    let result = unsafe { state.as_ref().post_load(version_id as u32) };
    if result.is_err() {
        -1
    } else {
        0
    }
}

static VMSTATE_SERIAL_REGS: VMStateDescription = VMStateDescription {
    name: c_str!("serial/regs").as_ptr(),
    version_id: 2,
    minimum_version_id: 2,
    fields: vmstate_fields! {
        vmstate_of!(SerialRegisters, flags),
        vmstate_of!(SerialRegisters, line_control),
        vmstate_of!(SerialRegisters, receive_status_error_clear),
        vmstate_of!(SerialRegisters, control),
        vmstate_of!(SerialRegisters, dmacr),
        vmstate_of!(SerialRegisters, int_enabled),
        vmstate_of!(SerialRegisters, int_level),
        vmstate_of!(SerialRegisters, read_fifo),
        vmstate_of!(SerialRegisters, ilpr),
        vmstate_of!(SerialRegisters, ibrd),
        vmstate_of!(SerialRegisters, fbrd),
        vmstate_of!(SerialRegisters, ifl),
        vmstate_of!(SerialRegisters, read_pos),
        vmstate_of!(SerialRegisters, read_count),
        vmstate_of!(SerialRegisters, read_trigger),
    },
    ..Zeroable::ZERO
};

pub static VMSTATE_SERIAL: VMStateDescription = VMStateDescription {
    name: c_str!("serial").as_ptr(),
    version_id: 2,
    minimum_version_id: 2,
    post_load: Some(serial_post_load),
    fields: vmstate_fields! {
        vmstate_unused!(core::mem::size_of::<u32>()),
        vmstate_struct!(SerialState, regs, &VMSTATE_SERIAL_REGS, BqlRefCell<SerialRegisters>),
    },
    subsections: vmstate_subsections! {
        VMSTATE_SERIAL_CLOCK
    },
    ..Zeroable::ZERO
};

qemu_api::declare_properties! {
    SERIAL_PROPERTIES,
    qemu_api::define_property!(
        c_str!("chardev"),
        SerialState,
        char_backend,
        unsafe { &qdev_prop_chr },
        CharBackend
    ),
    qemu_api::define_property!(
        c_str!("migrate-clk"),
        SerialState,
        migrate_clock,
        unsafe { &qdev_prop_bool },
        bool,
        default = true
    ),
}
