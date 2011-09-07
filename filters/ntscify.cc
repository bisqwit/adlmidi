#include <stdio.h>
#include <string.h>
#include <vector>

#include "ntsc/snes_ntsc.h"
#include <assert.h>
#include "lanczos.hh"

static const unsigned X = 512;
static const unsigned Y = 360;

static const unsigned OutX = SNES_NTSC_OUT_WIDTH((X/2));
static const unsigned OutY = Y;//OutX*400/720;

SNES_NTSC_IN_T Buf[X*Y];
std::vector<Triplet<unsigned char,4> > OutBuf;
/*std::vector<Triplet<unsigned char,3> > ResultBuf;
std::vector<Triplet<LanczosFloat,4> > TmpBuf1, TmpBuf2;*/

snes_ntsc_t ntsc;

int main(int argc, char** argv)
{
    OutBuf.resize(OutX*Y);
    /*ResultBuf.resize(OutX*OutY);*/

    //const int bytes_per_outpix = 4;
    const int bytes_per_outpix = 2;

    fprintf(stderr, "Out: %ux%u\n", OutX,OutY);
    int phase=0;
    unsigned frames=0;
    for(;;)
    {
        static snes_ntsc_t ntsc;
        snes_ntsc_setup_t setup = snes_ntsc_composite;
        double satpos = sin(frames/50.0)*cos(frames*sin(frames/10.0));
        setup.saturation = 0.4*(1.0 - std::pow(1.0 - (+0.5 * 0.4*satpos), 0.3));
        setup.merge_fields = 0;
        setup.bleed        = 1;//sin(frames/60.0)*cos(0.2+frames/140.0)*sin(1.1+frames/10.0)*2+0.3;
        setup.fringing     = 1;//cos(frames*6/60.0+2)*0.6+0.5;
        setup.hue          = 0.02*sin(frames/14.0 + cos(satpos));
        snes_ntsc_init(&ntsc, &setup);

       {char*  p = (char*)& Buf;
        size_t n = sizeof(Buf);
        fprintf(stderr, "Reads %zu\n", n);
        while(n > 0)
        {
            int r = fread(p, 1, n, stdin);
            if(r <= 0) return 0;
            p += r;
            n -= r;
        }}
        phase=phase^1;

#if 1
        snes_ntsc_blit_hires(&ntsc,
            &Buf[0],
            X, // in row width
            phase, // phase
            X,Y, // in dimensions
            &OutBuf[0],
            OutX * bytes_per_outpix // out pitch
        );

        //TmpBuf1.assign(OutBuf.begin(), OutBuf.end());

        /*
        LanczosScale(OutX,Y,  OutX,OutY, OutBuf, TmpBuf2);

        #pragma omp parallel for
        for(size_t a=0; a< OutX*OutY; ++a)
            for(unsigned b=0; b<3; ++b)
            {
                int v = TmpBuf2[a][b];
                if(v < 0) v = 0; if(v > 255) v = 255;
                ResultBuf[a][b] = v;
            }

        fwrite(&ResultBuf[0], 1, ResultBuf.size()*3, stdout);
        */
#endif

       {const char* p = (const char*) &OutBuf[0];
        size_t      n = OutBuf.size() * bytes_per_outpix;
        fprintf(stderr, "Writes %zu\n", n);

        //memcpy(&OutBuf[0], Buf, n);

        while(n > 0)
        {
            int r = fwrite(p, 1, n, stdout);
            if(r <= 0) break;
            p += r;
            n -= r;
        }
        fprintf(stderr, "\r%u", ++frames);}
    }
}
