/*
 * Reverson ZDL body - 9-knob reverse reverb for Zoom G1on / MS-series.
 * The v2 core is pulled into this translation unit with the FDN bed
 * compiled out; state is carved lazily from the host ctx[3] arena.
 * Knob mapping: P1 Mix/Rev/Space, P2 Tone/Grain/Duck, P3 Mode/Trig/Predelay.
 */
#include <stdint.h>
#include "../../airwindows/common/zoom_params.h"
#include "reverson_params.h"

#define REVERSON_ENABLE_FDN 0
#include "../../../../core/rev_delay.c"
#include "../../../../core/rev_env.c"
#include "../../../../core/rev_swell.c"
#include "../../../../core/rev_rev.c"
#include "../../../../core/reverson.c"

#ifndef REVERSON_AUDIO_FUNC
#define REVERSON_AUDIO_FUNC Fx_REV_Reverson
#endif

#define REVERSON_DO_PRAGMA(x) _Pragma(#x)
#define REVERSON_EXPAND_PRAGMA(x) REVERSON_DO_PRAGMA(x)
#define REVERSON_CODE_SECTION(func) REVERSON_EXPAND_PRAGMA(CODE_SECTION(func, ".audio"))

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

#define REVERSON_ZDL_MAGIC 0x52565731u   /* "RVW1" */
#define REVERSON_ZDL_VERSION 1u
#define REVERSON_ARENA_FLOOR 524288u

static inline float revzdl_abs(float x) { return x < 0.0f ? -x : x; }

REVERSON_CODE_SECTION(REVERSON_AUDIO_FUNC)
void REVERSON_AUDIO_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf = ZDL_PTR(float *, ctx[5]);

    /* stock magic shuttle: preserve it first, always */
    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;   /* bypass: dry passthrough */

    volatile unsigned int *desc = ZDL_PTR(volatile unsigned int *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end = (uintptr_t)desc[1];
    unsigned int span = desc[2];
    uintptr_t bytes = end - base;
    if (base == 0u || end <= base) return;
    if ((base & 3u) != 0u || (end & 3u) != 0u || (span & 3u) != 0u) return;
    if (bytes < REVERSON_ARENA_FLOOR || span < bytes) return;
    if (bytes > 0x00800000u || span > 0x00800000u) return;

    /* header (magic/version + denormal seeds) then the core state block */
    uintptr_t hdr = (base + 3u) & ~(uintptr_t)3u;
    uintptr_t coreBase = hdr + 16u;
    uint32_t need = Reverson_state_size(44100.0f);
    if (coreBase + (uintptr_t)need > end) return;

    uint32_t *mag = (uint32_t *)hdr;
    Reverson *core = (Reverson *)coreBase;
    if (mag[0] != REVERSON_ZDL_MAGIC || mag[1] != REVERSON_ZDL_VERSION) {
        Reverson *r = Reverson_init((void *)core, need, 44100.0f);
        if (!r) return;
        mag[0] = REVERSON_ZDL_MAGIC;
        mag[1] = REVERSON_ZDL_VERSION;
        mag[2] = 0x1234567u;   /* denormal dither seeds */
        mag[3] = 0x89ABCDFu;
    }

    /* 9 knobs on the stock 0..0.14 raw rail, with the manifest defaults as
       fallbacks (untouched knobs ship the 'diiv' preset) */
    float mix      = zoom_param_norm(params[REVERSON_MIX_SLOT],      REVERSON_MIX_DEFAULT_NORM);
    float rev      = zoom_param_norm(params[REVERSON_REV_SLOT],      REVERSON_REV_DEFAULT_NORM);
    float space    = zoom_param_norm(params[REVERSON_SPACE_SLOT],    REVERSON_SPACE_DEFAULT_NORM);
    float tone     = zoom_param_norm(params[REVERSON_TONE_SLOT],     REVERSON_TONE_DEFAULT_NORM);
    float grain    = zoom_param_norm(params[REVERSON_GRAIN_SLOT],    REVERSON_GRAIN_DEFAULT_NORM);
    float duck     = zoom_param_norm(params[REVERSON_DUCK_SLOT],     REVERSON_DUCK_DEFAULT_NORM);
    float modev    = zoom_param_norm(params[REVERSON_MODE_SLOT],     REVERSON_MODE_DEFAULT_NORM);
    float trig     = zoom_param_norm(params[REVERSON_TRIG_SLOT],     REVERSON_TRIG_DEFAULT_NORM);
    float predelay = zoom_param_norm(params[REVERSON_PREDELAY_SLOT], REVERSON_PREDELAY_DEFAULT_NORM);

    Reverson_set_6knob(core, mix, rev, space, tone, grain, duck);
    Reverson_set_param(core, REVERSON_PARAM_TRIG, trig);
    Reverson_set_param(core, REVERSON_PARAM_PREDELAY, predelay);
    {
        int mode = (int)(modev * 5.0f + 0.5f);
        if (mode >= 1) {
            ReversonParams mp;
            Reverson_mode(mode, &mp);
            Reverson_set_param(core, REVERSON_PARAM_MIX, mp.mix);
            Reverson_set_param(core, REVERSON_PARAM_DECAY, mp.decay);
            Reverson_set_param(core, REVERSON_PARAM_TONE, mp.tone);
            Reverson_set_param(core, REVERSON_PARAM_REVLEN, mp.revlen);
            Reverson_set_param(core, REVERSON_PARAM_DUCK, mp.duck);
            Reverson_set_param(core, REVERSON_PARAM_GATE, mp.gate);
            Reverson_set_param(core, REVERSON_PARAM_SHAPE, mp.shape);
            Reverson_set_param(core, REVERSON_PARAM_MOD, mp.mod);
            Reverson_set_param(core, REVERSON_PARAM_SAT, mp.sat);
            Reverson_set_param(core, REVERSON_PARAM_WIDTH, mp.width);
            Reverson_set_param(core, REVERSON_PARAM_DENSITY, mp.density);
            Reverson_set_param(core, REVERSON_PARAM_BASS, mp.bass);
            Reverson_set_param(core, REVERSON_PARAM_DIFFUSION, mp.diffusion);
        }
    }

    int i;
    for (i = 0; i < 8; i++) {
        float xl = fxBuf[i];
        float xr = fxBuf[i + 8];
        /* denormal dither (same trick as StChorus): keep the delay lines
           from filling with denormals during silence */
        if (revzdl_abs(xl) < 1.18e-23f) xl = (float)mag[2] * 1.18e-17f;
        if (revzdl_abs(xr) < 1.18e-23f) xr = (float)mag[3] * 1.18e-17f;
        mag[2] ^= mag[2] << 13; mag[2] ^= mag[2] >> 17; mag[2] ^= mag[2] << 5;
        mag[3] ^= mag[3] << 13; mag[3] ^= mag[3] >> 17; mag[3] ^= mag[3] << 5;

        float ol, orr;
        Reverson_process_stereo(core, xl, xr, &ol, &orr);
        fxBuf[i] = ol;
        fxBuf[i + 8] = orr;
    }
}
