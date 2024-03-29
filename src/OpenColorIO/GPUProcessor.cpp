// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <sstream>

#include <OpenColorIO/OpenColorIO.h>

#include "GPUProcessor.h"
#include "GpuShader.h"
#include "GpuShaderUtils.h"
#include "Logging.h"
#include "ops/Allocation/AllocationOp.h"
#include "ops/Lut3D/Lut3DOp.h"
#include "ops/NoOp/NoOps.h"


OCIO_NAMESPACE_ENTER
{

namespace
{

void WriteShaderHeader(GpuShaderDescRcPtr & shaderDesc)
{
    const std::string fcnName(shaderDesc->getFunctionName());

    GpuShaderText ss(shaderDesc->getLanguage());

    ss.newLine();
    ss.newLine() << "// Declaration of the OCIO shader function";
    ss.newLine();

    ss.newLine() << ss.vec4fKeyword() << " " << fcnName 
                 << "(in "  << ss.vec4fKeyword() << " inPixel)";
    ss.newLine() << "{";
    ss.indent();
    ss.newLine() << ss.vec4fKeyword() << " " 
                 << shaderDesc->getPixelName() << " = inPixel;";

    shaderDesc->addToFunctionHeaderShaderCode(ss.string().c_str());
}


void WriteShaderFooter(GpuShaderDescRcPtr & shaderDesc)
{
    GpuShaderText ss(shaderDesc->getLanguage());

    ss.newLine();
    ss.indent();
    ss.newLine() << "return " << shaderDesc->getPixelName() << ";";
    ss.dedent();
    ss.newLine() << "}";

    shaderDesc->addToFunctionFooterShaderCode(ss.string().c_str());
}


OpRcPtrVec Create3DLut(const OpRcPtrVec & ops, unsigned edgelen)
{
    if(ops.size()==0) return OpRcPtrVec();

    const unsigned lut3DEdgeLen   = edgelen;
    const unsigned lut3DNumPixels = lut3DEdgeLen*lut3DEdgeLen*lut3DEdgeLen;

    Lut3DOpDataRcPtr lut = std::make_shared<Lut3DOpData>(lut3DEdgeLen);

    // Allocate 3D LUT image, RGBA
    std::vector<float> lut3D(lut3DNumPixels*4);
    GenerateIdentityLut3D(&lut3D[0], lut3DEdgeLen, 4, LUT3DORDER_FAST_BLUE);

    // Apply the lattice ops to it
    for(const auto & op : ops)
    {
        op->apply(&lut3D[0], &lut3D[0], lut3DNumPixels);
    }

    // Convert the RGBA image to an RGB image, in place.
    auto & lutArray = lut->getArray();
    for(unsigned i=0; i<lut3DNumPixels; ++i)
    {
        lutArray[3*i+0] = lut3D[4*i+0];
        lutArray[3*i+1] = lut3D[4*i+1];
        lutArray[3*i+2] = lut3D[4*i+2];
    }

    OpRcPtrVec newOps;
    CreateLut3DOp(newOps, lut, TRANSFORM_DIR_FORWARD);
    return newOps;
}

}


DynamicPropertyRcPtr GPUProcessor::Impl::getDynamicProperty(DynamicPropertyType type) const
{
    for(const auto & op : m_ops)
    {
        if(op->hasDynamicProperty(type))
        {
            return op->getDynamicProperty(type);
        }
    }

    throw Exception("Cannot find dynamic property; not used by GPU processor.");
}

void GPUProcessor::Impl::finalize(const OpRcPtrVec & rawOps,
                                  OptimizationFlags oFlags,
                                  FinalizationFlags fFlags)
{
    AutoMutex lock(m_mutex);

    // Prepare the list of ops.

    m_ops = rawOps;

    OptimizeOpVec(m_ops, BIT_DEPTH_F32, oFlags);
    FinalizeOpVec(m_ops, fFlags);
    UnifyDynamicProperties(m_ops);

    // Does the color processing introduce crosstalk between the pixel channels?

    m_hasChannelCrosstalk = false;
    for(const auto & op : m_ops)
    {
        if(op->hasChannelCrosstalk())
        {
            m_hasChannelCrosstalk = true;
            break;
        }
    }

    // Compute the cache id.

    std::stringstream ss;
    ss << "GPU Processor: oFlags " << oFlags
       << " fFlags " << fFlags
       << " ops :";
    for(const auto & op : m_ops)
    {
        ss << " " << op->getCacheID();
    }

    m_cacheID = ss.str();
}

void GPUProcessor::Impl::extractGpuShaderInfo(GpuShaderDescRcPtr & shaderDesc) const
{
    AutoMutex lock(m_mutex);

    OpRcPtrVec gpuOps;

    LegacyGpuShaderDesc * legacy = dynamic_cast<LegacyGpuShaderDesc*>(shaderDesc.get());
    if(legacy)
    {
        gpuOps = m_ops;

        // GPU Process setup
        //
        // Partition the original, raw opvec into 3 segments for GPU Processing
        //
        // Interior index range does not support the gpu shader.
        // This is used to bound our analytical shader text generation
        // start index and end index are inclusive.
        
        // These 3 op vecs represent the 3 stages in our gpu pipe.
        // 1) preprocess shader text
        // 2) 3D LUT process lookup
        // 3) postprocess shader text
        
        OpRcPtrVec gpuOpsHwPreProcess;
        OpRcPtrVec gpuOpsCpuLatticeProcess;
        OpRcPtrVec gpuOpsHwPostProcess;

        PartitionGPUOps(gpuOpsHwPreProcess,
                        gpuOpsCpuLatticeProcess,
                        gpuOpsHwPostProcess,
                        gpuOps);

        LogDebug("GPU Ops: 3DLUT");
        FinalizeOpVec(gpuOpsCpuLatticeProcess, FINALIZATION_DEFAULT);
        OpRcPtrVec gpuLut = Create3DLut(gpuOpsCpuLatticeProcess, legacy->getEdgelen());

        gpuOps.clear();
        gpuOps += gpuOpsHwPreProcess;
        gpuOps += gpuLut;
        gpuOps += gpuOpsHwPostProcess;

        OptimizeOpVec(gpuOps, BIT_DEPTH_F32, OPTIMIZATION_DEFAULT);
        FinalizeOpVec(gpuOps, FINALIZATION_DEFAULT);
    }
    else
    {
        gpuOps = m_ops;
    }

    // Create the shader program information
    for(const auto & op : gpuOps)
    {
        op->extractGpuShaderInfo(shaderDesc);
    }

    WriteShaderHeader(shaderDesc);
    WriteShaderFooter(shaderDesc);

    shaderDesc->finalize();

    if(IsDebugLoggingEnabled())
    {
        LogDebug("GPU Shader");
        LogDebug(shaderDesc->getShaderText());
    }
}


//////////////////////////////////////////////////////////////////////////


void GPUProcessor::deleter(GPUProcessor * c)
{
    delete c;
}

GPUProcessor::GPUProcessor()
    :   m_impl(new Impl)
{
}

GPUProcessor::~GPUProcessor()
{
    delete m_impl;
    m_impl = nullptr;
}

bool GPUProcessor::isNoOp() const
{
    return getImpl()->isNoOp();
}

bool GPUProcessor::hasChannelCrosstalk() const
{
    return getImpl()->hasChannelCrosstalk();
}

const char * GPUProcessor::getCacheID() const
{
    return getImpl()->getCacheID();
}

DynamicPropertyRcPtr GPUProcessor::getDynamicProperty(DynamicPropertyType type) const
{
    return getImpl()->getDynamicProperty(type);
}

void GPUProcessor::extractGpuShaderInfo(GpuShaderDescRcPtr & shaderDesc) const
{
    return getImpl()->extractGpuShaderInfo(shaderDesc);
}


}
OCIO_NAMESPACE_EXIT

