// Copyright (C) 2024 Intel Corporation.
// Author(s): Zhao Liu <zhao1.liu@intel.com>
// SPDX-License-Identifier: GPL-2.0-or-later

use common::static_assert;

use crate::bindings;

/// A safe wrapper around [`bindings::Fifo8`].
#[repr(transparent)]
#[derive(Debug)]
pub struct Fifo8(bindings::Fifo8);

static_assert!(size_of::<Fifo8>() <= size_of::<bindings::Fifo8>());

impl Fifo8 {
    /// Create a `Fifo8` struct with given capacity.
    pub fn new(capacity: u32) -> Self {
        let mut fifo: bindings::Fifo8 = Default::default();
        // SAFETY: requirements relayed to callers of Fifo8::new
        unsafe { bindings::fifo8_create(&mut fifo as *mut _, capacity) };
        Self { 0: fifo }
    }

    pub fn reset(&mut self) {
        unsafe { bindings::fifo8_reset(&mut self.0 as *mut _) }
    }

    pub fn push(&mut self, data: u8) {
        unsafe { bindings::fifo8_push(&mut self.0 as *mut _, data) }
    }

    pub fn pop(&mut self) -> Option<u8> {
        if self.is_empty() {
            None
        } else {
            Some(unsafe { bindings::fifo8_pop(&mut self.0 as *mut _) })
        }
    }

    pub fn is_empty(&self) -> bool {
        unsafe { bindings::fifo8_is_empty(&self.0 as *const _) }
    }

    pub fn is_full(&self) -> bool {
        unsafe { bindings::fifo8_is_full(&self.0 as *const _) }
    }

    pub fn num_used(&self) -> u32 {
        unsafe { bindings::fifo8_num_used(&self.0 as *const _) }
    }

    pub fn delete(&mut self) {
        // SAFETY: the only way to obtain a Fifo8 safely is via methods that
        // take a Pin<&mut Self>, therefore the fifo is pinned
        unsafe { bindings::fifo8_destroy(&mut self.0 as *mut _) }
    }
}

impl Drop for Fifo8 {
    fn drop(&mut self) {
        self.delete()
    }
}
