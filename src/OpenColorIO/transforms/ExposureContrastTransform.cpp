// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <OpenColorIO/OpenColorIO.h>

#include "ops/exposurecontrast/ExposureContrastOpData.h"

OCIO_NAMESPACE_ENTER
{

ExposureContrastStyle ConvertStyle(ExposureContrastOpData::Style style)
{
    switch (style)
    {
    case ExposureContrastOpData::STYLE_VIDEO:
    case ExposureContrastOpData::STYLE_VIDEO_REV:
        return EXPOSURE_CONTRAST_VIDEO;

    case ExposureContrastOpData::STYLE_LOGARITHMIC:
    case ExposureContrastOpData::STYLE_LOGARITHMIC_REV:
        return EXPOSURE_CONTRAST_LOGARITHMIC;

    case ExposureContrastOpData::STYLE_LINEAR:
    case ExposureContrastOpData::STYLE_LINEAR_REV:
        return EXPOSURE_CONTRAST_LINEAR;
    }

    std::stringstream ss("Unknown ExposureContrast style: ");
    ss << style;

    throw Exception(ss.str().c_str());
}

ExposureContrastTransformRcPtr ExposureContrastTransform::Create()
{
    return ExposureContrastTransformRcPtr(new ExposureContrastTransform(), &deleter);
}

void ExposureContrastTransform::deleter(ExposureContrastTransform* t)
{
    delete t;
}


///////////////////////////////////////////////////////////////////////////

class ExposureContrastTransform::Impl : public ExposureContrastOpData
{
public:
    Impl()
        : ExposureContrastOpData()
        , m_direction(TRANSFORM_DIR_FORWARD)
    {
    }

    Impl(const Impl &) = delete;

    ~Impl() {}

    Impl& operator=(const Impl & rhs)
    {
        if (this != &rhs)
        {
            ExposureContrastOpData::operator=(rhs);
            m_direction = rhs.m_direction;
        }
        return *this;
    }

    bool equals(const Impl & rhs) const
    {
        if (this == &rhs) return true;

        return ExposureContrastOpData::operator==(rhs)
               && m_direction == rhs.m_direction;
    }

    TransformDirection m_direction;
};

///////////////////////////////////////////////////////////////////////////

ExposureContrastTransform::ExposureContrastTransform()
    : m_impl(new ExposureContrastTransform::Impl)
{
}

ExposureContrastTransform::~ExposureContrastTransform()
{
    delete m_impl;
    m_impl = nullptr;
}

ExposureContrastTransform & ExposureContrastTransform::operator= (const ExposureContrastTransform & rhs)
{
    if (this != &rhs)
    {
        *m_impl = *rhs.m_impl;
    }
    return *this;
}

TransformRcPtr ExposureContrastTransform::createEditableCopy() const
{
    ExposureContrastTransformRcPtr transform = ExposureContrastTransform::Create();
    *transform->m_impl = *m_impl;
    return transform;
}

TransformDirection ExposureContrastTransform::getDirection() const
{
    return getImpl()->m_direction;
}

void ExposureContrastTransform::setDirection(TransformDirection dir)
{
    getImpl()->m_direction = dir;
}

void ExposureContrastTransform::validate() const
{
    Transform::validate();
    getImpl()->validate();
}

FormatMetadata & ExposureContrastTransform::getFormatMetadata()
{
    return m_impl->getFormatMetadata();
}

const FormatMetadata & ExposureContrastTransform::getFormatMetadata() const
{
    return m_impl->getFormatMetadata();
}

ExposureContrastStyle ExposureContrastTransform::getStyle() const
{
    return ConvertStyle(getImpl()->getStyle());
}

void ExposureContrastTransform::setStyle(ExposureContrastStyle style)
{
    getImpl()->setStyle(ExposureContrastOpData::ConvertStyle(style, TRANSFORM_DIR_FORWARD));
}

double ExposureContrastTransform::getExposure() const
{
    return getImpl()->getExposure();
}

void ExposureContrastTransform::setExposure(double exposure)
{
    getImpl()->setExposure(exposure);
}

void ExposureContrastTransform::makeExposureDynamic()
{
    getImpl()->getExposureProperty()->makeDynamic();
}

bool ExposureContrastTransform::isExposureDynamic() const
{
    return getImpl()->getExposureProperty()->isDynamic();
}

double ExposureContrastTransform::getContrast() const
{
    return getImpl()->getContrast();
}

void ExposureContrastTransform::setContrast(double contrast)
{
    getImpl()->setContrast(contrast);
}

void ExposureContrastTransform::makeContrastDynamic()
{
    getImpl()->getContrastProperty()->makeDynamic();
}

bool ExposureContrastTransform::isContrastDynamic() const
{
    return getImpl()->getContrastProperty()->isDynamic();
}

double ExposureContrastTransform::getGamma() const
{
    return getImpl()->getGamma();
}

void ExposureContrastTransform::setGamma(double gamma)
{
    getImpl()->setGamma(gamma);
}

void ExposureContrastTransform::makeGammaDynamic()
{
    getImpl()->getGammaProperty()->makeDynamic();
}

bool ExposureContrastTransform::isGammaDynamic() const
{
    return getImpl()->getGammaProperty()->isDynamic();
}

double ExposureContrastTransform::getPivot() const
{
    return getImpl()->getPivot();
}

void ExposureContrastTransform::setPivot(double pivot)
{
    getImpl()->setPivot(pivot);
}

double ExposureContrastTransform::getLogExposureStep() const
{
    return getImpl()->getLogExposureStep();
}

void ExposureContrastTransform::setLogExposureStep(double logExposureStep)
{
    getImpl()->setLogExposureStep(logExposureStep);
}

double ExposureContrastTransform::getLogMidGray() const
{
    return getImpl()->getLogMidGray();
}

void ExposureContrastTransform::setLogMidGray(double logMidGray)
{
    return getImpl()->setLogMidGray(logMidGray);
}


std::ostream& operator<< (std::ostream & os, const ExposureContrastTransform & t)
{
    os << "<ExposureContrast ";
    os << "direction=" << TransformDirectionToString(t.getDirection());
    os << ", style=" << ExposureContrastStyleToString(t.getStyle());

    os << ", exposure=" << t.getExposure();
    os << ", contrast=" << t.getContrast();
    os << ", gamma=" << t.getGamma();
    os << ", pivot=" << t.getPivot();
    os << ", logExposureStep=" << t.getLogExposureStep();
    os << ", logMidGray=" << t.getLogMidGray();
    if (t.isExposureDynamic())
    {
        os << ", exposureDynamic";
    }
    if (t.isContrastDynamic())
    {
        os << ", contrastDynamic";
    }
    if (t.isGammaDynamic())
    {
        os << ", gammaDynamic";
    }

    os << ">";
    return os;
}

}
OCIO_NAMESPACE_EXIT

////////////////////////////////////////////////////////////////////////////////


#ifdef OCIO_UNIT_TEST

namespace OCIO = OCIO_NAMESPACE;
#include "UnitTest.h"

OCIO_ADD_TEST(ExposureContrastTransform, basic)
{
    OCIO::ExposureContrastTransformRcPtr ec = OCIO::ExposureContrastTransform::Create();
    OCIO_CHECK_EQUAL(ec->getDirection(), OCIO::TRANSFORM_DIR_FORWARD);
    OCIO_CHECK_EQUAL(ec->getStyle(), OCIO::EXPOSURE_CONTRAST_LINEAR);
    OCIO_CHECK_EQUAL(ec->getExposure(), 0.0);
    OCIO_CHECK_EQUAL(ec->getContrast(), 1.0);
    OCIO_CHECK_EQUAL(ec->getGamma(), 1.0);
    OCIO_CHECK_EQUAL(ec->getPivot(), 0.18);
    OCIO_CHECK_EQUAL(ec->getLogExposureStep(),
                     OCIO::ExposureContrastOpData::LOGEXPOSURESTEP_DEFAULT);
    OCIO_CHECK_EQUAL(ec->getLogMidGray(),
                     OCIO::ExposureContrastOpData::LOGMIDGRAY_DEFAULT);
    OCIO_CHECK_NO_THROW(ec->validate());

    OCIO_CHECK_NO_THROW(ec->setDirection(OCIO::TRANSFORM_DIR_INVERSE));
    OCIO_CHECK_EQUAL(ec->getDirection(), OCIO::TRANSFORM_DIR_INVERSE);
    OCIO_CHECK_NO_THROW(ec->validate());

    OCIO_CHECK_NO_THROW(ec->setStyle(OCIO::EXPOSURE_CONTRAST_LOGARITHMIC));
    OCIO_CHECK_EQUAL(ec->getStyle(), OCIO::EXPOSURE_CONTRAST_LOGARITHMIC);
    OCIO_CHECK_NO_THROW(ec->validate());

    OCIO_CHECK_NO_THROW(ec->setStyle(OCIO::EXPOSURE_CONTRAST_VIDEO));
    OCIO_CHECK_EQUAL(ec->getStyle(), OCIO::EXPOSURE_CONTRAST_VIDEO);
    OCIO_CHECK_NO_THROW(ec->validate());
}

OCIO_ADD_TEST(ExposureContrastTransform, processor)
{
    OCIO::ConfigRcPtr config = OCIO::Config::Create();

    OCIO::ExposureContrastTransformRcPtr ec = OCIO::ExposureContrastTransform::Create();
    OCIO_CHECK_NO_THROW(ec->setStyle(OCIO::EXPOSURE_CONTRAST_VIDEO));
    ec->setExposure(1.1);
    ec->makeExposureDynamic();
    ec->setContrast(0.5);
    ec->setGamma(1.5);

    OCIO::ConstProcessorRcPtr processor = config->getProcessor(ec);
    OCIO::ConstCPUProcessorRcPtr cpuProcessor = processor->getDefaultCPUProcessor();

    float pixel[3] = { 0.2f, 0.3f, 0.4f };
    cpuProcessor->applyRGB(pixel);

    const float error = 1e-5f;
    OCIO_CHECK_CLOSE(pixel[0], 0.32340f, error);
    OCIO_CHECK_CLOSE(pixel[1], 0.43834f, error);
    OCIO_CHECK_CLOSE(pixel[2], 0.54389f, error);

    // Changing the original transform does not change the processor.
    ec->setExposure(2.1);

    pixel[0] = 0.2f;
    pixel[1] = 0.3f;
    pixel[2] = 0.4f;
    cpuProcessor->applyRGB(pixel);
    OCIO_CHECK_CLOSE(pixel[0], 0.32340f, error);
    OCIO_CHECK_CLOSE(pixel[1], 0.43834f, error);
    OCIO_CHECK_CLOSE(pixel[2], 0.54389f, error);

    OCIO::DynamicPropertyRcPtr dpExposure;
    OCIO_CHECK_NO_THROW(dpExposure = cpuProcessor->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE));
    dpExposure->setValue(2.1);

    // Gamma is a property of ExposureContrast but here it is not defined as dynamic.
    OCIO_CHECK_THROW(cpuProcessor->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_GAMMA), OCIO::Exception);

    // Processor has been changed by dpExposure.
    pixel[0] = 0.2f;
    pixel[1] = 0.3f;
    pixel[2] = 0.4f;
    cpuProcessor->applyRGB(pixel);
    OCIO_CHECK_CLOSE(pixel[0], 0.42965f, error);
    OCIO_CHECK_CLOSE(pixel[1], 0.58235f, error);
    OCIO_CHECK_CLOSE(pixel[2], 0.72258f, error);

    // dpExposure can be used to change the processor.
    dpExposure->setValue(0.8);
    pixel[0] = 0.2f;
    pixel[1] = 0.3f;
    pixel[2] = 0.4f;
    cpuProcessor->applyRGB(pixel);
    OCIO_CHECK_CLOSE(pixel[0], 0.29698f, error);
    // Adjust error for SSE approximation.
    OCIO_CHECK_CLOSE(pixel[1], 0.40252f, error*2.0f);
    OCIO_CHECK_CLOSE(pixel[2], 0.49946f, error);
}

// This test verifies that if there are several ops in a processor that contain
// a given dynamic property that
// 1) All the ops for which the dynamic property is enabled respond to changes
//    in the property, and
// 2) Ops where a given dynamic property is not enabled continue to use the
//    initial value and do not respond to changes to the property.
OCIO_ADD_TEST(ExposureContrastTransform, processor_several_ec)
{
    OCIO::ConfigRcPtr config = OCIO::Config::Create();

    //
    // Build expected values using 2 E/C with no dynamic parameters.
    //

    // 2 different exposure values.
    const double a = 1.1;
    const double b = 2.1;
    OCIO::ExposureContrastTransformRcPtr ec1 = OCIO::ExposureContrastTransform::Create();
    OCIO_CHECK_NO_THROW(ec1->setStyle(OCIO::EXPOSURE_CONTRAST_LOGARITHMIC));

    ec1->setExposure(a);
    ec1->setContrast(0.5);
    ec1->setGamma(1.5);

    const float srcPixel[3] = { 0.2f, 0.3f, 0.4f };

    // Will hold results for exposure a.
    float pixel_a[3] = { srcPixel[0], srcPixel[1], srcPixel[2] };
    // Will hold results for exposure a applied twice.
    float pixel_aa[3];
    {
        OCIO::ConstProcessorRcPtr processor = config->getProcessor(ec1);
        OCIO::ConstCPUProcessorRcPtr cpuProcessor = processor->getDefaultCPUProcessor();
        cpuProcessor->applyRGB(pixel_a);

        pixel_aa[0] = pixel_a[0];
        pixel_aa[1] = pixel_a[1];
        pixel_aa[2] = pixel_a[2];
        cpuProcessor->applyRGB(pixel_aa);
    }

    OCIO::ExposureContrastTransformRcPtr ec2 = OCIO::ExposureContrastTransform::Create();
    OCIO_CHECK_NO_THROW(ec2->setStyle(OCIO::EXPOSURE_CONTRAST_LOGARITHMIC));

    ec2->setExposure(b);
    ec2->setContrast(0.5);
    ec2->setGamma(1.5);

    // Will hold results for exposure b.
    float pixel_b[3] = { srcPixel[0], srcPixel[1], srcPixel[2] };
    // Will hold results for exposure b applied twice.
    float pixel_bb[3];
    // Will hold results for exposure a applied then exposure b applied.
    float pixel_ab[3] = { pixel_a[0], pixel_a[1], pixel_a[2] };
    {
        OCIO::ConstProcessorRcPtr processor = config->getProcessor(ec2);
        OCIO::ConstCPUProcessorRcPtr cpuProcessor = processor->getDefaultCPUProcessor();
        cpuProcessor->applyRGB(pixel_b);

        pixel_bb[0] = pixel_b[0];
        pixel_bb[1] = pixel_b[1];
        pixel_bb[2] = pixel_b[2];

        cpuProcessor->applyRGB(pixel_bb);
        cpuProcessor->applyRGB(pixel_ab);
    }

    // Make exposure of second E/C dynamic.
    ec2->makeExposureDynamic();
    const float error = 1e-6f;

    //
    // Test with two E/C where only the second one is dynamic.
    //
    OCIO::GroupTransformRcPtr grp1 = OCIO::GroupTransform::Create();
    ec2->setExposure(a);
    grp1->appendTransform(ec1);  // ec1 exposure is a
    grp1->appendTransform(ec2);  // ec2 exposure is a

    {
        OCIO::ConstProcessorRcPtr processor = config->getProcessor(grp1);
        OCIO::ConstCPUProcessorRcPtr cpuProcessor = processor->getDefaultCPUProcessor();

        // Make second exposure dynamic. Value is still a.
        OCIO::DynamicPropertyRcPtr dpExposure;
        OCIO_CHECK_NO_THROW(dpExposure = cpuProcessor->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE));

        float pixel[3] = { srcPixel[0], srcPixel[1], srcPixel[2] };

        // Apply a then a.
        cpuProcessor->applyRGB(pixel);

        OCIO_CHECK_CLOSE(pixel[0], pixel_aa[0], error);
        OCIO_CHECK_CLOSE(pixel[1], pixel_aa[1], error);
        OCIO_CHECK_CLOSE(pixel[2], pixel_aa[2], error);

        // Change the 2nd exposure.
        dpExposure->setValue(b);
        pixel[0] = srcPixel[0];
        pixel[1] = srcPixel[1];
        pixel[2] = srcPixel[2];

        // Apply a then b.
        cpuProcessor->applyRGB(pixel);

        OCIO_CHECK_CLOSE(pixel[0], pixel_ab[0], error);
        OCIO_CHECK_CLOSE(pixel[1], pixel_ab[1], error);
        OCIO_CHECK_CLOSE(pixel[2], pixel_ab[2], error);
    }

    // Make exposure of first E/C dynamic.
    ec1->makeExposureDynamic();

    //
    // Test with two E/C where both are dynamic.
    //
    OCIO::GroupTransformRcPtr grp2 = OCIO::GroupTransform::Create();
    grp2->appendTransform(ec1);
    grp2->appendTransform(ec2);

    // Change both exposure values.
    ec1->setExposure(0.123);
    ec2->setExposure(0.456);
    {
        OCIO::ConstProcessorRcPtr processor = config->getProcessor(grp2);
        OCIO::ConstCPUProcessorRcPtr cpuProcessor = processor->getDefaultCPUProcessor();

        // The dynamic property is common to both ops.
        OCIO::DynamicPropertyRcPtr dpExposure;
        OCIO_CHECK_NO_THROW(dpExposure = cpuProcessor->getDynamicProperty(OCIO::DYNAMIC_PROPERTY_EXPOSURE));

        float pixel[3] = { srcPixel[0], srcPixel[1], srcPixel[2] };

        // Change both exposures to a.
        dpExposure->setValue(a);

        // Apply a twice.
        cpuProcessor->applyRGB(pixel);

        OCIO_CHECK_CLOSE(pixel[0], pixel_aa[0], error);
        OCIO_CHECK_CLOSE(pixel[1], pixel_aa[1], error);
        OCIO_CHECK_CLOSE(pixel[2], pixel_aa[2], error);

        // Changing the dynamic property is changing both exposures to b.
        dpExposure->setValue(b);
        pixel[0] = srcPixel[0];
        pixel[1] = srcPixel[1];
        pixel[2] = srcPixel[2];

        // Apply b twice.
        cpuProcessor->applyRGB(pixel);

        OCIO_CHECK_CLOSE(pixel[0], pixel_bb[0], error);
        OCIO_CHECK_CLOSE(pixel[1], pixel_bb[1], error);
        OCIO_CHECK_CLOSE(pixel[2], pixel_bb[2], error);
    }
}

#endif