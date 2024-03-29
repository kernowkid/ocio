// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <algorithm>
#include <cmath>

#include <OpenColorIO/OpenColorIO.h>

#include "BitDepthUtils.h"
#include "ops/Gamma/GammaOpCPU.h"
#include "ops/Gamma/GammaOpUtils.h"

#include "SSE.h"


OCIO_NAMESPACE_ENTER
{

// Note: The parameters are validated when the op is created so that the
// math below does not require checks for divide by 0, etc.


// Base class for the Gamma (i.e. basic style) operation renderers.
class GammaBasicOpCPU : public OpCPU
{
public:
    GammaBasicOpCPU() = delete;
    GammaBasicOpCPU(const GammaBasicOpCPU &) = delete;
    explicit GammaBasicOpCPU(ConstGammaOpDataRcPtr & gamma);

    void apply(const void * inImg, void * outImg, long numPixels) const override;

protected:
    void update(ConstGammaOpDataRcPtr & gamma);

private:
    float m_redGamma;
    float m_grnGamma;
    float m_bluGamma;
    float m_alpGamma;
};

class GammaMoncurveOpCPU : public OpCPU
{
protected:
    explicit GammaMoncurveOpCPU(ConstGammaOpDataRcPtr &) : OpCPU() {}

protected:
    RendererParams m_red;
    RendererParams m_green;
    RendererParams m_blue;
    RendererParams m_alpha;
};

class GammaMoncurveOpCPUFwd : public GammaMoncurveOpCPU
{
public:
    explicit GammaMoncurveOpCPUFwd(ConstGammaOpDataRcPtr & gamma);

    void apply(const void * inImg, void * outImg, long numPixels) const override;

protected:
    void update(ConstGammaOpDataRcPtr & gamma);
};

class GammaMoncurveOpCPURev : public GammaMoncurveOpCPU
{
public:
    explicit GammaMoncurveOpCPURev(ConstGammaOpDataRcPtr & gamma);

    void apply(const void * inImg, void * outImg, long numPixels) const override;

protected:
    void update(ConstGammaOpDataRcPtr & gamma);

};


ConstOpCPURcPtr GetGammaRenderer(ConstGammaOpDataRcPtr & gamma)
{
    switch(gamma->getStyle())
    {
        case GammaOpData::MONCURVE_FWD:
        {
            return std::make_shared<GammaMoncurveOpCPUFwd>(gamma);
            break;
        }

        case GammaOpData::MONCURVE_REV:
        {
            return std::make_shared<GammaMoncurveOpCPURev>(gamma);
            break;
        }

        case GammaOpData::BASIC_FWD:
        case GammaOpData::BASIC_REV:
        {
            return std::make_shared<GammaBasicOpCPU>(gamma);
            break;
        }
    }

    throw Exception("Unsupported Gamma style");
}




GammaBasicOpCPU::GammaBasicOpCPU(ConstGammaOpDataRcPtr & gamma)
    :   OpCPU()
    ,   m_redGamma(0.0f)
    ,   m_grnGamma(0.0f)
    ,   m_bluGamma(0.0f)
    ,   m_alpGamma(0.0f)
{
    update(gamma);
}

void GammaBasicOpCPU::update(ConstGammaOpDataRcPtr & gamma)
{
    // The gamma calculations are done in normalized space.
    // Compute the scale factors for integer in/out depths.

    // Calculate the actual power used in the function.
    m_redGamma = (float)(
        gamma->getStyle() == GammaOpData::BASIC_FWD
        ? gamma->getRedParams()[0]
        : 1. / gamma->getRedParams()[0]);

    m_grnGamma = (float)(
        gamma->getStyle() == GammaOpData::BASIC_FWD 
        ? gamma->getGreenParams()[0] 
        : 1. / gamma->getGreenParams()[0]);

    m_bluGamma = (float)(
        gamma->getStyle() == GammaOpData::BASIC_FWD 
        ? gamma->getBlueParams()[0] 
        : 1. / gamma->getBlueParams()[0]);

    m_alpGamma = (float)(
        gamma->getStyle() == GammaOpData::BASIC_FWD 
        ? gamma->getAlphaParams()[0]
        : 1. / gamma->getAlphaParams()[0]);
}

void GammaBasicOpCPU::apply(const void * inImg, void * outImg, long numPixels) const
{
    const float * in = (const float *)inImg;
    float * out = (float *)outImg;

#ifdef USE_SSE
    const __m128 gamma = _mm_set_ps(m_alpGamma, m_bluGamma, m_grnGamma, m_redGamma);
      
    for(long idx=0; idx<numPixels; ++idx)
    {
        __m128 pixel = _mm_set_ps(in[3], in[2], in[1], in[0]);

        pixel = ssePower(pixel, gamma);

        _mm_storeu_ps(out, pixel);

        in  += 4;
        out += 4;
    }
#else
    for(long idx=0; idx<numPixels; ++idx)
    {
        const float pixel[4] = { std::max(0.0f, in[0]), 
                                 std::max(0.0f, in[1]), 
                                 std::max(0.0f, in[2]),
                                 std::max(0.0f, in[3]) };

        out[0] = powf(pixel[0], m_redGamma);
        out[1] = powf(pixel[1], m_grnGamma);
        out[2] = powf(pixel[2], m_bluGamma);
        out[3] = powf(pixel[3], m_alpGamma);

        in  += 4;
        out += 4;
    }
#endif
}

GammaMoncurveOpCPUFwd::GammaMoncurveOpCPUFwd(ConstGammaOpDataRcPtr & gamma)
    :   GammaMoncurveOpCPU(gamma)
{
    update(gamma);
}

void GammaMoncurveOpCPUFwd::update(ConstGammaOpDataRcPtr & gamma)
{
    ComputeParamsFwd(gamma->getRedParams(),   m_red);
    ComputeParamsFwd(gamma->getGreenParams(), m_green);
    ComputeParamsFwd(gamma->getBlueParams(),  m_blue);
    ComputeParamsFwd(gamma->getAlphaParams(), m_alpha);
}

void GammaMoncurveOpCPUFwd::apply(const void * inImg, void * outImg, long numPixels) const
{
    const float * in = (const float *)inImg;
    float * out = (float *)outImg;

#ifdef USE_SSE
    const __m128 scale
      = _mm_set_ps(m_alpha.scale, m_blue.scale,
                   m_green.scale, m_red.scale);

    const __m128 offset
      = _mm_set_ps(m_alpha.offset, m_blue.offset,
                   m_green.offset, m_red.offset);

    const __m128 gamma
      = _mm_set_ps(m_alpha.gamma, m_blue.gamma,
                   m_green.gamma, m_red.gamma);

    const __m128 breakPnt
      = _mm_set_ps(m_alpha.breakPnt, m_blue.breakPnt,
                   m_green.breakPnt, m_red.breakPnt);

    const __m128 slope
      = _mm_set_ps(m_alpha.slope, m_blue.slope,
                   m_green.slope, m_red.slope);

    for(long idx=0; idx<numPixels; ++idx)
    {
        __m128 pixel = _mm_set_ps(in[3], in[2], in[1], in[0]);

        __m128 data = _mm_add_ps(_mm_mul_ps(pixel, scale), offset);

        data = ssePower(data, gamma);

        __m128 flag = _mm_cmpgt_ps( pixel, breakPnt);

        data = _mm_or_ps(_mm_and_ps(flag, data),
                         _mm_andnot_ps(flag, _mm_mul_ps(pixel, slope )));

        _mm_storeu_ps(out, data);

        in  += 4;
        out += 4;
    }
#else
    const float red[5] 
        = { m_red.scale,  m_red.offset,
            m_red.gamma,  m_red.breakPnt, m_red.slope };
    const float grn[5]
        = { m_green.scale, m_green.offset, 
            m_green.gamma, m_green.breakPnt, m_green.slope };
    const float blu[5]
        = { m_blue.scale,  m_blue.offset,
            m_blue.gamma,  m_blue.breakPnt, m_blue.slope };
    const float alp[5]
        = { m_alpha.scale, m_alpha.offset, 
            m_alpha.gamma, m_alpha.breakPnt, m_alpha.slope };

    for(long idx=0; idx<numPixels; ++idx)
    {
        const float pixel[4] = { in[0], in[1], in[2], in[3] };

        const float data[4] = { powf(pixel[0] * red[0] + red[1], red[2]),
                                powf(pixel[1] * grn[0] + grn[1], grn[2]),
                                powf(pixel[2] * blu[0] + blu[1], blu[2]),
                                powf(pixel[3] * alp[0] + alp[1], alp[2]) };

        out[0] = pixel[0]<=red[3] ? pixel[0] * red[4] : data[0];
        out[1] = pixel[1]<=grn[3] ? pixel[1] * grn[4] : data[1];
        out[2] = pixel[2]<=blu[3] ? pixel[2] * blu[4] : data[2];
        out[3] = pixel[3]<=alp[3] ? pixel[3] * alp[4] : data[3];

        in  += 4;
        out += 4;
    }
#endif
}

GammaMoncurveOpCPURev::GammaMoncurveOpCPURev(ConstGammaOpDataRcPtr & gamma)
    :   GammaMoncurveOpCPU(gamma)
{
    update(gamma);
}

void GammaMoncurveOpCPURev::update(ConstGammaOpDataRcPtr & gamma)
{
    ComputeParamsRev(gamma->getRedParams(),   m_red);
    ComputeParamsRev(gamma->getGreenParams(), m_green);
    ComputeParamsRev(gamma->getBlueParams(),  m_blue);
    ComputeParamsRev(gamma->getAlphaParams(), m_alpha);
}

void GammaMoncurveOpCPURev::apply(const void * inImg, void * outImg, long numPixels) const
{
    const float * in = (const float *)inImg;
    float * out = (float *)outImg;

#ifdef USE_SSE
    const __m128 scale
      = _mm_set_ps(m_alpha.scale, m_blue.scale,
                   m_green.scale, m_red.scale);

    const __m128 offset
      = _mm_set_ps(m_alpha.offset, m_blue.offset,
                   m_green.offset, m_red.offset);

    const __m128 gamma
      = _mm_set_ps(m_alpha.gamma, m_blue.gamma,
                   m_green.gamma, m_red.gamma);

    const __m128 breakPnt
      = _mm_set_ps(m_alpha.breakPnt, m_blue.breakPnt,
                   m_green.breakPnt, m_red.breakPnt);

    const __m128 slope
      = _mm_set_ps(m_alpha.slope, m_blue.slope,
                   m_green.slope, m_red.slope);

    for(long idx=0; idx<numPixels; ++idx)
    {
        __m128 pixel = _mm_set_ps(in[3], in[2], in[1], in[0]);

        __m128 data = ssePower(pixel, gamma);

        data = _mm_sub_ps(_mm_mul_ps(data, scale), offset);

        __m128 flag = _mm_cmpgt_ps(pixel, breakPnt);

        data = _mm_or_ps(_mm_and_ps(flag, data),
                         _mm_andnot_ps(flag, _mm_mul_ps(pixel, slope)));

        _mm_storeu_ps(out, data);

        in  += 4;
        out += 4;
    }
#else
    const float red[5] 
        = { m_red.gamma,  m_red.scale,
            m_red.offset, m_red.breakPnt, m_red.slope };
    const float grn[5]
        = { m_green.gamma, m_green.scale, 
            m_green.offset,m_green.breakPnt, m_green.slope };
    const float blu[5]
        = { m_blue.gamma,  m_blue.scale,
            m_blue.offset, m_blue.breakPnt, m_blue.slope  };
    const float alp[5]
        = { m_alpha.gamma,  m_alpha.scale, 
            m_alpha.offset, m_alpha.breakPnt, m_alpha.slope };

    for(long idx=0; idx<numPixels; ++idx)
    {
        const float pixel[4] = { in[0], in[1], in[2], in[3] };

        const float data[4] = { powf(pixel[0], red[0]) * red[1] - red[2],
                                powf(pixel[1], grn[0]) * grn[1] - grn[2],
                                powf(pixel[2], blu[0]) * blu[1] - blu[2],
                                powf(pixel[3], alp[0]) * alp[1] - alp[2] };

        out[0] = pixel[0]<=red[3] ? pixel[0] * red[4] : data[0];
        out[1] = pixel[1]<=grn[3] ? pixel[1] * grn[4] : data[1];
        out[2] = pixel[2]<=blu[3] ? pixel[2] * blu[4] : data[2];
        out[3] = pixel[3]<=alp[3] ? pixel[3] * alp[4] : data[3];

        in  += 4;
        out += 4;
    }
#endif

}

}
OCIO_NAMESPACE_EXIT