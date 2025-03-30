// Copyright 2024, Linaro Limited
// Author(s): Manos Pitsidianakis <manos.pitsidianakis@linaro.org>
// SPDX-License-Identifier: GPL-2.0-or-later

//! Serial QEMU Device Model
//!
//! This library implements a device model for the PrimeCell® UART (Serial)
//! device in QEMU.
//!
//! # Library crate
//!
//! See [`SerialState`](crate::device::SerialState) for the device model type
//! and [`registers`] module for register types.

use qemu_api::c_str;

mod device;
mod device_class;
mod registers;

pub use device::serial_create;

pub const TYPE_SERIAL: &::std::ffi::CStr = c_str!("serial");
