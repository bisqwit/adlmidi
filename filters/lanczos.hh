#include <math.h>
#include <alloca.h>

#include <valarray>

# define likely(x)       __builtin_expect(!!(x), 1)
# define unlikely(x)     __builtin_expect(!!(x), 0)

#if 1
  typedef float LanczosFloat;
  #define LanczosSin sinf

  //LanczosFloat LanczosSin(LanczosFloat x) { return 1.0f; }
#else
  typedef double LanczosFloat;
  #define LanczosSin sin
#endif

template<int Radius, bool check>
inline LanczosFloat Lanczos_pi(LanczosFloat x_pi)
{
    if(unlikely(x_pi == (LanczosFloat)0.0)) return (LanczosFloat)1.0;
    if(check)
    {
        if (x_pi <= (LanczosFloat)(-Radius*M_PI)
         || x_pi >= (LanczosFloat)( Radius*M_PI)) return (LanczosFloat)0.0;
    }

    LanczosFloat x_pi_div_Radius = x_pi / Radius;

    //LanczosFloat a = sin(x_pi)            / x_pi;
    //LanczosFloat b = sin(x_pi_div_Radius) / x_pi_div_Radius;
    //return a * b;

    return LanczosSin(x_pi) * LanczosSin(x_pi_div_Radius) / (x_pi * x_pi_div_Radius);
}
template<int Radius>
inline LanczosFloat Lanczos(LanczosFloat x)
{
    return Lanczos_pi<Radius,true>(x * (LanczosFloat)M_PI);
}

const int DefaultTripletCount = 4; // Better than 3 for -ftree-vectorize, also adds alpha support

/* Triplet is a N-element tuple. We use them to maximize
 * the compiler's chances for vectorizing optimizations.
 * It's kind of valarray, but with build-time specified size.
 */
template<typename T, int count = DefaultTripletCount>
struct Triplet
{
    T values[count];

    template<typename V>
    Triplet(V a,V b,V c)
        { values[0]=a; values[1]=b; values[2]=c; }
    template<typename V>
    Triplet(V a,V b,V c,V d)
        { values[0]=a; values[1]=b; values[2]=c; values[3]=d; }

    template<typename V>
    Triplet(const Triplet<V>& b)
        { for(size_t n=0; n<count; ++n) values[n] = b.values[n]; }

    template<typename V>
    Triplet(V a)
        { T tmp=a; for(size_t n=0; n<count; ++n) values[n] = tmp; }

    Triplet() { }

    T&       operator[] (size_t ind)       { return values[ind]; }
    const T& operator[] (size_t ind) const { return values[ind]; }

    #define bop(op) \
        template<typename B> \
        Triplet<T>& operator op (const Triplet<B>& b) \
            { for(size_t n=0; n<count; ++n) values[n]  op b.values[n]; \
              return *this; } \
        \
        template<typename B> \
        Triplet<T>& operator op (const B & b) \
            { for(size_t n=0; n<count; ++n) values[n]  op b; \
              return *this; }

    bop(*=)
    bop(+=)
    bop(/=)
    bop(>>=)
    bop(&=)

    #undef bop

    #define uop(op) \
        template<typename B> \
        Triplet<T> operator op (const B& b) const \
            { Triplet<T> res = *this; res op##= b; return res; }

    uop(*)
    uop(/)
    uop(>>)
    uop(&)

    #undef uop
};

template<typename A, typename B>
Triplet<A> operator>> (A a, const Triplet<B>& b)
    { Triplet<A> res = a; res >>= b; return res; }

template<typename A, typename B>
Triplet<A> operator* (A a, const Triplet<B>& b)
    { Triplet<A> res = a; res *= b; return res; }

template<typename T>
struct LanczosFloatValue              { typedef LanczosFloat          result; };
template<typename T>
struct LanczosFloatValue<Triplet<T> > { typedef Triplet<LanczosFloat> result; };

/* The actual basis for scaling. */

template<typename SrcTab, typename DestTab>
class Lanczos2DBase
{
    const SrcTab& src; DestTab& tgt;

    // Note: In this vocabulary,
    //  y denotes outer loop and
    //  x denotes inner loop, but
    // it could be vice versa.
    int xinc_src, yinc_src;
    int xinc_tgt, yinc_tgt;
    int ylimit;
public:
    Lanczos2DBase(
        const SrcTab& src_, DestTab& tgt_,
        int sxi,int syi, int txi,int tyi, int ylim)
        : src(src_), tgt(tgt_),
          xinc_src(sxi), yinc_src(syi),
          xinc_tgt(txi), yinc_tgt(tyi),
          ylimit(ylim)
        { }

    void ScaleOne
        (int srcpos, int tgtpos, int nmax,
         const LanczosFloat contrib[], LanczosFloat density_rev) const
    {
        typename LanczosFloatValue
            <typename DestTab::value_type>::result res = 0.0;
        //LanczosFloat r=0, g=0, b=0;

        int srctemp = srcpos;

        //#pragma omp parallel for reduction(+:r,g,b)
        for(int n=0; n<nmax; ++n)
        {
            res     += contrib[n] * src.at(srctemp);
            srctemp += xinc_src;   // source x increment
        }

        // Multiplication is faster than division, so we use the reciprocal.
        res *= density_rev;

        tgt.at(tgtpos) = res;
    }

    void StripeLoop(int tx, int sx, int nmax,
                    const LanczosFloat contrib[], LanczosFloat density) const;
};

template<typename SrcTab, typename DestTab>
void Lanczos2DBase<SrcTab,DestTab>
    ::StripeLoop(int tx, int sx, int nmax, const LanczosFloat contrib[], LanczosFloat density) const
{
    int srcpos = sx * xinc_src; // source x pos at y = 0
    int tgtpos = tx * xinc_tgt; // target x pos at y = 0

    /*
    fprintf(stderr, "StripeLoop sx=%d, tx=%d, srcpos=%d, tgtpos=%d, srcsize=%d, tgtsize=%d, ylimit=%d\n",
        sx,tx, srcpos, tgtpos,
        (int)src.size(), (int)tgt.size(), ylimit);*/

    const LanczosFloat density_rev = (density == 0.0 || density == 1.0)
        ? 1.0
        : (1.0 / density);

    for(int y=ylimit; y-->0; )
    {
        /*fprintf(stderr, "- within: srcpos=%d, tgtpos=%d, y=%d\n", srcpos,tgtpos, y);*/
        ScaleOne(srcpos, tgtpos, nmax, contrib, density_rev);

        srcpos += yinc_src;  // source y increment
        tgtpos += yinc_tgt;  // target y increment
    }
}

template<typename SrcTab, typename DestTab>
class HorizScaler: public Lanczos2DBase<SrcTab,DestTab>
{
public:
    /*
            <-------------->
            <-------------->
            <-------------->
            <-------------->
            <-------------->
            <-------------->

            For each output column (out_size = {ow}),
            {h} rows (source and target) get processed

            On each row,
              {nmax} source columns get transformed
              into 1 target column

            Target:
               New column stride = {1}
               New row    stride = {ow}
            Source:
               Next column stride = {1}
               Next row    stride = {iw}
    */

    HorizScaler(
        int iw,int ow, int h,
        const SrcTab& src, DestTab& tgt)
        : Lanczos2DBase<SrcTab,DestTab>(
            src,tgt,
            1,  // xinc_src ok
            iw, // yinc_src ok
            1,  // xinc_tgt ok
            ow, // yinc_tgt ok
            h   // ylimit   ok
         ) { }
};

template<typename SrcTab, typename DestTab>
class VertScaler: public Lanczos2DBase<SrcTab,DestTab>
{
public:
    /*
            ^^^^^^^^^^^^^^^^
            ||||||||||||||||
            ||||||||||||||||
            ||||||||||||||||
            ||||||||||||||||
            vvvvvvvvvvvvvvvv

            For each output row (out_size = {oh}),
            {w} columns (source and target) get processed

            On each column,
              {nmax} source rows get transformed
              into 1 target row

            Target:
               New row    stride = {w}
               New column stride = {1}
            Source:
               Next row    stride = {w}
               Next column stride = {1}
    */

    VertScaler(
        int w,
        const SrcTab& src, DestTab& tgt)

        : Lanczos2DBase<SrcTab,DestTab>(
            src,tgt,
            w, // xinc_src ok
            1, // yinc_src ok
            w, // xinc_tgt ok
            1, // yinc_tgt ok
            w  // ylimit   ok
         ) { }
};

/*template<typename SrcTab, typename DestTab>
class ScalarScaler: private Lanczos2DBase<SrcTab, DestTab>
{
public:
    ScalarScaler(const SrcTab& src, DestTab& tgt)
        : Lanczos2DBase<SrcTab,DestTab>(src,tgt, 1,1,1,1,1) { }

    void StripeLoop(int tx, int sx, int nmax,
                    const LanczosFloat contrib[], LanczosFloat density) const
    {
        const LanczosFloat density_rev = (density == 0.0 || density == 1.0)
            ? 1.0
            : (1.0 / density);
        ScaleOne(sx, tx, nmax, contrib, density_rev);
    }
};*/

struct LanczosCoreCalcRes
{
    int start;
    int nmax;
    LanczosFloat density;
};

template<int FilterRadius>
inline LanczosCoreCalcRes LanczosCoreCalc
    (int in_size,
     LanczosFloat center, LanczosFloat support, LanczosFloat scale,
     LanczosFloat contrib[])
{
    const int start = std::max((int)(center-support+(LanczosFloat)0.5), 0);
    const int end   = std::min((int)(center+support+(LanczosFloat)0.5), in_size);
    const int nmax = end-start;

    const LanczosFloat scale_pi = scale * M_PI;

    const LanczosFloat s_min = -FilterRadius*M_PI;
    const LanczosFloat s_max =  FilterRadius*M_PI;

    LanczosFloat s_pi     = (start-center+(LanczosFloat)0.5) * scale_pi;

    LanczosFloat density  = 0.0;

    { int n=0;
      for(; n < nmax && unlikely(s_pi < s_min); ++n, s_pi += scale_pi)
        {}
      for(; n < nmax && likely(s_pi < s_max); ++n, s_pi += scale_pi)
      {
        LanczosFloat l = Lanczos_pi<FilterRadius,false> (s_pi);
        contrib[n] = l;
        density += l;
      }
    }

    LanczosCoreCalcRes res;
    res.start   = start;
    res.nmax    = nmax;
    res.density = density;
    return res;
}

/* A generic Lanczos scaler suitable for
 * converting something to something else
 * at once.
 * For image pixels, use Triplet<type>
 * For stereo samples, use Triplet<type, 2>
 * For mono samples, just use type
 */
template<typename Handler>
void LanczosScale(int in_size, int out_size, Handler& target)
{
    const int FilterRadius = 3;
    const LanczosFloat blur         = 1.0;

    const LanczosFloat factor       = out_size / (LanczosFloat)in_size;
    const LanczosFloat scale        = std::min(factor, (LanczosFloat)1.0) / blur;
    const LanczosFloat support      = FilterRadius / scale;

    const size_t contrib_size = std::min(in_size, 5+int(2*support));
    LanczosFloat contrib[contrib_size];

    /*fprintf(stderr, "Scaling (%d->%d), contrib=%d\n",
        in_size, out_size, (int)contrib_size);*/

    /* FIXME: does private() really work for stack arrays, especially autosize ones? */

    #pragma omp parallel for schedule(static) private(contrib)
    for(int outpos=0; outpos<out_size; ++outpos)
    {
        LanczosFloat center = (outpos+0.5) / factor;
        LanczosCoreCalcRes res
            = LanczosCoreCalc<FilterRadius>(in_size, center, support, scale, contrib);
        target.StripeLoop(outpos, res.start, res.nmax, &contrib[0], res.density);
    }
}

#if 0
/* Suitable for audio resampling
 *
 * For stereo samples, use Triplet<type, 2>
 * For mono samples, just use type
 */
template<typename SrcT = short, typename DestT = LanczosFloat>
class ContiguousLanczosScaler
{
    static const int FilterRadius = 3;

    std::vector<T> in_buffer;
    double factor, scale, support;


private:
    void SetRatio(int in_rate, int out_rate)
    {
        const LanczosFloat blur         = 1.0;

        factor       = out_size / (LanczosFloat)in_size;
        scale        = std::min(factor, (LanczosFloat)1.0) / blur;
        support      = FilterRadius / scale;
    }
};
#endif

template<typename SrcTab, typename DestTab>
void LanczosScale(int wid,int hei, int out_wid,int out_hei,
                  const SrcTab& src,
                  DestTab& tgt)
{
    const size_t in_dim  = wid*hei;
    const size_t out_dim = out_wid*out_hei;

    assert(src.size() == in_dim);

    if(out_wid != wid || out_hei != hei)
    {
      #if 1
        typedef SrcTab TmpSrcTab;
        const SrcTab& tmpsrc = src;
      #else
        typedef DestTab TmpSrcTab;
        DestTab tmpsrc(in_dim);
        std::copy(&src[0],&src[in_dim], &tmpsrc[0]);
      #endif

        if(out_wid != wid)
        {
            // X scaling    (sx*sy   -> dx*sy)
            tgt.resize(out_wid*hei);

            HorizScaler<TmpSrcTab, DestTab> handler_x(wid,out_wid, hei, tmpsrc,tgt);
            LanczosScale(wid, out_wid, handler_x);

            if(out_hei != hei)
            {
                // Also Y scaling    (dx*sy  ->  dx*dy)
                typedef DestTab TempTab;
                TempTab tmp(out_dim);
                tmp.swap(tgt);

                VertScaler<TempTab, DestTab> handler_y(out_wid, tmp,tgt);
                LanczosScale(hei, out_hei, handler_y);
            }
        }
        else if(out_hei != hei)
        {
            // Only Y scaling    (x*sy   -> x*dy)
            tgt.resize(out_dim);
            VertScaler<TmpSrcTab, DestTab> handler_y(wid, tmpsrc,tgt);
            LanczosScale(hei, out_hei, handler_y);
        }
    }
    else
    {
        tgt.resize(out_dim);
        // No scalings
        std::copy(&src[0],&src[in_dim], &tgt[0]);
    }
}
