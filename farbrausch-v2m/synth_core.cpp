#include "types.h"
#include "synth.h"
#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// TODO:
// - VU meters?

// Ye olde original V2 bugs you can turn on and off :)
#define BUG_V2_FM_RANGE 0     // Broken sine range reduction for FM oscis

// Debugging tools
#define DEBUGSCOPES 0
#define COVERAGE    0

// --------------------------------------------------------------------------
// Debug scopes
// --------------------------------------------------------------------------

#if DEBUGSCOPES || defined EMSCRIPTEN
#include "scope.h"
#define DEBUG_PLOT_OPEN(which, title, rate, w, h) scopeOpen((which), (title), (rate), (w), (h))
#define DEBUG_PLOT_VAL(which, value) do { float t=value; scopeSubmit((which), &t, 1); } while(0)
#define DEBUG_PLOT(which, data, nsamples) scopeSubmit((which), (data), (nsamples))
#define DEBUG_PLOT_STRIDED(which, data, stride, nsamples) scopeSubmitStrided((which), (data), (stride), (nsamples))
#define DEBUG_PLOT_UPDATE() scopeUpdateAll()
#else
#define DEBUG_PLOT_OPEN(which, title, rate, w, h)
#define DEBUG_PLOT_VAL(which, value)
#define DEBUG_PLOT(which, data, nsamples)
#define DEBUG_PLOT_STRIDED(which, data, stride, nsamples)
#define DEBUG_PLOT_UPDATE()
#endif

#define DEBUG_PLOT_CHAN(which, ch) ((unsigned char *)(which)+(ch))
#define DEBUG_PLOT_STEREO(which, data, nsamples) \
    DEBUG_PLOT_STRIDED(DEBUG_PLOT_CHAN(which, 0), &(data)->l, 2, (nsamples)); \
    DEBUG_PLOT_STRIDED(DEBUG_PLOT_CHAN(which, 1), &(data)->r, 2, (nsamples))

// --------------------------------------------------------------------------
// Code coverage
// --------------------------------------------------------------------------

#if COVERAGE
#include <stdio.h>

static const char *code_coverage[500];
#define COVER(desc) code_coverage[__COUNTER__] = (desc)

static int synthGetNumCoverage();
#else
#define COVER(desc)
#endif

// --------------------------------------------------------------------------
// Constants.
// --------------------------------------------------------------------------

// Natural constants
static const float fclowest = 1.220703125e-4f; // 2^(-13) - clamp EGs to 0 below this (their nominal range is 0..128)
static const float fcpi_2  = 1.5707963267948966192313216916398f;
static const float fcpi    = 3.1415926535897932384626433832795f;
static const float fc1p5pi = 4.7123889803846898576939650749193f;
static const float fc2pi   = 6.28318530717958647692528676655901f;
static const float fc32bit = 2147483648.0f; // 2^31 (original code has (2^31)-1, but this ends up rounding up to 2^31 anyway)

// Synth constants
static const float fcoscbase   = 261.6255653f; // Oscillator base freq of C4 (middle C)
static const float fcsrbase    = 44100.0f;     // Base sampling rate
static const float fcboostfreq = 150.0f;       // Bass boost cut-off freq
static const float fcframebase = 128.0f;       // size of a frame in samples
static const float fcdcflt     = 126.0f;
static const float fccfframe   = 11.0f;

static const float fcfmmax     = 2.0f;
static const float fcattackmul = -0.09375f; // -0.0859375
static const float fcattackadd = 7.0f;
static const float fcsusmul    = 0.0019375f;
static const float fcgain      = 0.6f;
static const float fcgainh     = 0.6f;
static const float fcmdlfomul  = 1973915.49f;
static const float fccpdfalloff = 0.9998f; // @@@BUG this should probably depend on sampling rate.

static const float fcdcoffset  = 3.814697265625e-6f; // 2^-18

// --------------------------------------------------------------------------
// General helper functions.
// --------------------------------------------------------------------------

#define COUNTOF(x)  (int)(sizeof(x)/sizeof(*(x)))

// Float bitcasts. Union-based type punning to maximize compiler
// compatibility.
union FloatBits
{
    float f;
    uint32_t u;
};

static float bits2float(uint32_t u)
{
    FloatBits x;
    x.u = u;
    return x.f;
}

// Fast arctangent
static float fastatan(float x)
{
    // extract sign
    float sign = 1.0f;
    if (x < 0.0f)
    {
        sign = -1.0f;
        x = -x;
    }

    // we have two rational approximations: one for |x| < 1.0 and one for
    // |x| >= 1.0, both of the general form
    //   r(x) = (cx1*x + cx3*x^3) / (cxm0 + cxm2*x^2 + cxm4*x^4) + bias
    // original V2 code uses doubles here but frankly the coefficients
    // just aren't accurate enough to warrant it :)
    static const float coeffs[2][6] = {
        //          cx1          cx3         cxm0         cxm2         cxm4         bias
        {          1.0f, 0.43157974f,        1.0f, 0.05831938f, 0.76443945f,        0.0f },
        { -0.431597974f,       -1.0f, 0.05831938f,        1.0f, 0.76443945f, 1.57079633f },
    };
    const float *c = coeffs[x >= 1.0f]; // interestingly enough, V2 code does this test wrong (cmovge instead of cmovae)
    float x2 = x*x;
    float r = (c[1]*x2 + c[0])*x / ((c[4]*x2 + c[3])*x2 + c[2]) + c[5];
    return r * sign;
}

// Fast sine for x in [-pi/2, pi/2]
// This is a single-precision odd Minimax polynomial, not a Taylor series!
// Coefficients courtesy of Robin Green.
static float fastsin(float x)
{
    float x2 = x*x;
    return (((-0.00018542f*x2 + 0.0083143f)*x2 - 0.16666f)*x2 + 1.0f) * x;
}

// Fast sine with range check (for x >= 0)
// Applies symmetries, then funnels into fastsin.
static float fastsinrc(float x)
{
    // @@@BUG
    // NB this range reduction really only works for values >=0,
    // yet FM-sine oscillators will also pass in negative values.
    // This is not a good idea. At all. But it's what the original
    // V2 code does. :)

    // first range reduction: mod with 2pi
    x = fmodf(x, fc2pi);
    // now x in [-2pi,2pi]

#if !BUG_V2_FM_RANGE
    if (x < 0.0f)
        x += fc2pi;
#endif

    // need to reduce to [-pi/2, pi/2] to call fastsin
    if (x > fc1p5pi) // x in (3pi/2,2pi]
        x -= fc2pi; // sin(x) = sin(x-2pi)
    else if (x > fcpi_2) // x in (pi/2,3pi/2]
        x = fcpi - x; // sin(x) = -sin(x-pi) = sin(-(x-pi)) = sin(pi-x)

    return fastsin(x);
}

static float calcfreq(float x)
{
    return powf(2.0f, (x - 1.0f)*10.0f);
}

static float calcfreq2(float x)
{
    return powf(2.0f, (x - 1.0f)*fccfframe);
}

// square
static inline float sqr(float x)
{
    return x*x;
}

template<typename T>
static inline T min(T a, T b)
{
    return a < b ? a : b;
}

template<typename T>
static inline T max(T a, T b)
{
    return a > b ? a : b;
}

template<typename T>
static inline T clamp(T x, T min, T max)
{
    return (x < min) ? min : (x > max) ? max : x;
}

// uniform randon number generator
// just a linear congruential generator, nothing fancy.
static inline uint32_t urandom(uint32_t *seed)
{
    *seed = *seed * 196314165 + 907633515;
    return *seed;
}

// uniform random float in [-1,1)
static inline float frandom(uint32_t *seed)
{
    uint32_t bits = urandom(seed); // random 32-bit value
    float f = bits2float((bits >> 9) | 0x40000000); // random float in [2,4)
    return f - 3.0f; // uniform random float in [-1,1)
}

// 32-bit value into float with 23 bits percision
static inline float utof23(uint32_t x)
{
    float f = bits2float((x >> 9) | 0x3f800000); // 1 + x/(2^32)
    return f - 1.0f;
}

// float from [0,1) into 0.32 unsigned fixed-point
// this loses a bit, but that's what V2 does.
static inline uint32_t ftou32(float v)
{
    return 2u * (int)(v * fc32bit);
}

// linear interpolation between a and b using t.
static inline float lerp(float a, float b, float t)
{
    return a + t * (b-a);
}

// DEBUG
#include <stdarg.h>
#include <stdio.h>

// --------------------------------------------------------------------------
// Building blocks
// --------------------------------------------------------------------------

union StereoSample
{
    struct
    {
        float l, r;
    };
    float ch[2];
};

// LRC filter.
// The state variables are 'l' and 'b'. The time series for l and b
// correspond to a resonant low-pass and band-pass respectively, hence
// the name. 'step' returns 'h', which is just the "missing" resonant
// high-pass.
//
// Note that 'freq' here isn't actually a frequency at all, it's actually
// 2*(1 - cos(2pi*freq/SR)), but V2 calls this "frequency" anyway :)
struct V2LRC
{
    float l, b;

    void init()
    {
        l = b = 0.0f;
    }

    // Single step
    float step(float in, float freq, float reso)
    {
        l += freq * b;
        float h = in - b*reso - l;
        b += freq * h;
        return h;
    }

    // 2x oversampled step (the good stuff)
    float step_2x(float in, float freq, float reso)
    {
        // the filters get slightly biased inputs to avoid the state variables
        // getting too close to 0 for prolonged periods of time (which would
        // cause denormals to appear)
        in += fcdcoffset;

        // step 1
        l += freq * b - fcdcoffset; // undo bias here (1 sample delay)
        b += freq * (in - b*reso - l);

        // step 2
        l += freq * b;
        float h = in - b*reso - l;
        b += freq * h;

        return h;
    }
};

// Moog filter state
struct V2Moog
{
    float b[5]; // filter state

    void init()
    {
        b[0] = b[1] = b[2] = b[3] = b[4] = 0.0f;
    }

    float step(float realin, float f, float p, float q)
    {
        float in = realin + fcdcoffset; // again, biased in
        float t1, t2, t3, b4;

        in -= q * b[4]; // feedback
        t1 = b[1]; b[1] = (in + b[0]) * p - b[1] * f;
        t2 = b[2]; b[2] = (t1 + b[1]) * p - b[2] * f;
        t3 = b[3]; b[3] = (t2 + b[2]) * p - b[3] * f;
        b4   = (t3 + b[3]) * p - b[4] * f;

        b4 -= b4*b4*b4 * (1.0f/6.0f); // clipping
        b4 -= fcdcoffset; // un-bias
        b[4] = b4 - fcdcoffset;
        b[0] = realin;

        return b4;
    }
};

// DC filter state. Just a highpass used with a very low cut-off
// to remove DC offsets from a signal.
struct V2DCF
{
    float xm1; // x(n-1)
    float ym1; // y(n-1)

    void init()
    {
        xm1 = ym1 = 0.0f;
    }

    float step(float in, float R)
    {
        // y(n) = x(n) - x(n-1) + R*y(n-1)
        float y = (fcdcoffset + R*ym1 - xm1 + in) - fcdcoffset;
        xm1 = in;
        ym1 = y;
        return y;
    }
};

// Constant-length delay line
class V2Delay
{
    uint32_t pos, len;
    float *buf;

public:
    V2Delay()
        : pos(0), len(0), buf(0)
    {
    }

    void init(float *buf, uint32_t len)
    {
        this->buf = buf;
        this->len = len;
        reset();
    }

    template<uint32_t N>
    void init(float (&buf)[N])
    {
        init(buf, N);
    }

    void reset()
    {
        memset(buf, 0, sizeof(*buf)*len);
        pos = 0;
    }

    inline float fetch() const
    {
        return buf[pos];	// EMSCRIPTEN luckily this seems to be properly aligned
    }

    inline void feed(float v)
    {
        buf[pos] = v;
        if (++pos == len)
            pos = 0;
    }
};

// debug stuff
/*static void checkRange(const float *src, int nsamples)
{
    for (int i=0; i < nsamples; i++)
        assert(src[i] >= -1.5f && src[i] < 1.5f);
}

static void checkRange(const StereoSample *src, int nsamples)
{
    checkRange(&src[0].l, nsamples * 2);
}*/

// --------------------------------------------------------------------------
// V2 Instance
// --------------------------------------------------------------------------

struct V2Instance
{
    static const int MAX_FRAME_SIZE = 280; // in samples

    // Stuff that depends on the sample rate
    float SRfcsamplesperms;
    float SRfcobasefrq;
    float SRfclinfreq;
    float SRfcBoostCos, SRfcBoostSin;
    float SRfcdcfilter;

    int SRcFrameSize;
    float SRfciframe;

    // buffers
    float vcebuf[MAX_FRAME_SIZE];
    float vcebuf2[MAX_FRAME_SIZE];
    StereoSample levelbuf[MAX_FRAME_SIZE]; // original V2 overlaps level buffer with voice buffers
    StereoSample chanbuf[MAX_FRAME_SIZE];
    float aux1buf[MAX_FRAME_SIZE];
    float aux2buf[MAX_FRAME_SIZE];
    StereoSample mixbuf[MAX_FRAME_SIZE];
    StereoSample auxabuf[MAX_FRAME_SIZE];
    StereoSample auxbbuf[MAX_FRAME_SIZE];

    void calcNewSampleRate(int samplerate)
    {
        float sr = (float)samplerate;

        SRfcsamplesperms = sr / 1000.0f;
        SRfcobasefrq = (fcoscbase * fc32bit) / sr;
        SRfclinfreq = fcsrbase / sr;
        SRfcdcfilter = 1.0f - fcdcflt / sr;

        // frame size
        SRcFrameSize = (int)(fcframebase * sr / fcsrbase + 0.5f);
        SRfciframe = 1.0f / (float)SRcFrameSize;

        assert(SRcFrameSize <= MAX_FRAME_SIZE);

        // low shelving EQ
        float boost = (fcboostfreq * fc2pi) / sr;
        SRfcBoostCos = cos(boost);
        SRfcBoostSin = sin(boost);
    }
};

// --------------------------------------------------------------------------
// Oscillator
// --------------------------------------------------------------------------

struct syVOsc
{
    float mode;    // OSC_* (as float. it's all floats in here)
    float ring;
    float pitch;
    float detune;
    float color;
    float gain;
};

struct V2Osc
{
    enum Mode
    {
        OSC_OFF     = 0,
        OSC_TRI_SAW = 1,
        OSC_PULSE   = 2,
        OSC_SIN     = 3,
        OSC_NOISE   = 4,
        OSC_FM_SIN  = 5,
        OSC_AUXA    = 6,
        OSC_AUXB    = 7,
    };

    int mode;          // OSC_*
    bool ring;          // ring modulation on/off
    uint32_t cnt;           // wave counter
    int freq;          // wave counter inc (8x/sample)
    uint32_t brpt;          // break point for tri/pulse wave
    float nffrq, nfres;  // noise filter freq/resonance
    uint32_t nseed;         // noise random seed
    float gain;          // output gain
    V2LRC nf;           // noise filter
    float note;
    float pitch;

    V2Instance *inst;   // V2 instance we belong to.

    void init(V2Instance *instance, int idx)
    {
        static const uint32_t seeds[] = { 0xdeadbeefu, 0xbaadf00du, 0xd3adc0deu };
        assert(idx < COUNTOF(seeds));

        cnt = 0;
        nf.init();
        nseed = seeds[idx];
        inst = instance;
    }

    void noteOn()
    {
        chgPitch();
    }

    void chgPitch()
    {
        nffrq = inst->SRfclinfreq * calcfreq((pitch + 64.0f) / 128.0f);
        freq = (int)(inst->SRfcobasefrq * powf(2.0f, (pitch + note - 60.0f) / 12.0f));
    }

    void set(const syVOsc *para)
    {
        mode = (int)para->mode;
        ring = (((int)para->ring) & 1) != 0;

        pitch = (para->pitch - 64.0f) + (para->detune - 64.0f) / 128.0f;
        chgPitch();
        gain = para->gain / 128.0f;

        float col = para->color / 128.0f;
        brpt = ftou32(col);
        nfres = 1.0f - sqrtf(col);
    }

    void render(float *dest, int nsamples)
    {
        switch (mode & 7)
        {
        case OSC_OFF:     break;
        case OSC_TRI_SAW: renderTriSaw(dest, nsamples); break;
        case OSC_PULSE:   renderPulse(dest, nsamples); break;
        case OSC_SIN:     renderSin(dest, nsamples); break;
        case OSC_NOISE:   renderNoise(dest, nsamples); break;
        case OSC_FM_SIN:  renderFMSin(dest, nsamples); break;
        case OSC_AUXA:    renderAux(dest, inst->auxabuf, nsamples); break;
        case OSC_AUXB:    renderAux(dest, inst->auxbbuf, nsamples); break;
        }

        DEBUG_PLOT(this, dest, nsamples);
    }

private:
    inline void output(float *dest, float x)
    {
        if (ring)
            *dest *= x;
        else
            *dest += x;
    }

    // Oscillator state machine (read description of renderTriSaw for context)
    //
    // We keep track of whether the current sample is in the up or down phase,
    // whether the previous sample was, and if the waveform counter wrapped
    // around on the transition. This allows us to figure out which of
    // the cases above we fall into. Note this code uses a different bit ordering
    // from the ASM version that is hopefully a bit easier to understand.
    //
    // For reference: our bits map to the ASM version as follows (MSB->LSB order)
    //   (o)ld_up
    //   (c)arry
    //   (n)ew_up

    enum OSMTransitionCode    // carry:old_up:new_up
    {
        OSMTC_DOWN = 0,         // old=down, new=down, no carry
        // 1 is an invalid configuration
        OSMTC_UP_DOWN = 2,      // old=up, new=down, no carry
        OSMTC_UP = 3,           // old=up, new=up, no carry
        OSMTC_DOWN_UP_DOWN = 4, // old=down, new=down, carry
        OSMTC_DOWN_UP = 5,      // old=down, new=up, carry
        // 6 is an invalid configuration
        OSMTC_UP_DOWN_UP = 7    // old=up, new=up, carry
    };

    inline uint32_t osm_init() // our state field: old_up:new_up
    {
        return (cnt - freq) < brpt ? 3 : 0;
    }

    inline uint32_t osm_tick(uint32_t &state) // returns transition code
    {
        // old_up = new_up, new_up = (cnt < brpt)
        state = ((state << 1) | (cnt < brpt)) & 3;

        // we added freq to cnt going from the previous sample to the current one.
        // so if cnt is less than freq, we carried.
        uint32_t transition_code = state | (cnt < (uint32_t)freq ? 4 : 0);

        // finally, tick the oscillator
        cnt += freq;

        return transition_code;
    }

    void renderTriSaw(float *dest, int nsamples)
    {
        // Okay, so here's the general idea: instead of the classical sawtooth
        // or triangle waves, V2 uses a generalized triangle wave that looks like
        // this:
        //
        //       /\                  /\           |
        //      /   \               /   \         |
        //     /      \            /      \       |
        // ---/---------\---------/---------\> t  |
        //   /            \      /                |
        //  /               \   /                 |
        // /                  \/                  |
        // [-----1 period-----]
        // [-----] "break point" (brpt)
        //
        // If brpt=1/2 (ignoring fixed-point scaling), you get a regular triangle
        // wave. The example shows brpt=1/3, which gives an asymmetrical triangle
        // wave. At the extremes, brpt=0 gives a pure saw-down wave, and brpt=1
        // (if that was a possible value, which it isn't) gives a pure saw-up wave.
        //
        // Purely point-sampling this (or any other) waveform would cause notable
        // aliasing. The standard ways to avoid this issue are to either:
        // 1) Over-sample by a certain amount and then use a low-pass filter to
        //    (hopefully) get rid of the frequencies that would alias, or
        // 2) Generate waveforms from a Fourier series that's cut off below the
        //    Nyquist frequency, ensuring there's no aliasing to begin with.
        // V2 does neither. Instead it computes the convolution of the continuous
        // waveform with an analytical low-pass filter. The ideal low-pass in
        // terms of frequency response would be a sinc filter, which unfortunately
        // has infinite support. Instead, V2 just uses a simple box filter. This
        // doesn't exactly have favorable frequency-domain characteristics, but
        // it's still much better than point sampling and has the advantage that
        // it's fairly simple analytically. It boils down to computing the average
        // value of the waveform over the interval [t,t+h], where t is the current
        // time and h = 1/SR (SR=sampling rate), which is in turn:
        //
        //    f_box(t) = 1/h * (integrate(x=t..t+h) f(x) dx)
        //
        // Now there's a bunch of cases for these intervals [t,t+h] that we need to
        // consider. Bringing up the diagram again, and adding some intervals at the
        // bottom:
        //
        //       /\                  /\           |
        //      /   \               /   \         |
        //     /      \            /      \       |
        // ---/---------\---------/---------\> t  |
        //   /            \      /                |
        //  /               \   /                 |
        // /                  \/                  |
        // [-a-]      [-c]
        //     [--b--]       [-d--]
        //   [-----------e-----------]
        //          [-----------f-----------]
        //
        // a) is purely in the saw-up region,
        // b) starts in the saw-up region and ends in saw-down,
        // c) is purely in the saw-down region,
        // d) starts during saw-down and ends in saw-up.
        // e) starts during saw-up and ends in saw-up, but passes through saw-down
        // f) starts saw-down, ends saw-down, passes through saw-up.
        //
        // For simplicity here, I draw different-sized intervals sampling a fixed-
        // frequency wave, even though in practice it's the other way round, but
        // this way it's easier to put it all into a single picture.
        //
        // The original assembly code goes through a few gyrations to encode all
        // these possible cases into a bitmask and then does a single switch.
        // In practice, for all but very high-frequency waves, we're hitting the
        // "easy" cases a) and c) almost all the time.
        COVER("Osc tri/saw");

        // calc helper values
        float f = utof23(freq);
        float omf = 1.0f - f;
        float rcpf = 1.0f / f;
        float col = utof23(brpt);

        // m1 = 2/col = slope of saw-up wave
        // m2 = -2/(1-col) = slope of saw-down wave
        // c1 = gain/2*m1 = gain/col = scaled integration constant
        // c2 = gain/2*m2 = -gain/(1-col) = scaled integration constant
        float c1 = gain / col;
        float c2 = -gain / (1.0f - col);

        uint32_t state = osm_init();

        for (int i=0; i < nsamples; i++)
        {
            float p = utof23(cnt) - col;
            float y = 0.0f;

            // state machine action
            switch (osm_tick(state))
            {
            case OSMTC_UP: // case a)
                // average of linear function = just sample in the middle
                y = c1 * (p + p - f);
                break;

            case OSMTC_DOWN: // case c)
                // again, average of a linear function
                y = c2 * (p + p - f);
                break;

            case OSMTC_UP_DOWN: // case b)
                y = rcpf * (c2 * sqr(p) - c1 * sqr(p-f));
                break;

            case OSMTC_DOWN_UP: // case d)
                y = -rcpf * (gain + c2*sqr(p + omf) - c1*sqr(p));
                break;

            case OSMTC_UP_DOWN_UP: // case e)
                y = -rcpf * (gain + c1*omf*(p + p + omf));
                break;

            case OSMTC_DOWN_UP_DOWN: // case f)
                y = -rcpf * (gain + c2*omf*(p + p + omf));
                break;

                // INVALID CASES
            default:
                assert(false);
                break;
            }

            output(dest + i, y + gain);
        }
    }

    void renderPulse(float *dest, int nsamples)
    {
        // This follows the same general pattern as renderTriSaw above, except
        // this time the waveform is a pulse wave with variable pulse width,
        // which means we get very simple integrals. The state machine works
        // the exact same way, see above for description.
        COVER("Osc pulse");

        // calc helper values
        float f = utof23(freq);
        float gdf = gain / f;
        float col = utof23(brpt);

        float cc121 = gdf * 2.0f * (col - 1.0f) + gain;
        float cc212 = gdf * 2.0f * col - gain;

        uint32_t state = osm_init();

        for (int i=0; i < nsamples; i++)
        {
            float p = utof23(cnt);
            float out = 0.0f;

            switch (osm_tick(state))
            {
            case OSMTC_UP:
                out = gain;
                break;

            case OSMTC_DOWN:
                out = -gain;
                break;

            case OSMTC_UP_DOWN:
                out = gdf * 2.0f * (col - p) + gain;
                break;

            case OSMTC_DOWN_UP:
                out = gdf * 2.0f * p - gain;
                break;

            case OSMTC_UP_DOWN_UP:
                out = cc121;
                break;

            case OSMTC_DOWN_UP_DOWN:
                out = cc212;
                break;

                // INVALID CASES
            default:
                assert(false);
                break;
            }

            output(dest + i, out);
        }
    }

    void renderSin(float *dest, int nsamples)
    {
        COVER("Osc sin");

        // Sine is already a perfectly bandlimited waveform, so we needn't
        // worry about aliasing here.
        for (int i=0; i < nsamples; i++)
        {
            // Brace yourselves: The name is a lie! It's actually a cosine wave!
            uint32_t phase = cnt + 0x40000000; // quarter-turn (pi/2) phase offset
            cnt += freq; // step the oscillator

            // range reduce to [0,pi]
            if (phase & 0x80000000) // Symmetry: cos(x) = cos(-x)
                phase = ~phase; // V2 uses ~ not - which is slightly off but who cares

            // convert to t in [1,2)
            float t = bits2float((phase >> 8) | 0x3f800000); // 1.0f + (phase / (2^31))

            // and then to t in [-pi/2,pi/2)
            // i know the V2 ASM code says "scale/move to (-pi/4 .. pi/4)".
            // trust me, it's lying.
            t = t * fcpi - fc1p5pi;

            output(dest + i, gain * fastsin(t));
        }
    }

    void renderNoise(float *dest, int nsamples)
    {
        COVER("Osc noise");

        V2LRC flt = nf;
        uint32_t seed = nseed;

        for (int i=0; i < nsamples; i++)
        {
            // uniform random value (noise)
            float n = frandom(&seed);

            // filter
            float h = flt.step(n, nffrq, nfres);
            float x = nfres*(flt.l + h) + flt.b;

            output(dest + i, gain * x);
        }

        flt = nf;
        nseed = seed;
    }

    void renderFMSin(float *dest, int nsamples)
    {
        COVER("Osc FM");

        // V2's take on FM is a bit unconventional but fairly slick and flexible.
        // The carrier wave is always a sine, but the modulator is whatever happens
        // to be in the voice buffer at that point - which is the output of the
        // previous oscillators. So you can use all the oscillator waveforms
        // (including noise, other FMs and the aux bus oscillators!) as modulator
        // if you are so inclined.
        //
        // And it's very little code too :)
        for (int i=0; i < nsamples; i++)
        {
            float mod = dest[i] * fcfmmax;
            float t = (utof23(cnt) + mod) * fc2pi;
            cnt += freq;

            float out = gain * fastsinrc(t);
            if (ring)
                dest[i] *= out;
            else
                dest[i] = out;
        }
    }

    void renderAux(float *dest, const StereoSample *src, int nsamples)
    {
        COVER("Osc aux");

        float g = gain * fcgain;
        for (int i=0; i < nsamples; i++)
        {
            float aux = g * (src[i].l + src[i].r);
            if (ring)
                aux *= dest[i];
            dest[i] = aux;
        }
    }
};

// --------------------------------------------------------------------------
// Envelope Generator
// --------------------------------------------------------------------------

struct syVEnv
{
    float ar;  // attack rate
    float dr;  // decay rate
    float sl;  // sustain level
    float sr;  // sustain rate
    float rr;  // release rate
    float vol; // volume
};

struct V2Env
{
    // Slightly different state ordering here than in V2 code, to make
    // things simpler.
    enum State
    {
        OFF,
        RELEASE,

        ATTACK,
        DECAY,
        SUSTAIN,
    };

    float out;
    State state;
    float val;   // output value (0.0-128.0)
    float atd;   // attack delta (added every frame in stAttack, transition ->stDecay at 128.0)
    float dcf;   // decay factor (mul'd every frame in stDecay, transition ->stSustain at sul)
    float sul;   // sustain level (defines stDecay->stSustain transition point)
    float suf;   // sustain factor (mul'd every frame in stSustain, transition ->stRelease at gate off or ->stOff at 0.0)
    float ref;   // release factor (mul'd every frame in stRelease, transition ->stOff at 0.0)
    float gain;

    void init(V2Instance *)
    {
        state = OFF;
    }

    void set(const syVEnv *para)
    {
        // ar: 2^7 (128) to 2^-4 (0.03, ca. 10 secs at 344frames/sec)
        atd = powf(2.0f, para->ar * fcattackmul + fcattackadd);

        // dcf: 0 (5msecs thanks to volramping) up to almost 1
        dcf = 1.0f - calcfreq2(1.0f - para->dr / 128.0f);

        // sul: 0..127 is fine already
        sul = para->sl;

        // suf: 1/128 (15ms till it's gone) up to 128 (15ms till it's fully there)
        suf = powf(2.0f, fcsusmul * (para->sr - 64.0f));

        // ref: 0 (5ms thanks to volramping) up to almost 1
        ref = 1.0f - calcfreq2(1.0f - para->rr / 128.0f);
        gain = para->vol / 128.0f;
    }

    void tick(bool gate)
    {
        // process immediate gate transition
        if (state <= RELEASE && gate) // gate on
            state = ATTACK;
        else if (state >= ATTACK && !gate) // gate off
            state = RELEASE;

        // process current state
        switch (state)
        {
        case OFF:
            COVER("EG off");
            val = 0.0f;
            break;

        case ATTACK:
            COVER("EG atk");
            val += atd;
            if (val >= 128.0f)
            {
                val = 128.0f;
                state = DECAY;
            }
            break;

        case DECAY:
            COVER("EG dcy");
            val *= dcf;
            if (val <= sul)
            {
                val = sul;
                state = SUSTAIN;
            }
            break;

        case SUSTAIN:
            COVER("EG sus");
            val *= suf;
            if (val > 128.0f)
                val = 128.0f;
            break;

        case RELEASE:
            COVER("EG rel");
            val *= ref;
            break;
        }

        // avoid underflow to denormals
        if (val <= fclowest)
        {
            val = 0.0f;
            state = OFF;
        }

        out = val * gain;
        DEBUG_PLOT_VAL(this, out / 128.0f);
    }
};

// --------------------------------------------------------------------------
// Filter
// --------------------------------------------------------------------------

struct syVFlt
{
    float mode;
    float cutoff;
    float reso;
};

struct V2Flt
{
    enum Mode
    {
        BYPASS,
        LOW,
        BAND,
        HIGH,
        NOTCH,
        ALL,
        MOOGL,
        MOOGH
    };

    int mode;
    float cfreq;
    float res;
    float moogf, moogp, moogq; // moog filter coeffs
    V2LRC lrc;
    V2Moog moog;

    V2Instance *inst;

    void init(V2Instance *instance)
    {
        lrc.init();
        moog.init();
        inst = instance;
    }

    void set(const syVFlt *para)
    {
        mode = (int)para->mode;
        float f = calcfreq(para->cutoff / 128.0f) * inst->SRfclinfreq;
        float r = para->reso / 128.0f;

        if (mode < MOOGL)
        {
            COVER("VCF set regular");
            res = 1.0f - r;
            cfreq = f;
        }
        else
        {
            COVER("VCF set moog");

            // @@@BUG? V2 code for this part looks suspicious.
            f *= 0.25f;
            float t = 1.0f - f;

            moogp = f + 0.8f * f * t;
            moogf = 1.0f - moogp - moogp;
            moogq = 4.0f * r * (1.0f + 0.5f * t * (1.0f - t + 5.6f * t * t));
        }
    }

    void render(float *dest, const float *src, int nsamples, int step=1)
    {
        V2LRC flt;
        V2Moog m;

        switch (mode & 7)
        {
        case BYPASS:
            COVER("VCF bypass");
            // @@@BUG ignores step? this is wrong but I suppose this
            // never gets hit for stereo case.
            if (dest != src)
                memmove(dest, src, nsamples * sizeof(float));
            break;

        case LOW:
            COVER("VCF low");
            flt = lrc;
            for (int i=0; i < nsamples; i++)
            {
                flt.step_2x(src[i*step], cfreq, res);
                dest[i*step] = flt.l;
            }
            lrc = flt;
            break;

        case BAND:
            COVER("VCF band");
            flt = lrc;
            for (int i=0; i < nsamples; i++)
            {
                flt.step_2x(src[i*step], cfreq, res);
                dest[i*step] = flt.b;
            }
            lrc = flt;
            break;

        case HIGH:
            COVER("VCF high");
            flt = lrc;
            for (int i=0; i < nsamples; i++)
            {
                float h = flt.step_2x(src[i*step], cfreq, res);
                dest[i*step] = h;
            }
            lrc = flt;
            break;

        case NOTCH:
            COVER("VCF notch");
            flt = lrc;
            for (int i=0; i < nsamples; i++)
            {
                float h = flt.step_2x(src[i*step], cfreq, res);
                dest[i*step] = flt.l + h;
            }
            lrc = flt;
            break;

        case ALL:
            COVER("VCF all");
            flt = lrc;
            for (int i=0; i < nsamples; i++)
            {
                float h = flt.step_2x(src[i*step], cfreq, res);
                dest[i*step] = flt.l + flt.b + h;
            }
            lrc = flt;
            break;

        case MOOGL:
            COVER("VCF moog low");
            // Moog filters are 2x oversampled, so run filter twice.
            m = moog;
            for (int i=0; i < nsamples; i++)
            {
                float in = src[i*step];
                m.step(in, moogf, moogp, moogq);
                dest[i*step] = m.step(in, moogf, moogp, moogq);
            }
            moog = m;
            break;

        case MOOGH:
            COVER("VCF moog high");
            m = moog;
            for (int i=0; i < nsamples; i++)
            {
                float in = src[i*step];
                m.step(in, moogf, moogp, moogq);
                dest[i*step] = in - m.step(in, moogf, moogp, moogq);
            }
            moog = m;
            break;
        }

        DEBUG_PLOT_STRIDED(this, dest, step, nsamples);
    }
};

// --------------------------------------------------------------------------
// Low Frequency Oscillator
// --------------------------------------------------------------------------

struct syVLFO
{
    float mode;    // 0=saw, 1=tri, 2=pulse, 3=sin, 4=s&h
    float sync;    // 0=free, 1=in sync with keyon
    float egmode;  // 0=continuous 1=one-shot (EG mode)
    float rate;    // rate (0Hz..~43Hz)
    float phase;   // start phase shift
    float pol;     // polarity: +, -, +/-
    float amp;     // amplification (0..1)
};

struct V2LFO
{
    enum Mode
    {
        SAW,
        TRI,
        PULSE,
        SIN,
        S_H
    };

    float out;
    int mode;         // mode
    bool sync;        // sync mode
    bool eg;          // envelope generator mode
    int freq;         // frequency
    uint32_t cntr;    // counter
    uint32_t cphase;  // counter sync phase
    float gain;       // output gain
    float dc;         // output dc
    uint32_t nseed;   // random seed
    uint32_t last;    // last counter value (for s&h transition)

    void init(V2Instance *)
    {
        cntr = last = 0;
        nseed = rand(); // not really, but close enough...
    }

    void set(const syVLFO *para)
    {
        mode = (int)para->mode;
        sync = (int)para->sync != 0;
        eg = (int)para->egmode != 0;
        freq = (int)(0.5f * fc32bit * calcfreq(para->rate / 128.0f));
        cphase = ftou32(para->phase / 128.0f);

        switch ((int)para->pol)
        {
        case 0: // +
            COVER("LFO pol +");
            gain = para->amp;
            dc = 0.0f;
            break;

        case 1: // -
            COVER("LFO pol -");
            gain = -para->amp;
            dc = 0.0f;
            break;

        case 2: // +/-
            COVER("LFO pol +/-");
            gain = para->amp;
            dc = -0.5f * para->amp;
            break;
        }
    }

    void keyOn()
    {
        if (sync)
        {
            COVER("LFO sync");
            cntr = cphase;
            last = ~0u;
        }
    }

    void tick()
    {
        float v;
        uint32_t x;

        switch (mode & 7)
        {
        case SAW:
        default:
            COVER("LFO saw");
            v = utof23(cntr);
            break;

        case TRI:
            COVER("LFO tri");
            x = (cntr << 1) ^ (int32_t(cntr) >> 31);
            v = utof23(x);
            break;

        case PULSE:
            COVER("LFO pulse");
            x = int32_t(cntr) >> 31;
            v = utof23(x);
            break;

        case SIN:
            COVER("LFO sin");
            v = utof23(cntr);
            v = fastsinrc(v * fc2pi) * 0.5f + 0.5f;
            break;

        case S_H:
            COVER("LFO sample+hold");
            if (cntr < last)
                nseed = urandom(&nseed);
            last = cntr;
            v = utof23(nseed);
            break;
        }

        out = v * gain + dc;
        cntr += freq;
        if (cntr < (uint32_t)freq && eg) // in one-shot mode, clamp at wrap-around
            cntr = ~0u;

        DEBUG_PLOT_VAL(this, out / 128.0f);
    }
};

// --------------------------------------------------------------------------
// Distortion
// --------------------------------------------------------------------------

struct syVDist
{
    float mode;    // see below
    float ingain;  // -12dB .. 36dB
    float param1;  // outgain / crush / outfreq / cutoff
    float param2;  // offset / xor / jitter / reso
};

struct V2Dist
{
    enum Mode
    {
        OFF = 0,
        OVERDRIVE,
        CLIP,
        BITCRUSHER,
        DECIMATOR,

        FLT_BASE  = DECIMATOR,
        FLT_LOW   = FLT_BASE + V2Flt::LOW,
        FLT_BAND  = FLT_BASE + V2Flt::BAND,
        FLT_HIGH  = FLT_BASE + V2Flt::HIGH,
        FLT_NOTCH = FLT_BASE + V2Flt::NOTCH,
        FLT_ALL   = FLT_BASE + V2Flt::ALL,
        FLT_MOOGL = FLT_BASE + V2Flt::MOOGL,
        FLT_MOOGH = FLT_BASE + V2Flt::MOOGH,
    };

    int mode;
    float gain1;     // input gain for all fx
    float gain2;     // output gain for od/clip
    float offs;      // offs for od/clip
    float crush1;    // 1/crush_factor
    int crush2;      // crush_factor
    int crxor;       // xor value for crush
    uint32_t dcount; // decimator counter
    uint32_t dfreq;  // decimator frequency
    float dvall;     // last decimator value (mono/left)
    float dvalr;     // last decimator value (mono/right)
    V2Flt fltl;      // filter mono/left
    V2Flt fltr;      // filter right

    void init(V2Instance *instance)
    {
        dcount = 0;
        dvall = dvalr = 0.0f;
        fltl.init(instance);
        fltr.init(instance);
    }

    void set(const syVDist *para)
    {
        float x;

        mode = (int)para->mode;
        gain1 = powf(2.0f, (para->ingain - 32.0f) / 16.0f);

        switch (mode)
        {
        case OFF:
            break;

        case OVERDRIVE:
            gain2 = (para->param1 / 128.0f) / atan(gain1);
            offs = gain1 * 2.0f * ((para->param2 / 128.0f) - 0.5f);
            break;

        case CLIP:
            gain2 = para->param1 / 128.0f;
            offs = gain1 * 2.0f * ((para->param2 / 128.0f) - 0.5f);
            break;

        case BITCRUSHER:
            x = para->param1 * 256.0f + 1.0f;
            crush2 = (int)x;
            crush1 = gain1 * (32768.0f / x);
            crxor = ((int)para->param2) << 9;
            break;

        case DECIMATOR:
            dfreq = ftou32(calcfreq(para->param1 / 128.0f));
            break;

        default: // filters
        {
            syVFlt setup;
            setup.cutoff = para->param1;
            setup.reso = para->param2;
            setup.mode = (float)(mode - FLT_BASE);
            fltl.set(&setup);
            fltr.set(&setup);
        }
            break;
        }
    }

    void renderMono(float *dest, const float *src, int nsamples)
    {
        switch (mode)
        {
        case OFF:
            if (dest != src)
                memmove(dest, src, nsamples * sizeof(float));
            break;

        case OVERDRIVE:
            COVER("DIST overdrive");
            for (int i=0; i < nsamples; i++)
                dest[i] = overdrive(src[i]);
            break;

        case CLIP:
            COVER("DIST clip");
            for (int i=0; i < nsamples; i++)
                dest[i] = clip(src[i]);
            break;

        case BITCRUSHER:
            COVER("DIST bitcrusher");
            for (int i=0; i < nsamples; i++)
                dest[i] = bitcrusher(src[i]);
            break;

        case DECIMATOR:
            COVER("DIST mono decimator");
            for (int i=0; i < nsamples; i++)
            {
                decimator_tick(src[i], 0.0f);
                dest[i] = dvall;
            }
            break;

        default: // filters
            COVER("DIST mono filter");
            fltl.render(dest, src, nsamples);
            break;
        }

        DEBUG_PLOT(this, dest, nsamples);
    }

    void renderStereo(StereoSample *dest, const StereoSample *src, int nsamples)
    {
        // @@@BUG this matches the original V2 code, but frankly I have my doubts
        // that always running the Moog filters in Mono mode is intentional...
        switch (mode)
        {
        case DECIMATOR:
            COVER("DIST stereo decimator");
            for (int i = 0; i < nsamples; i++)
            {
                decimator_tick(src[i].l, src[i].r);
                dest[i].l = dvall;
                dest[i].r = dvalr;
            }
            break;

        case FLT_LOW:
        case FLT_BAND:
        case FLT_HIGH:
        case FLT_NOTCH:
        case FLT_ALL:
            COVER("DIST stereo filter");
            fltl.render(&dest[0].l, &src[0].l, nsamples, 2);
            fltr.render(&dest[0].r, &src[0].r, nsamples, 2);
            break;

        default:
            // everything else we presume to be stateless and just pass through the
            // mono version.
            renderMono(&dest[0].l, &src[0].l, nsamples*2);
        }

        DEBUG_PLOT_STEREO(this, dest, nsamples);
    }

private:
    inline float overdrive(float in)
    {
        return gain2 * fastatan(in * gain1 + offs);
    }

    inline float clip(float in)
    {
        return gain2 * clamp(in * gain1 + offs, -1.0f, 1.0f);
    }

    inline float bitcrusher(float in)
    {
        int t = (int)(in * crush1);
        t = clamp(t * crush2, -0x7fff, 0x7fff) ^ crxor;
        return (float)t / 32768.0f;
    }

    inline void decimator_tick(float l, float r)
    {
        dcount += dfreq;
        if (dcount < dfreq) // carry
        {
            dvall = l;
            dvalr = r;
        }
    }
};

// --------------------------------------------------------------------------
// DC filter
// --------------------------------------------------------------------------

// This is just a high-pass with very low cut-off to remove DC offsets from
// the signal.
struct V2DCFilter
{
    V2DCF fl;         // left/mono filter state
    V2DCF fr;         // right filter state
    V2Instance *inst;

    void init(V2Instance *instance)
    {
        inst = instance;
        fl.init();
        fr.init();
    }

    void renderMono(float *dest, const float *src, int nsamples)
    {
        float R = inst->SRfcdcfilter;

        V2DCF l = fl;
        for (int i=0; i < nsamples; i++)
            dest[i] = l.step(src[i], R);
        fl = l;
    }

    void renderStereo(StereoSample *dest, const StereoSample *src, int nsamples)
    {
        float R = inst->SRfcdcfilter;

        V2DCF l = fl;
        V2DCF r = fr;
        for (int i=0; i < nsamples; i++)
        {
            dest[i].l = l.step(src[i].l, R);
            dest[i].r = r.step(src[i].r, R);
        }
        fl = l;
        fr = r;
    }
};

// --------------------------------------------------------------------------
// V2 Voice
// --------------------------------------------------------------------------

struct syVV2
{
    // Voice parameters.
    // changing any of these *will* invalidate all V2M files!
    static const int NOSC = 3; // number of oscillators
    static const int NFLT = 2; // number of filters (NB: changing this would complicate routing too)
    static const int NENV = 2; // number of envelope generators
    static const int NLFO = 2; // number of LFOs

    float panning;
    float transp;   // transpose
    syVOsc osc[NOSC];
    syVFlt flt[NFLT];
    float routing; // 0: single 1: serial 2: parallel
    float fltbal;  // parallel filter balance
    syVDist dist;
    syVEnv env[NENV];
    syVLFO lfo[NLFO];
    float oscsync; // 0: none 1: osc 2: full
};

struct V2Voice
{
    enum FilterRouting
    {
        FLTR_SINGLE = 0,
        FLTR_SERIAL,
        FLTR_PARALLEL
    };

    enum KeySync
    {
        SYNC_NONE = 0,
        SYNC_OSC,
        SYNC_FULL
    };

    int note;
    float velo;
    bool gate;

    float curvol;
    float volramp;

    float xpose;     // transpose
    int fmode;     // FLTR_*
    float lvol;      // left volume
    float rvol;      // right volume
    float f1gain;    // filter 1 gain
    float f2gain;    // filter 2 gain

    int keysync;

    V2Osc osc[syVV2::NOSC];
    V2Flt vcf[syVV2::NFLT];
    V2Env env[syVV2::NENV];
    V2LFO lfo[syVV2::NLFO];
    V2Dist dist;    // distorter
    V2DCFilter dcf; // post DC filter

    V2Instance *inst;

    void init(V2Instance *instance)
    {
        for (int i=0; i < syVV2::NOSC; i++)
            osc[i].init(instance, i);
        for (int i=0; i < syVV2::NFLT; i++)
            vcf[i].init(instance);
        for (int i=0; i < syVV2::NENV; i++)
            env[i].init(instance);
        for (int i=0; i < syVV2::NLFO; i++)
            lfo[i].init(instance);
        dist.init(instance);
        dcf.init(instance);
        inst = instance;
    }

    void tick()
    {
        for (int i=0; i < syVV2::NENV; i++)
            env[i].tick(gate);

        for (int i=0; i < syVV2::NLFO; i++)
            lfo[i].tick();

        // volume ramping slope
        volramp = (env[0].out / 128.0f - curvol) * inst->SRfciframe;
        DEBUG_PLOT_VAL(&curvol, curvol);
    }

    void render(StereoSample *dest, int nsamples)
    {
        assert(nsamples <= V2Instance::MAX_FRAME_SIZE);

        // clear voice buffer
        float *voice = inst->vcebuf;
        float *voice2 = inst->vcebuf2;
        memset(voice, 0, nsamples * sizeof(*voice));

        // oscillators -> voice buffer
        for (int i=0; i < syVV2::NOSC; i++)
            osc[i].render(voice, nsamples);

        // voice buffer -> filters -> voice buffer
        switch (fmode)
        {
        case FLTR_SINGLE:
            COVER("VOICE filter single");
            vcf[0].render(voice, voice, nsamples);
            break;

        case FLTR_SERIAL:
        default:
            COVER("VOICE filter serial");
            vcf[0].render(voice, voice, nsamples);
            vcf[1].render(voice, voice, nsamples);
            break;

        case FLTR_PARALLEL:
            COVER("VOICE filter parallel");
            vcf[1].render(voice2, voice, nsamples);
            vcf[0].render(voice, voice, nsamples);
            for (int i=0; i < nsamples; i++)
                voice[i] = voice[i]*f1gain + voice2[i]*f2gain;
            break;
        }

        // voice buffer -> distortion -> voice buffer
        dist.renderMono(voice, voice, nsamples);

        // voice buffer -> dc filter -> voice buffer
        dcf.renderMono(voice, voice, nsamples);

        DEBUG_PLOT(this, voice, nsamples);

        // voice buffer (mono) -> +=output buffer (stereo)
        // original ASM code has chan buffer hardwired as output here
        float cv = curvol;
        for (int i=0; i < nsamples; i++)
        {
            float out = voice[i] * cv;
            cv += volramp;

            dest[i].l += lvol * out + fcdcoffset;
            dest[i].r += rvol * out + fcdcoffset;
        }

        curvol = cv;
    }

    void set(const syVV2 *para)
    {
        xpose = para->transp - 64.0f;
        updateNote();

        fmode = (int)para->routing;
        keysync = (int)para->oscsync;

        // equal power panning
        float p = para->panning / 128.0f;
        lvol = sqrtf(1.0f - p);
        rvol = sqrtf(p);

        // filter balance for parallel
        float x = (para->fltbal - 64.0f) / 64.0f;
        if (x >= 0.0f)
        {
            f2gain = 1.0f;
            f1gain = 1.0f - x;
        }
        else
        {
            f1gain = 1.0f;
            f2gain = 1.0f + x;
        }

        // subsections
        for (int i=0; i < syVV2::NOSC; i++)
            osc[i].set(&para->osc[i]);

        for (int i=0; i < syVV2::NENV; i++)
            env[i].set(&para->env[i]);

        for (int i=0; i < syVV2::NFLT; i++)
            vcf[i].set(&para->flt[i]);

        for (int i=0; i < syVV2::NLFO; i++)
            lfo[i].set(&para->lfo[i]);

        dist.set(&para->dist);
    }

    void noteOn(int note, int vel)
    {
        this->note = note;
        updateNote();

        velo = (float)vel;
        gate = true;

        // reset EGs
        for (int i=0; i < syVV2::NENV; i++)
            env[i].state = V2Env::ATTACK;

        // process sync
        switch (keysync)
        {
        case SYNC_FULL:
            COVER("VOICE noteOn sync full");
            for (int i=0; i < syVV2::NENV; i++)
                env[i].val = 0.0f;
            curvol = 0.0f;

            for (int i=0; i < syVV2::NOSC; i++)
                osc[i].init(inst, i);

            for (int i=0; i < syVV2::NFLT; i++)
                vcf[i].init(inst);

            dist.init(inst);
            // fall-through

        case SYNC_OSC:
            COVER("VOICE noteOn sync osc");
            for (int i=0; i < syVV2::NOSC; i++)
                osc[i].cnt = 0;
            // fall-through

        case SYNC_NONE:
        default:
            break;
        }

        for (int i=0; i < syVV2::NOSC; i++)
            osc[i].chgPitch();

        for (int i=0; i < syVV2::NLFO; i++)
            lfo[i].keyOn();

        dcf.init(inst);
    }

    void noteOff()
    {
        gate = false;
    }

private:
    void updateNote()
    {
        float n = xpose + (float)note;
        for (int i=0; i < syVV2::NOSC; i++)
            osc[i].note = n;
    }
};

// --------------------------------------------------------------------------
// Bass boost (fixed low shelving EQ)
// --------------------------------------------------------------------------

struct syVBoost
{
    float amount;    // boost in dB (0..18)
};

struct V2Boost
{
    bool enabled;
    float a1, a2;      // normalized filter coeffs
    float b0, b1, b2;
    float x1[2];       // state variables
    float x2[2];
    float y1[2];
    float y2[2];

    V2Instance *inst;

    void init(V2Instance *instance)
    {
        inst = instance;
    }

    void set(const syVBoost *para)
    {
        enabled = ((int)para->amount) != 0;
        if (!enabled)
            return;

        COVER("BOOST set");

        // A = 10^(dBgain/40), or a rough approximation anyway
        float A = powf(2.0f, para->amount / 128.0f);

        // V2 code computes beta = sqrt((A^2 + 1) - (A-1)^2) for some reason
        // but applying the binomial formula just gives sqrt(2A)
        float beta = sqrtf(2.0f * A);

        // temp vars
        float bs = beta * inst->SRfcBoostSin;
        float Am1 = A - 1.0f;
        float Ap1 = A + 1.0f;
        float cAm1 = Am1 * inst->SRfcBoostCos;
        float cAp1 = Ap1 * inst->SRfcBoostCos;

        // a0 = (A+1) + (A-1)*cos + beta*sin
        float ia0 = 1.0f / (Ap1 + cAm1 + bs);

        b1 = 2.0f * A * (Am1 - cAp1) * ia0;
        a1 = -2.0f * (Am1 + cAp1) * ia0;
        a2 = (Ap1 + cAm1 - bs) * ia0;
        b0 = A * (Ap1 - cAm1 + bs) * ia0;
        b2 = A * (Ap1 - cAm1 - bs) * ia0;
    }

    void render(StereoSample *buf, int nsamples)
    {
        if (!enabled)
            return;

        COVER("BOOST render");

        for (int ch=0; ch < 2; ch++)
        {
            float xm1 = x1[ch], xm2 = x2[ch];
            float ym1 = y1[ch], ym2 = y2[ch];

            for (int i=0; i < nsamples; i++)
            {
                float x = buf[i].ch[ch] + fcdcoffset;

                // Second-order IIR filter
                float y = b0*x + b1*xm1 + b2*xm2 - a1*ym1 - a2*ym2;
                ym2 = ym1; ym1 = y;
                xm2 = xm1; xm1 = x;

                buf[i].ch[ch] = y;
            }

            x1[ch] = xm1; x2[ch] = xm2;
            y1[ch] = ym1; y2[ch] = ym2;
        }
    }
};

// --------------------------------------------------------------------------
// Modulating delay
// --------------------------------------------------------------------------

struct syVModDel
{
    float amount;    // dry/wet value (0=-wet, 64=dry, 127=wet)
    float fb;        // feedback (0=-100%, 64=0%, 127=~100%)
    float llength;   // length of left delay
    float rlength;   // length of right delay
    float mrate;     // modulation rate
    float mdepth;    // modulation depth
    float mphase;    // modulation stereo phase (0=-180deg, 64=0deg, 127=180deg)
};

struct V2ModDel
{
    float *db[2];    // left/right delay buffer
    uint32_t dbufmask;  // delay buffer mask (size-1, must be pow2)

    uint32_t dbptr;     // buffer write pos
    uint32_t dboffs[2]; // buffer read offset

    uint32_t mcnt;      // mod counter
    int mfreq;     // mod freq
    uint32_t mphase;    // mod phase
    uint32_t mmaxoffs;  // mod max offs (2048samples*depth)

    float fbval;     // feedback val
    float dryout;
    float wetout;

    V2Instance *inst;

    void init(V2Instance *instance, float *buf1, float *buf2, int buflen)
    {
        assert(buflen != 0 && (buflen & (buflen - 1)) == 0);
        db[0] = buf1;
        db[1] = buf2;
        dbufmask = buflen - 1;
        inst = instance;

        reset();
    }

    void reset()
    {
        dbptr = 0;
        mcnt = 0;

        memset(db[0], 0, (dbufmask + 1) * sizeof(float));
        memset(db[1], 0, (dbufmask + 1) * sizeof(float));
    }

    void set(const syVModDel *para)
    {
        wetout = (para->amount - 64.0f) / 64.0f;
        dryout = 1.0f - fabsf(wetout);
        fbval = (para->fb - 64.0f) / 64.0f;

        float lenscale = ((float)dbufmask - 1023.0f) / 128.0f;
        dboffs[0] = (int)(para->llength * lenscale);
        dboffs[1] = (int)(para->rlength * lenscale);

        mfreq = (int)(inst->SRfclinfreq * fcmdlfomul * calcfreq(para->mrate / 128.0f));
        mmaxoffs = (int)(para->mdepth * 1023.0f / 128.0f);
        mphase = ftou32((para->mphase - 64.0f) / 128.0f);
    }

    void renderAux2Main(StereoSample *dest, int nsamples)
    {
        if (!wetout)
            return;

        COVER("MODDEL aux->main");

        for (int i=0; i < nsamples; i++)
        {
            StereoSample x;

            float in = inst->aux2buf[i] + fcdcoffset;
            processSample(&x, in, in, 0.0f);

            dest[i].l += x.l;
            dest[i].r += x.r;
        }
    }

    void renderChan(StereoSample *chanbuf, int nsamples)
    {
        if (!wetout)
            return;

        COVER("MODDEL chan");

        float dry = dryout;
        for (int i=0; i < nsamples; i++)
            processSample(&chanbuf[i], chanbuf[i].l + fcdcoffset, chanbuf[i].r + fcdcoffset, dry);
    }

private:
    inline float processChanSample(float in, int ch, float dry)
    {
        // modulation is a triangle wave
        uint32_t counter = mcnt + (ch ? mphase : 0);
        counter = (counter < 0x80000000u) ? counter*2 : 0xffffffffu - counter*2;

        // determine effective offset
        uint64_t offs32_32 = (uint64_t)counter * mmaxoffs; // 32.32 fixed point
        uint32_t offs_int = uint32_t(offs32_32 >> 32) + dboffs[ch];
        uint32_t index = dbptr - offs_int;

        // linear interpolation using low-order bits of offs32_32.
        float *delaybuf = db[ch];
        float x = utof23((uint32_t)(offs32_32 & 0xffffffffu));
        float delayed = lerp(delaybuf[(index - 0) & dbufmask], delaybuf[(index - 1) & dbufmask], x);

        // mix and output
        delaybuf[dbptr] = in + delayed*fbval;
        return in*dry + delayed*wetout;
    }

    inline void processSample(StereoSample *out, float l, float r, float dry)
    {
        out->l = processChanSample(l, 0, dry);
        out->r = processChanSample(r, 1, dry);

        // tick
        mcnt += mfreq;
        dbptr = (dbptr + 1) & dbufmask;
    }
};

// --------------------------------------------------------------------------
// Stereo Compressor
// --------------------------------------------------------------------------

struct syVComp
{
    float mode;      // 0=off, 1=Peak, 2=RMS
    float stereo;    // 0=mono, 1=stereo
    float autogain;  // 0=off, 1=on
    float lookahead; // lookahead in ms
    float threshold; // threshold (-54dB .. 6dB)
    float ratio;     // (0=1:1 .. 127=1:inf)
    float attack;    // attack value
    float release;   // release value
    float outgain;   // output gain
};

struct V2Comp
{
    static const int COMPDLEN = 5700;
    static const int RMSLEN = 8192; // must be a power of 2

    enum Mode
    {
        MODE_OFF = 0,
        MODE_PEAK,
        MODE_RMS,
    };

    enum ModeBits
    {
        MODE_BIT_PEAK   = 0,
        MODE_BIT_RMS    = 1,
        MODE_BIT_MONO   = 0,
        MODE_BIT_STEREO = 2,
        MODE_BIT_ON     = 0,
        MODE_BIT_OFF    = 4,
    };

    int mode;      // bit 0: Peak/RMS, bit 1: Stereo, bit 2: off

    float invol;     // input gain (1/threshold, internal threshold is always 0dB)
    float ratio;
    float outvol;    // output gain (outgain * threshold)
    float attack;    // attack (lpf coeff, 0..1)
    float release;   // release (lpf coeff, 0..1)

    uint32_t dblen;     // lookahead buffer length
    uint32_t dbcnt;     // lookahead buffer offset

    float curgain[2]; // current gain
    float peakval[2]; // peak value
    float rmsval[2];  // rms current value
    uint32_t rmscnt;     // rms counter

    StereoSample dbuf[COMPDLEN]; // lookahead delay buffer
    StereoSample rmsbuf[RMSLEN]; // RMS ring buffer

    V2Instance *inst;

    void init(V2Instance *instance)
    {
        mode = MODE_BIT_STEREO;
        memset(dbuf, 0, sizeof(dbuf));
        inst = instance;

        reset();
    }

    void reset()
    {
        for (int i=0; i < 2; i++)
        {
            peakval[i] = 0.0f;
            rmsval[i] = 0.0f;
            curgain[i] = 1.0f;
        }
        memset(rmsbuf, 0, sizeof(rmsbuf));
        rmscnt = 0;
    }

    void set(const syVComp *para)
    {
        int oldmode = mode;
        switch ((int)para->mode)
        {
        case MODE_OFF:  mode = MODE_BIT_OFF; break;
        case MODE_PEAK: mode = MODE_BIT_PEAK | MODE_BIT_ON; break;
        case MODE_RMS:  mode = MODE_BIT_RMS | MODE_BIT_ON; break;
        default:        assert(false);
        }

        if (para->stereo != 0.0f)
            mode |= MODE_BIT_STEREO;

        if (mode != oldmode)
            reset();

        // @@@BUG: original V2 code uses "fcsamplesperms" here which is
        // hard-coded to 44.1kHz
        dblen = (int)(para->lookahead * inst->SRfcsamplesperms);

        float thresh = 8.0f * calcfreq(para->threshold / 128.0f);
        invol = 1.0f / thresh;
        if (para->autogain != 0.0f)
            thresh = 1.0f;
        outvol = thresh * powf(2.0f, (para->outgain - 64.0f) / 16.0f);
        ratio = para->ratio / 128.0f;

        // attack: 0 (!) ... 200ms (5Hz)
        attack = powf(2.0f, -para->attack * 12.0f / 128.0f);
        // release: 5ms .. 5s
        release = powf(2.0f, -para->release * 16.0f / 128.0f);
    }

    void render(StereoSample *buf, int nsamples)
    {
        if (mode & MODE_BIT_OFF)
            return;

        COVER("COMP render");

        // Step 1: level detect (fills LD buffers)
        StereoSample *levels = inst->levelbuf;
        switch (mode & (MODE_BIT_RMS | MODE_BIT_STEREO))
        {
        case MODE_BIT_PEAK | MODE_BIT_MONO:
            COVER("COMP level peak mono");
            for (int i=0; i < nsamples; i++)
                levels[i].l = levels[i].r = invol * doPeak(0.5f * (buf[i].l + buf[i].r), 0);
            break;

        case MODE_BIT_RMS | MODE_BIT_MONO:
            COVER("COMP level rms mono");
            for (int i=0; i < nsamples; i++)
            {
                levels[i].l = levels[i].r = invol * doRMS(0.5f * (buf[i].l + buf[i].r), 0);
                rmscnt = (rmscnt + 1) & (RMSLEN - 1);
            }
            break;

        case MODE_BIT_PEAK | MODE_BIT_STEREO:
            COVER("COMP level peak stereo");
            for (int i=0; i < nsamples; i++)
            {
                levels[i].l = invol * doPeak(buf[i].l, 0);
                levels[i].r = invol * doPeak(buf[i].r, 1);
            }
            break;

        case MODE_BIT_RMS | MODE_BIT_STEREO:
            COVER("COMP level rms stereo");
            for (int i=0; i < nsamples; i++)
            {
                levels[i].l = invol * doRMS(buf[i].l, 0);
                levels[i].r = invol * doRMS(buf[i].r, 1);
                rmscnt = (rmscnt + 1) & (RMSLEN - 1);
            }
            break;
        }

        // Step 2: compress!
        for (int ch=0; ch < 2; ch++)
        {
            float gain = curgain[ch];
            uint32_t dbind = dbcnt;

            for (int i=0; i < nsamples; i++)
            {
                // lookahead delay line
                float v = outvol * dbuf[dbind].ch[ch];
                dbuf[dbind].ch[ch] = invol * buf[i].ch[ch];
                if (++dbind >= dblen)
                    dbind = 0;

                // determine dest gain
                float dgain = 1.0f;
                float lvl = levels[i].ch[ch];
                if (lvl >= 1.0f)
                    dgain = 1.0f / (1.0f + ratio * (lvl - 1.0f));

                // and compress
                gain += (dgain < gain ? attack : release) * (dgain - gain);
                buf[i].ch[ch] = v * gain;
            }

            curgain[ch] = gain;
            if (ch == 1)
                dbcnt = dbind;
        }
    }

private:
    // level detection variants
    inline float doPeak(float in, int ch)
    {
        peakval[ch] = max(peakval[ch] * fccpdfalloff + fcdcoffset, fabsf(in));
        return peakval[ch];
    }

    inline float doRMS(float in, int ch)
    {
        float insq = sqr(in + fcdcoffset);
        rmsval[ch] += insq - rmsbuf[rmscnt].ch[ch]; // add new sample, remove oldest
        rmsbuf[rmscnt].ch[ch] = insq; // keep track of value we added
        return sqrtf(rmsval[ch] / (float)RMSLEN);
    }
};

// --------------------------------------------------------------------------
// Stereo reverb
// --------------------------------------------------------------------------

struct syVReverb
{
    float revtime;
    float highcut;
    float lowcut;
    float vol;
};

struct V2Reverb
{
    float gainc[4];  // feedback gain for comb filter delays 0-3
    float gaina[2];  // feedback gain for allpas delays 0-1
    float gainin;    // input gain
    float damp;      // high cut (1-val^2)
    float lowcut;    // low cut (val^2)

    V2Delay combd[2][4];  // left/right comb filter delay lines
    float combl[2][4];     // left/right comb delay filter buffers
    V2Delay alld[2][2];   // left/right allpass filters
    float hpf[2];          // memory for low cut filters

    V2Instance *inst;

    // delay line buffers
    float bcombl0[1309];
    float bcombl1[1635];
    float bcombl2[1811];
    float bcombl3[1926];
    float balll0[220];
    float balll1[74];
    float bcombr0[1327];
    float bcombr1[1631];
    float bcombr2[1833];
    float bcombr3[1901];
    float ballr0[205];
    float ballr1[77];

    void init(V2Instance *instance)
    {
        // init filters
        combd[0][0].init(bcombl0);
        combd[0][1].init(bcombl1);
        combd[0][2].init(bcombl2);
        combd[0][3].init(bcombl3);
        alld[0][0].init(balll0);
        alld[0][1].init(balll1);
        combd[1][0].init(bcombr0);
        combd[1][1].init(bcombr1);
        combd[1][2].init(bcombr2);
        combd[1][3].init(bcombr3);
        alld[1][0].init(ballr0);
        alld[1][1].init(ballr1);

        inst = instance;
        reset();
    }

    void reset()
    {
        for (int ch=0; ch < 2; ch++)
        {
            // comb
            for (int i=0; i < 4; i++)
            {
                combd[ch][i].reset();
                combl[ch][i] = 0.0f;
            }

            // allpass
            for (int i=0; i < 2; i++)
                alld[ch][i].reset();

            // low cut
            hpf[ch] = 0.0f;
        }
    }

    void set(const syVReverb *para)
    {
        static const float gaincdef[4] = {
            0.966384599f, 0.958186359f, 0.953783929f, 0.950933178f
        };
        static const float gainadef[2] = {
            0.994260075f, 0.998044717f
        };

        float e = inst->SRfclinfreq * sqr(64.0f / (para->revtime + 1.0f));
        for (int i=0; i < 4; i++)
            gainc[i] = powf(gaincdef[i], e);

        for (int i=0; i < 2; i++)
            gaina[i] = powf(gainadef[i], e);

        damp = inst->SRfclinfreq * (para->highcut / 128.0f);
        gainin = para->vol / 128.0f;
        lowcut = inst->SRfclinfreq * sqr(sqr(para->lowcut / 128.0f));
    }

    void render(StereoSample *dest, int nsamples)
    {
        const float *inbuf = inst->aux1buf;

        COVER("RVB render");

        for (int i=0; i < nsamples; i++)
        {
            float in = inbuf[i] * gainin + fcdcoffset;

            for (int ch=0; ch < 2; ch++)
            {
                // parallel comb filters
                float cur = 0.0f;
                for (int j=0; j < 4; j++)
                {
                    float dv = gainc[j] * combd[ch][j].fetch();
                    float nv = (j & 1) ? (dv - in) : (dv + in); // alternate phase on combs
                    float lp = combl[ch][j] + damp * (nv - combl[ch][j]);
                    combd[ch][j].feed(lp);
                    cur += lp;
                }

                // serial allpass filters
                for (int j=0; j < 2; j++)
                {
                    float dv = alld[ch][j].fetch();
                    float dz = cur + gaina[j] * dv;
                    alld[ch][j].feed(dz);
                    cur = dv - gaina[j] * dz;
                }

                // low cut and output
                hpf[ch] += lowcut * (cur - hpf[ch]);
                dest[i].ch[ch] += cur - hpf[ch];
            }
        }
    }
};

// --------------------------------------------------------------------------
// Channel
// --------------------------------------------------------------------------

struct syVChan
{
    float chanvol;
    float auxarcv; // aux a receive
    float auxbrcv; // aux b receive
    float auxasnd; // aux a send
    float auxbsnd; // aux b send
    float aux1;
    float aux2;
    float fxroute;
    syVBoost boost;
    syVDist dist;
    syVModDel chorus;
    syVComp comp;
};

struct V2Chan
{
    enum FXRouting
    {
        FXR_DIST_THEN_CHORUS = 0,
        FXR_CHORUS_THEN_DIST,
    };

    float chgain;    // channel gain
    float a1gain;    // aux1 gain
    float a2gain;    // aux2 gain
    float aasnd;     // aux a send gain
    float absnd;     // aux b send gain
    float aarcv;     // aux a receive gain
    float abrcv;     // aux b receive gain
    int fxr;
    V2DCFilter dcf1;
    V2Boost boost;
    V2Dist dist;
    V2DCFilter dcf2;
    V2ModDel chorus;
    V2Comp comp;

    V2Instance *inst;

    void init(V2Instance *instance, float *delbuf1, float *delbuf2, int buflen)
    {
        inst = instance;
        dcf1.init(inst);
        boost.init(inst);
        dist.init(inst);
        dcf2.init(inst);
        chorus.init(inst, delbuf1, delbuf2, buflen);
        comp.init(inst);
    }

    void set(const syVChan *para)
    {
        aarcv = para->auxarcv / 128.0f;
        abrcv = para->auxbrcv / 128.0f;
        aasnd = fcgain * (para->auxasnd / 128.0f);
        absnd = fcgain * (para->auxbsnd / 128.0f);
        chgain = fcgain * (para->chanvol / 128.0f);
        a1gain = chgain * fcgainh * (para->aux1 / 128.0f);
        a2gain = chgain * fcgainh * (para->aux2 / 128.0f);
        fxr = (int)para->fxroute;
        dist.set(&para->dist);
        chorus.set(&para->chorus);
        comp.set(&para->comp);
        boost.set(&para->boost);
    }

    void process(int nsamples)
    {
        StereoSample *chan = inst->chanbuf;

        // AuxA/B receive (stereo)
        accumulate(chan, inst->auxabuf, nsamples, aarcv);
        accumulate(chan, inst->auxbbuf, nsamples, abrcv);

        // Filters
        dcf1.renderStereo(chan, chan, nsamples);
        DEBUG_PLOT_STEREO(&dcf1, chan, nsamples);
        comp.render(chan, nsamples);
        boost.render(chan, nsamples);
        if (fxr == FXR_DIST_THEN_CHORUS)
        {
            dist.renderStereo(chan, chan, nsamples);
            dcf2.renderStereo(chan, chan, nsamples);
            chorus.renderChan(chan, nsamples);
        }
        else // FXR_CHORUS_THEN_DIST
        {
            chorus.renderChan(chan, nsamples);
            dist.renderStereo(chan, chan, nsamples);
            dcf2.renderStereo(chan, chan, nsamples);
        }

        // Aux1/2 send (mono)
        accumulateMonoMix(inst->aux1buf, chan, nsamples, a1gain);
        accumulateMonoMix(inst->aux2buf, chan, nsamples, a2gain);

        // AuxA/B send (stereo)
        accumulate(inst->auxabuf, chan, nsamples, aasnd);
        accumulate(inst->auxbbuf, chan, nsamples, absnd);

        // Channel buffer to mix buffer (stereo)
        accumulate(inst->mixbuf, chan, nsamples, chgain);

        DEBUG_PLOT_STEREO(this, chan, nsamples);
    }

private:
    void accumulate(StereoSample *dest, const StereoSample *src, int nsamples, float gain)
    {
        if (gain == 0.0f)
            return;

        for (int i = 0; i < nsamples; i++)
        {
            dest[i].l += gain * src[i].l;
            dest[i].r += gain * src[i].r;
        }
    }

    void accumulateMonoMix(float *dest, const StereoSample *src, int nsamples, float gain)
    {
        if (gain == 0.0f)
            return;

        for (int i = 0; i < nsamples; i++)
            dest[i] += gain * (src[i].l + src[i].r);
    }
};

// --------------------------------------------------------------------------
// Sound definitions
// --------------------------------------------------------------------------

struct V2Mod
{
    uint8_t source;   // source: vel/ctl1-7/aenv/env2/lfo1/lfo2
    uint8_t val;      // 0=-1 .. 128=1
    uint8_t dest;     // destination (index into V2Sound)
};

struct V2Sound
{
    uint8_t voice[sizeof(syVV2) / sizeof(float)];
    uint8_t chan[sizeof(syVChan) / sizeof(float)];
    uint8_t maxpoly;
    uint8_t modnum;
    V2Mod modmatrix[1]; // actually modnum entries!
};

union V2PatchMap
{
    uint32_t offsets[128];    // offsets into raw_data[]
    uint8_t raw_data[1];      // variable size
};

// --------------------------------------------------------------------------
// Ronan
// --------------------------------------------------------------------------

struct syWRonan
{
    uint8_t mem[64*1024]; // "that should be enough" --synth.asm. :)
};

#ifdef RONAN

extern "C"
{
void ronanCBInit(syWRonan *pthis);
void ronanCBTick(syWRonan *pthis);
void ronanCBNoteOn(syWRonan *pthis);
void ronanCBNoteOff(syWRonan *pthis);
void ronanCBSetCtl(syWRonan *pthis, uint32_t ctl, uint32_t val);
void ronanCBProcess(syWRonan *pthis, float *buf, uint32_t len);
void ronanCBSetSR(syWRonan *pthis, int samplerate);
}

#else

static inline void ronanCBInit(syWRonan *) {}
static inline void ronanCBTick(syWRonan *) {}
static inline void ronanCBNoteOn(syWRonan *) {}
static inline void ronanCBNoteOff(syWRonan *) {}
static inline void ronanCBSetCtl(syWRonan *, uint32_t, uint32_t) {}
static inline void ronanCBProcess(syWRonan *, float *, uint32_t) {}
static inline void ronanCBSetSR(syWRonan *, int) {}

extern "C" void synthSetLyrics(void *, const char **) {}

#endif

// --------------------------------------------------------------------------
// Synth
// --------------------------------------------------------------------------
#ifdef EMSCRIPTEN
extern uint32_t readUintAt(const uint8_t *buf, int idx);
#endif

struct V2ChanInfo
{
    uint8_t pgm;    // program
    uint8_t ctl[7]; // controllers
};

// V2Synth holds a V2Instance.
// In the original code these are one and the same struct (SYN) but that
// would turn out fairly awkward in this C++ version, hence the split.
struct V2Synth
{
    static const int POLY  = 64;
    static const int CHANS = 16;

    const V2PatchMap *patchmap;
    uint32_t mrstat;          // running status in MIDI decoding
    uint32_t curalloc;
    int samplerate;
    int chanmap[POLY];   // voice -> chan
    uint32_t allocpos[POLY];
    int voicemap[CHANS]; // chan -> choice
    int tickd;           // number of finished samples left in mix buffer

    V2ChanInfo chans[CHANS];
    syVV2 voicesv[POLY];
    V2Voice voicesw[POLY];
    syVChan chansv[CHANS];
    V2Chan chansw[CHANS];

    struct Globals
    {
        syVReverb rvbparm;
        syVModDel delparm;
        float vlowcut;
        float vhighcut;
        syVComp cprparm;
        uint8_t guicolor;
        uint8_t _pad[3];
    } globals;

    V2Reverb reverb;
    V2ModDel delay;
    V2DCFilter dcf;
    V2Comp compr;
    float lcfreq;    // low cut freq
    float lcbuf[2];  // low cut buf l/r
    float hcfreq;    // high cut freq
    float hcbuf[2];  // high cut buf l/r

    bool initialized;

    // delay buffers
    float maindelbuf[2][32768];
    float chandelbuf[CHANS][2][2048];

    V2Instance instance;

    syWRonan ronan;

    void init(const void *patchmap, int samplerate)
    {
        // Ahem, so this is somewhat dubious, but we don't use
        // virtual functions or anything so it should be fine. Ahem.
        // Look away please :)
        memset(this, 0, sizeof(*this));

        // set sampling rate
        this->samplerate = samplerate;
        instance.calcNewSampleRate(samplerate);
        ronanCBSetSR(&ronan, samplerate);

        // patch map
        this->patchmap = (const V2PatchMap*)patchmap;	// this may be unaligned shit

        // init voices
        for (int i = 0; i < POLY; i++)
        {
            chanmap[i] = -1;
            voicesw[i].init(&instance);
        }

        // init channels
        for (int i = 0; i < CHANS; i++)
        {
            chans[i].ctl[6] = 0x7f;
            chansw[i].init(&instance, chandelbuf[i][0], chandelbuf[i][1], COUNTOF(chandelbuf[i][0]));
        }

        // global filters
        reverb.init(&instance);
        delay.init(&instance, maindelbuf[0], maindelbuf[1], COUNTOF(maindelbuf[0]));
        ronanCBInit(&ronan);
        compr.init(&instance);
        dcf.init(&instance);

#ifdef EMSCRIPTEN
		int buf_size= 1024; // keep in sync with output audio buffer  (see SAMPLE_BUF_SIZE in apadper.cpp!)
		// just copy the output of the different channels
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[0], 0), "channel 0", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[1], 0), "channel 1", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[2], 0), "channel 2", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[3], 0), "channel 3", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[4], 0), "channel 4", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[5], 0), "channel 5", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[6], 0), "channel 6", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[7], 0), "channel 7", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[8], 0), "channel 8", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[9], 0), "channel 9", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[10], 0), "channel 10", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[11], 0), "channel 11", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[12], 0), "channel 12", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[13], 0), "channel 13", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[14], 0), "channel 14", buf_size, 0, 0);
		DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[15], 0), "channel 15", buf_size, 0, 0);
#endif

        // debug plots (uncomment the ones you want)
        //int sr_plot = 44100/10; // plot rate
        //int sr_lfo = 800;
        //int w = 800, h = 150;

        //DEBUG_PLOT_OPEN(&voicesw[1].osc[0], "Voice 1 VCO 0", sr_plot, w, h);	// oscillator - 3 per voice
        //DEBUG_PLOT_OPEN(&voicesw[1].osc[1], "Voice 1 VCO 1", sr_plot, w, h);
        //DEBUG_PLOT_OPEN(&voicesw[1].vcf[0], "Voice 1 VCF 0", sr_plot, w, h);
        //DEBUG_PLOT_OPEN(&voicesw[1].env[0], "Voice 1 Env 0", sr_lfo, w, h);	// envelope - 2 per voice
        //DEBUG_PLOT_OPEN(&voicesw[1].lfo[0], "Voice 1 LFO 0", sr_lfo, w, h);
        //DEBUG_PLOT_OPEN(&voicesw[1].dist, "Voice 1 Dist", sr_plot, w, h);
        //DEBUG_PLOT_OPEN(&voicesw[1], "Voice 1 final", sr_plot, w, h);			// voice output
        //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[0].dcf1, 0), "Chan 0 DCF1 L", sr_plot, w, h);
        //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[0].dcf1, 1), "Chan 0 DCF1 R", sr_plot, w, h);
        //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[0], 0), "Channel 0 L", sr_plot, w, h);
        //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&chansw[0], 1), "Channel 0 R", sr_plot, w, h);
        //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&instance.mixbuf, 0), "Mix L", sr_plot, w, h);
        //DEBUG_PLOT_OPEN(DEBUG_PLOT_CHAN(&instance.mixbuf, 1), "Mix R", sr_plot, w, h);

        initialized = true;
    }

    void render(float *buf, int nsamples, float *buf2, bool add)
    {
        int todo = nsamples;

        // fragment loop - chunk everything into frames.
        while (todo)
        {
            // do we need to render a new frame?
            if (!tickd)
                tick();

            // copy to dest buffer(s)
            const StereoSample *src = &instance.mixbuf[instance.SRcFrameSize - tickd];
            int nread = min(todo, tickd);
            if (!buf2) // interleaved samples
            {
                if (!add)
                {
                    COVER("OUT interleaved set");
                    memcpy(buf, src, nread * sizeof(StereoSample));
                } else
                {
                    COVER("OUT interleaved add");
                    for (int i = 0; i < nread; i++)
                    {
                        buf[i*2 + 0] += src[i].l;
                        buf[i*2 + 1] += src[i].r;
                    }
                }
                buf += 2*nread;
            }
            else // buf = left, buf2 = right
            {
                if (!add)
                {
                    COVER("OUT separate set");
                    for (int i=0; i < nread; i++)
                    {
                        buf[i]  = src[i].l;
                        buf2[i] = src[i].r;
                    }
                }
                else
                {
                    COVER("OUT separate add");
                    for (int i = 0; i < nread; i++)
                    {
                        buf[i]  += src[i].l;
                        buf2[i] += src[i].r;
                    }
                }

                buf  += nread;
                buf2 += nread;
            }

            todo  -= nread;
            tickd -= nread;
        }

        DEBUG_PLOT_UPDATE();
    }

    void processMIDI(const uint8_t *cmd)
    {
        while (*cmd != 0xfd) // until end of stream
        {
            if (*cmd & 0x80) // start of message
                mrstat = *cmd++;

            if (mrstat < 0x80) // we don't have a current message? uhm...
                break;

            int chan = mrstat & 0xf;
            switch ((mrstat >> 4) & 7)
            {
            case 1: // Note on
                if (cmd[1] != 0) // velocity==0 is actually a note off
                {
                    COVER("MIDI note on");
                    if (chan == CHANS - 1)
                        ronanCBNoteOn(&ronan);

                    // calculate current polyphony for this channel
                    const V2Sound *sound = getpatch(chans[chan].pgm);
                    int npoly = 0;
                    for (int i = 0; i < POLY; i++)
                        npoly += (chanmap[i] == chan);

                    // voice allocation. this is equivalent to the original V2 code,
                    // but hopefully simpler to follow.
                    int usevoice = -1;
                    int chanmask, chanfind;

                    if (!npoly || npoly < sound->maxpoly) // even if maxpoly is 0, allow at least 1.
                    {
                        // if we haven't reached polyphony limit yet, try to find a free voice
                        // first.
                        for (int i=0; i < POLY; i++)
                        {
                            if (chanmap[i] < 0)
                            {
                                usevoice = i;
                                break;
                            }
                        }

                        // okay, need to find a free voice. we'll take any channel.
                        COVER("SYN find voice any");
                        chanmask = 0;
                        chanfind = 0;
                    } else
                    {
                        // if we're at polyphony limit, we know there's at least one voice
                        // used by this channel, so we can limit ourselves to killing
                        // voices from our own chan.
                        COVER("SYN find voice channel");
                        chanmask = 0xf;
                        chanfind = chan;
                    }

                    // don't have a voice yet? kill oldest eligible one with gate off.
                    if (usevoice < 0)
                    {
                        COVER("SYN replace voice gate off");
                        uint32_t oldest = curalloc;
                        for (int i = 0; i < POLY; i++)
                        {
                            if ((chanmap[i] & chanmask) == chanfind && !voicesw[i].gate && allocpos[i] < oldest)
                            {
                                oldest = allocpos[i];
                                usevoice = i;
                            }
                        }
                    }

                    // still no voice? okay, just take the oldest one we can find, period.
                    if (usevoice < 0)
                    {
                        COVER("SYN replace voice oldest");
                        uint32_t oldest = curalloc;
                        for (int i = 0; i < POLY; i++)
                        {
                            if ((chanmap[i] & chanmask) == chanfind && allocpos[i] < oldest)
                            {
                                oldest = allocpos[i];
                                usevoice = i;
                            }
                        }
                    }

                    // we have our voice - assign it!
                    assert(usevoice >= 0);
                    chanmap[usevoice] = chan;
                    voicemap[chan] = usevoice;
                    allocpos[usevoice] = curalloc++;

                    // and note on!
                    storeV2Values(usevoice);
                    voicesw[usevoice].noteOn(cmd[0], cmd[1]);
                    cmd += 2;
                    break;
                }
                // fall-through (for when we had a note off)

            case 0: // Note off
                COVER("MIDI note off");
                if (chan == CHANS - 1)
                    ronanCBNoteOff(&ronan);

                for (int i = 0; i < POLY; i++)
                {
                    if (chanmap[i] != chan)
                        continue;
                    V2Voice *voice = &voicesw[i];
                    if (voice->note == cmd[0] && voice->gate)
                        voice->noteOff();
                }
                cmd += 2;
                break;

            case 2: // Aftertouch
                COVER("MIDI aftertouch");
                cmd++; // ignored
                break;

            case 3: // Control change
            {
                COVER("MIDI controller change");
                int ctrl = cmd[0];
                uint8_t val = cmd[1];
                if (ctrl >= 1 && ctrl <= 7)
                {
                    chans[chan].ctl[ctrl - 1] = val;
                    if (chan == CHANS-1)
                        ronanCBSetCtl(&ronan, ctrl, val);
                }
                else if (ctrl == 120) // CC #120: all sound off
                {
                    COVER("MIDI CC all sound off");
                    for (int i=0; i < POLY; i++)
                    {
                        if (chanmap[i] != chan)
                            continue;

                        voicesw[i].init(&instance);
                        chanmap[i] = -1;
                    }
                }
                else if (ctrl == 123) // CC #123: all notes off
                {
                    COVER("MIDI CC all notes off");
                    if (chan == CHANS-1)
                        ronanCBNoteOff(&ronan);

                    for (int i=0; i < POLY; i++)
                    {
                        if (chanmap[i] == chan)
                            voicesw[i].noteOff();
                    }
                }
                cmd += 2;
                break;
            }

            case 4: // Program change
            {
                COVER("MIDI program change");
                uint8_t pgm = *cmd++ & 0x7f;
                // did the program actually change?
                if (chans[chan].pgm != pgm)
                {
                    COVER("MIDI program change real");
                    chans[chan].pgm = pgm;

                    // need to turn all voices on this channel off.
                    for (int i=0; i < POLY; i++)
                    {
                        if (chanmap[i] == chan)
                            chanmap[i] = -1;
                    }
                }

                // either way, reset controllers
                for (int i=0; i < 6; i++)
                    chans[chan].ctl[i] = 0;
                break;
            }

            case 5: // Pitch bend
                COVER("MIDI pitch bend");
                cmd += 2; // ignored
                break;

            case 6: // Poly Aftertouch
                COVER("MIDI poly aftertouch");
                cmd += 2; // ignored
                break;

            case 7: // System
                COVER("MIDI system exclusive");
                if (chan == 0xf) // Reset
                    init(patchmap, samplerate);
                break; // rest ignored
            }
        }
    }

    void setGlobals(const uint8_t *para)
    {
        if (!initialized)
            return;

        // copy over
        float *globf = (float *)&globals;		// note: alignment Ok.. structure exclusively made of floats (with some padding at the end)
        for (int i = 0; i < (int)(sizeof(globals)/sizeof(float)); i++)
            globf[i] = para[i];

        // set
        reverb.set(&globals.rvbparm);
        delay.set(&globals.delparm);
        compr.set(&globals.cprparm);
        lcfreq = sqr((globals.vlowcut + 1.0f) / 128.0f);
        hcfreq = sqr((globals.vhighcut + 1.0f) / 128.0f);
    }

    void getPoly(int *dest)	// seems to be unused here (see synthGetPoly)
    {
        for (int i = 0; i <= CHANS; i++)
            dest[i] = 0;

        for (int i = 0; i < POLY; i++)
        {
            int chan = chanmap[i];
            if (chan < 0)
                continue;

            dest[chan]++;
            dest[CHANS]++;
        }
    }

    void getPgm(int *dest)	// seems to be unused here (see synthGetPgm)
    {
        for (int i = 0; i < CHANS; i++)
            dest[i] = chans[i].pgm;
    }

private:
    const V2Sound *getpatch(int pgm) const
    {
        assert(pgm >= 0 && pgm < 128);
        return (const V2Sound *)&patchmap->raw_data[readUintAt((uint8_t*)patchmap->offsets, pgm)];
    }

    float getmodsource(const V2Voice *voice, int chan, int source) const
    {
        float in = 0.0f;

        switch (source)
        {
        case 0: // velocity
            COVER("MOD src vel");
            in = voice->velo;
            break;

        case 1: case 2: case 3: case 4: case 5: case 6: case 7: // controller value
            COVER("MOD src ctl");
            in = chans[chan].ctl[source-1];
            break;

        case 8: case 9: // EG output
            COVER("MOD src EG");
            in = voice->env[source-8].out;
            break;

        case 10: case 11: // LFO output
            COVER("MOD src LFO");
            in = voice->lfo[source-10].out;
            break;

        default: // note
            COVER("MOD src note");
            in = 2.0f * (voice->note - 48.0f);
            break;
        }

        return in;
    }

    void storeV2Values(int vind)
    {
        assert(vind >= 0 && vind < POLY);
        int chan = chanmap[vind];
        if (chan < 0)
            return;

        // get patch definition
        const V2Sound *patch = getpatch(chans[chan].pgm);

        // voice data
        syVV2 *vpara = &voicesv[vind];
        float *vparaf = (float *)vpara;
        V2Voice *voice = &voicesw[vind];

        // copy voice dependent data
        for (int i = 0; i < COUNTOF(patch->voice); i++)
            vparaf[i] = (float)patch->voice[i];

        // modulation matrix
        for (int i = 0; i < patch->modnum; i++)
        {
            const V2Mod *mod = &patch->modmatrix[i];
            if (mod->dest >= COUNTOF(patch->voice))
                continue;

            float scale = (mod->val - 64.0f) / 64.0f;
            vparaf[mod->dest] = clamp(vparaf[mod->dest] + scale*getmodsource(voice, chan, mod->source), 0.0f, 128.0f);
        }

        voice->set(vpara);
    }

    void storeChanValues(int chan)
    {
        assert(chan >= 0 && chan < CHANS);

        // get patch definition
        const V2Sound *patch = getpatch(chans[chan].pgm);

        // chan data
        syVChan *cpara = &chansv[chan];
        float *cparaf = (float *)cpara;
        V2Chan *cwork = &chansw[chan];
        V2Voice *voice = &voicesw[voicemap[chan]];

        // copy channel dependent data
        for (int i = 0; i < COUNTOF(patch->chan); i++)
            cparaf[i] = (float)patch->chan[i];

        // modulation matrix
        for (int i = 0; i < patch->modnum; i++)
        {
            const V2Mod *mod = &patch->modmatrix[i];
            int dest = mod->dest - COUNTOF(patch->voice);
            if (dest < 0 || dest >= COUNTOF(patch->chan))
                continue;

            float scale = (mod->val - 64.0f) / 64.0f;
            cparaf[dest] = clamp(cparaf[dest] + scale*getmodsource(voice, chan, mod->source), 0.0f, 128.0f);
        }

        cwork->set(cpara);
    }

    void tick()
    {
        // voices
        for (int i = 0; i < POLY; i++)
        {
            if (chanmap[i] < 0)
                continue;

            storeV2Values(i);
            voicesw[i].tick();

            // if EG1 finished, turn off voice
            if (voicesw[i].env[0].state == V2Env::OFF)
                chanmap[i] = -1;
        }

        // chans
        for (int i = 0; i < CHANS; i++)
            storeChanValues(i);

        ronanCBTick(&ronan);
        tickd = instance.SRcFrameSize;
        renderFrame();

#if COVERAGE
        // print coverage updates as they happen
        static int cur_frame;
        static const char *old_coverage[COUNTOF(code_coverage)];
        int ccount = synthGetNumCoverage();
        for (int i = 0; i < ccount; i++)
        {
            if (old_coverage[i] != code_coverage[i])
            {
                old_coverage[i] = code_coverage[i];
                printf("[%5d,%3d] %s\n", cur_frame, i, code_coverage[i]);
            }
        }
        cur_frame++;
#endif
    }

    void renderFrame()
    {
        int nsamples = instance.SRcFrameSize;

        // clear output buffer
        memset(instance.mixbuf, 0, nsamples * sizeof(StereoSample));

        // clear aux buffers
        memset(instance.aux1buf, 0, nsamples * sizeof(float));
        memset(instance.aux2buf, 0, nsamples * sizeof(float));
        memset(instance.auxabuf, 0, nsamples * sizeof(StereoSample));
        memset(instance.auxbbuf, 0, nsamples * sizeof(StereoSample));

        // process all channels
        for (int chan = 0; chan < CHANS; chan++)
        {
            // check if any voices are active on this channel
            int voice = 0;
            while (voice < POLY && chanmap[voice] != chan)
                voice++;

            if (voice == POLY)
                continue;

            // clear channel buffer
            memset(instance.chanbuf, 0, nsamples * sizeof(StereoSample));

            // render all voices on this channel
            for (; voice < POLY; voice++)
            {
                if (chanmap[voice] != chan)
                    continue;

                voicesw[voice].render(instance.chanbuf, nsamples);
            }

            // channel 15 -> Ronan
            if (chan == CHANS - 1)
                ronanCBProcess(&ronan, &instance.chanbuf[0].l, nsamples);

            chansw[chan].process(nsamples);
        }

        // global filters
        StereoSample *mix = instance.mixbuf;
        reverb.render(mix, nsamples);
        delay.renderAux2Main(mix, nsamples);
        dcf.renderStereo(mix, mix, nsamples);

        // low cut/high cut
        float lcf = lcfreq, hcf = hcfreq;
        for (int i = 0; i < nsamples; i++)
        {
            for (int ch = 0; ch < 2; ch++)
            {
                // low cut
                float x = mix[i].ch[ch] - lcbuf[ch];
                lcbuf[ch] += lcf * x;

                // high cut
                if (hcf != 1.0f)
                {
                    hcbuf[ch] += hcf * (x - hcbuf[ch]);
                    x = hcbuf[ch];
                }

                mix[i].ch[ch] = x;
            }
        }

        // sum compressor
        compr.render(mix, nsamples);

        DEBUG_PLOT_STEREO(mix, mix, nsamples);
    }
};

// --------------------------------------------------------------------------
// C-style interface
// --------------------------------------------------------------------------

#ifdef EMSCRIPTEN
int *getVoiceMap(void *pthis)
{
    return ((V2Synth *)pthis)->voicemap;
}
#endif

unsigned int synthGetSize()
{
    return sizeof(V2Synth);
}

void synthInit(void *pthis, const void *patchmap, int samplerate)
{
    ((V2Synth *)pthis)->init(patchmap, samplerate);
}

void synthRender(void *pthis, void *buf, int smp, void *buf2, int add)
{
    ((V2Synth *)pthis)->render((float *)buf, smp, (float *)buf2, add != 0);
}

void synthProcessMIDI(void *pthis, const void *ptr)
{
    ((V2Synth *)pthis)->processMIDI((const uint8_t *)ptr);
}

void synthSetGlobals(void *pthis, const void *ptr)
{
    ((V2Synth *)pthis)->setGlobals((const uint8_t *)ptr);
}

void synthGetPoly(void *pthis, void *dest)
{
    ((V2Synth *)pthis)->getPoly((int*)dest);
}

void synthGetPgm(void *pthis, void *dest)
{
    ((V2Synth *)pthis)->getPgm((int*)dest);
}

void synthSetVUMode(void *, int)
{
}

void synthGetChannelVU(void *, int, float *, float *)
{
}

void synthGetMainVU(void *, float *, float *)
{
}

long synthGetFrameSize(void *pthis)
{
    return ((V2Synth *)pthis)->instance.SRcFrameSize;
}

extern "C" void *synthGetSpeechMem(void *pthis)
{
    return &((V2Synth *)pthis)->ronan;
}

#if COVERAGE
void synthPrintCoverage()
{
    int end = synthGetNumCoverage();
    printf("synth coverage:\n");
    for (int i = 0; i < end; i++)
        printf("[%3d] %s\n", i, code_coverage[i] ? code_coverage[i] : "<not hit>");
}

static int synthGetNumCoverage()
{
    return __COUNTER__;
}
#endif

// vim: sw=2:sts=2:et:cino=\:0l1g0(0
