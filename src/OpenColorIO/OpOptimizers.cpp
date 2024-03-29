// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <algorithm>
#include <iterator>
#include <sstream>

#include <OpenColorIO/OpenColorIO.h>

#include "Logging.h"
#include "Op.h"
#include "ops/Lut1D/Lut1DOp.h"
#include "ops/Lut1D/Lut1DOpData.h"

OCIO_NAMESPACE_ENTER
{
    namespace
    {
    const int MAX_OPTIMIZATION_PASSES = 8;

    void RemoveNoOpTypes(OpRcPtrVec & opVec)
    {
        OpRcPtrVec::iterator iter = opVec.begin();
        while (iter != opVec.end())
        {
            ConstOpRcPtr o = (*iter);
            if (o->data()->getType() == OpData::NoOpType)
            {
                iter = opVec.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }

    int RemoveNoOps(OpRcPtrVec & opVec)
    {
        int count = 0;

        OpRcPtrVec::iterator iter = opVec.begin();
        while (iter != opVec.end())
        {
            if ((*iter)->isNoOp())
            {
                iter = opVec.erase(iter);
                ++count;
            }
            else
            {
                ++iter;
            }
        }

        return count;
    }

    int RemoveInverseOps(OpRcPtrVec & opVec)
    {
        int count      = 0;
        int firstindex = 0; // this must be a signed int

        while (firstindex < static_cast<int>(opVec.size() - 1))
        {
            ConstOpRcPtr first  = opVec[firstindex];
            ConstOpRcPtr second = opVec[firstindex + 1];

            // The common case of inverse ops is to have a deep nesting:
            // ..., A, B, B', A', ...
            //
            // Consider the above, when firstindex reaches B:
            //
            //         |
            // ..., A, B, B', A', ...
            //
            // We will remove B and B'.
            // Firstindex remains pointing at the original location:
            //
            //         |
            // ..., A, A', ...
            //
            // We then decrement firstindex by 1,
            // to backstep and reconsider the A, A' case:
            //
            //      |            <-- firstindex decremented
            // ..., A, A', ...
            //

            if (first->isSameType(second) && first->isInverse(second))
            {
                opVec.erase(opVec.begin() + firstindex, opVec.begin() + firstindex + 2);
                ++count;

                firstindex = std::max(0, firstindex - 1);
            }
            else
            {
                ++firstindex;
            }
        }

        return count;
    }

    int CombineOps(OpRcPtrVec & opVec)
    {
        int count      = 0;
        int firstindex = 0; // this must be a signed int

        OpRcPtrVec tmpops;

        while (firstindex < static_cast<int>(opVec.size() - 1))
        {
            ConstOpRcPtr first  = opVec[firstindex];
            ConstOpRcPtr second = opVec[firstindex + 1];

            if (first->canCombineWith(second))
            {
                tmpops.clear();
                first->combineWith(tmpops, second);

                // tmpops may have any number of ops in it. (0, 1, 2, ...)
                // (size 0 would occur potentially iff the combination
                // results in a no-op)
                //
                // No matter the number, we need to swap them in for the
                // original ops

                // Erase the initial two ops we've combined
                opVec.erase(opVec.begin() + firstindex, opVec.begin() + firstindex + 2);

                // Insert the new ops (which may be empty) at
                // this location
                opVec.insert(opVec.begin() + firstindex, tmpops.begin(), tmpops.end());

                // Decrement firstindex by 1,
                // to backstep and reconsider the A, A' case.
                // See RemoveInverseOps for the full discussion of
                // why this is appropriate
                firstindex = std::max(0, firstindex - 1);

                // We've done something so increment the count!
                ++count;
            }
            else
            {
                ++firstindex;
            }
        }

        return count;
    }
    } // namespace

    // (Note: the term "separable" in mathematics refers to a multi-dimensional
    // function where the dimensions are independent of each other.)
    //
    // The goal here is to speed up calculations by replacing the contiguous separable
    // (channel independent) list of ops from the first op onwards with a single
    // LUT1D whose domain is sampled for the target bit depth.  A typical use-case
    // would be a list of ops that starts with a gamma that is processing integer 10i
    // pixels.  Rather than convert to float and apply the power function on each
    // pixel, it's better to build a 1024 entry LUT and just do a look-up.
    //
    unsigned FindSeparablePrefix(const OpRcPtrVec & ops)
    {
        unsigned prefixLen = 0;

        // Loop over the ops until we get to one that cannot be combined.
        //
        // Note: For some ops such as Matrix and CDL, the separability depends upon
        //       the parameters.
        for (const auto & op : ops)
        {
            // TODO: Dynamic bypassed ops can be 'optimizied' like any other ops.

            // In OCIO, the hasChannelCrosstalk method returns false for separable ops.
            if (op->hasChannelCrosstalk() || op->isDynamic())
            {
                break;
            }

            // Op is separable, keep going.
            prefixLen++;
        }

        // If the only op is a 1D LUT, there is actually nothing to optimize
        // so set the length to 0.  (This also avoids an infinite loop.)
        // (If it is an inverse 1D LUT, proceed since we want to replace it with a 1D LUT.)
        if (prefixLen == 1)
        {
            ConstOpRcPtr constOp0 = ops[0];
            if (constOp0->data()->getType() == OpData::Lut1DType &&
                constOp0->getDirection() == TRANSFORM_DIR_FORWARD)
            {
                return 0;
            }
        }

        // Some ops are so fast that it may not make sense to replace just one of those.
        // E.g., if it's just a single matrix, it may not be faster to replace it with a LUT.
        // So make sure there are some more expensive ops to combine.
        unsigned expensiveOps = 0u;
        for (unsigned i = 0; i < prefixLen; ++i)
        {
            auto op = ops[i];

            if (op->hasChannelCrosstalk())
            {
                // Non-separable ops (should never get here).
                throw Exception("Non-separable op.");
            }

            ConstOpRcPtr constOp = op;
            switch (constOp->data()->getType())
            {
                // Potentially separable, but inexpensive ops.
                // TODO: Perhaps a LUT is faster once the conversion to float is considered?
                case OpData::MatrixType:
                case OpData::RangeType:
                {
                    break;
                }

                // Potentially separable, and more expensive.
                default:
                {
                    expensiveOps++;
                    break;
                }
            }
        }

        if (expensiveOps == 0)
        {
            return 0;
        }

        // TODO: The main source of potential lossiness is where there is a 1D LUT
        // that has extended range values followed by something that clamps.  In
        // that case, the clamp would get baked into the LUT entries and therefore
        // result in a different interpolated value.  Could look for that case and
        // turn off the optimization.

        return prefixLen;
    }

    // Use functional composition to replace a string of separable ops at the head of
    // the op list with a single 1D LUT that is built to do a look-up for the input bit-depth.
    void OptimizeSeparablePrefix(OpRcPtrVec & ops, BitDepth in, OptimizationFlags /*oFlags*/)
    {
        // TODO: Take care of the dynamic properties.

        if (ops.empty())
        {
            return;
        }

        // TODO: Investigate whether even the F32 case could be sped up via interpolating 
        //       in a half-domain Lut1D (e.g. replacing a string of exponent, log, etc.).
        if(in == BIT_DEPTH_F32 || in == BIT_DEPTH_UINT32)
        {
            return;
        }

        const unsigned prefixLen = FindSeparablePrefix(ops);
        if (prefixLen == 0)
        {
            return; // Nothing to do.
        }

        OpRcPtrVec prefixOps;
        for (unsigned i = 0; i < prefixLen; ++i)
        {
            prefixOps.push_back(ops[i]->clone());
        }

        // Make a domain for the LUT.  (Will be half-domain for target == 16f.)
        Lut1DOpDataRcPtr newDomain = Lut1DOpData::MakeLookupDomain(in);

        // Send the domain through the prefix ops.
        // Note: This sets the outBitDepth of newDomain to match prefixOps.
        Lut1DOpData::ComposeVec(newDomain, prefixOps);

        // Remove the prefix ops.
        ops.erase(ops.begin(), ops.begin() + prefixLen);

        // Insert the new LUT to replace the prefix ops.
        OpRcPtrVec lutOps;
        CreateLut1DOp(lutOps, newDomain, TRANSFORM_DIR_FORWARD);

        ops.insert(ops.begin(), lutOps.begin(), lutOps.end());
    }

    void OptimizeOpVec(OpRcPtrVec & ops, const BitDepth & inBitDepth,
                       OptimizationFlags oFlags)
    {
        if (ops.empty())
            return;

        if (IsDebugLoggingEnabled())
        {
            LogDebug("Optimizing Op Vec...");
            LogDebug(SerializeOpVec(ops, 4));
        }

        // As the input and output bit-depths represent the color processing
        // request and they may be altered by the following optimizations,
        // preserve their values.

        OpRcPtrVec::size_type originalSize = ops.size();
        int total_noops                    = 0;
        int total_inverseops               = 0;
        int total_combines                 = 0;
        int passes                         = 0;

        while (passes <= MAX_OPTIMIZATION_PASSES)
        {
            int noops      = RemoveNoOps(ops);
            int inverseops = RemoveInverseOps(ops);
            int combines   = CombineOps(ops);

            if (noops == 0 && inverseops == 0 && combines == 0)
            {
                // No optimization progress was made, so stop trying.
                break;
            }

            total_noops += noops;
            total_inverseops += inverseops;
            total_combines += combines;

            ++passes;
        }

        if (!ops.empty())
        {
            if((oFlags & OPTIMIZATION_COMP_SEPARABLE_PREFIX)
                    == OPTIMIZATION_COMP_SEPARABLE_PREFIX)
            {
                OptimizeSeparablePrefix(ops, inBitDepth, oFlags);
            }
        }

        OpRcPtrVec::size_type finalSize = ops.size();

        if (passes == MAX_OPTIMIZATION_PASSES)
        {
            std::ostringstream os;
            os << "The max number of passes, " << passes << ", ";
            os << "was reached during optimization. This is likely a sign ";
            os << "that either the complexity of the color transform is ";
            os << "very high, or that some internal optimizers are in conflict ";
            os << "(undo-ing / redo-ing the other's results).";
            LogDebug(os.str());
        }

        if (IsDebugLoggingEnabled())
        {
            std::ostringstream os;
            os << "Optimized ";
            os << originalSize << "->" << finalSize << ", ";
            os << passes << " passes, ";
            os << total_noops << " noops removed, ";
            os << total_inverseops << " inverse ops removed\n";
            os << total_combines << " ops combines\n";
            os << SerializeOpVec(ops, 4);
            LogDebug(os.str());
        }
    }

}
OCIO_NAMESPACE_EXIT

///////////////////////////////////////////////////////////////////////////////

#ifdef OCIO_UNIT_TEST

#include "UnitTest.h"
#include "ops/CDL/CDLOps.h"
#include "ops/Exponent/ExponentOps.h"
#include "ops/Gamma/GammaOps.h"
#include "ops/Log/LogOps.h"
#include "ops/Matrix/MatrixOps.h"
#include "ops/Range/RangeOps.h"
#include "ops/exposurecontrast/ExposureContrastOps.h"

namespace OCIO = OCIO_NAMESPACE;

OCIO_ADD_TEST(OpOptimizers, RemoveInverseOps)
{
    OCIO::OpRcPtrVec ops;

    const double exp[4] = {1.2, 1.3, 1.4, 1.5};

    const double logSlope[3]  = {0.18, 0.18, 0.18};
    const double linSlope[3]  = {2.0, 2.0, 2.0};
    const double linOffset[3] = {0.1, 0.1, 0.1};
    const double base         = 10.0;
    const double logOffset[3] = {1.0, 1.0, 1.0};

    OCIO::CreateExponentOp(ops, exp, OCIO::TRANSFORM_DIR_FORWARD);
    OCIO::CreateLogOp(ops, base, logSlope, logOffset, linSlope, linOffset,
                      OCIO::TRANSFORM_DIR_FORWARD);
    OCIO::CreateLogOp(ops, base, logSlope, logOffset, linSlope, linOffset,
                      OCIO::TRANSFORM_DIR_INVERSE);
    OCIO::CreateExponentOp(ops, exp, OCIO::TRANSFORM_DIR_INVERSE);

    OCIO_CHECK_EQUAL(ops.size(), 4);
    OCIO::RemoveInverseOps(ops);
    OCIO_CHECK_EQUAL(ops.size(), 0);

    ops.clear();
    OCIO::CreateExponentOp(ops, exp, OCIO::TRANSFORM_DIR_FORWARD);
    OCIO::CreateExponentOp(ops, exp, OCIO::TRANSFORM_DIR_INVERSE);
    OCIO::CreateLogOp(ops, base, logSlope, logOffset, linSlope, linOffset,
                      OCIO::TRANSFORM_DIR_INVERSE);
    OCIO::CreateLogOp(ops, base, logSlope, logOffset, linSlope, linOffset,
                      OCIO::TRANSFORM_DIR_FORWARD);
    OCIO::CreateExponentOp(ops, exp, OCIO::TRANSFORM_DIR_FORWARD);

    OCIO_CHECK_EQUAL(ops.size(), 5);
    OCIO::RemoveInverseOps(ops);
    OCIO_CHECK_EQUAL(ops.size(), 1);
}

OCIO_ADD_TEST(OpOptimizers, CombineOps)
{
    double m1[4] = {2.0, 2.0, 2.0, 1.0};
    double m2[4] = {0.5, 0.5, 0.5, 1.0};
    double m3[4] = {0.6, 0.6, 0.6, 1.0};
    double m4[4] = {0.7, 0.7, 0.7, 1.0};

    const double exp[4] = {1.2, 1.3, 1.4, 1.5};

    {
        OCIO::OpRcPtrVec ops;
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_FORWARD);

        OCIO_CHECK_EQUAL(ops.size(), 1);
        OCIO::CombineOps(ops);
        OCIO_CHECK_EQUAL(ops.size(), 1);
    }

    {
        OCIO::OpRcPtrVec ops;
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateScaleOp(ops, m3, OCIO::TRANSFORM_DIR_FORWARD);

        OCIO_CHECK_EQUAL(ops.size(), 2);
        OCIO::CombineOps(ops);
        OCIO_CHECK_EQUAL(ops.size(), 1);
    }

    {
        OCIO::OpRcPtrVec ops;
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateScaleOp(ops, m3, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateScaleOp(ops, m4, OCIO::TRANSFORM_DIR_FORWARD);

        OCIO_CHECK_EQUAL(ops.size(), 3);
        OCIO::CombineOps(ops);
        OCIO_CHECK_EQUAL(ops.size(), 1);
    }

    {
        OCIO::OpRcPtrVec ops;
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateScaleOp(ops, m2, OCIO::TRANSFORM_DIR_FORWARD);

        OCIO_CHECK_EQUAL(ops.size(), 2);
        OCIO::CombineOps(ops);
        OCIO_CHECK_EQUAL(ops.size(), 0);
    }

    {
        OCIO::OpRcPtrVec ops;
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_INVERSE);

        OCIO_CHECK_EQUAL(ops.size(), 2);
        OCIO::CombineOps(ops);
        OCIO_CHECK_EQUAL(ops.size(), 0);
    }

    {
        OCIO::OpRcPtrVec ops;
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_FORWARD);

        OCIO_CHECK_EQUAL(ops.size(), 5);
        OCIO::CombineOps(ops);
        OCIO_CHECK_EQUAL(ops.size(), 1);
    }

    {
        OCIO::OpRcPtrVec ops;
        OCIO::CreateExponentOp(ops, exp, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateScaleOp(ops, m1, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateScaleOp(ops, m2, OCIO::TRANSFORM_DIR_FORWARD);
        OCIO::CreateExponentOp(ops, exp, OCIO::TRANSFORM_DIR_INVERSE);

        OCIO_CHECK_EQUAL(ops.size(), 4);
        OCIO::CombineOps(ops);
        OCIO_CHECK_EQUAL(ops.size(), 0);
    }
}

OCIO_ADD_TEST(OptimizeSeparablePrefix, inexpensive_prefix)
{
    // Test that only inexpensive ops are not replaced.

    OCIO::OpRcPtrVec originalOps;

    OCIO::MatrixOpDataRcPtr matrix = std::make_shared<OCIO::MatrixOpData>();
    matrix->setArrayValue(0, 2.);

    OCIO_CHECK_NO_THROW(OCIO::CreateMatrixOp(originalOps, matrix, OCIO::TRANSFORM_DIR_FORWARD));
    OCIO_REQUIRE_EQUAL(originalOps.size(), 1);

    OCIO::RangeOpDataRcPtr range
        = std::make_shared<OCIO::RangeOpData>(0., 1., -1. / 65535, 65540. / 65535.);

    OCIO_CHECK_NO_THROW(OCIO::CreateRangeOp(originalOps, range, OCIO::TRANSFORM_DIR_FORWARD));
    OCIO_REQUIRE_EQUAL(originalOps.size(), 2);

    OCIO::OpRcPtrVec optimizedOps;
    for (auto op : originalOps)
    {
        optimizedOps.push_back(op->clone());
    }

    // Optimize it.
    OCIO_CHECK_NO_THROW(OCIO::OptimizeSeparablePrefix(optimizedOps,
                                                      OCIO::BIT_DEPTH_UINT8,
                                                      OCIO::OPTIMIZATION_VERY_GOOD));

    // Validate the result.

    OCIO_REQUIRE_EQUAL(optimizedOps.size(), 2U);

    OCIO_CHECK_NO_THROW(FinalizeOpVec(originalOps, OCIO::FINALIZATION_DEFAULT));
    OCIO_CHECK_NO_THROW(FinalizeOpVec(optimizedOps, OCIO::FINALIZATION_DEFAULT));

    OCIO_CHECK_EQUAL(std::string(originalOps[0]->getCacheID()),
                     std::string(optimizedOps[0]->getCacheID()));

    OCIO_CHECK_EQUAL(std::string(originalOps[1]->getCacheID()),
                     std::string(optimizedOps[1]->getCacheID()));
}

namespace
{

void compareRender(OCIO::OpRcPtrVec ops1, OCIO::OpRcPtrVec ops2, unsigned line)
{
    std::vector<float> img1 = {
        51000 / 65535.0f, 54000 / 65535.0f, 58000 / 65535.0f, 10000 / 65535.0f,
        2920 / 65535.0f,  944 / 65535.0f,   57775 / 65535.0f, 65500 / 65535.0f,
        32000 / 65535.0f, 25000 / 65535.0f, 0 / 65535.0f,     0 / 65535.0f,
        65535 / 65535.0f, 10 / 65535.0f,    15000 / 65535.0f, 65535 / 65535.0f};

    std::vector<float> img2 = img1;

    for (const auto & op : ops1)
    {
        op->apply(&img1[0], &img1[0], 4);
    }

    for (const auto & op : ops2)
    {
        op->apply(&img2[0], &img2[0], 4);
    }

    for (size_t idx = 0; idx < img1.size(); ++idx)
    {
        OCIO_CHECK_CLOSE_FROM(img1[idx], img2[idx], 2e-5f, line);
    }
}
} // namespace

OCIO_ADD_TEST(OptimizeSeparablePrefix, gamma_prefix)
{
    OCIO::OpRcPtrVec originalOps;

    OCIO::GammaOpData::Params params1 = {2.6};
    OCIO::GammaOpData::Params paramsA = {1.};

    OCIO::GammaOpDataRcPtr gamma1
        = std::make_shared<OCIO::GammaOpData>(OCIO::GammaOpData::BASIC_REV, 
                                              params1, params1, params1, paramsA);

    OCIO_CHECK_NO_THROW(OCIO::CreateGammaOp(originalOps, gamma1, OCIO::TRANSFORM_DIR_FORWARD));
    OCIO_REQUIRE_EQUAL(originalOps.size(), 1);

    OCIO::OpRcPtrVec optimizedOps;
    for (auto op : originalOps)
    {
        optimizedOps.push_back(op->clone());
    }

    // Optimize it.
    OCIO_CHECK_NO_THROW(OCIO::OptimizeSeparablePrefix(optimizedOps,
                                                      OCIO::BIT_DEPTH_UINT16,
                                                      OCIO::OPTIMIZATION_VERY_GOOD));

    // Validate the result.

    OCIO_REQUIRE_EQUAL(optimizedOps.size(), 1);

    OCIO::ConstOpRcPtr o1              = optimizedOps[0];
    OCIO::ConstLut1DOpDataRcPtr oData1 = OCIO::DynamicPtrCast<const OCIO::Lut1DOpData>(o1->data());
    OCIO_REQUIRE_ASSERT(oData1);
    OCIO_CHECK_EQUAL(oData1->getType(), OCIO::OpData::Lut1DType);
    OCIO_CHECK_EQUAL(oData1->getArray().getLength(), 65536);

    OCIO_CHECK_NO_THROW(FinalizeOpVec(originalOps, OCIO::FINALIZATION_DEFAULT));
    OCIO_CHECK_NO_THROW(FinalizeOpVec(optimizedOps, OCIO::FINALIZATION_DEFAULT));

    compareRender(originalOps, optimizedOps, __LINE__);

    // However, if the input bit depth is F32, it should not be optimized.

    originalOps.clear();

    OCIO::GammaOpDataRcPtr gamma2
        = std::make_shared<OCIO::GammaOpData>(OCIO::GammaOpData::BASIC_REV, 
                                              params1, params1, params1, paramsA);

    OCIO_CHECK_NO_THROW(OCIO::CreateGammaOp(originalOps, gamma2, OCIO::TRANSFORM_DIR_FORWARD));
    OCIO_REQUIRE_EQUAL(originalOps.size(), 1);

    // Optimize it.
    OCIO_CHECK_NO_THROW(OCIO::OptimizeSeparablePrefix(originalOps,
                                                      OCIO::BIT_DEPTH_F32,
                                                      OCIO::OPTIMIZATION_VERY_GOOD));

    OCIO_REQUIRE_EQUAL(originalOps.size(), 1);
    OCIO::ConstOpRcPtr o2 = originalOps[0];
    OCIO_CHECK_EQUAL(o2->data()->getType(), OCIO::OpData::GammaType);
}

OCIO_ADD_TEST(OptimizeSeparablePrefix, multi_op_prefix)
{
    // Test prefix optimization of a complex transform.

    OCIO::OpRcPtrVec originalOps;

    OCIO::MatrixOpDataRcPtr matrix = std::make_shared<OCIO::MatrixOpData>();
    matrix->setArrayValue(0, 2.);

    OCIO_CHECK_NO_THROW(OCIO::CreateMatrixOp(originalOps, matrix, OCIO::TRANSFORM_DIR_FORWARD));
    OCIO_REQUIRE_EQUAL(originalOps.size(), 1);

    OCIO::RangeOpDataRcPtr range
        = std::make_shared<OCIO::RangeOpData>(0., 1., -1000./65535., 66000./65535);

    OCIO_CHECK_NO_THROW(OCIO::CreateRangeOp(originalOps, range, OCIO::TRANSFORM_DIR_FORWARD));
    OCIO_REQUIRE_EQUAL(originalOps.size(), 2);

    const OCIO::CDLOpData::ChannelParams slope(1.35, 1.1, 0.071);
    const OCIO::CDLOpData::ChannelParams offset(0.05, -0.23, 0.11);
    const OCIO::CDLOpData::ChannelParams power(1.27, 0.81, 0.2);
    const double saturation = 1.;

    OCIO::CDLOpDataRcPtr cdl
        = std::make_shared<OCIO::CDLOpData>(OCIO::CDLOpData::CDL_V1_2_FWD, 
                                            slope, offset, power, saturation);

    OCIO_CHECK_NO_THROW(OCIO::CreateCDLOp(originalOps, cdl, OCIO::TRANSFORM_DIR_FORWARD));
    OCIO_REQUIRE_EQUAL(originalOps.size(), 3);

    OCIO::OpRcPtrVec optimizedOps = originalOps;

    // Optimize it.
    OCIO_CHECK_NO_THROW(OCIO::OptimizeSeparablePrefix(optimizedOps,
                                                      OCIO::BIT_DEPTH_UINT8,
                                                      OCIO::OPTIMIZATION_VERY_GOOD));

    // Validate the result.

    OCIO_REQUIRE_EQUAL(optimizedOps.size(), 1U);

    OCIO::ConstOpRcPtr o              = optimizedOps[0];
    OCIO::ConstLut1DOpDataRcPtr oData = OCIO::DynamicPtrCast<const OCIO::Lut1DOpData>(o->data());
    OCIO_CHECK_EQUAL(oData->getType(), OCIO::OpData::Lut1DType);
    OCIO_CHECK_EQUAL(oData->getArray().getLength(), 256);

    OCIO_CHECK_NO_THROW(FinalizeOpVec(originalOps, OCIO::FINALIZATION_DEFAULT));
    OCIO_CHECK_NO_THROW(FinalizeOpVec(optimizedOps, OCIO::FINALIZATION_DEFAULT));

    compareRender(originalOps, optimizedOps, __LINE__);
}

OCIO_ADD_TEST(OptimizeSeparablePrefix, op_with_dyn_properties)
{
    // Test prefix optimization of a complex transform.

    OCIO::OpRcPtrVec originalOps;

    OCIO::MatrixOpDataRcPtr matrix = std::make_shared<OCIO::MatrixOpData>();
    matrix->setArrayValue(0, 2.);

    OCIO_CHECK_NO_THROW(OCIO::CreateMatrixOp(originalOps, matrix, OCIO::TRANSFORM_DIR_FORWARD));
    OCIO_REQUIRE_EQUAL(originalOps.size(), 1);

    OCIO::ExposureContrastOpDataRcPtr exposure = std::make_shared<OCIO::ExposureContrastOpData>();

    exposure->setExposure(1.2);
    exposure->setPivot(0.5);

    OCIO_CHECK_NO_THROW(
        OCIO::CreateExposureContrastOp(originalOps, exposure, OCIO::TRANSFORM_DIR_FORWARD));
    OCIO_REQUIRE_EQUAL(originalOps.size(), 2);

    exposure = exposure->clone();
    exposure->getExposureProperty()->makeDynamic();

    OCIO_CHECK_NO_THROW(
        OCIO::CreateExposureContrastOp(originalOps, exposure, OCIO::TRANSFORM_DIR_FORWARD));
    OCIO_REQUIRE_EQUAL(originalOps.size(), 3);

    // Optimize it.

    OCIO_CHECK_NO_THROW(OCIO::OptimizeSeparablePrefix(originalOps,
                                                      OCIO::BIT_DEPTH_UINT8,
                                                      OCIO::OPTIMIZATION_VERY_GOOD));

    // Validate the result.

    OCIO_REQUIRE_EQUAL(originalOps.size(), 2U);

    OCIO::ConstOpRcPtr o              = originalOps[0];
    OCIO::ConstLut1DOpDataRcPtr oData = OCIO::DynamicPtrCast<const OCIO::Lut1DOpData>(o->data());
    OCIO_CHECK_ASSERT(oData);
    OCIO_CHECK_EQUAL(oData->getType(), OCIO::OpData::Lut1DType);

    o = originalOps[1];
    OCIO::ConstExposureContrastOpDataRcPtr exp =
        OCIO::DynamicPtrCast<const OCIO::ExposureContrastOpData>(o->data());

    OCIO_CHECK_ASSERT(exp);
    OCIO_CHECK_EQUAL(exp->getType(), OCIO::OpData::ExposureContrastType);
    OCIO_CHECK_ASSERT(exp->isDynamic());
}

// TODO: Add separable prefix tests that mix in more non-separable ops.

// TODO: Add synColor unit tests opt_prefix_test1

#endif // OCIO_UNIT_TEST
