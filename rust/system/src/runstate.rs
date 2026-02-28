// Copyright 2024 Red Hat, Inc.
// Author(s): Paolo Bonzini <pbonzini@redhat.com>
// SPDX-License-Identifier: GPL-2.0-or-later

use crate::bindings;

pub enum WakeupReason {
    NONE,
    RTC,
    PMTIMER,
    OTHER,
}

pub fn qemu_system_wakeup_request(reason: WakeupReason) -> util::Result<()> {
    let reason = match reason {
        WakeupReason::NONE => bindings::QEMU_WAKEUP_REASON_NONE,
        WakeupReason::RTC => bindings::QEMU_WAKEUP_REASON_RTC,
        WakeupReason::PMTIMER => bindings::QEMU_WAKEUP_REASON_PMTIMER,
        WakeupReason::OTHER => bindings::QEMU_WAKEUP_REASON_OTHER,
    };
    unsafe { util::Error::with_errp(|errp| bindings::qemu_system_wakeup_request(reason, errp)) }
}
