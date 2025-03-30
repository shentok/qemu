// Copyright 2024, Linaro Limited
// Author(s): Manos Pitsidianakis <manos.pitsidianakis@linaro.org>
// SPDX-License-Identifier: GPL-2.0-or-later

//! Device registers exposed as typed structs which are backed by arbitrary
//! integer bitmaps. [`InterruptStatus`], [`FifoControl`], [`LineControl`], etc.

// For more detail see the PL011 Technical Reference Manual DDI0183:
// https://developer.arm.com/documentation/ddi0183/latest/

use bilge::prelude::*;
use bits::bits;
use migration::{impl_vmstate_bitsized, impl_vmstate_forward};

/// Offset of each register from the base memory address of the device.
#[doc(alias = "readeable offset")]
#[allow(non_camel_case_types)]
#[repr(u64)]
#[derive(Debug, Eq, PartialEq, common::TryInto)]
pub enum ReadeableRegisterOffset {
    /// Data Register
    RBR_THR = 0x0,
    IER = 0x1,
    IIR_FCR = 0x2,
    LCR = 0x3,
    MCR = 0x4,
    LSR = 0x5,
    MSR = 0x6,
    SCR = 0x7,
}

#[doc(alias = "writeeable offset")]
#[allow(non_camel_case_types)]
#[repr(u64)]
#[derive(Debug, Eq, PartialEq, common::TryInto)]
pub enum WriteableRegisterOffset {
    /// Data Register
    RBR_THR = 0x0,
    IER = 0x1,
    IIR_FCR = 0x2,
    LCR = 0x3,
    MCR = 0x4,
    SCR = 0x7,
}

bits! {
    #[derive(Default)]
    pub struct InterruptEnable(u8) {
        /// Enable Modem status interrupt
        MSI = 1 << 3,
        /// Enable receiver line status interrupt
        RLSI = 1 << 2,
        /// Enable Transmitter holding register int
        THRI = 1 << 1,
        /// Enable receiver data interrupt
        RDI = 1 << 0,
    }
}
impl_vmstate_forward!(InterruptEnable);

bits! {
    #[derive(Default)]
    pub struct InterruptStatus(u8) {
        /// No interrupts pending
        NO_INT = 0x01,
        /// Mask for the interrupt ID
        ID = 0x06,

        /// Modem status interrupt
        MSI = 0x00,
        /// /* Transmitter holding register empty */
        THRI = 0x02,
        /// Receiver data interrupt
        RDI = 0x04,
        /// Receiver line status interrupt
        RLSI = 0x06,
        /// Character Timeout Indication
        CTI = 0x0C,

        /// Fifo enabled, but not functioning
        FENF = 0x80,
        /// Fifo enabled
        FE = 0xC0,
    }
}
impl_vmstate_forward!(InterruptStatus);

bits! {
    #[derive(Default)]
    pub struct ModemControl(u8) {
        /// Enable loopback test mode
        LOOP = 1 << 4,
        /// Out2 complement
        OUT2 = 1 << 3,
        /// Out1 complement
        OUT1 = 1 << 2,
        /// RTS complement
        RTS = 1 << 1,
        /// DTR complement
        DTR = 1 << 0,
    }
}
impl_vmstate_forward!(ModemControl);

bits! {
    #[derive(Default)]
    pub struct ModemStatus(u8) {
        /// Data Carrier Detect
        DCD = 1 << 7,
        /// Ring Indicator
        RI = 1 << 6,
        /// Data Set Ready
        DSR = 1 << 5,
        /// Clear to Send
        CTS = 1 << 4,
        /// Delta DCD
        DDCD = 1 << 3,
        /// Trailing edge ring indicator
        TERI = 1 << 2,
        /// Delta DSR
        DDSR = 1 << 1,
        /// Delta CTS
        DCTS = 1 << 0,

        /// Any of the delta bits
        ANY_DELTA = bits!(Self as u8: DDCD | TERI | DDSR | DCTS),
    }
}
impl_vmstate_forward!(ModemStatus);

bits! {
    #[derive(Default)]
    pub struct LineStatus(u8) {
        /// Receiver FIFO error
        RFE = 1 << 7,
        /// Transmitter empty
        TEMT = 1 << 6,
        /// Transmit-hold-register empty
        THRE = 1 << 5,
        /// Break interrupt indicator
        BI = 1 << 4,
        /// Frame error indicator
        FE = 1 << 3,
        /// Parity error indicator
        PE = 1 << 2,
        /// Overrun error indicator
        OE = 1 << 1,
        /// Receiver data ready
        DR = 1 << 0,

        /// Any of the lsr-interrupt-triggering status bits
        INT_ANY = bits!(Self as u8: BI | FE | PE | OE),
    }
}
impl_vmstate_forward!(LineStatus);

pub enum Itl {
    /// 1 byte ITL
    Itl1,
    /// 4 bytes ITL
    Itl2,
    /// 8 bytes ITL
    Itl3,
    /// 14 bytes ITL
    Itl4,
}

bits! {
    #[derive(Default)]
    pub struct FifoControl(u8) {
    /// DMA Mode Select
    DMS = 1 << 3,
    /// XMIT Fifo Reset
    XFR = 1 << 2,
    /// RCVR Fifo Reset
    RFR = 1 << 1,
    /// FIFO Enable
    FE = 1 << 0,
    }
}
impl_vmstate_forward!(FifoControl);

impl FifoControl {
    pub fn get_itl(&self) -> Itl {
        match u8::from(self.0) & 0xC0 {
            0x00 => Itl::Itl1,
            0x40 => Itl::Itl2,
            0x80 => Itl::Itl3,
            0xC0 => Itl::Itl4,
            _ => unreachable!("masked value outside 0xC0 range"),
        }
    }
}

#[bitsize(16)]
#[derive(Clone, Copy, DebugBits, FromBits)]
/// Flag Register, `UARTFR`
///
/// This has the usual inbound RS232 modem-control signals, plus flags
/// for RX and TX FIFO fill levels and a BUSY flag.
pub struct Divider {
    pub low: u8,
    pub high: u8,
}
impl_vmstate_bitsized!(Divider);

impl Default for Divider {
    fn default() -> Self {
        0x0c.into()
    }
}

#[bitsize(8)]
#[derive(Clone, Copy, Default, DebugBits, FromBits)]
pub struct LineControl {
    /// Word length select
    pub wls: u2,
    /// Extra stop
    pub es: bool,
    /// Parity enable
    pub pen: bool,
    /// Even parity select
    pub eps: bool,
    /// Stick parity
    pub sp: bool,
    /// Set break
    pub sb: bool,
    /// Divisor latch access bit
    pub dlab: bool,
}

impl_vmstate_bitsized!(LineControl);
