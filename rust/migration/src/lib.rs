// SPDX-License-Identifier: GPL-2.0-or-later

pub use qemu_macros::ToMigrationState;
pub use util_sys as bindings;

pub mod migratable;
pub use migratable::*;

// preserve one-item-per-"use" syntax, it is clearer
// for prelude-like modules
#[rustfmt::skip]
pub mod prelude;

pub mod vmstate;
pub use vmstate::*;
