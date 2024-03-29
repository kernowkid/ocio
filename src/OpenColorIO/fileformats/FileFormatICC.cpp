// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <OpenColorIO/OpenColorIO.h>

#include "transforms/FileTransform.h"
#include "pystring/pystring.h"
#include "ops/Lut1D/Lut1DOp.h"
#include "ops/Matrix/MatrixOps.h"
#include "ops/Gamma/GammaOps.h"
#include <sstream>

#include "iccProfileReader.h"

/*
Support for ICC profiles.
ICC color management is the de facto standard in areas such as printing
and OS-level color management.
ICC profiles are a widely used method of storing color information for
computer displays and that is the main purpose of this format reader.
The "matrix/TRC" model for a monitor is parsed and converted into
an OCIO compatible form.
Other types of ICC profiles are not currently supported in this reader.
*/

OCIO_NAMESPACE_ENTER
{

    class LocalCachedFile : public CachedFile
    {
    public:
        LocalCachedFile() = default;
        ~LocalCachedFile() = default;

        // Matrix part
        double mMatrix44[16]{ 0.0 };

        // Gamma
        float mGammaRGB[4]{ 1.0f };

        Lut1DOpDataRcPtr lut;
    };

    typedef OCIO_SHARED_PTR<LocalCachedFile> LocalCachedFileRcPtr;

    class LocalFileFormat : public FileFormat
    {
    public:
        LocalFileFormat() = default;
        ~LocalFileFormat() = default;

        void getFormatInfo(FormatInfoVec & formatInfoVec) const override;

        CachedFileRcPtr read(
            std::istream & istream,
            const std::string & fileName) const override;

        void buildFileOps(OpRcPtrVec & ops,
                          const Config & config,
                          const ConstContextRcPtr & context,
                          CachedFileRcPtr untypedCachedFile,
                          const FileTransform & fileTransform,
                          TransformDirection dir) const override;

        bool isBinary() const override
        {
            return true;
        }

    private:
        static void ThrowErrorMessage(const std::string & error,
            const std::string & fileName);
    };

    void LocalFileFormat::getFormatInfo(FormatInfoVec & formatInfoVec) const
    {
        FormatInfo info;
        info.name = "International Color Consortium profile";
        info.extension = "icc";
        info.capabilities = FORMAT_CAPABILITY_READ;
        formatInfoVec.push_back(info);

        // .icm and .pf are also fine
        info.name = "Image Color Matching profile";
        info.extension = "icm";
        formatInfoVec.push_back(info);
        info.name = "ICC profile";
        info.extension = "pf";
        formatInfoVec.push_back(info);
    }

    void LocalFileFormat::ThrowErrorMessage(const std::string & error,
        const std::string & fileName)
    {
        std::ostringstream os;
        os << "Error parsing .icc file (";
        os << fileName;
        os << ").  ";
        os << error;

        throw Exception(os.str().c_str());
    }

    // Try and load the format
    // Raise an exception if it can't be loaded.
    CachedFileRcPtr LocalFileFormat::read(
        std::istream & istream,
        const std::string & fileName) const
    {
        SampleICC::IccContent icc;
        istream.seekg(0);
        if (!istream.good()
            || !SampleICC::Read32(istream, &icc.mHeader.size, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.cmmId, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.version, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.deviceClass, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.colorSpace, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.pcs, 1)
            || !SampleICC::Read16(istream, &icc.mHeader.date.year, 1)
            || !SampleICC::Read16(istream, &icc.mHeader.date.month, 1)
            || !SampleICC::Read16(istream, &icc.mHeader.date.day, 1)
            || !SampleICC::Read16(istream, &icc.mHeader.date.hours, 1)
            || !SampleICC::Read16(istream, &icc.mHeader.date.minutes, 1)
            || !SampleICC::Read16(istream, &icc.mHeader.date.seconds, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.magic, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.platform, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.flags, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.manufacturer, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.model, 1)
            || !SampleICC::Read64(istream, &icc.mHeader.attributes, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.renderingIntent, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.illuminant.X, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.illuminant.Y, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.illuminant.Z, 1)
            || !SampleICC::Read32(istream, &icc.mHeader.creator, 1)
            || (SampleICC::Read8(istream, 
                &icc.mHeader.profileID,
                sizeof(icc.mHeader.profileID))
                    != sizeof(icc.mHeader.profileID))
            || (SampleICC::Read8(istream,
                &icc.mHeader.reserved[0],
                sizeof(icc.mHeader.reserved))
                    != sizeof(icc.mHeader.reserved))
            )
        {
            ThrowErrorMessage("Error loading header.", fileName);
        }

        // TODO: Capture device name and creation date metadata
        // in order to help users select the correct profile.

        if (icc.mHeader.magic != icMagicNumber)
        {
            ThrowErrorMessage("Wrong magic number.", fileName);
        }

        icUInt32Number count, i;

        if (!SampleICC::Read32(istream, &count, 1))
        {
            ThrowErrorMessage("Error loading number of tags.", fileName);
        }

        icc.mTags.resize(count);

        // Read Tag offset table. 
        for (i = 0; i<count; i++) {
            if (!SampleICC::Read32(istream, &icc.mTags[i].mTagInfo.sig, 1)
                || !SampleICC::Read32(istream, &icc.mTags[i].mTagInfo.offset, 1)
                || !SampleICC::Read32(istream, &icc.mTags[i].mTagInfo.size, 1))
            {
                ThrowErrorMessage(
                    "Error loading tag offset table from header.",
                    fileName);
            }
        }

        // Validate
        std::string error;
        if (!icc.Validate(error))
        {
            ThrowErrorMessage(error, fileName);
        }

        LocalCachedFileRcPtr cachedFile
            = LocalCachedFileRcPtr(new LocalCachedFile());

        // Matrix part of the Matrix/TRC Model
        {
            const SampleICC::IccTagXYZ * red =
                dynamic_cast<SampleICC::IccTagXYZ*>(
                    icc.LoadTag(istream, icSigRedColorantTag));
            const SampleICC::IccTagXYZ * green =
                dynamic_cast<SampleICC::IccTagXYZ*>(
                    icc.LoadTag(istream, icSigGreenColorantTag));
            const SampleICC::IccTagXYZ * blue =
                dynamic_cast<SampleICC::IccTagXYZ*>(
                    icc.LoadTag(istream, icSigBlueColorantTag));

            if (!red || !green || !blue)
            {
                ThrowErrorMessage("Illegal matrix tag in ICC profile.",
                    fileName);
            }

            cachedFile->mMatrix44[0] =  (double)(*red).GetXYZ().X / 65536.0;
            cachedFile->mMatrix44[1] =  (double)(*green).GetXYZ().X / 65536.0;
            cachedFile->mMatrix44[2] =  (double)(*blue).GetXYZ().X / 65536.0;
            cachedFile->mMatrix44[3] =  0.0;
                                        
            cachedFile->mMatrix44[4] =  (double)(*red).GetXYZ().Y / 65536.0;
            cachedFile->mMatrix44[5] =  (double)(*green).GetXYZ().Y / 65536.0;
            cachedFile->mMatrix44[6] =  (double)(*blue).GetXYZ().Y / 65536.0;
            cachedFile->mMatrix44[7] =  0.0;
                                        
            cachedFile->mMatrix44[8] =  (double)(*red).GetXYZ().Z / 65536.0;
            cachedFile->mMatrix44[9] =  (double)(*green).GetXYZ().Z / 65536.0;
            cachedFile->mMatrix44[10] = (double)(*blue).GetXYZ().Z / 65536.0;
            cachedFile->mMatrix44[11] = 0.0;

            cachedFile->mMatrix44[12] = 0.0;
            cachedFile->mMatrix44[13] = 0.0;
            cachedFile->mMatrix44[14] = 0.0;
            cachedFile->mMatrix44[15] = 1.0;
        }

        // Extract the "B" Curve part of the Matrix/TRC Model
        const SampleICC::IccTag * redTRC =
            icc.LoadTag(istream, icSigRedTRCTag);
        const SampleICC::IccTag * greenTRC =
            icc.LoadTag(istream, icSigGreenTRCTag);
        const SampleICC::IccTag * blueTRC =
            icc.LoadTag(istream, icSigBlueTRCTag);
        if (!redTRC || !greenTRC || !blueTRC)
        {
            ThrowErrorMessage("Illegal curve tag in ICC profile.", fileName);
        }

        static const std::string strSameType(
            "All curves in the ICC profile must be of the same type.");
        if (redTRC->IsParametricCurve())
        {
            if (!greenTRC->IsParametricCurve() || !blueTRC->IsParametricCurve())
            {
                ThrowErrorMessage(strSameType, fileName);
            }

            const SampleICC::IccTagParametricCurve * red =
                dynamic_cast<const SampleICC::IccTagParametricCurve*>(redTRC);
            const SampleICC::IccTagParametricCurve * green =
                dynamic_cast<const SampleICC::IccTagParametricCurve*>(greenTRC);
            const SampleICC::IccTagParametricCurve * blue =
                dynamic_cast<const SampleICC::IccTagParametricCurve*>(blueTRC);

            if (!red || !green || !blue)
            {
                ThrowErrorMessage(strSameType, fileName);
            }

            if (red->GetNumParam() != 1
                || green->GetNumParam() != 1
                || blue->GetNumParam() != 1)
            {
                ThrowErrorMessage(
                    "Expecting 1 param in parametric curve tag of ICC profile.",
                    fileName);
            }

            cachedFile->mGammaRGB[0] = SampleICC::icFtoD(red->GetParam()[0]);
            cachedFile->mGammaRGB[1] = SampleICC::icFtoD(green->GetParam()[0]);
            cachedFile->mGammaRGB[2] = SampleICC::icFtoD(blue->GetParam()[0]);
            cachedFile->mGammaRGB[3] = 1.0f;
        }
        else
        {
            if (greenTRC->IsParametricCurve() || blueTRC->IsParametricCurve())
            {
                ThrowErrorMessage(strSameType, fileName);
            }
            const SampleICC::IccTagCurve * red =
                dynamic_cast<const SampleICC::IccTagCurve*>(redTRC);
            const SampleICC::IccTagCurve * green =
                dynamic_cast<const SampleICC::IccTagCurve*>(greenTRC);
            const SampleICC::IccTagCurve * blue =
                dynamic_cast<const SampleICC::IccTagCurve*>(blueTRC);

            if (!red || !green || !blue)
            {
                ThrowErrorMessage(strSameType, fileName);
            }

            const size_t curveSize = red->GetCurve().size();
            if (green->GetCurve().size() != curveSize
                || blue->GetCurve().size() != curveSize)
            {
                ThrowErrorMessage(
                    "All curves in the ICC profile must be of the same length.",
                    fileName);
            }

            if (0 == curveSize)
            {
                ThrowErrorMessage("Curves with no values in ICC profile.",
                    fileName);
            }
            else if (1 == curveSize)
            {
                // The curve value shall be interpreted as a gamma value.
                //
                // In this case, the 16-bit curve value is to be interpreted as
                // an unsigned fixed-point 8.8 number.
                // (But we want to multiply by 65535 to undo the normalization
                // applied by SampleICC)
                cachedFile->mGammaRGB[0] =
                    red->GetCurve()[0] * 65535.0f / 256.0f;
                cachedFile->mGammaRGB[1] =
                    green->GetCurve()[0] * 65535.0f / 256.0f;
                cachedFile->mGammaRGB[2] =
                    blue->GetCurve()[0] * 65535.0f / 256.0f;
                cachedFile->mGammaRGB[3] = 1.0f;

            }
            else
            {
                // The LUT stored in the profile takes gamma-corrected values
                // and linearizes them.
                // The entries are encoded as 16-bit ints that may be
                // normalized by 65535 to interpret them as [0,1].
                // The LUT will be inverted to convert output-linear values
                // into values that may be sent to the display.
                const auto lutLength = static_cast<unsigned long>(curveSize);
                cachedFile->lut = std::make_shared<Lut1DOpData>(lutLength);

                const auto & rc = red->GetCurve();
                const auto & gc = green->GetCurve();
                const auto & bc = blue->GetCurve();

                auto & lutData = cachedFile->lut->getArray();

                for (unsigned long i = 0; i < lutLength; ++i)
                {
                    lutData[i * 3 + 0] = rc[i];
                    lutData[i * 3 + 1] = gc[i];
                    lutData[i * 3 + 2] = bc[i];
                }

                // Set the file bit-depth based on what is in the ICC profile
                // (even though SampleICC has normalized the values).
                cachedFile->lut->setFileOutputBitDepth(BIT_DEPTH_UINT16);
            }
        }

        return cachedFile;
    }

    void
    LocalFileFormat::buildFileOps(OpRcPtrVec & ops,
                                  const Config & /*config*/,
                                  const ConstContextRcPtr & /*context*/,
                                  CachedFileRcPtr untypedCachedFile,
                                  const FileTransform & fileTransform,
                                  TransformDirection dir) const
    {
        LocalCachedFileRcPtr cachedFile =
            DynamicPtrCast<LocalCachedFile>(untypedCachedFile);

        // This should never happen.
        if (!cachedFile)
        {
            std::ostringstream os;
            os << "Cannot build Op. Invalid cache type.";
            throw Exception(os.str().c_str());
        }

        TransformDirection newDir = CombineTransformDirections(dir,
            fileTransform.getDirection());
        if (newDir == TRANSFORM_DIR_UNKNOWN)
        {
            std::ostringstream os;
            os << "Cannot build file format transform,";
            os << " unspecified transform direction.";
            throw Exception(os.str().c_str());
        }

        // The matrix in the ICC profile converts monitor RGB to the CIE XYZ
        // based version of the ICC profile connection space (PCS).
        // Because the PCS white point is D50, the ICC profile builder must
        // adapt the native device matrix to D50.
        // The ICC spec recommends a von Kries style chromatic adaptation
        // using the "Bradford" matrix.
        // However for the purposes of OCIO, it is much more convenient
        // for the profile to be balanced to D65 since that is the native
        // white point that most displays will be balanced to.
        // The matrix below is the Bradford matrix to convert
        // a D50 XYZ to a D65 XYZ.
        // In most cases, combining this with the matrix in the ICC profile
        // recovers what would be the actual matrix for a D65 native monitor.
        static const double D50_to_D65_m44[] = {
             0.955509474537, -0.023074829492, 0.063312392987, 0.0,
            -0.028327238868,  1.00994465504,  0.021055592145, 0.0,
             0.012329273379, -0.020536209966, 1.33072998567,  0.0,
             0.0,             0.0,            0.0,            1.0
        };

        if (cachedFile->lut)
        {
            cachedFile->lut->setInterpolation(fileTransform.getInterpolation());
        }

        // The matrix/TRC transform in the ICC profile converts
        // display device code values to the CIE XYZ based version 
        // of the ICC profile connection space (PCS).
        // So we will adopt this convention as the "forward" direction.
        if (newDir == TRANSFORM_DIR_FORWARD)
        {
            if (cachedFile->lut)
            {
                CreateLut1DOp(ops, cachedFile->lut, TRANSFORM_DIR_FORWARD);
            }
            else
            {
                const GammaOpData::Params redParams   = { cachedFile->mGammaRGB[0] };
                const GammaOpData::Params greenParams = { cachedFile->mGammaRGB[1] };
                const GammaOpData::Params blueParams  = { cachedFile->mGammaRGB[2] };
                const GammaOpData::Params alphaParams = { cachedFile->mGammaRGB[3] };
                auto gamma = std::make_shared<GammaOpData>(GammaOpData::BASIC_FWD, 
                                                           redParams,
                                                           greenParams,
                                                           blueParams,
                                                           alphaParams);
                CreateGammaOp(ops, gamma, TRANSFORM_DIR_FORWARD);
            }

            CreateMatrixOp(ops, cachedFile->mMatrix44, TRANSFORM_DIR_FORWARD);

            CreateMatrixOp(ops, D50_to_D65_m44, TRANSFORM_DIR_FORWARD);
        }
        else if (newDir == TRANSFORM_DIR_INVERSE)
        {
            CreateMatrixOp(ops, D50_to_D65_m44, TRANSFORM_DIR_INVERSE);

            // The ICC profile tags form a matrix that converts RGB to CIE XYZ.
            // Invert since we are building a PCS -> device transform.
            CreateMatrixOp(ops, cachedFile->mMatrix44, TRANSFORM_DIR_INVERSE);

            // The LUT / gamma stored in the ICC profile works in
            // the gamma->linear direction.
            if (cachedFile->lut)
            {
                CreateLut1DOp(ops, cachedFile->lut, TRANSFORM_DIR_INVERSE);
            }
            else
            {
                const GammaOpData::Params redParams   = { cachedFile->mGammaRGB[0] };
                const GammaOpData::Params greenParams = { cachedFile->mGammaRGB[1] };
                const GammaOpData::Params blueParams  = { cachedFile->mGammaRGB[2] };
                const GammaOpData::Params alphaParams = { cachedFile->mGammaRGB[3] };
                auto gamma = std::make_shared<GammaOpData>(GammaOpData::BASIC_REV,
                                                           redParams,
                                                           greenParams,
                                                           blueParams,
                                                           alphaParams);

                CreateGammaOp(ops, gamma, TRANSFORM_DIR_FORWARD);
            }
        }

    }

    FileFormat * CreateFileFormatICC()
    {
        return new LocalFileFormat();
    }
}
OCIO_NAMESPACE_EXIT

#ifdef OCIO_UNIT_TEST

namespace OCIO = OCIO_NAMESPACE;
#include "UnitTest.h"
#include "UnitTestUtils.h"

OCIO_ADD_TEST(FileFormatICC, types)
{
    // Test types used
    OCIO_CHECK_EQUAL(1, sizeof(icUInt8Number));
    OCIO_CHECK_EQUAL(2, sizeof(icUInt16Number));
    OCIO_CHECK_EQUAL(4, sizeof(icUInt32Number));

    OCIO_CHECK_EQUAL(4, sizeof(icInt32Number));

    OCIO_CHECK_EQUAL(4, sizeof(icS15Fixed16Number));
}

OCIO::LocalCachedFileRcPtr LoadICCFile(const std::string & fileName)
{
    return OCIO::LoadTestFile<OCIO::LocalFileFormat, OCIO::LocalCachedFile>(
        fileName, std::ios_base::binary);
}

OCIO_ADD_TEST(FileFormatICC, test_file)
{
    OCIO::LocalCachedFileRcPtr iccFile;
    {
        // This example uses a profile with a 1024-entry LUT for the TRC.
        const std::string iccFileName("icc-test-3.icm");
        OCIO::OpRcPtrVec ops;
        OCIO::ContextRcPtr context = OCIO::Context::Create();
        OCIO_CHECK_NO_THROW(BuildOpsTest(ops, iccFileName, context,
                                         OCIO::TRANSFORM_DIR_INVERSE));
        OCIO_CHECK_NO_THROW(OCIO::FinalizeOpVec(ops, OCIO::FINALIZATION_EXACT));
        OCIO_CHECK_EQUAL(4, ops.size());
        OCIO_CHECK_EQUAL("<FileNoOp>", ops[0]->getInfo());
        OCIO_CHECK_EQUAL("<MatrixOffsetOp>", ops[1]->getInfo());
        OCIO_CHECK_EQUAL("<MatrixOffsetOp>", ops[2]->getInfo());
        OCIO_CHECK_EQUAL("<Lut1DOp>", ops[3]->getInfo());

        std::vector<float> v0(4, 0.0f);
        v0[0] = 1.0f;
        std::vector<float> v1(4, 0.0f);
        v1[1] = 1.0f;
        std::vector<float> v2(4, 0.0f);
        v2[2] = 1.0f;
        std::vector<float> v3(4, 0.0f);
        v3[3] = 1.0f;

        std::vector<float> tmp = v0;
        ops[1]->apply(&tmp[0], 1);
        OCIO_CHECK_EQUAL(1.04788959f, tmp[0]);
        OCIO_CHECK_EQUAL(0.0295844227f, tmp[1]);
        OCIO_CHECK_EQUAL(-0.00925218873f, tmp[2]);
        OCIO_CHECK_EQUAL(0.0f, tmp[3]);

        tmp = v1;
        ops[1]->apply(&tmp[0], 1);
        OCIO_CHECK_EQUAL(0.0229206420f, tmp[0]);
        OCIO_CHECK_EQUAL(0.990481913f, tmp[1]);
        OCIO_CHECK_EQUAL(0.0150730424f, tmp[2]);
        OCIO_CHECK_EQUAL(0.0f, tmp[3]);

        tmp = v2;
        ops[1]->apply(&tmp[0], 1);
        OCIO_CHECK_EQUAL(-0.0502183065f, tmp[0]);
        OCIO_CHECK_EQUAL(-0.0170795303f, tmp[1]);
        OCIO_CHECK_EQUAL(0.751668930f, tmp[2]);
        OCIO_CHECK_EQUAL(0.0f, tmp[3]);

        tmp = v3;
        ops[1]->apply(&tmp[0], 1);
        OCIO_CHECK_EQUAL(0.0f, tmp[0]);
        OCIO_CHECK_EQUAL(0.0f, tmp[1]);
        OCIO_CHECK_EQUAL(0.0f, tmp[2]);
        OCIO_CHECK_EQUAL(1.0f, tmp[3]);

        tmp = v0;
        ops[2]->apply(&tmp[0], 1);
        OCIO_CHECK_EQUAL(3.13411215332385f, tmp[0]);
        OCIO_CHECK_EQUAL(-0.978787296139183f, tmp[1]);
        OCIO_CHECK_EQUAL(0.0719830443856949f, tmp[2]);
        OCIO_CHECK_EQUAL(0.0f, tmp[3]);

        tmp = v1;
        ops[2]->apply(&tmp[0], 1);
        OCIO_CHECK_EQUAL(-1.61739245955187f, tmp[0]);
        OCIO_CHECK_EQUAL(1.91627958642662f, tmp[1]);
        OCIO_CHECK_EQUAL(-0.228985850247545f, tmp[2]);
        OCIO_CHECK_EQUAL(0.0f, tmp[3]);

        tmp = v2;
        ops[2]->apply(&tmp[0], 1);
        OCIO_CHECK_EQUAL(-0.49063340456472f, tmp[0]);
        OCIO_CHECK_EQUAL(0.033454714231382f, tmp[1]);
        OCIO_CHECK_EQUAL(1.4053851315845f, tmp[2]);
        OCIO_CHECK_EQUAL(0.0f, tmp[3]);

        tmp = v3;
        ops[2]->apply(&tmp[0], 1);
        OCIO_CHECK_EQUAL(0.0f, tmp[0]);
        OCIO_CHECK_EQUAL(0.0f, tmp[1]);
        OCIO_CHECK_EQUAL(0.0f, tmp[2]);
        OCIO_CHECK_EQUAL(1.0f, tmp[3]);

        // Knowing the LUT has 1024 elements
        // and is inverted, verify values for a given index
        // are converted to index * step
        const float error = 1e-5f;
        
        // value at index 200
        tmp[0] = 0.0317235067f;
        tmp[1] = 0.0317235067f;
        tmp[2] = 0.0317235067f;
        ops[3]->apply(&tmp[0], 1);
        OCIO_CHECK_CLOSE(200.0f / 1023.0f, tmp[0], error);
        OCIO_CHECK_CLOSE(200.0f / 1023.0f, tmp[1], error);
        OCIO_CHECK_CLOSE(200.0f / 1023.0f, tmp[2], error);


        // Get cached file to access LUT size
        OCIO_CHECK_NO_THROW(iccFile = LoadICCFile(iccFileName));

        OCIO_REQUIRE_ASSERT(iccFile);
        OCIO_REQUIRE_ASSERT(iccFile->lut);

        OCIO_CHECK_EQUAL(iccFile->lut->getFileOutputBitDepth(), OCIO::BIT_DEPTH_UINT16);

        const auto & lutArray = iccFile->lut->getArray();
        OCIO_CHECK_EQUAL(1024, lutArray.getLength());

        OCIO_CHECK_EQUAL(0.0317235067f, lutArray[200 * 3 + 0]);
        OCIO_CHECK_EQUAL(0.0317235067f, lutArray[200 * 3 + 1]);
        OCIO_CHECK_EQUAL(0.0317235067f, lutArray[200 * 3 + 2]);
    }

    {
        // This test uses a profile where the TRC is a 1-entry curve,
        // to be interpreted as a gamma value.
        const std::string iccFileName("icc-test-1.icc");
        OCIO_CHECK_NO_THROW(iccFile = LoadICCFile(iccFileName));

        OCIO_CHECK_ASSERT((bool)iccFile);
        OCIO_CHECK_ASSERT(!(bool)(iccFile->lut));

        OCIO_CHECK_EQUAL(0.609741211f, iccFile->mMatrix44[0]);
        OCIO_CHECK_EQUAL(0.205276489f, iccFile->mMatrix44[1]);
        OCIO_CHECK_EQUAL(0.149185181f, iccFile->mMatrix44[2]);
        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[3]);

        OCIO_CHECK_EQUAL(0.311111450f, iccFile->mMatrix44[4]);
        OCIO_CHECK_EQUAL(0.625671387f, iccFile->mMatrix44[5]);
        OCIO_CHECK_EQUAL(0.0632171631f, iccFile->mMatrix44[6]);
        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[7]);
        
        OCIO_CHECK_EQUAL(0.0194702148f, iccFile->mMatrix44[8]);
        OCIO_CHECK_EQUAL(0.0608673096f, iccFile->mMatrix44[9]);
        OCIO_CHECK_EQUAL(0.744567871f, iccFile->mMatrix44[10]);
        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[11]);

        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[12]);
        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[13]);
        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[14]);
        OCIO_CHECK_EQUAL(1.0f, iccFile->mMatrix44[15]);

        OCIO_CHECK_EQUAL(2.19921875f, iccFile->mGammaRGB[0]);
        OCIO_CHECK_EQUAL(2.19921875f, iccFile->mGammaRGB[1]);
        OCIO_CHECK_EQUAL(2.19921875f, iccFile->mGammaRGB[2]);
        OCIO_CHECK_EQUAL(1.0f, iccFile->mGammaRGB[3]);
    }

    {
        // This test uses a profile where the TRC is 
        // a parametric curve of type 0 (a single gamma value).
        const std::string iccFileName("icc-test-2.pf");
        OCIO_CHECK_NO_THROW(iccFile = LoadICCFile(iccFileName));

        OCIO_CHECK_ASSERT((bool)iccFile);
        OCIO_CHECK_ASSERT(!(bool)(iccFile->lut));

        OCIO_CHECK_EQUAL(0.504470825f, iccFile->mMatrix44[0]);
        OCIO_CHECK_EQUAL(0.328125000f, iccFile->mMatrix44[1]);
        OCIO_CHECK_EQUAL(0.131607056f, iccFile->mMatrix44[2]);
        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[3]);

        OCIO_CHECK_EQUAL(0.264923096f, iccFile->mMatrix44[4]);
        OCIO_CHECK_EQUAL(0.682678223f, iccFile->mMatrix44[5]);
        OCIO_CHECK_EQUAL(0.0523834229f, iccFile->mMatrix44[6]);
        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[7]);

        OCIO_CHECK_EQUAL(0.0144805908f, iccFile->mMatrix44[8]);
        OCIO_CHECK_EQUAL(0.0871734619f, iccFile->mMatrix44[9]);
        OCIO_CHECK_EQUAL(0.723556519f, iccFile->mMatrix44[10]);
        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[11]);

        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[12]);
        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[13]);
        OCIO_CHECK_EQUAL(0.0f, iccFile->mMatrix44[14]);
        OCIO_CHECK_EQUAL(1.0f, iccFile->mMatrix44[15]);

        OCIO_CHECK_EQUAL(2.17384338f, iccFile->mGammaRGB[0]);
        OCIO_CHECK_EQUAL(2.17384338f, iccFile->mGammaRGB[1]);
        OCIO_CHECK_EQUAL(2.17384338f, iccFile->mGammaRGB[2]);
        OCIO_CHECK_EQUAL(1.0f, iccFile->mGammaRGB[3]);
    }
}

OCIO_ADD_TEST(FileFormatICC, test_apply)
{
    OCIO::ContextRcPtr context = OCIO::Context::Create();
    {
        const std::string iccFileName("icc-test-3.icm");
        OCIO::OpRcPtrVec ops;
        OCIO_CHECK_NO_THROW(BuildOpsTest(ops, iccFileName, context,
                                         OCIO::TRANSFORM_DIR_FORWARD));

        OCIO_CHECK_NO_THROW(OCIO::OptimizeFinalizeOpVec(ops));

        // apply ops
        float srcImage[] = {
            -0.1f, 0.0f, 0.3f, 0.0f,
            0.4f, 0.5f, 0.6f, 0.5f,
            0.7f, 1.0f, 1.9f, 1.0f };

        const float dstImage[] = {
            0.013221f, 0.005287f, 0.069636f, 0.0f,
            0.188847f, 0.204323f, 0.330955f, 0.5f,
            0.722887f, 0.882591f, 1.078655f, 1.0f };
        const float error = 1e-5f;

        OCIO::OpRcPtrVec::size_type numOps = ops.size();
        for (OCIO::OpRcPtrVec::size_type i = 0; i < numOps; ++i)
        {
            ops[i]->apply(srcImage, 3);
        }

        // compare results
        for (unsigned int i = 0; i<12; ++i)
        {
            OCIO_CHECK_CLOSE(srcImage[i], dstImage[i], error);
        }

        // inverse
        OCIO::OpRcPtrVec opsInv;
        OCIO_CHECK_NO_THROW(BuildOpsTest(opsInv, iccFileName, context,
                                         OCIO::TRANSFORM_DIR_INVERSE));
        OCIO_CHECK_NO_THROW(OCIO::OptimizeFinalizeOpVec(opsInv));

        numOps = opsInv.size();
        for (OCIO::OpRcPtrVec::size_type i = 0; i < numOps; ++i)
        {
            opsInv[i]->apply(srcImage, 3);
        }

        const float bckImage[] = {
            // neg values are clamped by the LUT and won't round-trip
            0.0f, 0.0f, 0.3f, 0.0f,
            0.4f, 0.5f, 0.6f, 0.5f,
            // >1 values are clamped by the LUT and won't round-trip
            0.7f, 1.0f, 1.0f, 1.0f };

        // compare results
        for (unsigned int i = 0; i<12; ++i)
        {
            OCIO_CHECK_CLOSE(srcImage[i], bckImage[i], error);
        }
    }

    {
        const std::string iccFileName("icc-test-2.pf");
        OCIO::OpRcPtrVec ops;
        OCIO_CHECK_NO_THROW(BuildOpsTest(ops, iccFileName, context,
                                         OCIO::TRANSFORM_DIR_FORWARD));
        OCIO_CHECK_NO_THROW(OCIO::OptimizeFinalizeOpVec(ops));

        // apply ops
        float srcImage[] = {
            -0.1f, 0.0f, 0.3f, 0.0f,
             0.4f, 0.5f, 0.6f, 0.5f,
             0.7f, 1.0f, 1.9f, 1.0f };

        const float dstImage[] = {
            0.012437f, 0.004702f, 0.070333f, 0.0f,
            0.188392f, 0.206965f, 0.343595f, 0.5f,

// Gamma SSE vs. not SEE implementations explain the differences.
#ifdef USE_SSE
            1.210458f, 1.058771f, 4.003655f, 1.0f };
#else
            1.210462f, 1.058761f, 4.003706f, 1.0f };
#endif

        OCIO::OpRcPtrVec::size_type numOps = ops.size();
        for (OCIO::OpRcPtrVec::size_type i = 0; i < numOps; ++i)
        {
            ops[i]->apply(srcImage, 3);
        }

        // Compare results
        const float error = 2e-5f;

        for (unsigned int i = 0; i<12; ++i)
        {
            OCIO_CHECK_CLOSE(srcImage[i], dstImage[i], error);
        }

        // inverse
        OCIO::OpRcPtrVec opsInv;
        OCIO_CHECK_NO_THROW(BuildOpsTest(opsInv, iccFileName, context,
                                         OCIO::TRANSFORM_DIR_INVERSE));
        OCIO_CHECK_NO_THROW(OCIO::OptimizeFinalizeOpVec(opsInv));

        numOps = opsInv.size();
        for (OCIO::OpRcPtrVec::size_type i = 0; i < numOps; ++i)
        {
            opsInv[i]->apply(srcImage, 3);
        }

        // Negative values are clamped by the LUT and won't round-trip.
        const float bckImage[] = {
            0.0f, 0.0f, 0.3f, 0.0f,
            0.4f, 0.5f, 0.6f, 0.5f,
            0.7f, 1.0f, 1.9f, 1.0f };

        // Compare results
        const float error2 = 2e-4f;

        for (unsigned int i = 0; i<12; ++i)
        {
            OCIO_CHECK_CLOSE(srcImage[i], bckImage[i], error2);
        }
    }

}

OCIO_ADD_TEST(FileFormatICC, endian)
{
    unsigned char test[8];
    test[0] = 0x11;
    test[1] = 0x22;
    test[2] = 0x33;
    test[3] = 0x44;
    test[4] = 0x55;
    test[5] = 0x66;
    test[6] = 0x77;
    test[7] = 0x88;

    SampleICC::Swap32Array((void*)test, 2);

    OCIO_CHECK_EQUAL(test[0], 0x44);
    OCIO_CHECK_EQUAL(test[1], 0x33);
    OCIO_CHECK_EQUAL(test[2], 0x22);
    OCIO_CHECK_EQUAL(test[3], 0x11);

    OCIO_CHECK_EQUAL(test[4], 0x88);
    OCIO_CHECK_EQUAL(test[5], 0x77);
    OCIO_CHECK_EQUAL(test[6], 0x66);
    OCIO_CHECK_EQUAL(test[7], 0x55);

    SampleICC::Swap16Array((void*)test, 4);

    OCIO_CHECK_EQUAL(test[0], 0x33);
    OCIO_CHECK_EQUAL(test[1], 0x44);

    OCIO_CHECK_EQUAL(test[2], 0x11);
    OCIO_CHECK_EQUAL(test[3], 0x22);

    OCIO_CHECK_EQUAL(test[4], 0x77);
    OCIO_CHECK_EQUAL(test[5], 0x88);

    OCIO_CHECK_EQUAL(test[6], 0x55);
    OCIO_CHECK_EQUAL(test[7], 0x66);

    SampleICC::Swap64Array((void*)test, 1);

    OCIO_CHECK_EQUAL(test[7], 0x33);
    OCIO_CHECK_EQUAL(test[6], 0x44);
    OCIO_CHECK_EQUAL(test[5], 0x11);
    OCIO_CHECK_EQUAL(test[4], 0x22);
    OCIO_CHECK_EQUAL(test[3], 0x77);
    OCIO_CHECK_EQUAL(test[2], 0x88);
    OCIO_CHECK_EQUAL(test[1], 0x55);
    OCIO_CHECK_EQUAL(test[0], 0x66);

}

#endif // OCIO_UNIT_TEST
