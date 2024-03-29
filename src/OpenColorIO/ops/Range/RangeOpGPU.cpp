// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <limits>

#include <OpenColorIO/OpenColorIO.h>

#include "MathUtils.h"
#include "ops/Range/RangeOpGPU.h"


OCIO_NAMESPACE_ENTER
{

void GetRangeGPUShaderProgram(GpuShaderDescRcPtr & shaderDesc,
                              ConstRangeOpDataRcPtr & range)
{
    GpuShaderText ss(shaderDesc->getLanguage());
    ss.indent();

    ss.newLine() << "";
    ss.newLine() << "// Add a Range processing";
    ss.newLine() << "";

    if(range->scales())
    {
        const double scale[3]
            = { range->getScale(),
                range->getScale(),
                range->getScale() };

        const double offset[3] 
            = { range->getOffset(), 
                range->getOffset(), 
                range->getOffset() };

        ss.newLine() << shaderDesc->getPixelName() << ".rgb = "
                     << shaderDesc->getPixelName() << ".rgb * "
                     << ss.vec3fConst(scale[0], scale[1], scale[2])
                     << " + "
                     << ss.vec3fConst(offset[0], offset[1], offset[2])
                     << ";";
    }

    if(!range->minIsEmpty())
    {
        const double lowerBound[3] 
            = { range->getMinOutValue(), 
                range->getMinOutValue(), 
                range->getMinOutValue() };

        ss.newLine() << shaderDesc->getPixelName() << ".rgb = "
                     << "max(" << ss.vec3fConst(lowerBound[0],
                                                lowerBound[1],
                                                lowerBound[2]) << ", "
                     << shaderDesc->getPixelName()
                     << ".rgb);";
    }

    if (!range->maxIsEmpty())
    {
        const double upperBound[3]
            = { range->getMaxOutValue(),
                range->getMaxOutValue(),
                range->getMaxOutValue() };

        ss.newLine() << shaderDesc->getPixelName() << ".rgb = "
            << "min(" << ss.vec3fConst(upperBound[0],
                                       upperBound[1],
                                       upperBound[2]) << ", "
            << shaderDesc->getPixelName()
            << ".rgb);";
    }

    shaderDesc->addToFunctionShaderCode(ss.string().c_str());
}


}
OCIO_NAMESPACE_EXIT
