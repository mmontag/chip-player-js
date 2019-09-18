
#include "types.h"
#include "v2mconv.h"
#include "sounddef.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

#define printf2(str, ...)

extern const char * const v2mconv_errors[] =
{
    "no error",
    "V2M file contains no patch data! (forgot program changes?)",
    "V2M file was made with unknown synth version!",
};

// nicht drüber nachdenken.
static struct _ssbase
{
    const uint8_t   *patchmap;
    const uint8_t   *globals;
    uint32_t    timediv;
    uint32_t    timediv2;
    uint32_t    maxtime;
    const uint8_t   *gptr;
    uint32_t  gdnum;
    struct _basech
    {
        uint32_t    notenum;
        const uint8_t      *noteptr;
        uint32_t    pcnum;
        const uint8_t      *pcptr;
        uint32_t    pbnum;
        const uint8_t      *pbptr;
        struct _bcctl
        {
            uint32_t       ccnum;
            const uint8_t  *ccptr;
        } ctl[7];
    } chan[16];

    const uint8_t *mididata;
    int midisize;
    int patchsize;
    int globsize;
    int maxp;
    const uint8_t *newpatchmap;

    const uint8_t *speechdata;
    int spsize;

} base;

static void readfile(const unsigned char *inptr, const int inlen)
{
    const uint8_t *d = inptr;
    memset(&base, 0, sizeof(base));

    base.timediv  = (*((uint32_t*)(d)));
    base.timediv2 = 10000*base.timediv;
    base.maxtime  = *((uint32_t*)(d + 4));
    base.gdnum    = *((uint32_t*)(d + 8));
    d += 12;
    base.gptr = d;
    d += 10*base.gdnum;
    for (int ch = 0; ch < 16; ch++)
    {
        _ssbase::_basech &c = base.chan[ch];
        c.notenum = *((uint32_t*)d);
        d += 4;
        if (c.notenum)
        {
            c.noteptr = d;
            d += 5*c.notenum;
            c.pcnum = *((uint32_t*)d);
            d += 4;
            c.pcptr = d;
            d += 4*c.pcnum;
            c.pbnum = *((uint32_t*)d);
            d += 4;
            c.pbptr = d;
            d += 5*c.pbnum;
            for (int cn = 0; cn < 7; cn++)
            {
                _ssbase::_basech::_bcctl &cc = c.ctl[cn];
                cc.ccnum = *((uint32_t*)d);
                d += 4;
                cc.ccptr = d;
                d += 4*cc.ccnum;
            }
        }
    }
    base.midisize = d - inptr;
    base.globsize = *((uint32_t*)d);
    if (base.globsize < 0 || base.globsize > 131072) return;
    d += 4;
    base.globals = d;
    d += base.globsize;
    base.patchsize = *((uint32_t*)d);
    if (base.patchsize < 0 || base.patchsize > 1048576) return;
    d += 4;
    base.patchmap = d;
    d += base.patchsize;

    if (d - inptr < inlen)
    {
        base.spsize = *((uint32_t*)d);
        d += 4;
        base.speechdata = d;
        d += base.spsize;

        // small sanity check
        if (base.spsize < 0 || base.spsize > 8192 || (d - inptr) > inlen)
        {
            base.spsize = 0;
            base.speechdata = 0;
        }
    } else
    {
        base.spsize = 0;
        base.speechdata = 0;
    }

    printf2("after read: est %d, is %d\n", inlen, d - inptr);
    printf2("midisize: %d, globs: %d, patches: %d\n", base.midisize, base.globsize, base.patchsize);
}

int patchesused[128];

// gives version deltas
int CheckV2MVersion(const unsigned char *inptr, const int inlen)
{
    int i;
    readfile(inptr,inlen);

    if (!base.patchsize)
        return -1;

    // determine highest used patch from progchange commands
    // (midiconv remaps the patches so that they're a
    //  continuous block from 0 to x)
    memset(patchesused, 0, sizeof(patchesused));
    base.maxp = 0;


    for (int ch = 0; ch < 16; ch++)
    {
        unsigned char p = 0;
        unsigned char v = 0;

        //int pcp = 0;
        int pct = 0;
        int pcn = base.chan[ch].pcnum;
        int np = 0;
        int nt = 0;
        int nn = base.chan[ch].notenum;

        int td;
        const uint8_t *ptr;
        for (int pcp = 0; pcp < pcn; pcp++)
        {
            // have there been notes? add last pgm
            ptr = base.chan[ch].pcptr;
            td  = 0x10000*ptr[2*pcn + pcp] + 0x100*ptr[1*pcn + pcp] + ptr[0*pcn + pcp];
            pct += td;
            if (pct > nt)
            {
                patchesused[p] = 1;
                if (p >= base.maxp) base.maxp = p + 1;
            }
            p += ptr[3*pcn + pcp];

            // walk notes until we reach current cmd
            while (np < nn && nt <= pct)
            {
                ptr = base.chan[ch].noteptr;
                td  = 0x10000*ptr[2*nn + np] + 0x100*ptr[1*nn + np] + ptr[0*nn + np];
                v  += ptr[4*nn+np];
                nt += td;
                np++;
            }
        }

        if (np < nn)
        {
            patchesused[p] = 1;
            if (p >= base.maxp) base.maxp = p + 1;
        }

    }
    printf2("patches used: %d\n", base.maxp);

    if (!base.maxp)
        return -1;

    // offset table to ptaches
    int *poffsets = (int *)base.patchmap;

    int matches = 0, best = -1;
    // for each version check...
    for (i = 0; i <= v2version; i++)
    {
        // ... if globals size is correct...
        if (base.globsize == v2gsizes[i])
        {
            printf2("globsize match: %d\n",i);
            // ... and if the size of all patches makes sense
            int p;
            for (p = 0; p < base.maxp - 1; p++)
            {
                int d = (poffsets[p + 1] - poffsets[p]) - (v2vsizes[i] - 3*255);
                if (d % 3) break;
                d /= 3;
                if (d != base.patchmap[poffsets[p] + v2vsizes[i] - 3*255 - 1])
                    break;
            }

            if (p == base.maxp - 1)
            {
                printf2("... ok!\n");
                best = i;
                matches++;
            } else
            {
                printf2("no match!\n");
            }
        }
    }

    // if we've got exactly one match, we're quite sure
    int ret = (matches >= 1) ? v2version - best : -2;
    printf2("found version delta: %d\n", ret);
    return ret;
}

//Klicke den Narren über Dir und das Schauspiel wird beginnen...
/*static int transEnv(int enVal)
{
    float dv = (float)(1.0f - pow(2.0f, -enVal/128.0f*11.0f));
    dv = (float)sqrt(dv); // square root to correct value
    int nEnVal = (int)(-log(1.0f - dv)/log(2.0f)*128.0f/11.0f);
    if (nEnVal < 0)
    {
        printf2("!!clamping enval lower bound!\n");
        nEnVal = 0;
    } else if (nEnVal > 127)
    {
        printf2("!!clamping enval upper bound!\n");
        nEnVal = 127;
    }
    return nEnVal;
}*/

void ConvertV2M(const unsigned char *inptr, const int inlen, unsigned char **outptr, int *outlen)
{
    int i, p;
    // check version
    int vdelta = CheckV2MVersion(inptr, inlen);
    if (!vdelta) // if same, simply clone
    {
        *outptr = new uint8_t[inlen + 4];
        memset(*outptr, 0, inlen + 4);
        *outlen = inlen + 4;
        memcpy(*outptr, inptr, inlen);
        return;
    }
    else if (vdelta < 0) // if invalid...
    {
        *outptr = 0;
        *outlen = 0;
        return;
    }
    vdelta = v2version - vdelta;

    // fake base.maxp
    int maxp2 = ((int*)base.patchmap)[0]/4;
    if (maxp2 != base.maxp)
    {
        printf2("warning: patch count inconsistency: we:%d, they:%d\n", base.maxp, maxp2);
        base.maxp = maxp2;
    }

    // calc new size
    int gdiff = v2gsizes[v2version] - v2gsizes[vdelta];
    int pdiff = v2vsizes[v2version] - v2vsizes[vdelta];
    int newsize = inlen + gdiff + base.maxp*pdiff;
    printf2("old size: %d, new size: %d\n", inlen, newsize);

    // init new v2m
    *outlen = newsize + 4;
    uint8_t *newptr = *outptr = new uint8_t[newsize + 4];
    memset(newptr, 0, newsize + 4);

    // copy midi data
    memcpy(newptr, inptr, base.midisize);

    // new globals length...
    newptr += base.midisize;
    *(int *)newptr = v2ngparms;
    printf2("glob size: old %d, new %d\n", base.globsize, *(int *)newptr);
    newptr += 4;

    // copy/remap globals
    memcpy(newptr, v2initglobs, v2ngparms);
    const uint8_t *oldgptr = base.globals;
    for (i = 0; i < v2ngparms; i++)
    {
        if (v2gparms[i].version <= vdelta)
            newptr[i] = *oldgptr++;
    }
    newptr += v2ngparms;

    // new patch data length
    *(int *)newptr = base.patchsize + base.maxp*pdiff;
    printf2("patch size: old %d, new %d\n", base.patchsize, *(int *)newptr);
    newptr += 4;

    base.newpatchmap = newptr;

    int *poffsets = (int *)base.patchmap;
    int *noffsets = (int *)newptr;
    //const int ros = v2vsizes[vdelta] - 255*3 - 1;

    //uint8_t *nptr2 = newptr;

    // copy patch table...
    // gcc 5.3 seems to vectorize the loop, but
    // the loop is not correct for base.maxp == 10 at least
    // adding volatile to the iterator breaks the auto-vec
    for (volatile int q = 0; q < base.maxp; q++)
    {
        noffsets[q] = poffsets[q] + (q * pdiff);
    }

    newptr += 4*base.maxp;

    // for each patch...
    for (p = 0; p < base.maxp; p++)
    {
        const uint8_t *src = base.patchmap + poffsets[p];

        //const uint8_t *dest_soll = nptr2 + noffsets[p];
        printf2("p%d ist:%08x soll:%08x\n", p, newptr, dest_soll);

        // fill patch with default values
        memcpy(newptr, v2initsnd, v2nparms);

        // copy/remap patch data
        for (i = 0; i < v2nparms; i++)
        {
            if (v2parms[i].version <= vdelta)
            {
                *newptr = *src++;
                /*
                if (vdelta < 2 && (i == 33 || i == 36 || i == 39 || i == 42)) // fix envelopes
                    *newptr=transEnv(*newptr); */
            }
            newptr++;
        }

        // copy mod number
        const int modnum = *newptr ++= *src++;
        //        printf2("patch %d: %d mods\n",p,modnum);

        // copy/remap modulations
        for (i = 0; i < modnum; i++)
        {
            newptr[0] = src[0];
            newptr[1] = src[1];
            newptr[2] = src[2];
            for (int k = 0; k <= newptr[2]; k++)
                if (v2parms[k].version > vdelta) newptr[2]++;
            newptr += 3;
            src += 3;
        }
    }

    // copy speech
    *(uint32_t*)newptr = base.spsize;
    newptr += 4;
    memcpy(newptr, base.speechdata, base.spsize);
    newptr += base.spsize;

    printf2("est size: %d, real size: %d\n", newsize, newptr - *outptr);
}

unsigned long GetV2MPatchData(const unsigned char *inptr, const int inlen,
    unsigned char **outptr, const unsigned char **patchmap)
{
    int outlen;
    ConvertV2M(inptr, inlen, outptr, &outlen);

    const uint8_t *pm = base.newpatchmap;
    if (!pm) pm = base.patchmap;

    int *poffsets = (int*)pm;
    for (int i = 0; i < base.maxp; i++)
    {
        patchmap[i] = pm + poffsets[i];
    }
    return base.maxp;
}
