/*
 * GoldenEye native software-RSP mixer -- baseline classic-ABI primitives.
 *
 * The architectural pattern and scalar ADPCM/mix behavior are adapted from
 * the MIT-licensed Perfect Dark PC port (Copyright (c) 2022 Ryan Dwyer; see
 * LICENSES/perfect-dark-port-MIT.txt). N64 audio commands execute immediately
 * against a host-side DMEM buffer.
 * GoldenEye uses the older classic libaudio ABI, where aSetBuffer establishes
 * context consumed by later commands, so this file retains that small state.
 *
 * These primitives are intentionally scalar and straightforward. Correctness is
 * differential-tested against the project's known-working private/reference
 * mixer before more complex DSP operations are added.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <PR/abi.h>

#include "ge_mixer.h"

#define GE_MIXER_DMEM_SIZE 4096
#define GE_ROUND_UP_32(v) (((v) + 31) & ~31)
#define GE_ROUND_UP_16(v) (((v) + 15) & ~15)
#define GE_ROUND_UP_8(v)  (((v) + 7) & ~7)

unsigned long ge_mixer_ops;
unsigned long ge_mixer_saves;
unsigned long ge_mixer_hi;
unsigned long ge_mixer_overflow;
unsigned long ge_mixer_ovf_flags;
unsigned long ge_mixer_ovf_in;
unsigned long ge_mixer_ovf_out;
unsigned long ge_mixer_ovf_nbytes;

int ge_mixer_probe;
unsigned long ge_mixer_n_adpcm;
unsigned long ge_mixer_n_resample;
unsigned long ge_mixer_n_envmix;
unsigned long ge_mixer_n_mix;
unsigned long ge_mixer_n_interleave;
int ge_mixer_pk_adpcm;
int ge_mixer_pk_resample;
int ge_mixer_pk_envmix;
int ge_mixer_pk_mix;
int ge_mixer_pk_interleave;
int ge_mixer_pk_save;
int ge_mixer_pk_loadbuf;
int ge_mixer_pk_polef;
int ge_mixer_pk_envmix_in;
int ge_mixer_em_vol0 = -1;
int ge_mixer_em_vol1 = -1;
int ge_mixer_em_dry = -1;
int ge_mixer_em_wet = -1;
int ge_mixer_em_tgt0 = -1;
int ge_mixer_em_rate0 = -1;
int ge_mixer_em_flags = -1;

static void geProbePeak(const int16_t *samples, int count, int *peak)
{
    int i;
    int p = *peak;
    for (i = 0; i < count; i++) {
        int v = samples[i];
        if (v < 0) v = -v;
        if (v > p) p = v;
    }
    *peak = p;
}

static int16_t geClamp16(int32_t value)
{
    if (value < -0x8000) return -0x8000;
    if (value > 0x7fff) return 0x7fff;
    return (int16_t)value;
}

static int32_t geClamp32(int64_t value)
{
    if (value < INT32_MIN) return INT32_MIN;
    if (value > INT32_MAX) return INT32_MAX;
    return (int32_t)value;
}

static int16_t geExpandAdpcmNibble(uint8_t nibble, int shift)
{
    int value = nibble & 0xf;
    if (value & 8) value -= 16;
    return (int16_t)(value * (1 << shift));
}

@@GE_RESAMPLE_TABLE@@

static struct {
    uint16_t in;
    uint16_t out;
    uint16_t nbytes;
    uint16_t dry_right;
    uint16_t wet_left;
    uint16_t wet_right;
    int16_t vol[2];
    int16_t target[2];
    int32_t rate[2];
    int16_t vol_dry;
    int16_t vol_wet;
    ADPCM_STATE *adpcm_loop_state;
    int16_t adpcm_table[8][2][8];
    union {
        int16_t s16[GE_MIXER_DMEM_SIZE / sizeof(int16_t)];
        uint8_t u8[GE_MIXER_DMEM_SIZE];
    } dmem;
} geMixer;

void aSetBufferImpl(uint8_t flags, uint16_t in, uint16_t out, uint16_t nbytes)
{
    unsigned high;
    ge_mixer_ops++;

    if (flags & A_AUX) {
        high = in > out ? in : out;
        if (nbytes > high) high = nbytes;
        high += 320 + 32;
    } else {
        unsigned high_in = (unsigned)in + nbytes + 32;
        unsigned high_out = (unsigned)out + nbytes + 32;
        high = high_in > high_out ? high_in : high_out;
    }
    if (high > ge_mixer_hi) ge_mixer_hi = high;
    if (high > GE_MIXER_DMEM_SIZE) {
        if (ge_mixer_overflow == 0) {
            ge_mixer_ovf_flags = flags;
            ge_mixer_ovf_in = in;
            ge_mixer_ovf_out = out;
            ge_mixer_ovf_nbytes = nbytes;
        }
        ge_mixer_overflow++;
    }

    if (flags & A_AUX) {
        geMixer.dry_right = in;
        geMixer.wet_left = out;
        geMixer.wet_right = nbytes;
    } else {
        geMixer.in = in;
        geMixer.out = out;
        geMixer.nbytes = nbytes;
    }
}

void aClearBufferImpl(uint16_t addr, int nbytes)
{
    memset(geMixer.dmem.u8 + addr, 0, GE_ROUND_UP_16(nbytes));
}

void aLoadBufferImpl(const void *source_addr)
{
    memcpy(geMixer.dmem.u8 + geMixer.in, source_addr,
           GE_ROUND_UP_8(geMixer.nbytes));
    if (ge_mixer_probe) {
        geProbePeak(geMixer.dmem.s16 + geMixer.in / 2,
                    GE_ROUND_UP_8(geMixer.nbytes) / 2,
                    &ge_mixer_pk_loadbuf);
    }
}

void aSaveBufferImpl(int16_t *dest_addr)
{
    ge_mixer_saves++;
    memcpy(dest_addr, geMixer.dmem.u8 + geMixer.out,
           GE_ROUND_UP_8(geMixer.nbytes));
    if (ge_mixer_probe) {
        geProbePeak(dest_addr, GE_ROUND_UP_8(geMixer.nbytes) / 2,
                    &ge_mixer_pk_save);
    }
}

void aDMEMMoveImpl(uint16_t in_addr, uint16_t out_addr, int nbytes)
{
    memmove(geMixer.dmem.u8 + out_addr, geMixer.dmem.u8 + in_addr,
            GE_ROUND_UP_16(nbytes));
}

void aInterleaveImpl(uint16_t left, uint16_t right)
{
    int samples = GE_ROUND_UP_16(geMixer.nbytes) / (int)sizeof(int16_t);
    const int16_t *l = geMixer.dmem.s16 + left / sizeof(int16_t);
    const int16_t *r = geMixer.dmem.s16 + right / sizeof(int16_t);
    int16_t *dst = geMixer.dmem.s16 + geMixer.out / sizeof(int16_t);
    int i;

    for (i = 0; i < samples; i++) {
        *dst++ = l[i];
        *dst++ = r[i];
    }
    if (ge_mixer_probe) {
        ge_mixer_n_interleave++;
        geProbePeak(geMixer.dmem.s16 + geMixer.out / 2,
                    GE_ROUND_UP_16(geMixer.nbytes),
                    &ge_mixer_pk_interleave);
    }
}

void aLoadADPCMImpl(int num_entries_times_16, const int16_t *book_source_addr)
{
    memcpy(geMixer.adpcm_table, book_source_addr, num_entries_times_16);
}

void aSetLoopImpl(ADPCM_STATE *adpcm_loop_state)
{
    geMixer.adpcm_loop_state = adpcm_loop_state;
}

void aSetVolumeImpl(uint8_t flags, int16_t v, int16_t t, int16_t r)
{
    if (flags & A_AUX) {
        geMixer.vol_dry = v;
        geMixer.vol_wet = r;
    } else if (flags & A_VOL) {
        geMixer.vol[(flags & A_LEFT) ? 0 : 1] = v;
    } else {
        int channel = (flags & A_LEFT) ? 0 : 1;
        geMixer.target[channel] = v;
        geMixer.rate[channel] = (int32_t)((uint16_t)t << 16 | (uint16_t)r);
    }
}

void aMixImpl(int16_t gain, uint16_t in_addr, uint16_t out_addr)
{
    int nbytes = GE_ROUND_UP_32(geMixer.nbytes);
    int16_t *in = geMixer.dmem.s16 + in_addr / sizeof(int16_t);
    int16_t *out = geMixer.dmem.s16 + out_addr / sizeof(int16_t);
    int i;

    if (gain == (int16_t)-0x8000) {
        while (nbytes > 0) {
            for (i = 0; i < 16; i++) {
                *out = geClamp16((int32_t)*out - (int32_t)*in);
                out++;
                in++;
            }
            nbytes -= 16 * (int)sizeof(int16_t);
        }
        return;
    }

    while (nbytes > 0) {
        for (i = 0; i < 16; i++) {
            int32_t sample =
                (((int32_t)*out * 0x7fff + (int32_t)*in * gain) + 0x4000) >> 15;
            *out++ = geClamp16(sample);
            in++;
        }
        nbytes -= 16 * (int)sizeof(int16_t);
    }
    if (ge_mixer_probe) {
        ge_mixer_n_mix++;
        geProbePeak(geMixer.dmem.s16 + out_addr / 2,
                    GE_ROUND_UP_32(geMixer.nbytes) / 2,
                    &ge_mixer_pk_mix);
    }
}

void aADPCMdecImpl(uint8_t flags, ADPCM_STATE state)
{
    uint8_t *in = geMixer.dmem.u8 + geMixer.in;
    int16_t *out = geMixer.dmem.s16 + geMixer.out / sizeof(int16_t);
    int nbytes = GE_ROUND_UP_32(geMixer.nbytes);

    if (flags & A_INIT) {
        memset(out, 0, 16 * sizeof(int16_t));
    } else if (flags & A_LOOP) {
        memcpy(out, geMixer.adpcm_loop_state, 16 * sizeof(int16_t));
    } else {
        memcpy(out, state, 16 * sizeof(int16_t));
    }
    out += 16;

    while (nbytes > 0) {
        int shift = *in >> 4;
        int table_index = *in++ & 0xf;
        int16_t (*table)[8] = geMixer.adpcm_table[table_index];
        int half;

        for (half = 0; half < 2; half++) {
            int16_t decoded[8];
            int16_t prev1 = out[-1];
            int16_t prev2 = out[-2];
            int j;

            for (j = 0; j < 4; j++) {
                decoded[j * 2] = geExpandAdpcmNibble(*in >> 4, shift);
                decoded[j * 2 + 1] = geExpandAdpcmNibble(*in & 0xf, shift);
                in++;
            }

            for (j = 0; j < 8; j++) {
                int k;
                int32_t acc =
                    table[0][j] * prev2 +
                    table[1][j] * prev1 +
                    ((int32_t)decoded[j] << 11);

                for (k = 0; k < j; k++) {
                    acc += table[1][(j - k) - 1] * decoded[k];
                }

                acc >>= 11;
                *out++ = geClamp16(acc);
            }
        }

        nbytes -= 16 * (int)sizeof(int16_t);
    }

    memcpy(state, out - 16, 16 * sizeof(int16_t));
    if (ge_mixer_probe) {
        ge_mixer_n_adpcm++;
        geProbePeak(geMixer.dmem.s16 + geMixer.out / 2,
                    GE_ROUND_UP_32(geMixer.nbytes) / 2,
                    &ge_mixer_pk_adpcm);
    }
}

void aResampleImpl(uint8_t flags, uint16_t pitch, RESAMPLE_STATE state)
{
    int16_t temp[16];
    int16_t *in_initial = geMixer.dmem.s16 + geMixer.in / sizeof(int16_t);
    int16_t *in = in_initial;
    int16_t *out = geMixer.dmem.s16 + geMixer.out / sizeof(int16_t);
    int nbytes = GE_ROUND_UP_16(geMixer.nbytes);
    uint32_t accumulator;
    int i;

    if (flags & A_INIT) {
        memset(temp, 0, 5 * sizeof(int16_t));
    } else {
        memcpy(temp, state, sizeof(temp));
    }

    if (flags & A_LOOP) {
        memcpy(in - 8, temp + 8, 8 * sizeof(int16_t));
        in -= temp[5] / (int)sizeof(int16_t);
    }

    in -= 4;
    accumulator = (uint16_t)temp[4];
    memcpy(in, temp, 4 * sizeof(int16_t));

    do {
        for (i = 0; i < 8; i++) {
            const int16_t *table = geResampleTable[(accumulator * 64) >> 16];
            int32_t sample =
                ((in[0] * table[0] + 0x4000) >> 15) +
                ((in[1] * table[1] + 0x4000) >> 15) +
                ((in[2] * table[2] + 0x4000) >> 15) +
                ((in[3] * table[3] + 0x4000) >> 15);

            *out++ = geClamp16(sample);
            accumulator += (uint32_t)pitch << 1;
            in += accumulator >> 16;
            accumulator &= 0xffff;
        }
        nbytes -= 8 * (int)sizeof(int16_t);
    } while (nbytes > 0);

    state[4] = (int16_t)accumulator;
    memcpy(state, in, 4 * sizeof(int16_t));
    i = (int)((in - in_initial + 4) & 7);
    in -= i;
    if (i != 0) i = -8 - i;
    state[5] = (int16_t)i;
    memcpy(state + 8, in, 8 * sizeof(int16_t));
    if (ge_mixer_probe) {
        ge_mixer_n_resample++;
        geProbePeak(geMixer.dmem.s16 + geMixer.out / 2,
                    GE_ROUND_UP_16(geMixer.nbytes) / 2,
                    &ge_mixer_pk_resample);
    }
}

void aEnvMixerImpl(uint8_t flags, ENVMIX_STATE state)
{
    int16_t *input = geMixer.dmem.s16 + geMixer.in / sizeof(int16_t);
    int16_t *dry[2] = {
        geMixer.dmem.s16 + geMixer.out / sizeof(int16_t),
        geMixer.dmem.s16 + geMixer.dry_right / sizeof(int16_t)
    };
    int16_t *wet[2] = {
        geMixer.dmem.s16 + geMixer.wet_left / sizeof(int16_t),
        geMixer.dmem.s16 + geMixer.wet_right / sizeof(int16_t)
    };
    int nbytes = GE_ROUND_UP_16(geMixer.nbytes);
    int16_t target[2];
    int32_t rate[2];
    int16_t dry_factor;
    int16_t wet_factor;
    int32_t volumes[2][8];
    int c;
    int i;

    if (ge_mixer_probe) {
        geProbePeak(input, GE_ROUND_UP_16(geMixer.nbytes) / 2,
                    &ge_mixer_pk_envmix_in);
        if (geMixer.vol[0] > ge_mixer_em_vol0) ge_mixer_em_vol0 = geMixer.vol[0];
        if (geMixer.vol[1] > ge_mixer_em_vol1) ge_mixer_em_vol1 = geMixer.vol[1];
        if (geMixer.vol_dry > ge_mixer_em_dry) ge_mixer_em_dry = geMixer.vol_dry;
        if (geMixer.vol_wet > ge_mixer_em_wet) ge_mixer_em_wet = geMixer.vol_wet;
        if (geMixer.target[0] > ge_mixer_em_tgt0) ge_mixer_em_tgt0 = geMixer.target[0];
        ge_mixer_em_rate0 = geMixer.rate[0];
        ge_mixer_em_flags = flags;
    }

    if (flags & A_INIT) {
        target[0] = geMixer.target[0];
        target[1] = geMixer.target[1];
        rate[0] = geMixer.rate[0];
        rate[1] = geMixer.rate[1];
        dry_factor = geMixer.vol_dry;
        wet_factor = geMixer.vol_wet;

        for (c = 0; c < 2; c++) {
            int32_t step = rate[c] / 8;
            for (i = 0; i < 8; i++) {
                volumes[c][i] = geClamp32(
                    (int64_t)geMixer.vol[c] * 65536 +
                    (int64_t)step * (i + 1));
            }
        }
    } else {
        memcpy(volumes[0], state, 32);
        memcpy(volumes[1], state + 16, 32);
        target[0] = state[32];
        target[1] = state[35];
        rate[0] = (int32_t)(((uint32_t)(uint16_t)state[33] << 16) |
                            (uint16_t)state[34]);
        rate[1] = (int32_t)(((uint32_t)(uint16_t)state[36] << 16) |
                            (uint16_t)state[37]);
        dry_factor = state[38];
        wet_factor = state[39];
    }

    do {
        for (c = 0; c < 2; c++) {
            for (i = 0; i < 8; i++) {
                int16_t whole = (int16_t)((uint32_t)volumes[c][i] >> 16);
                int32_t dry_gain;

                if ((rate[c] >= 0 && whole > target[c]) ||
                    (rate[c] < 0 && whole < target[c])) {
                    volumes[c][i] = (int32_t)target[c] * 65536;
                    whole = target[c];
                }

                dry_gain = ((int32_t)whole * dry_factor + 0x4000) >> 15;
                dry[c][i] = geClamp16(
                    ((int32_t)dry[c][i] * 0x7fff +
                     (int32_t)input[i] * dry_gain + 0x4000) >> 15);

                if (flags & A_AUX) {
                    int32_t wet_gain =
                        ((int32_t)whole * wet_factor + 0x4000) >> 15;
                    wet[c][i] = geClamp16(
                        ((int32_t)wet[c][i] * 0x7fff +
                         (int32_t)input[i] * wet_gain + 0x4000) >> 15);
                }

                volumes[c][i] = geClamp32(
                    (int64_t)volumes[c][i] + rate[c]);
            }

            dry[c] += 8;
            if (flags & A_AUX) wet[c] += 8;
        }

        input += 8;
        nbytes -= 8 * (int)sizeof(int16_t);
    } while (nbytes > 0);

    memcpy(state, volumes[0], 32);
    memcpy(state + 16, volumes[1], 32);
    state[32] = target[0];
    state[33] = (int16_t)((uint32_t)rate[0] >> 16);
    state[34] = (int16_t)rate[0];
    state[35] = target[1];
    state[36] = (int16_t)((uint32_t)rate[1] >> 16);
    state[37] = (int16_t)rate[1];
    state[38] = dry_factor;
    state[39] = wet_factor;
    if (ge_mixer_probe) {
        ge_mixer_n_envmix++;
        geProbePeak(geMixer.dmem.s16 + geMixer.out / 2,
                    GE_ROUND_UP_16(geMixer.nbytes) / 2,
                    &ge_mixer_pk_envmix);
    }
}

void aPoleFilterImpl(uint8_t flags, int16_t gain, void *state)
{
    int16_t *filter_state = (int16_t *)state;
    const int16_t *input =
        geMixer.dmem.s16 + geMixer.in / sizeof(int16_t);
    int16_t *output =
        geMixer.dmem.s16 + geMixer.out / sizeof(int16_t);
    const int32_t coefficient = geMixer.adpcm_table[0][1][0];
    int32_t y;
    int samples = geMixer.nbytes / (int)sizeof(int16_t);
    int i;

    if (flags & A_INIT) {
        filter_state[0] = 0;
        filter_state[1] = 0;
        filter_state[2] = 0;
        filter_state[3] = 0;
    }

    y = filter_state[3];
    for (i = 0; i < samples; i++) {
        int32_t x = input[i];
        y = geClamp16((int32_t)(
            ((int64_t)gain * x + (int64_t)coefficient * y) >> 14));
        output[i] = (int16_t)y;
    }

    filter_state[2] = samples > 1 ? output[samples - 2] : filter_state[3];
    filter_state[3] = (int16_t)y;
    if (ge_mixer_probe) {
        geProbePeak(output, samples, &ge_mixer_pk_polef);
    }
}

static int geEnvClampMode = -1;
static unsigned long geEnvSampleFaults;
static unsigned long geEnvSourceFaults;

int geEnvSamplesGuard(int samples, int maxsamples)
{
    if (geEnvClampMode < 0) {
        const char *value = getenv("GETV_ENVCLAMP");
        geEnvClampMode = value && value[0] == '0' ? 0 : 1;
    }

    if (samples >= 0 && samples <= maxsamples) return samples;

    geEnvSampleFaults++;
    if (geEnvSampleFaults <= 8 || (geEnvSampleFaults & 0xffu) == 0) {
        fprintf(stderr, "[getv] env samples guard: %d outside 0..%d [count=%lu]\n",
                samples, maxsamples, geEnvSampleFaults);
    }
    if (!geEnvClampMode) return samples;
    if (samples < 0) return 0;
    return maxsamples;
}

int geEnvSourceGuard(const void *source)
{
    if (source) return 1;
    geEnvSourceFaults++;
    if (geEnvSourceFaults <= 8 || (geEnvSourceFaults & 0xffu) == 0) {
        fprintf(stderr, "[getv] env source guard: NULL [count=%lu]\n",
                geEnvSourceFaults);
    }
    return 0;
}

#ifdef GE_MIXER_TESTING
void geMixerTestGetState(int16_t *book, ADPCM_STATE **loop,
                         int16_t vol[2], int16_t target[2], int32_t rate[2],
                         int16_t *dry, int16_t *wet)
{
    memcpy(book, geMixer.adpcm_table, sizeof(geMixer.adpcm_table));
    *loop = geMixer.adpcm_loop_state;
    memcpy(vol, geMixer.vol, sizeof(geMixer.vol));
    memcpy(target, geMixer.target, sizeof(geMixer.target));
    memcpy(rate, geMixer.rate, sizeof(geMixer.rate));
    *dry = geMixer.vol_dry;
    *wet = geMixer.vol_wet;
}
#endif
