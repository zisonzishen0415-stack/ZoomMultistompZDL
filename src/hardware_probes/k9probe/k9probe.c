/*
 * k9probe.c
 *
 * Hardware probe for the LineSel-cloned synthesized edit-handler path on
 * knobs 4..9 (pages 2/3). The descriptor advertises the full 9-user-knob
 * ceiling (3 pages x 3 visible knobs). Knobs 1..3 use the hardware-proven
 * stock blobs (LineSel knob1/knob2 + AIR knob3); knobs 4..9 use the
 * linker's LineSel-cloned synthetic handlers (build/linker.py,
 * synthesize_linesel_edit_handlers, synth_edit_start_index=3).
 *
 * Hardware pass criterion:
 *   - K9Probe loads and appears in the Filter category browser,
 *   - navigating to page 2 and page 3 and touching every knob never
 *     freezes the pedal,
 *   - each knob audibly changes the output in its expected channel.
 *
 * Audio body: odd knobs (Knob1/3/5/7/9) boost the L block, even knobs
 * (Knob2/4/6/8) boost the R block, each with a distinct step
 * (k+1)*0.04 per normalized unit. Turning a knob is therefore audibly
 * identifiable and provably lands in params[5+k].
 */

#include <stdint.h>

#include "k9probe_params.h"

#pragma CODE_SECTION(Fx_FLT_K9Probe, ".audio")

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))

void Fx_FLT_K9Probe(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf  = ZDL_PTR(float *, ctx[5]);
    float *outBuf = ZDL_PTR(float *, ctx[6]);

    /* LineSel current-sample plumbing (ctx[11]/ctx[12]). Required to keep
     * audio routing alive even when the effect does almost no DSP. */
    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    float gainL = 1.0f;
    float gainR = 1.0f;
    int i;
    for (i = 0; i < K9PROBE_PARAM_COUNT; i++) {
        /* params[5+i] is whatever the knob handler wrote. The exact
         * normalization is not yet pinned down, so map through a wide
         * window (abs + clamp to [0,1]): any nonzero knob becomes audible. */
        float raw = params[K9PROBE_KNOB1_SLOT + i];
        if (raw < 0.0f) raw = -raw;
        if (raw > 1.0f) raw = 1.0f;
        float step = (float)(i + 1) * 0.04f * raw;
        if ((i & 1) == 0) gainL += step;   /* Knob1,3,5,7,9 -> L block */
        else              gainR += step;   /* Knob2,4,6,8   -> R block */
    }
    /* clamp so a garbage param value can never blow the output */
    if (gainL > 3.0f) gainL = 3.0f;
    if (gainR > 3.0f) gainR = 3.0f;

    /* LLLLLLLL RRRRRRRR block layout: first 8 floats are L, next 8 are R.
     * The output buffer is accumulated, never overwritten. */
    int n;
    for (n = 0; n < 8; n++) {
        outBuf[n]     += fxBuf[n]     * gainL;
        outBuf[n + 8] += fxBuf[n + 8] * gainR;
    }
}
