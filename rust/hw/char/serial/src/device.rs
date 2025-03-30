// Copyright 2024, Linaro Limited
// Author(s): Manos Pitsidianakis <manos.pitsidianakis@linaro.org>
// SPDX-License-Identifier: GPL-2.0-or-later

use std::{ffi::CStr, io, mem::size_of, pin::Pin};

use bql::prelude::*;
use chardev::prelude::*;
use common::prelude::*;
use glib_sys::guint;
use hwcore::prelude::*;
use migration::{self, bindings, prelude::*};
use qom::prelude::*;
use serial_sys::bindings::UART_FIFO_LENGTH;
use system::{prelude::*, qemu_system_wakeup_request, WakeupReason};
use util::{fifo8::Fifo8, prelude::*};

use crate::registers::{
    Divider, FifoControl, InterruptEnable, InterruptStatus, Itl, LineControl, LineStatus,
    ModemControl, ModemStatus, ReadeableRegisterOffset, WriteableRegisterOffset,
};

::trace::include_trace!("hw_char");

const MAX_XMIT_RETRY: u32 = 4;

// TODO: You must disable the UART before any of the control registers are
// reprogrammed. When the UART is disabled in the middle of transmission or
// reception, it completes the current character before stopping

#[repr(C)]
#[derive(Debug)]
pub struct SerialRegisters {
    pub divider: Divider,
    pub rbr: u8,
    pub thr: u8,
    pub tsr: u8,
    pub ier: InterruptEnable,
    pub isr: InterruptStatus,
    pub lcr: LineControl,
    pub mcr: ModemControl,
    pub lsr: LineStatus,
    pub msr: ModemStatus,
    pub scr: u8,
    pub fcr: FifoControl,

    pub fcr_vmstate: FifoControl,
    pub thr_ipending: std::os::raw::c_int,
    pub last_break_enable: std::os::raw::c_int,
    pub tsr_retry: u32,
    pub watch_tag: guint,
    pub last_xmit_ts: u64,
    pub recv_fifo: Fifo8,
    pub xmit_fifo: Fifo8,
    pub char_transmit_time: u64,
    pub timeout_ipending: std::os::raw::c_int,
    pub poll_msl: std::os::raw::c_int,
    pub recv_fifo_itl: u8,
}

impl SerialRegisters {
    #[allow(dead_code)]
    fn thr_ipending_needed(&self) -> bool {
        if self.ier.all_set(InterruptEnable::THRI) {
            let expected_value = (self.isr & InterruptStatus::ID) == InterruptStatus::THRI;
            self.thr_ipending != expected_value.into()
        } else {
            /* LSR.THRE will be sampled again when the interrupt is
             * enabled.  thr_ipending is not used in this case, do
             * not migrate it.
             */
            false
        }
    }
}

impl Default for SerialRegisters {
    fn default() -> Self {
        Self {
            divider: Default::default(),
            rbr: Default::default(),
            thr: Default::default(),
            tsr: Default::default(),
            ier: Default::default(),
            isr: Default::default(),
            lcr: Default::default(),
            mcr: Default::default(),
            lsr: Default::default(),
            msr: Default::default(),
            scr: Default::default(),
            fcr: Default::default(),
            fcr_vmstate: Default::default(),
            thr_ipending: Default::default(),
            last_break_enable: Default::default(),
            tsr_retry: Default::default(),
            watch_tag: Default::default(),
            last_xmit_ts: Default::default(),
            recv_fifo: Fifo8::new(UART_FIFO_LENGTH),
            xmit_fifo: Fifo8::new(UART_FIFO_LENGTH),
            char_transmit_time: Default::default(),
            timeout_ipending: Default::default(),
            poll_msl: Default::default(),
            recv_fifo_itl: Default::default(),
        }
    }
}

#[repr(C)]
#[derive(qom::Object, hwcore::Device)]
/// Serial Device Model in QEMU
pub struct SerialState {
    parent_obj: ParentField<SysBusDevice>,
    regs: Box<BqlRefCell<SerialRegisters>>,
    irq: InterruptSource,
    #[property(rename = "chardev")]
    chr: CharFrontend,
    #[property(default = 115200)]
    baudbase: u32,
    #[property(default = false)]
    wakeup: bool,
    fifo_timeout_timer: Box<Timer>,
    modem_status_poll: Box<Timer>,
    io: MemoryRegion,
}

// Some C users of this device embed its state struct into their own
// structs, so the size of the Rust version must not be any larger
// than the size of the C one. If this assert triggers you need to
// expand the padding_for_rust[] array in the C SerialState struct.
static_assert!(size_of::<SerialState>() <= size_of::<serial_sys::bindings::SerialState>());

qom_isa!(SerialState : SysBusDevice, DeviceState, Object);

unsafe impl ObjectType for SerialState {
    // No need for SerialClass. Just like OBJECT_DECLARE_SIMPLE_TYPE in C.
    type Class = <SysBusDevice as ObjectType>::Class;
    const TYPE_NAME: &'static CStr = crate::TYPE_SERIAL;
}

impl ObjectImpl for SerialState {
    type ParentType = SysBusDevice;

    const INSTANCE_INIT: Option<unsafe fn(ParentInit<Self>)> = Some(Self::init);
    const INSTANCE_POST_INIT: Option<fn(&Self)> = Some(Self::post_init);
    const CLASS_INIT: fn(&mut Self::Class) = Self::Class::class_init::<Self>;
}

impl DeviceImpl for SerialState {
    const REALIZE: Option<fn(&Self) -> util::Result<()>> = Some(Self::realize);
}

impl ResettablePhasesImpl for SerialState {
    const HOLD: Option<fn(&Self, ResetType)> = Some(Self::reset_hold);
}

impl SysBusDeviceImpl for SerialState {}

impl SerialState {
    /// Initializes a pre-allocated, uninitialized instance of `SerialState`.
    ///
    /// # Safety
    ///
    /// `self` must point to a correctly sized and aligned location for the
    /// `SerialState` type. It must not be called more than once on the same
    /// location/instance. All its fields are expected to hold uninitialized
    /// values with the sole exception of `parent_obj`.
    unsafe fn init(mut this: ParentInit<Self>) {
        uninit_field_mut!(*this, regs).write(Default::default());
        uninit_field_mut!(*this, irq).write(Default::default());
        uninit_field_mut!(*this, fifo_timeout_timer).write(unsafe { Box::new(Timer::new()) });
        uninit_field_mut!(*this, modem_status_poll).write(unsafe { Box::new(Timer::new()) });
        /* FIXME: uninit_field_mut!(*this, io).write(Default::default()); */

        Timer::init_full(
            // SAFETY: SerialState is pinned
            unsafe { Pin::new_unchecked(&mut *this.as_mut_ptr()) },
            None,
            CLOCK_VIRTUAL,
            Timer::NS,
            0,
            |s: &SerialState| {
                s.fifo_timeout_int(&mut s.regs.borrow_mut());
            },
            |s| &mut s.fifo_timeout_timer,
        );
        Timer::init_full(
            // SAFETY: SerialState is pinned
            unsafe { Pin::new_unchecked(&mut *this.as_mut_ptr()) },
            None,
            CLOCK_VIRTUAL,
            Timer::NS,
            0,
            |s: &SerialState| {
                s.update_msl(&mut s.regs.borrow_mut());
            },
            |s| &mut s.modem_status_poll,
        );
    }

    fn recv_fifo_put(&self, regs: &mut SerialRegisters, chr: u8) {
        /* Receive overruns do not overwrite FIFO contents. */
        if !regs.recv_fifo.is_full() {
            regs.recv_fifo.push(chr);
        } else {
            regs.lsr |= LineStatus::OE;
        }
    }

    fn update_irq(&self, regs: &mut SerialRegisters) {
        let tmp_isr = if regs.ier.all_set(InterruptEnable::RLSI)
            && regs.lsr.any_set(LineStatus::INT_ANY)
        {
            InterruptStatus::RLSI
        } else if regs.ier.all_set(InterruptEnable::RDI) && regs.timeout_ipending == 1 {
            /* Note that(regs.ier & Ier::RDI) can mask this interrupt,
             * this is not in the specification but is observed on existing
             * hardware. */
            InterruptStatus::CTI
        } else if regs.ier.all_set(InterruptEnable::RDI)
            && regs.lsr.all_set(LineStatus::DR)
            && (!regs.fcr.all_set(FifoControl::FE)
                || regs.recv_fifo.num_used() >= regs.recv_fifo_itl.into())
        {
            InterruptStatus::RDI
        } else if regs.ier.all_set(InterruptEnable::THRI) && regs.thr_ipending == 1 {
            InterruptStatus::THRI
        } else if regs.ier.all_set(InterruptEnable::MSI) && regs.msr.any_set(ModemStatus::ANY_DELTA)
        {
            InterruptStatus::MSI
        } else {
            InterruptStatus::NO_INT
        };

        regs.isr = tmp_isr | (regs.isr & 0xF0.into());

        if tmp_isr != InterruptStatus::NO_INT {
            self.irq.raise();
        } else {
            self.irq.lower();
        }
    }

    fn update_parameters(&self, regs: &mut SerialRegisters) {
        let mut frame_size = 1; // Start bit

        let parity = if regs.lcr.pen() {
            /* Parity bit. */
            frame_size += 1;
            if regs.lcr.eps() {
                b'E'
            } else {
                b'O'
            }
        } else {
            b'N'
        } as i8;
        let stop_bits = if regs.lcr.es() { 2 } else { 1 };

        let data_bits = u8::from(regs.lcr.wls()) + 5;
        frame_size += data_bits + stop_bits;
        /* Zero divisor should give about 3500 baud */
        let speed = if u16::from(regs.divider) == 0 {
            3500
        } else {
            (self.baudbase as f32 / u16::from(regs.divider) as f32).floor() as u32
        };

        regs.char_transmit_time =
            (NANOSECONDS_PER_SECOND / u64::from(speed)) * u64::from(frame_size);
        let sp = SerialParams {
            speed: speed as i32,
            parity: parity.into(),
            data_bits: data_bits.into(),
            stop_bits: stop_bits.into(),
        };
        let _ = self.chr.set_params(&sp);
        trace::trace_serial_update_parameters(
            speed.into(),
            parity.into(),
            data_bits.into(),
            stop_bits.into(),
        );
    }

    fn update_msl(&self, regs: &mut SerialRegisters) {
        self.modem_status_poll.delete();

        let flags = match self.chr.get_tiocm() {
            Ok(f) => f,
            Err(e) => {
                if e.kind() == std::io::ErrorKind::WouldBlock {
                    regs.poll_msl = -1;
                }
                return;
            }
        };

        let omsr = regs.msr;

        if flags.cts() {
            regs.msr |= ModemStatus::CTS;
        } else {
            regs.msr &= !ModemStatus::CTS;
        };
        if flags.dsr() {
            regs.msr |= ModemStatus::DSR;
        } else {
            regs.msr &= !ModemStatus::DSR;
        };
        if flags.car() {
            regs.msr |= ModemStatus::DCD;
        } else {
            regs.msr &= !ModemStatus::DCD;
        };
        if flags.ri() {
            regs.msr |= ModemStatus::RI;
        } else {
            regs.msr &= !ModemStatus::RI;
        };

        if regs.msr != omsr {
            /* Set delta bits */
            regs.msr = regs.msr | (u8::from(regs.msr ^ omsr) >> 4).into();
            /* Msr::TERI only if change was from 1 -> 0 */
            if regs.msr.all_set(ModemStatus::TERI) && !omsr.any_set(ModemStatus::RI) {
                regs.msr &= !ModemStatus::TERI;
            }
            self.update_irq(regs);
        }

        /* The real 16550A apparently has a 250ns response latency to line status changes.
        We'll be lazy and poll only every 10ms, and only poll it at all if MSI interrupts are turned on */

        if regs.poll_msl != 0 {
            self.modem_status_poll
                .modify(CLOCK_VIRTUAL.get_ns() + NANOSECONDS_PER_SECOND / 100);
        }
    }

    fn watch_cb(&self) {
        let mut regs = self.regs.borrow_mut();
        regs.watch_tag = 0;
        self.xmit(&mut regs)
    }

    fn xmit(&self, mut regs: &mut SerialRegisters) {
        loop {
            assert!(!regs.lsr.all_set(LineStatus::TEMT));
            if regs.tsr_retry == 0 {
                assert!(!regs.lsr.all_set(LineStatus::THRE));

                if regs.fcr.all_set(FifoControl::FE) {
                    assert!(!regs.xmit_fifo.is_empty());
                    regs.tsr = regs.xmit_fifo.pop().unwrap();
                    if regs.xmit_fifo.is_empty() {
                        regs.lsr |= LineStatus::THRE;
                    }
                } else {
                    regs.tsr = regs.thr;
                    regs.lsr |= LineStatus::THRE;
                }
                if regs.lsr.all_set(LineStatus::THRE) && regs.thr_ipending == 0 {
                    regs.thr_ipending = 1;
                    self.update_irq(&mut regs);
                }
            }

            if regs.mcr.all_set(ModemControl::LOOP) {
                /* in loopback mode, say that we just received a char */
                let tsr = regs.tsr;
                self.receive(&mut regs, &[tsr]);
            } else {
                let is_retry = match self.chr.write(&[regs.tsr]) {
                    Ok(0) => true,
                    Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => true,
                    _ => false,
                };

                if is_retry && regs.tsr_retry < MAX_XMIT_RETRY {
                    assert!(regs.watch_tag == 0);
                    regs.watch_tag = self.chr.add_watch(self, Self::watch_cb);
                    if regs.watch_tag > 0 {
                        regs.tsr_retry += 1;
                        return;
                    }
                }
            }
            regs.tsr_retry = 0;

            /* Transmit another byte if it is already available. It is only
            possible when FIFO is enabled and not empty. */
            if regs.lsr.all_set(LineStatus::THRE) {
                break;
            }
        }

        regs.last_xmit_ts = CLOCK_VIRTUAL.get_ns();
        regs.lsr |= LineStatus::TEMT;
    }

    fn write_fcr(&self, regs: &mut SerialRegisters, val: FifoControl) {
        /* Set fcr - val only has the bits that are supposed to "stick" */
        regs.fcr = val;

        if val.all_set(FifoControl::FE) {
            regs.isr |= InterruptStatus::FE;
            /* Set recv_fifo trigger Level */
            regs.recv_fifo_itl = match val.get_itl() {
                Itl::Itl1 => 1,
                Itl::Itl2 => 4,
                Itl::Itl3 => 8,
                Itl::Itl4 => 14,
            }
        } else {
            regs.isr &= !InterruptStatus::FE;
        }
    }

    fn update_tiocm(&self, regs: &SerialRegisters) -> io::Result<()> {
        let mut flags = self.chr.get_tiocm()?;

        flags.set_rts(regs.mcr.all_set(ModemControl::RTS));
        flags.set_dtr(regs.mcr.all_set(ModemControl::DTR));

        self.chr.set_tiocm(flags)
    }

    fn write(&self, offset: hwaddr, value: u64, _size: u32) {
        trace::trace_serial_write(offset, value);
        let val: u8 = value.try_into().expect("valid size is 1");
        let mut regs = self.regs.borrow_mut();
        use WriteableRegisterOffset::*;

        match WriteableRegisterOffset::try_from(offset) {
            Ok(RBR_THR) => {
                if regs.lcr.dlab() {
                    regs.divider.set_low(val);
                    self.update_parameters(&mut regs);
                } else {
                    regs.thr = val;
                    if regs.fcr.all_set(FifoControl::FE) {
                        /* xmit overruns overwrite data, so make space if needed */
                        if regs.xmit_fifo.is_full() {
                            regs.xmit_fifo.pop();
                        }
                        regs.xmit_fifo.push(val);
                    }
                    regs.thr_ipending = 0;
                    regs.lsr &= !(LineStatus::THRE | LineStatus::TEMT);
                    self.update_irq(&mut regs);
                    if regs.tsr_retry == 0 {
                        self.xmit(&mut regs);
                    }
                }
            }
            Ok(IER) => {
                if regs.lcr.dlab() {
                    regs.divider.set_high(val);
                    self.update_parameters(&mut regs);
                } else {
                    let changed = (regs.ier ^ val.into()) & 0x0f.into();
                    regs.ier = (val & 0x0f).into();
                    /* If the backend device is a real serial port, turn polling of the modem
                     * status lines on physical port on or off depending on Ier::MSI state.
                     */
                    if changed.all_set(InterruptEnable::MSI) && regs.poll_msl >= 0 {
                        if regs.ier.all_set(InterruptEnable::MSI) {
                            regs.poll_msl = 1;
                            self.update_msl(&mut regs);
                        } else {
                            self.modem_status_poll.delete();
                            regs.poll_msl = 0;
                        }
                    }

                    /* Turning on the THRE interrupt on IER can trigger the interrupt
                     * if LSR.THRE=1, even if it had been masked before by reading IIR.
                     * This is not in the datasheet, but Windows relies on it.  It is
                     * unclear if THRE has to be resampled every time THRI becomes
                     * 1, or only on the rising edge.  Bochs does the latter, and Windows
                     * always toggles IER to all zeroes and back to all ones, so do the
                     * same.
                     *
                     * If IER.THRI is zero, thr_ipending is not used.  Set it to zero
                     * so that the thr_ipending subsection is not migrated.
                     */
                    if changed.all_set(InterruptEnable::THRI) {
                        if regs.ier.all_set(InterruptEnable::THRI)
                            && regs.lsr.all_set(LineStatus::THRE)
                        {
                            regs.thr_ipending = 1;
                        } else {
                            regs.thr_ipending = 0;
                        }
                    }

                    if changed != 0 {
                        self.update_irq(&mut regs);
                    }
                }
            }
            Ok(IIR_FCR) => {
                let mut val = FifoControl::from(val);
                /* Did the enable/disable flag change? If so, make sure FIFOs
                 * get flushed */
                if (regs.fcr ^ val).all_set(FifoControl::FE) {
                    val |= FifoControl::XFR | FifoControl::RFR;
                }

                /* FIFO clear */

                if val.all_set(FifoControl::RFR) {
                    regs.lsr &= !(LineStatus::DR | LineStatus::BI);
                    self.fifo_timeout_timer.delete();
                    regs.timeout_ipending = 0;
                    regs.recv_fifo.reset();
                }

                if val.all_set(FifoControl::XFR) {
                    regs.lsr |= LineStatus::THRE;
                    regs.thr_ipending = 1;
                    regs.xmit_fifo.reset();
                }

                self.write_fcr(&mut regs, val & 0xC9.into());
                self.update_irq(&mut regs);
            }
            Ok(LCR) => {
                regs.lcr = val.into();
                self.update_parameters(&mut regs);
                let break_enable = LineControl::from(val).sb();
                if regs.last_break_enable != break_enable.into() {
                    regs.last_break_enable = break_enable.into();
                    let _ = self.chr.send_break(break_enable);
                }
            }
            Ok(MCR) => {
                let mcr: ModemControl = val.into();
                let old_mcr = regs.mcr;
                regs.mcr = mcr & 0x1f.into();
                if !mcr.all_set(ModemControl::LOOP) && regs.poll_msl >= 0 && old_mcr != regs.mcr {
                    let _ = self.update_tiocm(&regs);
                    /* Update the modem status after a one-character-send wait-time, since there may be a response
                    from the device/computer at the other end of the serial line */
                    self.modem_status_poll
                        .modify(CLOCK_VIRTUAL.get_ns() + regs.char_transmit_time);
                }
            }
            Ok(SCR) => {
                regs.scr = val;
            }
            Err(_) => {
                log_mask_ln!(Log::GuestError, "SerialState::write: Bad offset {offset}");
            }
        }
    }

    fn read(&self, offset: hwaddr, _size: u32) -> u64 {
        let mut regs = self.regs.borrow_mut();
        use ReadeableRegisterOffset::*;

        let result = match ReadeableRegisterOffset::try_from(offset) {
            Ok(RBR_THR) => {
                if regs.lcr.dlab() {
                    regs.divider.low()
                } else {
                    let result = if regs.fcr.all_set(FifoControl::FE) {
                        let ret = regs.recv_fifo.pop().unwrap_or(0);
                        if regs.recv_fifo.is_empty() {
                            regs.lsr &= !(LineStatus::DR | LineStatus::BI);
                        } else {
                            self.fifo_timeout_timer
                                .modify(CLOCK_VIRTUAL.get_ns() + regs.char_transmit_time * 4);
                        }
                        regs.timeout_ipending = 0;
                        ret
                    } else {
                        regs.lsr &= !(LineStatus::DR | LineStatus::BI);
                        regs.rbr
                    };
                    self.update_irq(&mut regs);
                    if !regs.mcr.all_set(ModemControl::LOOP) {
                        drop(regs);
                        /* in loopback mode, don't receive any data */
                        self.chr.accept_input();
                    }
                    result
                }
            }
            Ok(IER) => {
                if regs.lcr.dlab() {
                    regs.divider.high()
                } else {
                    regs.ier.into()
                }
            }
            Ok(IIR_FCR) => {
                let result = regs.isr;
                if (result & InterruptStatus::ID) == InterruptStatus::THRI {
                    regs.thr_ipending = 0;
                    self.update_irq(&mut regs);
                }
                result.into()
            }
            Ok(LCR) => regs.lcr.into(),
            Ok(MCR) => regs.mcr.into(),
            Ok(LSR) => {
                let result = regs.lsr;
                /* Clear break and overrun interrupts */
                if regs.lsr.any_set(LineStatus::BI | LineStatus::OE) {
                    regs.lsr &= !(LineStatus::BI | LineStatus::OE);
                    self.update_irq(&mut regs);
                }
                result.into()
            }
            Ok(MSR) => {
                if regs.mcr.all_set(ModemControl::LOOP) {
                    /* in loopback, the modem output pins are connected to the
                    inputs */
                    (u8::from(regs.mcr) & 0x0c) << 4
                        | (u8::from(regs.mcr) & 0x02) << 3
                        | (u8::from(regs.mcr) & 0x01) << 5
                } else {
                    if regs.poll_msl >= 0 {
                        self.update_msl(&mut regs);
                    }
                    let ret = regs.msr;
                    /* Clear delta bits & msr int after read, if they were set */
                    if regs.msr.any_set(ModemStatus::ANY_DELTA) {
                        regs.msr &= 0xF0.into();
                        self.update_irq(&mut regs);
                    }
                    ret.into()
                }
            }
            Ok(SCR) => regs.scr.into(),
            Err(_) => {
                log_mask_ln!(Log::GuestError, "SerialState::read: Bad offset {offset}");
                0
            }
        };
        trace::trace_serial_read(offset, result);
        result.into()
    }

    fn can_receive(&self) -> u32 {
        let regs = self.regs.borrow();

        if regs.fcr.all_set(FifoControl::FE) {
            if !regs.recv_fifo.is_full() {
                /*
                 * Advertise (fifo.itl - fifo.count) bytes when count < ITL, and 1
                 * if above. If UART_FIFO_LENGTH - fifo.count is advertised the
                 * effect will be to almost always fill the fifo completely before
                 * the guest has a chance to respond, effectively overriding the ITL
                 * that the guest has set.
                 */
                if regs.recv_fifo.num_used() <= regs.recv_fifo_itl.into() {
                    regs.recv_fifo_itl as u32 - regs.recv_fifo.num_used()
                } else {
                    1
                }
            } else {
                0
            }
        } else {
            if regs.lsr & LineStatus::DR == 0 {
                0
            } else {
                1
            }
        }
    }

    fn receive_break(&self) {
        let mut regs = self.regs.borrow_mut();

        regs.rbr = 0;
        /* When the LSR_DR is set a null byte is pushed into the fifo */
        self.recv_fifo_put(&mut regs, b'\0');
        regs.lsr |= LineStatus::BI | LineStatus::DR;

        self.update_irq(&mut regs);
    }

    /* There's data in recv_fifo and rbr has not been read for 4 char transmit
     * times */
    fn fifo_timeout_int(&self, mut regs: &mut SerialRegisters) {
        if !regs.recv_fifo.is_empty() {
            regs.timeout_ipending = 1;
            self.update_irq(&mut regs);
        }
    }

    fn receive(&self, mut regs: &mut SerialRegisters, buf: &[u8]) {
        trace::trace_pl011_receive(buf.len());

        if self.wakeup {
            let _ = qemu_system_wakeup_request(WakeupReason::OTHER);
        }
        if regs.fcr.all_set(FifoControl::FE) {
            for &c in buf {
                self.recv_fifo_put(regs, c);
            }
            regs.lsr |= LineStatus::DR;
            /* call the timeout receive callback in 4 char transmit time */
            self.fifo_timeout_timer
                .modify(CLOCK_VIRTUAL.get_ns() + regs.char_transmit_time * 4);
        } else {
            if regs.lsr.all_set(LineStatus::DR) {
                regs.lsr |= LineStatus::OE;
            }
            regs.rbr = buf[0];
            regs.lsr |= LineStatus::DR;
        }
        self.update_irq(&mut regs);
    }

    fn receive1(&self, buf: &[u8]) {
        self.receive(&mut self.regs.borrow_mut(), buf);
    }

    fn event(&self, event: Event) {
        if event == Event::CHR_EVENT_BREAK {
            self.receive_break();
        }
    }

    fn pre_save(&self) -> Result<(), migration::Infallible> {
        let mut regs = self.regs.borrow_mut();

        regs.fcr_vmstate = regs.fcr;

        Ok(())
    }

    fn pre_load(&self) -> Result<(), migration::Infallible> {
        let mut regs = self.regs.borrow_mut();

        regs.thr_ipending = -1;
        regs.poll_msl = -1;

        Ok(())
    }

    fn post_load(&self, version_id: u8) -> Result<(), migration::Infallible> {
        let mut regs = self.regs.borrow_mut();

        if version_id < 3 {
            regs.fcr_vmstate = 0.into();
        }
        if regs.thr_ipending == -1 {
            regs.thr_ipending = ((regs.isr & InterruptStatus::ID) == InterruptStatus::THRI).into();
        }

        if regs.tsr_retry > 0 {
            /* tsr_retry > 0 implies LSR.TEMT = 0 (transmitter not empty). */
            if regs.lsr.all_set(LineStatus::TEMT) {
                /*
                error_report("inconsistent state in serial device (tsr empty, tsr_retry=%d", regs.tsr_retry);
                return -1;
                */
            }

            if regs.tsr_retry > MAX_XMIT_RETRY {
                regs.tsr_retry = MAX_XMIT_RETRY;
            }

            assert!(regs.watch_tag == 0);
            regs.watch_tag = self.chr.add_watch(self, Self::watch_cb);
        } else {
            /* tsr_retry == 0 implies LSR.TEMT = 1 (transmitter empty). */
            if !regs.lsr.all_set(LineStatus::TEMT) {
                /*
                error_report("inconsistent state in serial device "
                             "(tsr not empty, tsr_retry=0");
                return -1;
                */
            }
        }

        regs.last_break_enable = regs.lcr.sb().into();

        /* Initialize fcr via setter to perform essential side-effects */
        let fcr_vmstate = regs.fcr_vmstate;
        self.write_fcr(&mut regs, fcr_vmstate);
        self.update_parameters(&mut regs);
        Ok(())
    }

    fn realize(&self) -> util::Result<()> {
        self.chr
            .enable_handlers(self, Self::can_receive, Self::receive1, Self::event);
        Ok(())
    }

    fn post_init(&self) {
        self.init_mmio(&self.io);
        self.init_irq(&self.irq);
    }

    fn reset_hold(&self, _type: ResetType) {
        let mut regs = self.regs.borrow_mut();

        if regs.watch_tag > 0 {
            /* g_source_remove(regs.watch_tag); */
            regs.watch_tag = 0;
        }

        regs.rbr = 0;
        regs.ier = 0.into();
        regs.isr = InterruptStatus::NO_INT;
        regs.lcr = Default::default();
        regs.lsr = LineStatus::TEMT | LineStatus::THRE;
        regs.msr = ModemStatus::DCD | ModemStatus::DSR | ModemStatus::CTS;
        /* Default to 9600 baud, 1 start bit, 8 data bits, 1 stop bit, no parity. */
        regs.divider = Default::default();
        regs.mcr = ModemControl::OUT2;
        regs.scr = 0;
        regs.tsr_retry = 0;
        regs.char_transmit_time = (NANOSECONDS_PER_SECOND / 9600) * 10;
        regs.poll_msl = 0;

        regs.timeout_ipending = 0;
        self.fifo_timeout_timer.delete();
        self.modem_status_poll.delete();

        regs.recv_fifo.reset();
        regs.xmit_fifo.reset();

        regs.last_xmit_ts = CLOCK_VIRTUAL.get_ns();

        regs.thr_ipending = 0;
        regs.last_break_enable = 0;
        self.irq.lower();

        self.update_msl(&mut regs);
        regs.msr &= !ModemStatus::ANY_DELTA;
    }
}

#[no_mangle]
pub static serial_io_ops: MemoryRegionOps<SerialState> =
    MemoryRegionOpsBuilder::<SerialState>::new()
        .read(&SerialState::read)
        .write(&SerialState::write)
        .little_endian()
        .valid_unaligned()
        .impl_sizes(1, 1)
        .build();

/*
static VMSTATE_SERIAL_THR_PENDING: VMStateDescription<SerialRegisters> =
    VMStateDescriptionBuilder::<SerialRegisters>::new()
        .name(c"serial/thr_ipending")
        .version_id(1)
        .minimum_version_id(1)
        .needed(&SerialRegisters::thr_ipending_needed)
        .fields(vmstate_fields! {
             vmstate_of!(SerialRegisters, thr_ipending),
        })
        .build();

impl_vmstate_struct!(
    SerialRegisters,
    VMStateDescriptionBuilder::<SerialRegisters>::new()
        .name(c"pl011/regs")
        .version_id(2)
        .minimum_version_id(2)
        .fields(vmstate_fields! {
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
        })
        .build()
);
*/

const VMSTATE_SERIAL: VMStateDescription<SerialState> =
    VMStateDescriptionBuilder::<SerialState>::new()
        .name(c"serial")
        .version_id(3)
        .minimum_version_id(2)
        .pre_save(&SerialState::pre_save)
        .pre_load(&SerialState::pre_load)
        .post_load(&SerialState::post_load)
        /*
        .fields(vmstate_fields! {
        vmstate_of!(SerialRegisters, divider).with_version_id(2),
        vmstate_of!(SerialRegisters, rbr),
        vmstate_of!(SerialRegisters, ier),
        vmstate_of!(SerialRegisters, iir),
        vmstate_of!(SerialRegisters, lcr),
        vmstate_of!(SerialRegisters, mcr),
        vmstate_of!(SerialRegisters, lsr),
        vmstate_of!(SerialRegisters, msr),
        vmstate_of!(SerialRegisters, scr),
        vmstate_of!(SerialRegisters, fcr_vmstate).with_version_id(3),
        })
        .subsections(vmstate_subsections! {
             VMSTATE_SERIAL_THR_PENDING
        })
        */
        .build();

#[no_mangle]
#[allow(non_upper_case_globals)]
pub static vmstate_serial: bindings::VMStateDescription = VMSTATE_SERIAL.get();
