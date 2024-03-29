// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <sstream>

#include <OpenColorIO/OpenColorIO.h>

#include "BitDepthUtils.h"
#include "ops/CDL/CDLOpData.h"
#include "ops/Matrix/MatrixOps.h"
#include "ops/Range/RangeOpData.h"
#include "Platform.h"


OCIO_NAMESPACE_ENTER
{

namespace DefaultValues
{
    const int FLOAT_DECIMALS = 7;
}


static const CDLOpData::ChannelParams kOneParams(1.0);
static const CDLOpData::ChannelParams kZeroParams(0.0);

// Original CTF styles:
static const char V1_2_FWD_NAME[] = "v1.2_Fwd";
static const char V1_2_REV_NAME[] = "v1.2_Rev";
static const char NO_CLAMP_FWD_NAME[] = "noClampFwd";
static const char NO_CLAMP_REV_NAME[] = "noClampRev";

// CLF styles (also allowed now in CTF):
static const char V1_2_FWD_CLF_NAME[] = "Fwd";
static const char V1_2_REV_CLF_NAME[] = "Rev";
static const char NO_CLAMP_FWD_CLF_NAME[] = "FwdNoClamp";
static const char NO_CLAMP_REV_CLF_NAME[] = "RevNoClamp";

CDLOpData::Style CDLOpData::GetStyle(const char* name)
{

// Get the style enum  of the CDL from the name stored
// in the "name" variable.
#define RETURN_STYLE_FROM_NAME(CDL_STYLE_NAME, CDL_STYLE)  \
if( 0==Platform::Strcasecmp(name, CDL_STYLE_NAME) )        \
{                                                          \
return CDL_STYLE;                                          \
}

    if (name && *name)
    {
        RETURN_STYLE_FROM_NAME(V1_2_FWD_NAME, CDL_V1_2_FWD);
        RETURN_STYLE_FROM_NAME(V1_2_FWD_CLF_NAME, CDL_V1_2_FWD);
        RETURN_STYLE_FROM_NAME(V1_2_REV_NAME, CDL_V1_2_REV);
        RETURN_STYLE_FROM_NAME(V1_2_REV_CLF_NAME, CDL_V1_2_REV);
        RETURN_STYLE_FROM_NAME(NO_CLAMP_FWD_NAME, CDL_NO_CLAMP_FWD);
        RETURN_STYLE_FROM_NAME(NO_CLAMP_FWD_CLF_NAME, CDL_NO_CLAMP_FWD);
        RETURN_STYLE_FROM_NAME(NO_CLAMP_REV_NAME, CDL_NO_CLAMP_REV);
        RETURN_STYLE_FROM_NAME(NO_CLAMP_REV_CLF_NAME, CDL_NO_CLAMP_REV);
    }

#undef RETURN_STYLE_FROM_NAME

    throw Exception("Unknown style for CDL.");
}

const char * CDLOpData::GetStyleName(CDLOpData::Style style)
{
    // Get the name of the CDL style from the enum stored
    // in the "style" variable.
    switch (style)
    {
        case CDL_V1_2_FWD:     return V1_2_FWD_CLF_NAME;
        case CDL_V1_2_REV:     return V1_2_REV_CLF_NAME;
        case CDL_NO_CLAMP_FWD: return NO_CLAMP_FWD_CLF_NAME;
        case CDL_NO_CLAMP_REV: return NO_CLAMP_REV_CLF_NAME;
    }

    throw Exception("Unknown style for CDL.");
}

CDLOpData::CDLOpData()
    :   OpData()
    ,   m_style(GetDefaultStyle())
    ,   m_slopeParams(1.0)
    ,   m_offsetParams(0.0)
    ,   m_powerParams(1.0)
    ,   m_saturation(1.0)
{
}

CDLOpData::CDLOpData(const CDLOpData::Style & style,
                     const ChannelParams & slopeParams,
                     const ChannelParams & offsetParams,
                     const ChannelParams & powerParams,
                     const double saturation)
    :   OpData()
    ,   m_style(style)
    ,   m_slopeParams(slopeParams)
    ,   m_offsetParams(offsetParams)
    ,   m_powerParams(powerParams)
    ,   m_saturation(saturation)
{
    validate();
}

CDLOpData::~CDLOpData()
{
}

CDLOpDataRcPtr CDLOpData::clone() const
{
    return std::make_shared<CDLOpData>(*this);
}

bool CDLOpData::operator==(const OpData& other) const
{
    if (this == &other) return true;

    if (!OpData::operator==(other)) return false;

    const CDLOpData* cdl = static_cast<const CDLOpData*>(&other);

    return m_style        == cdl->m_style 
        && m_slopeParams  == cdl->m_slopeParams
        && m_offsetParams == cdl->m_offsetParams
        && m_powerParams  == cdl->m_powerParams
        && m_saturation   == cdl->m_saturation;
}

void CDLOpData::setStyle(const CDLOpData::Style & style)
{
    m_style = style;
}

void CDLOpData::setSlopeParams(const ChannelParams & slopeParams)
{
    m_slopeParams = slopeParams;
}

void CDLOpData::setOffsetParams(const ChannelParams & offsetParams)
{
    m_offsetParams = offsetParams;
}

void CDLOpData::setPowerParams(const ChannelParams & powerParams)
{
    m_powerParams = powerParams;
}

void CDLOpData::setSaturation(const double saturation)
{
    m_saturation = saturation;
}

// Validate if a parameter is greater than or equal to threshold value.
void validateGreaterEqual(const char * name, 
                          const double value, 
                          const double threshold)
{
    if (!(value >= threshold))
    {
        std::ostringstream oss;
        oss << "CDL: Invalid '";
        oss << name;
        oss << "' " << value;
        oss << " should be greater than ";
        oss << threshold << ".";
        throw Exception(oss.str().c_str());
    }
}

// Validate if a parameter is greater than a threshold value.
void validateGreaterThan(const char * name, 
                        const double value, 
                         const double threshold)
{ 
    if (!(value > threshold))
    {
        std::ostringstream oss;
        oss << "CDLOpData: Invalid '";
        oss << name;
        oss << "' " << value;
        oss << " should be greater than ";
        oss << threshold << ".";
        throw Exception(oss.str().c_str());
    }
}

typedef void(*parameter_validation_function)(const char *, 
                                             const double, 
                                             const double);

template<parameter_validation_function fnVal>
void validateChannelParams(const char * name, 
                           const CDLOpData::ChannelParams& params,
                           double threshold)
{
    for (unsigned i = 0; i < 3; ++i)
    {
        fnVal(name, params[i], threshold);
    }
}

// Validate number of SOP parameters and saturation.
// The ASC v1.2 spec 2009-05-04 places the following restrictions:
//   slope >= 0, power > 0, sat >= 0, (offset unbounded).
void validateParams(const CDLOpData::ChannelParams& slopeParams,
                    const CDLOpData::ChannelParams& powerParams,
                    const double saturation)
{
    // slope >= 0
    validateChannelParams<validateGreaterEqual>("slope", slopeParams, 0.0);

    // power > 0
    validateChannelParams<validateGreaterThan>("power", powerParams, 0.0);

    // saturation >= 0
    validateGreaterEqual("saturation", saturation, 0.0);
}

bool CDLOpData::isNoOp() const
{
    return isIdentity()
        && !isClamping();
}

bool CDLOpData::isIdentity() const
{
    return  m_slopeParams  == kOneParams  &&
            m_offsetParams == kZeroParams &&
            m_powerParams  == kOneParams  &&
            m_saturation   == 1.0;
}

OpDataRcPtr CDLOpData::getIdentityReplacement() const
{
    OpDataRcPtr op;
    switch(getStyle())
    {
        // These clamp values below 0 -- replace with range.
        case CDL_V1_2_FWD:
        case CDL_V1_2_REV:
        {
            op = std::make_shared<RangeOpData>(0.,
                                               RangeOpData::EmptyValue(), // don't clamp high end
                                               0.,
                                               RangeOpData::EmptyValue());
            break;
        }

        // These pass through the full range of values -- replace with matrix.
        case CDL_NO_CLAMP_FWD:
        case CDL_NO_CLAMP_REV:
        {
            op = std::make_shared<MatrixOpData>();
            break;
        }
    }
    op->getFormatMetadata() = getFormatMetadata();
    return op;
}

bool CDLOpData::hasChannelCrosstalk() const
{
    return m_saturation != 1.0;
}

void CDLOpData::validate() const
{
    OpData::validate();

    validateParams(m_slopeParams, m_powerParams, m_saturation);
}

std::string CDLOpData::getSlopeString() const
{
    return GetChannelParametersString(m_slopeParams);
}

std::string CDLOpData::getOffsetString() const
{
    return GetChannelParametersString(m_offsetParams);
}

std::string CDLOpData::getPowerString() const
{
    return GetChannelParametersString(m_powerParams);
}

std::string CDLOpData::getSaturationString() const
{
    std::ostringstream oss;
    oss.precision(DefaultValues::FLOAT_DECIMALS);
    oss << m_saturation;
    return oss.str();
}

bool CDLOpData::isReverse() const
{
    const CDLOpData::Style style = getStyle();
    switch (style)
    {
        case CDLOpData::CDL_V1_2_FWD:     return false;
        case CDLOpData::CDL_V1_2_REV:     return true;
        case CDLOpData::CDL_NO_CLAMP_FWD: return false;
        case CDLOpData::CDL_NO_CLAMP_REV: return true;
    }
    return false;
}

bool CDLOpData::isClamping() const
{
    const CDLOpData::Style style = getStyle();
    switch (style)
    {
        case CDLOpData::CDL_V1_2_FWD:     return true;
        case CDLOpData::CDL_V1_2_REV:     return true;
        case CDLOpData::CDL_NO_CLAMP_FWD: return false;
        case CDLOpData::CDL_NO_CLAMP_REV: return false;
    }
    return false;
}

std::string CDLOpData::GetChannelParametersString(ChannelParams params)
{
    std::ostringstream oss;
    oss.precision(DefaultValues::FLOAT_DECIMALS);
    oss << params[0] << ", " << params[1] << ", " << params[2];
    return oss.str();
}

bool CDLOpData::isInverse(ConstCDLOpDataRcPtr & r) const
{
    return *r == *inverse();
}

CDLOpDataRcPtr CDLOpData::inverse() const
{
    CDLOpDataRcPtr cdl = clone();

    switch(cdl->getStyle())
    {
        case CDL_V1_2_FWD: cdl->setStyle(CDL_V1_2_REV); break;
        case CDL_V1_2_REV: cdl->setStyle(CDL_V1_2_FWD); break;
        case CDL_NO_CLAMP_FWD: cdl->setStyle(CDL_NO_CLAMP_REV); break;
        case CDL_NO_CLAMP_REV: cdl->setStyle(CDL_NO_CLAMP_FWD); break;
    }
    
    // Note that any existing metadata could become stale at this point but
    // trying to update it is also challenging since inverse() is sometimes
    // called even during the creation of new ops.
    return cdl;
}

void CDLOpData::finalize()
{
    AutoMutex lock(m_mutex);

    validate();

    std::ostringstream cacheIDStream;
    cacheIDStream << getID() << " ";

    cacheIDStream.precision(DefaultValues::FLOAT_DECIMALS);

    cacheIDStream << GetStyleName(getStyle()) << " ";
    cacheIDStream << getSlopeString() << " ";
    cacheIDStream << getOffsetString() << " ";
    cacheIDStream << getPowerString() << " ";
    cacheIDStream << getSaturationString() << " ";

    m_cacheID = cacheIDStream.str();
}

}
OCIO_NAMESPACE_EXIT


#ifdef OCIO_UNIT_TEST

namespace OCIO = OCIO_NAMESPACE;
#include "UnitTest.h"

OCIO_ADD_TEST(CDLOpData, accessors)
{
    OCIO::CDLOpData::ChannelParams slopeParams(1.35, 1.1, 0.71);
    OCIO::CDLOpData::ChannelParams offsetParams(0.05, -0.23, 0.11);
    OCIO::CDLOpData::ChannelParams powerParams(0.93, 0.81, 1.27);

    OCIO::CDLOpData cdlOp(OCIO::CDLOpData::CDL_V1_2_FWD,
                          slopeParams, offsetParams, powerParams, 1.23);

    // Update slope parameters with the same value.
    OCIO::CDLOpData::ChannelParams newSlopeParams(0.66);
    cdlOp.setSlopeParams(newSlopeParams);

    OCIO_CHECK_ASSERT(cdlOp.getSlopeParams() == newSlopeParams);
    OCIO_CHECK_ASSERT(cdlOp.getOffsetParams() == offsetParams);
    OCIO_CHECK_ASSERT(cdlOp.getPowerParams() == powerParams);
    OCIO_CHECK_EQUAL(cdlOp.getSaturation(), 1.23);

    // Update offset parameters with the same value.
    OCIO::CDLOpData::ChannelParams newOffsetParams(0.09);
    cdlOp.setOffsetParams(newOffsetParams);

    OCIO_CHECK_ASSERT(cdlOp.getSlopeParams() == newSlopeParams);
    OCIO_CHECK_ASSERT(cdlOp.getOffsetParams() == newOffsetParams);
    OCIO_CHECK_ASSERT(cdlOp.getPowerParams() == powerParams);
    OCIO_CHECK_EQUAL(cdlOp.getSaturation(), 1.23);

    // Update power parameters with the same value.
    OCIO::CDLOpData::ChannelParams newPowerParams(1.1);
    cdlOp.setPowerParams(newPowerParams);

    OCIO_CHECK_ASSERT(cdlOp.getSlopeParams() == newSlopeParams);
    OCIO_CHECK_ASSERT(cdlOp.getOffsetParams() == newOffsetParams);
    OCIO_CHECK_ASSERT(cdlOp.getPowerParams() == newPowerParams);
    OCIO_CHECK_EQUAL(cdlOp.getSaturation(), 1.23);

    // Update the saturation parameter.
    cdlOp.setSaturation(0.99);

    OCIO_CHECK_ASSERT(cdlOp.getSlopeParams() == newSlopeParams);
    OCIO_CHECK_ASSERT(cdlOp.getOffsetParams() == newOffsetParams);
    OCIO_CHECK_ASSERT(cdlOp.getPowerParams() == newPowerParams);
    OCIO_CHECK_EQUAL(cdlOp.getSaturation(), 0.99);

}

OCIO_ADD_TEST(CDLOpData, constructors)
{
    // Check default constructor.
    OCIO::CDLOpData cdlOpDefault;

    OCIO_CHECK_EQUAL(cdlOpDefault.getType(), OCIO::CDLOpData::CDLType);

    OCIO_CHECK_EQUAL(cdlOpDefault.getID(), "");
    OCIO_CHECK_ASSERT(cdlOpDefault.getFormatMetadata().getChildrenElements().empty());

    OCIO_CHECK_EQUAL(cdlOpDefault.getStyle(),
                     OCIO::CDLOpData::CDL_V1_2_FWD);

    OCIO_CHECK_ASSERT(!cdlOpDefault.isReverse());

    OCIO_CHECK_ASSERT(cdlOpDefault.getSlopeParams()
        == OCIO::CDLOpData::ChannelParams(1.0));
    OCIO_CHECK_ASSERT(cdlOpDefault.getOffsetParams() 
        == OCIO::CDLOpData::ChannelParams(0.0));
    OCIO_CHECK_ASSERT(cdlOpDefault.getPowerParams()
        == OCIO::CDLOpData::ChannelParams(1.0));
    OCIO_CHECK_EQUAL(cdlOpDefault.getSaturation(), 1.0);

    // Check complete constructor.
    OCIO::CDLOpData cdlOpComplete(OCIO::CDLOpData::CDL_NO_CLAMP_REV,
                                  OCIO::CDLOpData::ChannelParams(1.35, 1.1, 0.71),
                                  OCIO::CDLOpData::ChannelParams(0.05, -0.23, 0.11),
                                  OCIO::CDLOpData::ChannelParams(0.93, 0.81, 1.27),
                                  1.23);

    auto & metadata = cdlOpComplete.getFormatMetadata();
    metadata.addAttribute(OCIO::METADATA_NAME, "cdl-name");
    metadata.addAttribute(OCIO::METADATA_ID, "cdl-id");

    OCIO_CHECK_EQUAL(cdlOpComplete.getName(), "cdl-name");
    OCIO_CHECK_EQUAL(cdlOpComplete.getID(), "cdl-id");

    OCIO_CHECK_EQUAL(cdlOpComplete.getType(), OCIO::OpData::CDLType);

    OCIO_CHECK_EQUAL(cdlOpComplete.getStyle(),
                     OCIO::CDLOpData::CDL_NO_CLAMP_REV);

    OCIO_CHECK_ASSERT(cdlOpComplete.isReverse());

    OCIO_CHECK_ASSERT(cdlOpComplete.getSlopeParams()
        == OCIO::CDLOpData::ChannelParams(1.35, 1.1, 0.71));
    OCIO_CHECK_ASSERT(cdlOpComplete.getOffsetParams()
        == OCIO::CDLOpData::ChannelParams(0.05, -0.23, 0.11));
    OCIO_CHECK_ASSERT(cdlOpComplete.getPowerParams()
        == OCIO::CDLOpData::ChannelParams(0.93, 0.81, 1.27));
    OCIO_CHECK_EQUAL(cdlOpComplete.getSaturation(), 1.23);
}

OCIO_ADD_TEST(CDLOpData, inverse)
{
    OCIO::CDLOpData cdlOp(OCIO::CDLOpData::CDL_V1_2_FWD,
                          OCIO::CDLOpData::ChannelParams(1.35, 1.1, 0.71),
                          OCIO::CDLOpData::ChannelParams(0.05, -0.23, 0.11),
                          OCIO::CDLOpData::ChannelParams(0.93, 0.81, 1.27),
                          1.23);
    cdlOp.getFormatMetadata().addAttribute(OCIO::METADATA_ID, "test_id");
    cdlOp.getFormatMetadata().addChildElement(OCIO::METADATA_DESCRIPTION, "Inverse op test description");

    // Test CDL_V1_2_FWD inverse
    {
        cdlOp.setStyle(OCIO::CDLOpData::CDL_V1_2_FWD);
        const OCIO::CDLOpDataRcPtr invOp = cdlOp.inverse();

        // Ensure metadata is copied
        OCIO_CHECK_EQUAL(invOp->getID(), "test_id");
        OCIO_REQUIRE_EQUAL(invOp->getFormatMetadata().getChildrenElements().size(), 1);
        OCIO_CHECK_EQUAL(std::string(OCIO::METADATA_DESCRIPTION),
                         invOp->getFormatMetadata().getChildrenElements()[0].getName());
        OCIO_CHECK_EQUAL(std::string("Inverse op test description"),
                         invOp->getFormatMetadata().getChildrenElements()[0].getValue());

        // Ensure style is inverted
        OCIO_CHECK_EQUAL(invOp->getStyle(), OCIO::CDLOpData::CDL_V1_2_REV);

        OCIO_CHECK_ASSERT(invOp->isReverse());

        // Ensure CDL parameters are unchanged
        OCIO_CHECK_ASSERT(invOp->getSlopeParams()
                          == OCIO::CDLOpData::ChannelParams(1.35, 1.1, 0.71));

        OCIO_CHECK_ASSERT(invOp->getOffsetParams()
                         == OCIO::CDLOpData::ChannelParams(0.05, -0.23, 0.11));

        OCIO_CHECK_ASSERT(invOp->getPowerParams()
                          == OCIO::CDLOpData::ChannelParams(0.93, 0.81, 1.27));

        OCIO_CHECK_EQUAL(invOp->getSaturation(), 1.23);
    }

    // Test CDL_V1_2_REV inverse
    {
        cdlOp.setStyle(OCIO::CDLOpData::CDL_V1_2_REV);
        const OCIO::CDLOpDataRcPtr invOp = cdlOp.inverse();

        // Ensure metadata is copied
        OCIO_CHECK_EQUAL(invOp->getID(), "test_id");
        OCIO_CHECK_EQUAL(invOp->getFormatMetadata().getChildrenElements().size(), 1);

        // Ensure style is inverted
        OCIO_CHECK_EQUAL(invOp->getStyle(), OCIO::CDLOpData::CDL_V1_2_FWD);

        OCIO_CHECK_EQUAL(invOp->isReverse(), false);

        // Ensure CDL parameters are unchanged
        OCIO_CHECK_ASSERT(invOp->getSlopeParams()
                          == OCIO::CDLOpData::ChannelParams(1.35, 1.1, 0.71));

        OCIO_CHECK_ASSERT(invOp->getOffsetParams()
                          == OCIO::CDLOpData::ChannelParams(0.05, -0.23, 0.11));

        OCIO_CHECK_ASSERT(invOp->getPowerParams()
                          == OCIO::CDLOpData::ChannelParams(0.93, 0.81, 1.27));

        OCIO_CHECK_EQUAL(invOp->getSaturation(), 1.23);
    }

    // Test CDL_NO_CLAMP_FWD inverse
    {
        cdlOp.setStyle(OCIO::CDLOpData::CDL_NO_CLAMP_FWD);
        const OCIO::CDLOpDataRcPtr invOp = cdlOp.inverse();

        // Ensure metadata is copied
        OCIO_CHECK_EQUAL(invOp->getID(), "test_id");
        OCIO_CHECK_EQUAL(invOp->getFormatMetadata().getChildrenElements().size(), 1);

        // Ensure style is inverted
        OCIO_CHECK_EQUAL(invOp->getStyle(), OCIO::CDLOpData::CDL_NO_CLAMP_REV);
        OCIO_CHECK_ASSERT(invOp->isReverse());

        // Ensure CDL parameters are unchanged
        OCIO_CHECK_ASSERT(invOp->getSlopeParams()
                           == OCIO::CDLOpData::ChannelParams(1.35, 1.1, 0.71));

        OCIO_CHECK_ASSERT(invOp->getOffsetParams()
                           == OCIO::CDLOpData::ChannelParams(0.05, -0.23, 0.11));

        OCIO_CHECK_ASSERT(invOp->getPowerParams()
                           == OCIO::CDLOpData::ChannelParams(0.93, 0.81, 1.27));

        OCIO_CHECK_EQUAL(invOp->getSaturation(), 1.23);
    }

    // Test CDL_NO_CLAMP_REV inverse
    {
        cdlOp.setStyle(OCIO::CDLOpData::CDL_NO_CLAMP_REV);
        const OCIO::CDLOpDataRcPtr invOp = cdlOp.inverse();

        // Ensure metadata is copied
        OCIO_CHECK_EQUAL(invOp->getID(), "test_id");
        OCIO_CHECK_EQUAL(invOp->getFormatMetadata().getChildrenElements().size(), 1);

        // Ensure style is inverted
        OCIO_CHECK_EQUAL(invOp->getStyle(), OCIO::CDLOpData::CDL_NO_CLAMP_FWD);
        OCIO_CHECK_ASSERT(!invOp->isReverse());

        // Ensure CDL parameters are unchanged
        OCIO_CHECK_ASSERT(invOp->getSlopeParams()
                          == OCIO::CDLOpData::ChannelParams(1.35, 1.1, 0.71));

        OCIO_CHECK_ASSERT(invOp->getOffsetParams()
                          == OCIO::CDLOpData::ChannelParams(0.05, -0.23, 0.11));

        OCIO_CHECK_ASSERT(invOp->getPowerParams()
                          == OCIO::CDLOpData::ChannelParams(0.93, 0.81, 1.27));

        OCIO_CHECK_EQUAL(invOp->getSaturation(), 1.23);
    }
}


OCIO_ADD_TEST(CDLOpData, style)
{
    // Check default constructor
    OCIO::CDLOpData cdlOp;

    // Check CDL_V1_2_FWD
    cdlOp.setStyle(OCIO::CDLOpData::CDL_V1_2_FWD);
    OCIO_CHECK_EQUAL(cdlOp.getStyle(), OCIO::CDLOpData::CDL_V1_2_FWD);
    OCIO_CHECK_ASSERT(!cdlOp.isReverse());

    // Check CDL_V1_2_REV
    cdlOp.setStyle(OCIO::CDLOpData::CDL_V1_2_REV);
    OCIO_CHECK_EQUAL(cdlOp.getStyle(), OCIO::CDLOpData::CDL_V1_2_REV);
    OCIO_CHECK_ASSERT(cdlOp.isReverse());

    // Check CDL_NO_CLAMP_FWD
    cdlOp.setStyle(OCIO::CDLOpData::CDL_NO_CLAMP_FWD);
    OCIO_CHECK_EQUAL(cdlOp.getStyle(), OCIO::CDLOpData::CDL_NO_CLAMP_FWD);
    OCIO_CHECK_ASSERT(!cdlOp.isReverse());

    // Check CDL_NO_CLAMP_REV
    cdlOp.setStyle(OCIO::CDLOpData::CDL_NO_CLAMP_REV);
    OCIO_CHECK_EQUAL(cdlOp.getStyle(), OCIO::CDLOpData::CDL_NO_CLAMP_REV);
    OCIO_CHECK_ASSERT(cdlOp.isReverse());

    // Check unknown style
    OCIO_CHECK_THROW_WHAT(OCIO::CDLOpData::GetStyle("unknown_style"),
                          OCIO::Exception, 
                          "Unknown style for CDL");
}


OCIO_ADD_TEST(CDLOpData, validation_success)
{
    OCIO::CDLOpData cdlOp;

    // Set valid parameters
    const OCIO::CDLOpData::ChannelParams slopeParams(1.15);
    const OCIO::CDLOpData::ChannelParams offsetParams(-0.02);
    const OCIO::CDLOpData::ChannelParams powerParams(0.97);

    cdlOp.setStyle(OCIO::CDLOpData::CDL_V1_2_FWD);

    cdlOp.setSlopeParams(slopeParams);
    cdlOp.setOffsetParams(offsetParams);
    cdlOp.setPowerParams(powerParams);
    cdlOp.setSaturation(1.22);

    OCIO_CHECK_ASSERT(!cdlOp.isIdentity());
    OCIO_CHECK_ASSERT(!cdlOp.isNoOp());

    OCIO_CHECK_NO_THROW(cdlOp.validate());

    // Set an identity operation
    cdlOp.setSlopeParams(OCIO::kOneParams);
    cdlOp.setOffsetParams(OCIO::kZeroParams);
    cdlOp.setPowerParams(OCIO::kOneParams);
    cdlOp.setSaturation(1.0);

    OCIO_CHECK_ASSERT(cdlOp.isIdentity());
    OCIO_CHECK_ASSERT(!cdlOp.isNoOp());
    // Set to non clamping
    cdlOp.setStyle(OCIO::CDLOpData::CDL_NO_CLAMP_FWD);
    OCIO_CHECK_ASSERT(cdlOp.isIdentity());
    OCIO_CHECK_ASSERT(cdlOp.isNoOp());

    OCIO_CHECK_NO_THROW(cdlOp.validate());

    // Check for slope = 0
    cdlOp.setSlopeParams(OCIO::CDLOpData::ChannelParams(0.0));
    cdlOp.setOffsetParams(offsetParams);
    cdlOp.setPowerParams(powerParams);
    cdlOp.setSaturation(1.0);

    cdlOp.setStyle(OCIO::CDLOpData::CDL_V1_2_FWD);

    OCIO_CHECK_ASSERT(!cdlOp.isIdentity());
    OCIO_CHECK_ASSERT(!cdlOp.isNoOp());

    OCIO_CHECK_NO_THROW(cdlOp.validate());

    // Check for saturation = 0
    cdlOp.setSlopeParams(slopeParams);
    cdlOp.setOffsetParams(offsetParams);
    cdlOp.setPowerParams(powerParams);
    cdlOp.setSaturation(0.0);

    OCIO_CHECK_ASSERT(!cdlOp.isIdentity());
    OCIO_CHECK_ASSERT(!cdlOp.isNoOp());

    OCIO_CHECK_NO_THROW(cdlOp.validate());
}

OCIO_ADD_TEST(CDLOpData, validation_failure)
{
    OCIO::CDLOpData cdlOp;

    // Fail: invalid scale
    cdlOp.setSlopeParams(OCIO::CDLOpData::ChannelParams(-0.9));
    cdlOp.setOffsetParams(OCIO::CDLOpData::ChannelParams(0.01));
    cdlOp.setPowerParams(OCIO::CDLOpData::ChannelParams(1.2));
    cdlOp.setSaturation(1.17);

    OCIO_CHECK_THROW_WHAT(cdlOp.validate(), OCIO::Exception, "should be greater than 0");

    // Fail: invalid power
    cdlOp.setSlopeParams(OCIO::CDLOpData::ChannelParams(0.9));
    cdlOp.setOffsetParams(OCIO::CDLOpData::ChannelParams(0.01));
    cdlOp.setPowerParams(OCIO::CDLOpData::ChannelParams(-1.2));
    cdlOp.setSaturation(1.17);

    OCIO_CHECK_THROW_WHAT(cdlOp.validate(), OCIO::Exception, "should be greater than 0");

    // Fail: invalid saturation
    cdlOp.setSlopeParams(OCIO::CDLOpData::ChannelParams(0.9));
    cdlOp.setOffsetParams(OCIO::CDLOpData::ChannelParams(0.01));
    cdlOp.setPowerParams(OCIO::CDLOpData::ChannelParams(1.2));
    cdlOp.setSaturation(-1.17);

    OCIO_CHECK_THROW_WHAT(cdlOp.validate(), OCIO::Exception, "should be greater than 0");

    // Check for power = 0
    cdlOp.setSlopeParams(OCIO::CDLOpData::ChannelParams(0.7));
    cdlOp.setOffsetParams(OCIO::CDLOpData::ChannelParams(0.2));
    cdlOp.setPowerParams(OCIO::CDLOpData::ChannelParams(0.0));
    cdlOp.setSaturation(1.4);

    OCIO_CHECK_THROW_WHAT(cdlOp.validate(), OCIO::Exception, "should be greater than 0");
}

// TODO: CDLOp_inverse_bypass_test is missing

OCIO_ADD_TEST(CDLOpData, channel)
{
  {
    OCIO::CDLOpData cdlOp;

    // False: identity
    OCIO_CHECK_ASSERT(!cdlOp.hasChannelCrosstalk());
  }

  {
    OCIO::CDLOpData cdlOp;
    cdlOp.setSlopeParams(OCIO::CDLOpData::ChannelParams(-0.9));
    cdlOp.setOffsetParams(OCIO::CDLOpData::ChannelParams(0.01));
    cdlOp.setPowerParams(OCIO::CDLOpData::ChannelParams(1.2));

    // False: slope, offset, and power 
    OCIO_CHECK_ASSERT(!cdlOp.hasChannelCrosstalk());
  }

  {
    OCIO::CDLOpData cdlOp;
    cdlOp.setSaturation(1.17);

    // True: saturation
    OCIO_CHECK_ASSERT(cdlOp.hasChannelCrosstalk());
  }
}

#endif
