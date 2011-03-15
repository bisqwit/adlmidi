#include <math.h>
#include <stdio.h>

const unsigned X = 720;
const unsigned Y = 350;

static inline float apow(float a, float b)
{
    return expf(b * logf(a));
}

static void MakeRgb(int&dr, int&dg, int&db, float r,float g,float b)
{
    if(r>255) { g+=(r-255)/2; b+=(r-255)/2; r=255; }
    if(g>255) { r+=(g-255)/2; b+=(g-255)/2; g=255; }
    if(b>255) { r+=(b-255)/2; g+=(b-255)/2; b=255; }
    if(r>255) dr=255; else dr=r;
    if(g>255) dg=255; else dg=g;
    if(b>255) db=255; else db=b;
}

unsigned char Buf[X*Y*3];
unsigned char OutBuf[(X*2)*(Y*2)*3];

int main(int argc, char** argv)
{
    float cpow[Y][X][3];

    FILE* fi = stdin, *fo = stdout;
    if(argc == 3)
    {
        fi = fopen(argv[1], "rb");
        fo = fopen(argv[2], "wb");
    }

    float squares[9][9];
    for(int yd=-4; yd<=4; ++yd)
        for(int xd=-4; xd<=4; ++xd)
            squares[ (yd+4) ] [ (xd+4) ] = 36.0 / (1.0 + apow(xd*xd+yd*yd, 1.9));

    const unsigned N=7266;
    for(unsigned n=0; n<N; ++n)
    {
        if(fread(Buf, sizeof(Buf), 1, fi) != 1)
            break;
        fprintf(stderr, "\r%4u/%u", n,N); fflush(stderr);

        #pragma omp parallel for schedule(static)
        for(unsigned y=0; y<Y; ++y)
            for(unsigned p=y*X*3, x=0; x<X; x++)
            {
                cpow[y][x][0] = apow(Buf[p+x*3+0]/255.0, 4.0);
                cpow[y][x][1] = apow(Buf[p+x*3+1]/255.0, 3.0);
                cpow[y][x][2] = apow(Buf[p+x*3+2]/255.0, 6.0);
            }

        #pragma omp parallel for schedule(static)
        for(unsigned y=0; y<Y; ++y)
        {
            float r=0, g=0, b=0;
            float done=0;
            for(unsigned x=0; x< X*2; ++x)
            {
                unsigned xx = x/2;
                float rr = Buf[ (y*X+xx)*3 + 0];
                float gg = Buf[ (y*X+xx)*3 + 1];
                float bb = Buf[ (y*X+xx)*3 + 2];
                for(int yd=-4; yd<=4; ++yd)
                {
                    if(int(y)+yd < 0) continue; if(int(y)+yd >= int(Y)) break;
                    for(int xd=-4; xd<=4; ++xd)
                    {
                        if(int(xx)+xd < 0) continue; if(int(xx)+xd >= int(X)) break;
                        float power = squares[ (yd+4) ] [ (xd+4) ];
                        rr += cpow[y+yd][xx+xd][0] * power;
                        gg += cpow[y+yd][xx+xd][1] * power;
                        bb += cpow[y+yd][xx+xd][2] * power;
                    }
                }
                done -= 1.0;
                if(done < 0) done = 0; else if(done > 0.9) done = 0.9;
                float inv_factor = 1.0 / 100.0;
                for(int rounds=0; done<1; )
                {
                    int rdiff = fabs(r-rr);
                    int gdiff = fabs(g-gg);
                    int bdiff = fabs(b-bb);
                    if(!rdiff && !gdiff && !bdiff) break;
                    unsigned gstep=0, rstep=1, bstep=2;
                    if(rdiff > gdiff) { gstep=1; rstep=0; }
                    if(bdiff > gdiff && bdiff > rdiff) { ++gstep; ++rstep; bstep=0; }
                    for(unsigned step=0; step<3 && done<1; ++step)
                    { if(step == gstep && g != gg) { g = gg; done+=gdiff* inv_factor; }
                      if(step == rstep && r != rr) { r = rr; done+=rdiff* inv_factor; }
                      if(step == bstep && b != bb) { b = bb; done+=bdiff* inv_factor; } }
                    if(++rounds >= 3) break;
                }
                r = apow(r/255, 0.7)*255;
                g = apow(g/255, 0.7)*255;
                b = apow(b/255, 0.7)*255;
                int r1,g1,b1;
                int r2,g2,b2;
                MakeRgb(r2,g2,b2, r*.5, g*.5, b*.5);
                MakeRgb(r1,g1,b1, r,g,b);

                OutBuf[(x + y*2*(X*2))*3+0] = r1;
                OutBuf[(x + y*2*(X*2))*3+1] = g1;
                OutBuf[(x + y*2*(X*2))*3+2] = b1;
                OutBuf[(x + (y*2+1)*(X*2))*3+0] = r2;
                OutBuf[(x + (y*2+1)*(X*2))*3+1] = g2;
                OutBuf[(x + (y*2+1)*(X*2))*3+2] = b2;
            }
        }

        fwrite(OutBuf, sizeof(OutBuf), 1, fo);
        fflush(fo);
    }
    fclose(fo); fclose(fi);
}
