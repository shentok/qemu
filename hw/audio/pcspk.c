/*
 * QEMU PC speaker emulation
 *
 * Copyright (c) 2006 Joachim Henke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/isa/isa.h"
#include "hw/audio/model.h"
#include "qemu/audio.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qemu/error-report.h"
#include "hw/timer/i8254.h"
#include "migration/vmstate.h"
#include "hw/audio/pcspk.h"
#include "qapi/error.h"
#include "qom/object.h"
#include "trace.h"

#define PCSPK_BUF_LEN 1792
#define PCSPK_SAMPLE_RATE 32000
#define PCSPK_MAX_FREQ (PCSPK_SAMPLE_RATE >> 1)
#define PCSPK_MIN_COUNT DIV_ROUND_UP(PIT_FREQ, PCSPK_MAX_FREQ)

OBJECT_DECLARE_SIMPLE_TYPE(PCSpkState, PC_SPEAKER)

struct PCSpkState {
    ISADevice parent_obj;

    MemoryRegion ioport;
    uint32_t iobase;
    uint8_t sample_buf[PCSPK_BUF_LEN];
    AudioBackend *audio_be;
    SWVoiceOut *voice;
    PITCommonState *pit;
    unsigned int pit_count;
    unsigned int samples;
    unsigned int play_pos;
    uint8_t data_on;
    uint8_t dummy_refresh_clock;

    /*
     * The speaker gate can be toggled for much less than one audio timer
     * period.  Generate the waveform from the gate timing instead of
     * only when the audio backend polls us.
     */
    int64_t gate_on_ns;
    int64_t last_gen_ns;
    int64_t prefill_ns;
    bool active;
    bool prefill;
};

static const char *s_spk = "pcspk";

static inline void generate_samples(PCSpkState *s)
{
    unsigned int i;

    if (s->pit_count) {
        const uint32_t m = PCSPK_SAMPLE_RATE * s->pit_count;
        const uint32_t n = ((uint64_t)PIT_FREQ << 32) / m;

        /* multiple of wavelength for gapless looping */
        s->samples = (QEMU_ALIGN_DOWN(PCSPK_BUF_LEN * PIT_FREQ, m) / (PIT_FREQ >> 1) + 1) >> 1;
        for (i = 0; i < s->samples; ++i)
            s->sample_buf[i] = (64 & (n * i >> 25)) - 32;
    } else {
        s->samples = PCSPK_BUF_LEN;
        for (i = 0; i < PCSPK_BUF_LEN; ++i)
            s->sample_buf[i] = 128; /* silence */
    }
}

/*
 * Write up to n samples of the current waveform to the voice.  The
 * waveform is periodic and stored in sample_buf, so wrap play_pos.
 * Return the number of samples actually written.
 */
static uint64_t pcspk_emit(PCSpkState *s, uint64_t n)
{
    uint64_t done = 0;

    while (n > 0) {
        unsigned int chunk = MIN(n, s->samples - s->play_pos);

        chunk = audio_be_write(s->audio_be, s->voice,
                               &s->sample_buf[s->play_pos], chunk);
        if (!chunk) {
            break;
        }
        s->play_pos = (s->play_pos + chunk) % s->samples;
        n -= chunk;
        done += chunk;
    }

    return done;
}

/*
 * Emit the samples which correspond to the time elapsed since the last
 * generation.  This makes short gate pulses independent of the audio
 * timer period.
 */
static void pcspk_emit_until(PCSpkState *s, int64_t now)
{
    int64_t elapsed = now - s->last_gen_ns;
    uint64_t n, written;

    if (elapsed <= 0) {
        return;
    }

    n = muldiv64(elapsed, PCSPK_SAMPLE_RATE, NANOSECONDS_PER_SECOND);
    if (n) {
        written = pcspk_emit(s, n);
        /*
         * Only account for samples accepted by the backend; the rest
         * has to be generated on the next call.
         */
        s->last_gen_ns += muldiv64(written, NANOSECONDS_PER_SECOND,
                                   PCSPK_SAMPLE_RATE);
    }
}

static bool pcspk_update_waveform(PCSpkState *s)
{
    PITChannelInfo ch;
    unsigned int n;

    pit_get_channel_info(s->pit, 2, &ch);

    if (ch.mode != 3) {
        return false;
    }

    n = ch.initial_count;
    /* avoid frequencies that are not reproducible with sample rate */
    if (n < PCSPK_MIN_COUNT)
        n = 0;

    if (s->pit_count != n) {
        s->pit_count = n;
        s->play_pos = 0;
        generate_samples(s);
    }

    return s->samples != 0;
}

static void pcspk_callback(void *opaque, int free)
{
    PCSpkState *s = opaque;
    unsigned int n;
    int64_t now;

    if (!pcspk_update_waveform(s)) {
        return;
    }

    now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    if (!s->prefill && now - s->gate_on_ns >= s->prefill_ns) {
        /*
         * The gate has been on for a while.  Start filling the backend
         * buffer ahead of time to keep a long note from underrunning.
         */
        s->prefill = true;
    }

    if (!s->prefill) {
        pcspk_emit_until(s, now);
        return;
    }

    while (free > 0) {
        n = MIN(s->samples - s->play_pos, (unsigned int)free);
        n = audio_be_write(s->audio_be, s->voice, &s->sample_buf[s->play_pos], n);
        if (!n)
            break;
        s->play_pos = (s->play_pos + n) % s->samples;
        free -= n;
    }
    s->last_gen_ns = now;
}

static int pcspk_audio_init(PCSpkState *s)
{
    struct audsettings as = {PCSPK_SAMPLE_RATE, 1, AUDIO_FORMAT_U8, 0};
    unsigned int n;

    if (s->voice) {
        /* already initialized */
        return 0;
    }

    s->voice = audio_be_open_out(s->audio_be, s->voice, s_spk, s, pcspk_callback, &as);
    if (!s->voice) {
        error_report("pcspk: Could not open voice");
        return -1;
    }

    /*
     * Only generate ahead once a gate has been on for half a backend
     * buffer.  Shorter pulses are rendered exactly from their timing.
     */
    n = audio_be_get_buffer_size_out(s->audio_be, s->voice);
    s->prefill_ns = n > 0
        ? muldiv64(n, NANOSECONDS_PER_SECOND, PCSPK_SAMPLE_RATE) / 2
        : 20 * NANOSECONDS_PER_SECOND / 1000;

    return 0;
}

static uint64_t pcspk_io_read(void *opaque, hwaddr addr,
                              unsigned size)
{
    PCSpkState *s = opaque;
    PITChannelInfo ch;
    uint8_t val;

    pit_get_channel_info(s->pit, 2, &ch);

    s->dummy_refresh_clock ^= (1 << 4);

    val = ch.gate | (s->data_on << 1) | s->dummy_refresh_clock |
       (ch.out << 5);

    trace_pcspk_io_read(s->iobase, val);

    return val;
}

static void pcspk_io_write(void *opaque, hwaddr addr, uint64_t val,
                           unsigned size)
{
    PCSpkState *s = opaque;
    const int gate = val & 1;
    bool was_active = s->active;
    bool active;

    trace_pcspk_io_write(s->iobase, val);

    s->data_on = (val >> 1) & 1;
    pit_set_gate(s->pit, 2, gate);
    active = gate && s->data_on;

    if (active != was_active) {
        int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

        if (active) {
            s->gate_on_ns = now;
            s->last_gen_ns = now;
            s->prefill = false;
            s->play_pos = 0;
        } else {
            /*
             * Emit the tail of the pulse even if the audio timer never
             * got a chance to run while the gate was on.
             */
            if (s->voice && pcspk_update_waveform(s)) {
                pcspk_emit_until(s, now);
            }
        }
        s->active = active;
    }

    if (s->voice) {
        audio_be_set_active_out(s->audio_be, s->voice, active);
    }
}

static const MemoryRegionOps pcspk_io_ops = {
    .read = pcspk_io_read,
    .write = pcspk_io_write,
    .impl = {
        .min_access_size = 1,
        .max_access_size = 1,
    },
};

static void pcspk_initfn(Object *obj)
{
    PCSpkState *s = PC_SPEAKER(obj);

    memory_region_init_io(&s->ioport, OBJECT(s), &pcspk_io_ops, s, "pcspk", 1);
}

static void pcspk_realizefn(DeviceState *dev, Error **errp)
{
    ISADevice *isadev = ISA_DEVICE(dev);
    PCSpkState *s = PC_SPEAKER(dev);

    if (!s->pit) {
        error_setg(errp, "pcspk: No \"pit\" set or available");
        return;
    }

    isa_register_ioport(isadev, &s->ioport, s->iobase);

    if (s->audio_be && audio_be_check(&s->audio_be, errp)) {
        pcspk_audio_init(s);
        return;
    }
}

static const VMStateDescription vmstate_spk = {
    .name = "pcspk",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT8(data_on, PCSpkState),
        VMSTATE_UINT8(dummy_refresh_clock, PCSpkState),
        VMSTATE_END_OF_LIST()
    }
};

static const Property pcspk_properties[] = {
    DEFINE_AUDIO_PROPERTIES(PCSpkState, audio_be),
    DEFINE_PROP_UINT32("iobase", PCSpkState, iobase,  0x61),
    DEFINE_PROP_LINK("pit", PCSpkState, pit, TYPE_PIT_COMMON, PITCommonState *),
};

static void pcspk_class_initfn(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = pcspk_realizefn;
    set_bit(DEVICE_CATEGORY_SOUND, dc->categories);
    dc->vmsd = &vmstate_spk;
    device_class_set_props(dc, pcspk_properties);
    /* Reason: pit object link */
    dc->user_creatable = false;
}

static const TypeInfo pcspk_info = {
    .name           = TYPE_PC_SPEAKER,
    .parent         = TYPE_ISA_DEVICE,
    .instance_size  = sizeof(PCSpkState),
    .instance_init  = pcspk_initfn,
    .class_init     = pcspk_class_initfn,
};

static void pcspk_register(void)
{
    type_register_static(&pcspk_info);
}
type_init(pcspk_register)
