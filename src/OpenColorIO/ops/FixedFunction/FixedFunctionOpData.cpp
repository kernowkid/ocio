// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.



#include <sstream>

#include <OpenColorIO/OpenColorIO.h>

#include "ops/FixedFunction/FixedFunctionOpData.h"
#include "Platform.h"

OCIO_NAMESPACE_ENTER
{

namespace DefaultValues
{
    const int FLOAT_DECIMALS = 7;
}

static constexpr const char * RED_MOD_03_FWD = "RedMod03Fwd";
static constexpr const char * RED_MOD_03_REV = "RedMod03Rev";
static constexpr const char * RED_MOD_10_FWD = "RedMod10Fwd";
static constexpr const char * RED_MOD_10_REV = "RedMod10Rev";
static constexpr const char * GLOW_03_FWD = "Glow03Fwd";
static constexpr const char * GLOW_03_REV = "Glow03Rev";
static constexpr const char * GLOW_10_FWD = "Glow10Fwd";
static constexpr const char * GLOW_10_REV = "Glow10Rev";
static constexpr const char * DARK_TO_DIM_10 = "DarkToDim10";
static constexpr const char * DIM_TO_DARK_10 = "DimToDark10";
static constexpr const char * SURROUND = "Surround"; // Is old name for Rec2100Surround
static constexpr const char * REC_2100_SURROUND = "Rec2100Surround";

// NOTE: Converts the enumeration value to its string representation (i.e. CLF reader).
//       It could add details for error reporting.
//          
const char * FixedFunctionOpData::ConvertStyleToString(Style style, bool detailed)
{
    switch(style)
    {
        case ACES_RED_MOD_03_FWD:
            return detailed ? "ACES_RedMod03 (Forward)" : RED_MOD_03_FWD;
        case ACES_RED_MOD_03_INV:
            return detailed ? "ACES_RedMod03 (Inverse)" : RED_MOD_03_REV;
        case ACES_RED_MOD_10_FWD:
            return detailed ? "ACES_RedMod10 (Forward)" : RED_MOD_10_FWD;
        case ACES_RED_MOD_10_INV:
            return detailed ? "ACES_RedMod10 (Inverse)" : RED_MOD_10_REV;
        case ACES_GLOW_03_FWD:
            return detailed ? "ACES_Glow03 (Forward)" : GLOW_03_FWD;
        case ACES_GLOW_03_INV:
            return detailed ? "ACES_Glow03 (Inverse)" : GLOW_03_REV;
        case ACES_GLOW_10_FWD:
            return detailed ? "ACES_Glow10 (Forward)" : GLOW_10_FWD;
        case ACES_GLOW_10_INV:
            return detailed ? "ACES_Glow10 (Inverse)" : GLOW_10_REV;
        case ACES_DARK_TO_DIM_10_FWD:
            return detailed ? "ACES_DarkToDim10 (Forward)" : DARK_TO_DIM_10;
        case ACES_DARK_TO_DIM_10_INV: 
            return detailed ? "ACES_DarkToDim10 (Inverse)" : DIM_TO_DARK_10;
        case REC2100_SURROUND:
            return detailed ? "REC2100_Surround" : REC_2100_SURROUND;
    }

    std::stringstream ss("Unknown FixedFunction style: ");
    ss << style;

    throw Exception(ss.str().c_str());
}

FixedFunctionOpData::Style FixedFunctionOpData::GetStyle(const char * name)
{
    if (name && *name)
    {
        if (0 == Platform::Strcasecmp(name, RED_MOD_03_FWD))
        {
            return ACES_RED_MOD_03_FWD;
        }
        else if (0 == Platform::Strcasecmp(name, RED_MOD_03_REV))
        {
            return ACES_RED_MOD_03_INV;
        }
        else if (0 == Platform::Strcasecmp(name, RED_MOD_10_FWD))
        {
            return ACES_RED_MOD_10_FWD;
        }
        else if (0 == Platform::Strcasecmp(name, RED_MOD_10_REV))
        {
            return ACES_RED_MOD_10_INV;
        }
        else if (0 == Platform::Strcasecmp(name, GLOW_03_FWD))
        {
            return ACES_GLOW_03_FWD;
        }
        else if (0 == Platform::Strcasecmp(name, GLOW_03_REV))
        {
            return ACES_GLOW_03_INV;
        }
        else if (0 == Platform::Strcasecmp(name, GLOW_10_FWD))
        {
            return ACES_GLOW_10_FWD;
        }
        else if (0 == Platform::Strcasecmp(name, GLOW_10_REV))
        {
            return ACES_GLOW_10_INV;
        }
        else if (0 == Platform::Strcasecmp(name, DARK_TO_DIM_10))
        {
            return ACES_DARK_TO_DIM_10_FWD;
        }
        else if (0 == Platform::Strcasecmp(name, DIM_TO_DARK_10))
        {
            return ACES_DARK_TO_DIM_10_INV;
        }
        else if (0 == Platform::Strcasecmp(name, SURROUND) ||
                 0 == Platform::Strcasecmp(name, REC_2100_SURROUND) )
        {
            return REC2100_SURROUND;
        }
    }
    std::string st("Unknown FixedFunction style: ");
    st += name;
    throw Exception(st.c_str());
}

FixedFunctionOpData::Style FixedFunctionOpData::ConvertStyle(FixedFunctionStyle style)
{
    switch (style)
    {
        case FIXED_FUNCTION_ACES_RED_MOD_03:
        {
            return FixedFunctionOpData::ACES_RED_MOD_03_FWD;
        }
        case FIXED_FUNCTION_ACES_RED_MOD_10:
        {
            return FixedFunctionOpData::ACES_RED_MOD_10_FWD;
        }
        case FIXED_FUNCTION_ACES_GLOW_03:
        {
            return FixedFunctionOpData::ACES_GLOW_03_FWD;
        }
        case FIXED_FUNCTION_ACES_GLOW_10:
        {
            return FixedFunctionOpData::ACES_GLOW_10_FWD;
        }
        case FIXED_FUNCTION_ACES_DARK_TO_DIM_10:
        {
            return FixedFunctionOpData::ACES_DARK_TO_DIM_10_FWD;
        }
        case FIXED_FUNCTION_REC2100_SURROUND:
        {
            return FixedFunctionOpData::REC2100_SURROUND;
        }
    }

    std::stringstream ss("Unknown FixedFunction transform style: ");
    ss << style;

    throw Exception(ss.str().c_str());
}

FixedFunctionStyle FixedFunctionOpData::ConvertStyle(FixedFunctionOpData::Style style)
{
    switch (style)
    {
    case FixedFunctionOpData::ACES_RED_MOD_03_FWD:
    case FixedFunctionOpData::ACES_RED_MOD_03_INV:
        return FIXED_FUNCTION_ACES_RED_MOD_03;

    case FixedFunctionOpData::ACES_RED_MOD_10_FWD:
    case FixedFunctionOpData::ACES_RED_MOD_10_INV:
        return FIXED_FUNCTION_ACES_RED_MOD_10;

    case FixedFunctionOpData::ACES_GLOW_03_FWD:
    case FixedFunctionOpData::ACES_GLOW_03_INV:
        return FIXED_FUNCTION_ACES_GLOW_03;

    case FixedFunctionOpData::ACES_GLOW_10_FWD:
    case FixedFunctionOpData::ACES_GLOW_10_INV:
        return FIXED_FUNCTION_ACES_GLOW_10;

    case FixedFunctionOpData::ACES_DARK_TO_DIM_10_FWD:
    case FixedFunctionOpData::ACES_DARK_TO_DIM_10_INV:
        return FIXED_FUNCTION_ACES_DARK_TO_DIM_10;

    case FixedFunctionOpData::REC2100_SURROUND:
        return FIXED_FUNCTION_REC2100_SURROUND;
    }

    std::stringstream ss("Unknown FixedFunction style: ");
    ss << style;

    throw Exception(ss.str().c_str());
}

FixedFunctionOpData::FixedFunctionOpData()
    :   OpData()
    ,   m_style(ACES_RED_MOD_03_FWD)
{
}

FixedFunctionOpData::FixedFunctionOpData(const Params & params,
                                         Style style)
    :   OpData()
    ,   m_style(style)
    ,   m_params(params)
{
    validate();
}

FixedFunctionOpData::~FixedFunctionOpData()
{
}

FixedFunctionOpDataRcPtr FixedFunctionOpData::clone() const
{
    return std::make_shared<FixedFunctionOpData>(getParams(),
                                                 getStyle());
}

void FixedFunctionOpData::validate() const
{
    OpData::validate();

    if(m_style==REC2100_SURROUND)
    {
        if (m_params.size() != 1)
        {
            std::stringstream ss;
            ss  << "The style '" << ConvertStyleToString(m_style, true) 
                << "' must have one parameter but " 
                << m_params.size() << " found.";
            throw Exception(ss.str().c_str());
        }

        const double p = m_params[0];
        static const double low_bound = 0.001;
        static const double hi_bound = 100.;

        if (p < low_bound)
        {
            std::stringstream ss;
            ss << "Parameter " << p << " is less than lower bound " << low_bound;
            throw Exception(ss.str().c_str());
        }
        else if (p > hi_bound)
        {
            std::stringstream ss;
            ss << "Parameter " << p << " is greater than upper bound " << hi_bound;
            throw Exception(ss.str().c_str());
        }
    }
    else
    {
        if(m_params.size()!=0)
        {
            std::stringstream ss;
            ss  << "The style '" << ConvertStyleToString(m_style, true) 
                << "' must have zero parameters but " 
                << m_params.size() << " found.";
            throw Exception(ss.str().c_str());
        }
    }
}

bool FixedFunctionOpData::isInverse(ConstFixedFunctionOpDataRcPtr & r) const
{
    return *r == *inverse();
}

void FixedFunctionOpData::invert()
{
    // NB: The following assumes the op has already been validated.

    switch(getStyle())
    {
        case ACES_RED_MOD_03_FWD:
        {
            setStyle(ACES_RED_MOD_03_INV);
            break;
        }
        case ACES_RED_MOD_03_INV:
        {
            setStyle(ACES_RED_MOD_03_FWD);
            break;
        }
        case ACES_RED_MOD_10_FWD:
        {
            setStyle(ACES_RED_MOD_10_INV);
            break;
        }
        case ACES_RED_MOD_10_INV:
        {
            setStyle(ACES_RED_MOD_10_FWD);
            break;
        }
        case ACES_GLOW_03_FWD:
        {
            setStyle(ACES_GLOW_03_INV);
            break;
        }
        case ACES_GLOW_03_INV:
        {
            setStyle(ACES_GLOW_03_FWD);
            break;
        }
        case ACES_GLOW_10_FWD:
        {
            setStyle(ACES_GLOW_10_INV);
            break;
        }
        case ACES_GLOW_10_INV:
        {
            setStyle(ACES_GLOW_10_FWD);
            break;
        }
        case ACES_DARK_TO_DIM_10_FWD:
        {
            setStyle(ACES_DARK_TO_DIM_10_INV);
            break;
        }
        case ACES_DARK_TO_DIM_10_INV:
        {
            setStyle(ACES_DARK_TO_DIM_10_FWD);
            break;
        }
        case REC2100_SURROUND:
        {
            m_params[0] = 1. / m_params[0];
            break;
        }
    }

    // Note that any existing metadata could become stale at this point but
    // trying to update it is also challenging since inverse() is sometimes
    // called even during the creation of new ops.
}

FixedFunctionOpDataRcPtr FixedFunctionOpData::inverse() const
{
    FixedFunctionOpDataRcPtr func = clone();
    func->invert();
    return func;
}

bool FixedFunctionOpData::operator==(const OpData & other) const
{
    if (this == &other) return true;

    if (!OpData::operator==(other)) return false;

    const FixedFunctionOpData* fop = static_cast<const FixedFunctionOpData*>(&other);

    return getStyle() == fop->getStyle() && getParams()==fop->getParams();
}

void FixedFunctionOpData::finalize()
{
    AutoMutex lock(m_mutex);

    validate();

    std::ostringstream cacheIDStream;
    cacheIDStream << getID();

    cacheIDStream.precision(DefaultValues::FLOAT_DECIMALS);

    cacheIDStream << ConvertStyleToString(m_style, true) << " ";

    for( auto param: m_params ) cacheIDStream << param << " ";

    m_cacheID = cacheIDStream.str();
}

}
OCIO_NAMESPACE_EXIT


#ifdef OCIO_UNIT_TEST

namespace OCIO = OCIO_NAMESPACE;

#include "UnitTest.h"


OCIO_ADD_TEST(FixedFunctionOpData, aces_red_mod_style)
{
    OCIO::FixedFunctionOpData func;
    OCIO_CHECK_EQUAL(func.getStyle(), OCIO::FixedFunctionOpData::ACES_RED_MOD_03_FWD);
    OCIO_CHECK_EQUAL(func.getParams().size(), 0);
    OCIO_CHECK_NO_THROW(func.validate());
    OCIO_CHECK_NO_THROW(func.finalize());
    const std::string cacheID(func.getCacheID());

    OCIO_CHECK_NO_THROW(func.setStyle(OCIO::FixedFunctionOpData::ACES_RED_MOD_10_FWD));
    OCIO_CHECK_EQUAL(func.getStyle(), OCIO::FixedFunctionOpData::ACES_RED_MOD_10_FWD);
    OCIO_CHECK_NO_THROW(func.validate());
    OCIO_CHECK_NO_THROW(func.finalize());

    OCIO_CHECK_ASSERT(cacheID!=std::string(func.getCacheID()));

    OCIO::FixedFunctionOpDataRcPtr inv = func.inverse();
    OCIO_CHECK_EQUAL(inv->getStyle(), OCIO::FixedFunctionOpData::ACES_RED_MOD_10_INV);
    OCIO_CHECK_EQUAL(inv->getParams().size(), 0);
    OCIO_CHECK_ASSERT(cacheID!=std::string(inv->getCacheID()));

    OCIO::FixedFunctionOpData::Params p = func.getParams();
    p.push_back(1.);
    OCIO_CHECK_NO_THROW(func.setParams(p));
    OCIO_CHECK_THROW_WHAT(func.validate(), 
                          OCIO::Exception, 
                          "The style 'ACES_RedMod10 (Forward)' must have zero parameters but 1 found.");
}

OCIO_ADD_TEST(FixedFunctionOpData, aces_dark_to_dim10_style)
{
    OCIO::FixedFunctionOpData func(OCIO::FixedFunctionOpData::Params(), 
                                   OCIO::FixedFunctionOpData::ACES_DARK_TO_DIM_10_FWD);

    OCIO_CHECK_EQUAL(func.getStyle(), OCIO::FixedFunctionOpData::ACES_DARK_TO_DIM_10_FWD);
    OCIO_CHECK_EQUAL(func.getParams().size(), 0);
    OCIO_CHECK_NO_THROW(func.validate());
    OCIO_CHECK_NO_THROW(func.finalize());
    const std::string cacheID(func.getCacheID());

    OCIO::FixedFunctionOpDataRcPtr inv = func.inverse();
    OCIO_CHECK_EQUAL(inv->getStyle(), OCIO::FixedFunctionOpData::ACES_DARK_TO_DIM_10_INV);
    OCIO_CHECK_EQUAL(inv->getParams().size(), 0);
    OCIO_CHECK_ASSERT(cacheID!=std::string(inv->getCacheID()));

    OCIO::FixedFunctionOpData::Params p = func.getParams();
    p.push_back(1.);
    OCIO_CHECK_NO_THROW(func.setParams(p));
    OCIO_CHECK_THROW_WHAT(func.validate(), 
                          OCIO::Exception, 
                          "The style 'ACES_DarkToDim10 (Forward)' must have zero parameters but 1 found.");
}

OCIO_ADD_TEST(FixedFunctionOpData, rec2100_surround_style)
{
    OCIO::FixedFunctionOpData::Params params = { 2.0 };
    OCIO::FixedFunctionOpData func(params, OCIO::FixedFunctionOpData::REC2100_SURROUND);
    OCIO_CHECK_NO_THROW(func.validate());
    OCIO_CHECK_NO_THROW(func.finalize());
    const std::string cacheID(func.getCacheID());
    OCIO_CHECK_ASSERT(func.getParams() == params);

    OCIO::FixedFunctionOpDataRcPtr inv = func.inverse();
    OCIO_CHECK_EQUAL(inv->getParams()[0], 1. / func.getParams()[0]);
    OCIO_CHECK_ASSERT(cacheID!=std::string(inv->getCacheID()));

    OCIO_CHECK_ASSERT(func == func);
    OCIO_CHECK_ASSERT(!(func == *inv));

    params = func.getParams();
    params[0] = 120.;
    OCIO_CHECK_NO_THROW(func.setParams(params));
    OCIO_CHECK_THROW_WHAT(func.validate(), 
                          OCIO::Exception,
                          "Parameter 120 is greater than upper bound 100");

    params = func.getParams();
    params[0] = 0.00001;
    OCIO_CHECK_NO_THROW(func.setParams(params));
    OCIO_CHECK_THROW_WHAT(func.validate(),
                          OCIO::Exception,
                          "Parameter 1e-05 is less than lower bound 0.001");

    params = func.getParams();
    params.push_back(12);
    OCIO_CHECK_NO_THROW(func.setParams(params));
    OCIO_CHECK_THROW_WHAT(func.validate(),
                          OCIO::Exception,
                          "The style 'REC2100_Surround' must have "
                          "one parameter but 2 found.");

    params = func.getParams();
    params.clear();
    OCIO_CHECK_NO_THROW(func.setParams(params));
    OCIO_CHECK_THROW_WHAT(func.validate(),
                          OCIO::Exception,
                          "The style 'REC2100_Surround' must have "
                          "one parameter but 0 found.");
}

#endif
