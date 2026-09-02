#include "global.h"
#include "time_tint.h"
#include "main.h" // gMain (vblankCounter1)
#include "palette.h"
#include "rtc.h" // RtcCalcLocalTime, gLocalTime

//HYDRA ---- 24-hour day/night tint ---------------------------------------------
// One tint per hour: R,G,B (0-255) and a blend percentage. To keep the hue VIVID
// (not washed to grey), the tint is normalized to its dominant channel: the strongest
// channel is preserved near full brightness while the others are pulled down by the
// blend. That makes night read as saturated blue, dawn/dusk as saturated warm, etc.
// A single strength knob scales how pronounced the whole effect is.
// Noon (blend 0) is a true no-op (full brightness).

struct HourTint { u8 r, g, b, blend; }; // blend is a percentage 0..100

static const struct HourTint sHourTints[24] =
{
    [0]  = {  20,  30,  70, 55 }, // deep night - dark navy
    [1]  = {  15,  25,  65, 60 },
    [2]  = {  12,  22,  60, 65 }, // darkest
    [3]  = {  18,  28,  62, 62 },
    [4]  = {  40,  40,  90, 55 }, // pre-dawn - deep purple-blue
    [5]  = {  80,  60, 120, 45 }, // dawn start - vibrant purple
    [6]  = { 160,  80, 140, 35 }, // dawn - rich magenta-pink
    [7]  = { 255, 140,  80, 25 }, // sunrise - vibrant orange-gold
    [8]  = { 255, 180, 120, 20 }, // early morning - peach
    [9]  = { 255, 200, 150, 15 },
    [10] = { 255, 215, 180, 10 },
    [11] = { 255, 235, 210,  5 },
    [12] = { 255, 255, 255,  0 }, // noon - no tint
    [13] = { 255, 255, 255,  0 }, // noon - no tint
    [14] = { 255, 255, 240,  3 },
    [15] = { 255, 250, 220,  5 },
    [16] = { 255, 220, 160, 10 }, // late afternoon warm
    [17] = { 255, 180, 100, 20 }, // golden hour - rich amber
    [18] = { 255, 120,  80, 30 }, // sunset - vibrant orange-red
    [19] = { 200,  80, 160, 40 }, // dusk - deep magenta-purple
    [20] = { 140,  60, 140, 45 }, // twilight - rich purple
    [21] = {  90,  50, 120, 50 },
    [22] = {  50,  40, 100, 52 },
    [23] = {  25,  35,  85, 54 },
};

//HYDRA Pronouncement strength: effective blend = table blend% * (NUM/DEN).
// Higher = stronger / more saturated tint. 10/10 uses the table values as-is.
#define TINT_STRENGTH_NUM 20
#define TINT_STRENGTH_DEN 10

// Per-channel /256 multipliers for an hour, normalized to the tint's dominant channel
// so the hue stays vivid. 256 == identity. Filled into rMul/gMul/bMul.
static void BakeHourMul(u32 hour, u32 *rMul, u32 *gMul, u32 *bMul)
{
    u32 r = sHourTints[hour].r, g = sHourTints[hour].g, b = sHourTints[hour].b;
    u32 blend = sHourTints[hour].blend;
    u32 maxc = r, bEff, den;

    if (g > maxc) maxc = g;
    if (b > maxc) maxc = b;

    if (maxc == 0 || blend == 0) // no tint -> identity
    {
        *rMul = *gMul = *bMul = 256;
        return;
    }

    bEff = blend * TINT_STRENGTH_NUM / TINT_STRENGTH_DEN;
    if (bEff > 100)
        bEff = 100;

    // mult = (1 - bEff/100) + (bEff/100) * (channel / maxc)
    //      = ((100 - bEff)*maxc + bEff*channel) / (100 * maxc)   [in /256 fixed point]
    den = 100u * maxc;
    *rMul = (256u * ((100u - bEff) * maxc + bEff * r) + den / 2u) / den;
    *gMul = (256u * ((100u - bEff) * maxc + bEff * g) + den / 2u) / den;
    *bMul = (256u * ((100u - bEff) * maxc + bEff * b) + den / 2u) / den;
    if (*rMul > 256u) *rMul = 256u;
    if (*gMul > 256u) *gMul = 256u;
    if (*bMul > 256u) *bMul = 256u;
}

struct BlendSettings TimeTint_GetHourBlend(u32 hour)
{
    struct BlendSettings bs = {0};
    u32 r, g, b;

    if (hour >= 24)
        hour = 0;

    if (sHourTints[hour].blend == 0) // true full-brightness identity (noon)
    {
        bs.isTint = FALSE;
        bs.coeff = 0;
        bs.blendColor = 0;
        return bs;
    }

    BakeHourMul(hour, &r, &g, &b);
    // Overworld blendColor bytes are 8-bit, so cap the multiplier at 255 (~full).
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    bs.isTint = TRUE;
    bs.coeff = 16; // ignored for tints (TimeMixPalettes forces full strength)
    bs.blendColor = r | (g << 8) | (b << 16);
    return bs;
}

u16 TimeTint_GetInterpWeight(u32 minutes)
{
    if (minutes >= 60)
        minutes = 59;
    // 256 at minute 0 (all of the current hour) -> 0 at minute 60 (all of the next hour).
    return (u16)(256u - (256u * minutes) / 60u);
}

void TimeTint_GetEffectiveTime(u32 *hours, u32 *minutes)
{
#if TIME_TINT_DEBUG_SECONDS_PER_HOUR > 0
    // Fast preview from the global frame counter: secondsPerHour * 60fps / 60min ==
    // secondsPerHour frames per in-game minute.
    u32 totalMin = gMain.vblankCounter1 / TIME_TINT_DEBUG_SECONDS_PER_HOUR;
    *hours = (totalMin / 60u) % 24u;
    *minutes = totalMin % 60u;
#else
    RtcCalcLocalTime();
    *hours = (gLocalTime.hours >= 0 && gLocalTime.hours < 24) ? (u32)gLocalTime.hours : 0;
    *minutes = (gLocalTime.minutes >= 0 && gLocalTime.minutes < 60) ? (u32)gLocalTime.minutes : 0;
#endif
}

// Multiply one 5-bit channel by a /256 multiplier, clamp to 31. Same as the engine's
// tint path in TimeMixPalettes.
static s32 TintChannel(s32 src, u32 mul256)
{
    s32 v = ((s32)mul256 * src) >> 8;
    return (v > 31) ? 31 : v;
}

void TimeTint_ApplyToBattlePalette(u16 *palette, u16 count)
{
    u32 h0, h1, minutes, i;
    u32 r0, g0, b0, r1, g1, b1;
    s32 w;

    TimeTint_GetEffectiveTime(&h0, &minutes);
    h1 = (h0 + 1) % 24;

    BakeHourMul(h0, &r0, &g0, &b0);
    BakeHourMul(h1, &r1, &g1, &b1);
    w = TimeTint_GetInterpWeight(minutes); // 256 => all h0, 0 => all h1

    for (i = 0; i < count; i++)
    {
        u16 c;
        s32 r, g, b, rs, gs, bs2, re, ge, be;

        if ((i & 15) == 0) // color 0 of each 16-color slot is transparent
            continue;

        c = palette[i];
        r = c & 0x1F;
        g = (c >> 5) & 0x1F;
        b = (c >> 10) & 0x1F;

        // Tint by both anchors, then lerp by weight (exactly TimeMixPalettes' tint path).
        rs = TintChannel(r, r0); re = TintChannel(r, r1);
        gs = TintChannel(g, g0); ge = TintChannel(g, g1);
        bs2 = TintChannel(b, b0); be = TintChannel(b, b1);

        r = re + (((rs - re) * w) >> 8);
        g = ge + (((gs - ge) * w) >> 8);
        b = be + (((bs2 - be) * w) >> 8);

        palette[i] = (u16)((b << 10) | (g << 5) | r);
    }
}
