#include <stdio.h>
#include <vector>

#include "ntsc/snes_ntsc.h"
#include <assert.h>
#include "lanczos.hh"

static const unsigned X = 256;
static const unsigned Y = 240;

static const unsigned OutX = SNES_NTSC_OUT_WIDTH(X);
static const unsigned OutY = OutX*400/720;

SNES_NTSC_IN_T Buf[X*Y];
std::vector<Triplet<unsigned char,4> > OutBuf;
std::vector<Triplet<unsigned char,3> > ResultBuf;
std::vector<Triplet<LanczosFloat,4> > TmpBuf1, TmpBuf2;

snes_ntsc_t ntsc;

int main(int argc, char** argv)
{
    OutBuf.resize(OutX*Y);
    ResultBuf.resize(OutX*OutY);

    snes_ntsc_setup_t setup = snes_ntsc_composite;
    snes_ntsc_init(&ntsc, &setup);
    fprintf(stderr, "Out: %ux%u\n", OutX,OutY);
    int phase=0;
    unsigned frames=0;
    for(;;)
    {
        if(fread(Buf, 1, sizeof(Buf), stdin) != sizeof(Buf)) break;
        phase=phase^1;

        snes_ntsc_blit(&ntsc,
            &Buf[0],
            X, // in row width
            phase, // phase
            X,Y, // in dimensions
            &OutBuf[0],
            OutX * 4 // out pitch
        );

        //TmpBuf1.assign(OutBuf.begin(), OutBuf.end());

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
        fprintf(stderr, "\r%u", ++frames);
    }
}
