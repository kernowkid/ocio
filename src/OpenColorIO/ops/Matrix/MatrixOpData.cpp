// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include <sstream>

#include <OpenColorIO/OpenColorIO.h>

#include "HashUtils.h"
#include "MathUtils.h"
#include "ops/Matrix/MatrixOpData.h"
#include "Platform.h"

OCIO_NAMESPACE_ENTER
{

MatrixOpData::Offsets::Offsets()
{
    memset(m_values, 0, 4 * sizeof(double));
}

MatrixOpData::Offsets::Offsets(const Offsets & o)
{
    memcpy(m_values, o.m_values, 4 * sizeof(double));
}

MatrixOpData::Offsets::~Offsets()
{
}

MatrixOpData::Offsets& MatrixOpData::Offsets::operator=(const Offsets & o)
{
    if (this != &o)
    {
        memcpy(m_values, o.m_values, 4 * sizeof(double));
    }
    return *this;
}

bool MatrixOpData::Offsets::operator==(const Offsets & o) const
{
    return (memcmp(m_values, o.m_values, 4 * sizeof(double)) == 0);
}

template<typename T>
void MatrixOpData::Offsets::setRGB(const T * v3)
{
    if (!v3)
    {
        throw Exception("Matrix: setRGB NULL pointer.");
    }

    m_values[0] = v3[0];
    m_values[1] = v3[1];
    m_values[2] = v3[2];
    m_values[3] = (T)0.;
}

template void MatrixOpData::Offsets::setRGB(const float * v3);
template void MatrixOpData::Offsets::setRGB(const double * v3);

template<typename T>
void MatrixOpData::Offsets::setRGBA(const T * v4)
{
    if (!v4)
    {
        throw Exception("Matrix: setRGBA NULL pointer.");
    }

    m_values[0] = v4[0];
    m_values[1] = v4[1];
    m_values[2] = v4[2];
    m_values[3] = v4[3];
}

template void MatrixOpData::Offsets::setRGBA(const float * v4);
template void MatrixOpData::Offsets::setRGBA(const double * v4);

bool MatrixOpData::Offsets::isNotNull() const
{
    static const double zero4[] = { 0., 0., 0., 0. };
    return (memcmp(m_values, zero4, 4 * sizeof(double)) != 0);
}

void MatrixOpData::Offsets::scale(double s)
{
    for (unsigned int i = 0; i < 4; ++i)
    {
        m_values[i] *= s;
    }
}

MatrixOpData::MatrixArray::MatrixArray(unsigned long dimension,
                                       unsigned long numColorComponents)
{
    resize(dimension, numColorComponents);
    fill();
}

MatrixOpData::MatrixArray::~MatrixArray()
{
}

MatrixOpData::MatrixArray & MatrixOpData::MatrixArray::operator=(const ArrayDouble & a)
{
    if (this == &a) return *this;

    *(ArrayDouble*)this = a;

    validate();

    return *this;
}

MatrixOpData::MatrixArrayPtr
    MatrixOpData::MatrixArray::inner(const MatrixArray & B) const
{
    // Use operator= to make sure we have a 4x4 copy
    // of the original matrices.
    MatrixArray A_4x4 = *this;
    MatrixArray B_4x4 = B;
    const ArrayDouble::Values & Avals = A_4x4.getValues();
    const ArrayDouble::Values & Bvals = B_4x4.getValues();
    const unsigned long dim = 4;

    MatrixArrayPtr OutPtr = std::make_shared<MatrixArray>(dim, 4);
    ArrayDouble::Values& Ovals = OutPtr->getValues();

    // Note: The matrix elements are stored in the vector
    // in row-major order.
    // [ a00, a01, a02, a03, a10, a11, a12, a13, a20, ... a44 ]
    for (unsigned long row = 0; row<dim; ++row)
    {
        for (unsigned long col = 0; col<dim; ++col)
        {
            double accum = 0.;
            for (unsigned long i = 0; i<dim; ++i)
            {
                accum += Avals[row * dim + i] * Bvals[i * dim + col];
            }
            Ovals[row * dim + col] = accum;
        }
    }

    return OutPtr;
}

void MatrixOpData::MatrixArray::inner(const MatrixOpData::Offsets & b,
                                      Offsets & out) const
{
    const unsigned long dim = getLength();
    const ArrayDouble::Values & Avals = getValues();

    for (unsigned long i = 0; i<dim; ++i)
    {
        double accum = 0.;
        for (unsigned long j = 0; j<dim; ++j)
        {
            accum += Avals[i * dim + j] * b[j];
        }
        out[i] = accum;
    }
}

MatrixOpData::MatrixArrayPtr
    MatrixOpData::MatrixArray::inverse() const
{
    // Call validate to ensure that the matrix is 4x4,
    // will be expanded if only 3x3.
    validate();

    MatrixArray t(*this);

    const unsigned long dim = 4;
    // Create a new matrix array.
    // The new matrix is initialized as identity.
    MatrixArrayPtr invPtr = std::make_shared<MatrixArray>(dim, 4);
    MatrixArray & s = *invPtr;

    // Inversion starts with identity (without bit-depth scaling).
    s[0] = 1.;
    s[5] = 1.;
    s[10] = 1.;
    s[15] = 1.;

    // From Imath
    // Code copied from Matrix44<T>::gjInverse (bool singExc) const in ImathMatrix.h

    // Forward elimination.

    for (int i = 0; i < 3; i++)
    {
        int pivot = i;

        double pivotsize = t[i*dim + i];

        if (pivotsize < 0)
            pivotsize = -pivotsize;

        for (int j = i + 1; j < 4; j++)
        {
            double tmp = t[j*dim + i];

            if (tmp < 0.0)
                tmp = -tmp;

            if (tmp > pivotsize)
            {
                pivot = j;
                pivotsize = tmp;
            }
        }

        if (pivotsize == 0.0)
        {
            throw Exception("Singular Matrix can't be inverted.");
        }

        if (pivot != i)
        {
            for (int j = 0; j < 4; j++)
            {
                double tmp;

                tmp = t[i*dim + j];
                t[i*dim + j] = t[pivot*dim + j];
                t[pivot*dim + j] = tmp;

                tmp = s[i*dim + j];
                s[i*dim + j] = s[pivot*dim + j];
                s[pivot*dim + j] = tmp;
            }
        }

        for (int j = i + 1; j < 4; j++)
        {
            double f = t[j*dim + i] / t[i*dim + i];

            for (int k = 0; k < 4; k++)
            {
                t[j*dim + k] -= f * t[i*dim + k];
                s[j*dim + k] -= f * s[i*dim + k];
            }
        }
    }

    // Backward substitution.

    for (int i = 3; i >= 0; --i)
    {
        double f;
                
        // TODO: Perhaps change to throw even if f is near
        //       zero (nearly singular).
        if ((f = t[i*dim + i]) == 0.0)
        {
            throw Exception("Singular Matrix can't be inverted.");
        }

        for (int j = 0; j < 4; j++)
        {
            t[i*dim + j] /= f;
            s[i*dim + j] /= f;
        }

        for (int j = 0; j < i; j++)
        {
            f = t[j*dim + i];

            for (int k = 0; k < 4; k++)
            {
                t[j*dim + k] -= f * t[i*dim + k];
                s[j*dim + k] -= f * s[i*dim + k];
            }
        }
    }

    return invPtr;
}

template<typename T>
void MatrixOpData::MatrixArray::setRGB(const T * values)
{
    Values & v = getValues();

    v[0] = values[0];
    v[1] = values[1];
    v[2] = values[2];
    v[3] = (T)0.0;

    v[4] = values[3];
    v[5] = values[4];
    v[6] = values[5];
    v[7] = (T)0.0;

    v[8] = values[6];
    v[9] = values[7];
    v[10] = values[8];
    v[11] = (T)0.0;

    v[12] = (T)0.0;
    v[13] = (T)0.0;
    v[14] = (T)0.0;
    v[15] = (T)1.0;
}

template void MatrixOpData::MatrixArray::setRGB(const float * values);
template void MatrixOpData::MatrixArray::setRGB(const double * values);

unsigned long MatrixOpData::MatrixArray::getNumValues() const
{
    return getLength() * getLength();
}

bool MatrixOpData::MatrixArray::isUnityDiagonal() const
{
    const unsigned long dim = getLength();
    const ArrayDouble::Values & values = getValues();

    for (unsigned long i = 0; i<dim; ++i)
    {
        for (unsigned long j = 0; j<dim; ++j)
        {
            if (i == j)
            {
                if (values[i*dim + j] != 1.0)  // Strict comparison intended
                {
                    return false;
                }
            }
            else
            {
                if (values[i*dim + j] != 0.0)  // Strict comparison intended
                {
                    return false;
                }
            }
        }
    }

    return true;
}

void MatrixOpData::MatrixArray::fill()
{
    const unsigned long dim = getLength();
    ArrayDouble::Values & values = getValues();

    memset(&values[0], 0, values.size() * sizeof(double));

    for (unsigned long i = 0; i<dim; ++i)
    {
        for (unsigned long j = 0; j<dim; ++j)
        {
            if (i == j)
            {
                values[i*dim + j] = 1.0;
            }
        }
    }
}

void MatrixOpData::MatrixArray::expandFrom3x3To4x4()
{
    const Values oldValues = getValues();

    resize(4, 4);

    setRGB(oldValues.data());
}

void MatrixOpData::MatrixArray::setRGBA(const float * values)
{
    Values & v = getValues();

    v[0] = values[0];
    v[1] = values[1];
    v[2] = values[2];
    v[3] = values[3];

    v[4] = values[4];
    v[5] = values[5];
    v[6] = values[6];
    v[7] = values[7];

    v[8] = values[8];
    v[9] = values[9];
    v[10] = values[10];
    v[11] = values[11];

    v[12] = values[12];
    v[13] = values[13];
    v[14] = values[14];
    v[15] = values[15];
}

void MatrixOpData::MatrixArray::setRGBA(const double * values)
{
    Values & v = getValues();
    memcpy(&v[0], values, 16 * sizeof(double));
}

void MatrixOpData::MatrixArray::validate() const
{
    ArrayDouble::validate();

    // A 4x4 matrix is the canonical form, convert if it is only a 3x3.
    if (getLength() == 3)
    {
        const_cast<MatrixArray*>(this)->expandFrom3x3To4x4();
    }
    else if (getLength() != 4)
    {
        throw Exception("Matrix: array content issue.");
    }

    if (getNumColorComponents() != 4)
    {
        throw Exception("Matrix: dimensions must be 4x4.");
    }
}

////////////////////////////////////////////////

MatrixOpData::MatrixOpData()
    : OpData()
    , m_array(4, 4)
{
}

MatrixOpData::~MatrixOpData()
{
}

MatrixOpDataRcPtr MatrixOpData::clone() const
{
    return std::make_shared<MatrixOpData>(*this);
}

void MatrixOpData::setArrayValue(unsigned long index, double value)
{
    m_array.getValues()[index] = value;
}

void  MatrixOpData::setRGB(const float* values)
{
    m_array.setRGB(values);
}

template<typename T>
void MatrixOpData::setRGBA(const T * values)
{
    m_array.setRGBA(values);
}

template void MatrixOpData::setRGBA(const float * values);
template void MatrixOpData::setRGBA(const double * values);

void MatrixOpData::validate() const
{
    OpData::validate();

    try
    {
        m_array.validate();
    }
    catch (Exception & e)
    {
        std::ostringstream oss;
        oss << "Matrix array content issue: ";
        oss << e.what();

        throw Exception(oss.str().c_str());
    }
}

// We do a number of exact floating-point comparisons in the following
// methods. Note that this op may be used to do very fine adjustments
// to pixels. Therefore it is problematic to attempt to judge values
// passed in from a user's transform as to whether they are "close enough"
// to e.g. 1 or 0. However, we still want to allow a matrix and its
// inverse to be composed and be able to call the result an identity
// (recognizing it won't quite be). Therefore, the strategy here is to do
// exact compares on users files but to "clean up" matrices as part of
// composition to make this work in practice. The concept is that the
// tolerances are moved to where errors are introduced rather than
// indiscriminately applying them to all user ops.

bool MatrixOpData::isUnityDiagonal() const
{
    return m_array.isUnityDiagonal();
}

bool MatrixOpData::isNoOp() const
{
    return isIdentity();
}

// For all ops, an "Identity" is an op that only does bit-depth conversion
// and is therefore a candidate for the optimizer to remove.
bool MatrixOpData::isIdentity() const
{
    if (hasOffsets() || hasAlpha() || !isDiagonal())
    {
        return false;
    }

    return isMatrixIdentity();
}
         
bool MatrixOpData::isMatrixIdentity() const
{
    // Now check the diagonal elements.

    const double maxDiff = 1e-6;

    const ArrayDouble & a = getArray();
    const ArrayDouble::Values & m = a.getValues();
    const unsigned long dim = a.getLength();

    for (unsigned long i = 0; i<dim; ++i)
    {
        for (unsigned long j = 0; j<dim; ++j)
        {
            if (i == j)
            {
                if (!EqualWithAbsError(m[i*dim + j], 1.0, maxDiff))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

bool MatrixOpData::isDiagonal() const
{
    const ArrayDouble & a = getArray();
    const ArrayDouble::Values & m = a.getValues();
    const unsigned long max = a.getNumValues();
    const unsigned long dim = a.getLength();

    for (unsigned long idx = 0; idx<max; ++idx)
    {
        if ((idx % (dim + 1)) != 0) // Not on the diagonal
        {
            if (m[idx] != 0.0) // Strict comparison intended
            {
                return false;
            }
        }
    }

    return true;
}

bool MatrixOpData::hasAlpha() const
{
    const ArrayDouble & a = getArray();
    const ArrayDouble::Values & m = a.getValues();

    // Now check the diagonal elements.

    const double maxDiff = 1e-6;

    return

        // Last column.
        (m[3] != 0.0) || // Strict comparison intended
        (m[7] != 0.0) ||
        (m[11] != 0.0) ||

        // Diagonal.
        !EqualWithAbsError(m[15], 1.0, maxDiff) ||

        // Bottom row.
        (m[12] != 0.0) || // Strict comparison intended
        (m[13] != 0.0) ||
        (m[14] != 0.0) ||

        // Alpha offset
        (m_offsets[3] != 0.0);

}

MatrixOpDataRcPtr MatrixOpData::CreateDiagonalMatrix(double diagValue)
{
    // Create a matrix with no offset.
    MatrixOpDataRcPtr pM = std::make_shared<MatrixOpData>();

    pM->validate();

    pM->setArrayValue(0, diagValue);
    pM->setArrayValue(5, diagValue);
    pM->setArrayValue(10, diagValue);
    pM->setArrayValue(15, diagValue);

    return pM;
}

double MatrixOpData::getOffsetValue(unsigned long index) const
{
    const unsigned long dim = getArray().getLength();
    if (index >= dim)
    {
        // TODO: should never happen. Consider assert.
        std::ostringstream oss;
        oss << "Matrix array content issue: '";
        oss << getID().c_str();
        oss << "' offset index out of range '";
        oss << index;
        oss << "'. ";

        throw Exception(oss.str().c_str());
    }

    return m_offsets[index];
}

void MatrixOpData::setOffsetValue(unsigned long index, double value)
{
    const unsigned long dim = getArray().getLength();
    if (index >= dim)
    {
        // TODO: should never happen. Consider assert.
        std::ostringstream oss;
        oss << "Matrix array content issue: '";
        oss << getID().c_str();
        oss << "' offset index out of range '";
        oss << index;
        oss << "'. ";

        throw Exception(oss.str().c_str());
    }

    m_offsets[index] = value;
}

MatrixOpDataRcPtr MatrixOpData::compose(ConstMatrixOpDataRcPtr & B) const
{
    // Ensure that both matrices will have the right dimension (ie. 4x4).
    if (m_array.getLength() != 4 || B->m_array.getLength() != 4)
    {
        // Note: By design, only 4x4 matrices are instantiated.
        // The CLF 3x3 (and 3x4) matrices are automatically converted
        // to 4x4 matrices, and a Matrix Transform only expects 4x4 matrices.
        throw Exception("MatrixOpData: array content issue.");
    }

    // TODO: May want to revisit how the metadata is set.
    FormatMetadataImpl newDesc = getFormatMetadata();
    newDesc.combine(B->getFormatMetadata());

    MatrixOpDataRcPtr out = std::make_shared<MatrixOpData>();

    out->setFileInputBitDepth(getFileInputBitDepth());
    out->setFileOutputBitDepth(B->getFileOutputBitDepth());

    out->getFormatMetadata() = newDesc;

    // By definition, A.compose(B) implies that op A precedes op B
    // in the opList. The LUT format coefficients follow matrix math:
    // vec2 = A x vec1 where A is 3x3 and vec is 3x1.
    // So the composite operation in matrix form is vec2 = B x A x vec1.
    // Hence we compute B x A rather than A x B.

    MatrixArrayPtr outPtr = B->m_array.inner(this->m_array);

    out->getArray() = *outPtr.get();

    // Compute matrix B times offsets from A.

    Offsets offs;

    B->m_array.inner(getOffsets(), offs);

    const unsigned long dim = this->m_array.getLength();

    // Determine overall scaling of the offsets prior to any catastrophic
    // cancellation that may occur during the add.
    double val, max_val = 0.;
    for (unsigned long i = 0; i<dim; ++i)
    {
        val = fabs(offs[i]);
        max_val = max_val > val ? max_val : val;
        val = fabs(B->getOffsets()[i]);
        max_val = max_val > val ? max_val : val;
    }

    // Add offsets from B.
    for (unsigned long i = 0; i<dim; ++i)
    {
        offs[i] += B->getOffsets()[i];
    }

    out->setOffsets(offs);

    // To enable use of strict float comparisons above, we adjust the
    // result so that values very near integers become exactly integers.
    out->cleanUp(max_val);

    return out;
}

void MatrixOpData::cleanUp(double offsetScale)
{
    const ArrayDouble & a = getArray();
    const ArrayDouble::Values & m = a.getValues();
    const unsigned long dim = a.getLength();

    // Estimate the magnitude of the matrix.
    double max_val = 0.;
    for (unsigned long i = 0; i<dim; ++i)
    {
        for (unsigned long j = 0; j<dim; ++j)
        {
            const double val = fabs(m[i * dim + j]);
            max_val = max_val > val ? max_val : val;
        }
    }

    // Determine an absolute tolerance.
    // TODO: For double matrices a smaller tolerance could be used.  However we
    // have matrices that may have been quantized to less than double precision
    // either from being written to files or via the factories that take float
    // args.  In any case, the tolerance is small enough to pick up anything
    // that would be significant in the context of color management.
    const double scale = max_val > 1e-4 ? max_val : 1e-4;
    const double abs_tol = scale * 1e-6;

    // Replace values that are close to integers by exact values.
    for (unsigned long i = 0; i<dim; ++i)
    {
        for (unsigned long j = 0; j<dim; ++j)
        {
            const double val = m[i * dim + j];
            const double round_val = round(val);
            const double diff = fabs(val - round_val);
            if (diff < abs_tol)
            {
                setArrayValue(i * dim + j, round_val);
            }
        }
    }

    // Do likewise for the offsets.
    const double scale2 = offsetScale > 1e-4 ? offsetScale : 1e-4;
    const double abs_tol2 = scale2 * 1e-6;

    for (unsigned long i = 0; i<dim; ++i)
    {
        const double val = getOffsets()[i];
        const double round_val = round(val);
        const double diff = fabs(val - round_val);
        if (diff < abs_tol2)
        {
            setOffsetValue(i, round_val);
        }
    }
}

bool MatrixOpData::operator==(const OpData & other) const
{
    if (this == &other) return true;

    if (!OpData::operator==(other)) return false;

    const MatrixOpData* mop = static_cast<const MatrixOpData*>(&other);

    return (m_array == mop->m_array &&
            m_offsets == mop->m_offsets);
}

MatrixOpDataRcPtr MatrixOpData::inverse() const
{
    // Get the inverse matrix.
    MatrixArrayPtr invMatrixArray = m_array.inverse();
    // MatrixArray::inverse() will throw for singular matrices.

    // Calculate the inverse offset.
    const Offsets& offsets = getOffsets();
    Offsets invOffsets;
    if (offsets.isNotNull())
    {
        invMatrixArray->inner(offsets, invOffsets);
        invOffsets.scale(-1);
    }

    MatrixOpDataRcPtr invOp = std::make_shared<MatrixOpData>();
    invOp->setFileInputBitDepth(getFileOutputBitDepth());
    invOp->setFileOutputBitDepth(getFileInputBitDepth());

    invOp->setRGBA(&(invMatrixArray->getValues()[0]));
    invOp->setOffsets(invOffsets);
    invOp->getFormatMetadata() = getFormatMetadata();

    // No need to call validate(), the invOp will have proper dimension,
    // bit-depths, matrix and offets values.

    // Note that any existing metadata could become stale at this point but
    // trying to update it is also challenging since inverse() is sometimes
    // called even during the creation of new ops.
    return invOp;
}

OpDataRcPtr MatrixOpData::getIdentityReplacement() const
{
    return std::make_shared<MatrixOpData>();
}

void MatrixOpData::finalize()
{
    AutoMutex lock(m_mutex);

    validate();

    std::ostringstream cacheIDStream;
    cacheIDStream << getID();

    md5_state_t state;
    md5_byte_t digest[16];

    // TODO: array and offset do not require double precison in cache.
    md5_init(&state);
    md5_append(&state,
        (const md5_byte_t *)&(getArray().getValues()[0]),
        (int)(16 * sizeof(double)));
    md5_append(&state,
        (const md5_byte_t *)getOffsets().getValues(),
        (int)(4 * sizeof(double)));
    md5_finish(&state, digest);

    cacheIDStream << GetPrintableHash(digest);
    m_cacheID = cacheIDStream.str();
}

void MatrixOpData::scale(double inScale, double outScale)
{
    const double combinedScale = inScale * outScale;
    getArray().scale(combinedScale);

    m_offsets.scale(outScale);
}

}
OCIO_NAMESPACE_EXIT


#ifdef OCIO_UNIT_TEST

namespace OCIO = OCIO_NAMESPACE;
#include "UnitTest.h"

OCIO_ADD_TEST(MatrixOpData, empty)
{
    OCIO::MatrixOpData m;
    OCIO_CHECK_ASSERT(m.isNoOp());
    OCIO_CHECK_ASSERT(m.isUnityDiagonal());
    OCIO_CHECK_ASSERT(m.isDiagonal());
    OCIO_CHECK_NO_THROW(m.validate());
    OCIO_CHECK_EQUAL(m.getType(), OCIO::OpData::MatrixType);

    OCIO_CHECK_EQUAL(m.getArray().getLength(), 4);
    OCIO_CHECK_EQUAL(m.getArray().getNumValues(), 16);
    OCIO_CHECK_EQUAL(m.getArray().getNumColorComponents(), 4);

    m.getArray().resize(3, 3);

    OCIO_CHECK_EQUAL(m.getArray().getNumValues(), 9);
    OCIO_CHECK_EQUAL(m.getArray().getLength(), 3);
    OCIO_CHECK_EQUAL(m.getArray().getNumColorComponents(), 3);
    OCIO_CHECK_NO_THROW(m.validate());
}

OCIO_ADD_TEST(MatrixOpData, accessors)
{
    OCIO::MatrixOpData m;
    OCIO_CHECK_ASSERT(m.isNoOp());
    OCIO_CHECK_ASSERT(m.isUnityDiagonal());
    OCIO_CHECK_ASSERT(m.isDiagonal());
    OCIO_CHECK_ASSERT(m.isIdentity());
    OCIO_CHECK_NO_THROW(m.validate());

    m.setArrayValue(15, 1 + 1e-5f);

    OCIO_CHECK_ASSERT(!m.isNoOp());
    OCIO_CHECK_ASSERT(!m.isUnityDiagonal());
    OCIO_CHECK_ASSERT(m.isDiagonal());
    OCIO_CHECK_ASSERT(!m.isIdentity());
    OCIO_CHECK_NO_THROW(m.validate());

    m.setArrayValue(1, 1e-5f);
    m.setArrayValue(15, 1.0f);

    OCIO_CHECK_ASSERT(!m.isNoOp());
    OCIO_CHECK_ASSERT(!m.isUnityDiagonal());
    OCIO_CHECK_ASSERT(!m.isDiagonal());
    OCIO_CHECK_ASSERT(!m.isIdentity());
    OCIO_CHECK_NO_THROW(m.validate());

    OCIO_CHECK_EQUAL(m.getFileInputBitDepth(), OCIO::BIT_DEPTH_UNKNOWN);
    OCIO_CHECK_EQUAL(m.getFileOutputBitDepth(), OCIO::BIT_DEPTH_UNKNOWN);
    m.setFileInputBitDepth(OCIO::BIT_DEPTH_UINT10);
    m.setFileOutputBitDepth(OCIO::BIT_DEPTH_UINT8);
    OCIO_CHECK_EQUAL(m.getFileInputBitDepth(), OCIO::BIT_DEPTH_UINT10);
    OCIO_CHECK_EQUAL(m.getFileOutputBitDepth(), OCIO::BIT_DEPTH_UINT8);

    OCIO::MatrixOpData m1{ m };
    OCIO_CHECK_EQUAL(m1.getFileInputBitDepth(), OCIO::BIT_DEPTH_UINT10);
    OCIO_CHECK_EQUAL(m1.getFileOutputBitDepth(), OCIO::BIT_DEPTH_UINT8);

    OCIO::MatrixOpData m2;
    m2 = m;
    OCIO_CHECK_EQUAL(m2.getFileInputBitDepth(), OCIO::BIT_DEPTH_UINT10);
    OCIO_CHECK_EQUAL(m2.getFileOutputBitDepth(), OCIO::BIT_DEPTH_UINT8);
}

OCIO_ADD_TEST(MatrixOpData, offsets)
{
    OCIO::MatrixOpData m;
    OCIO_CHECK_ASSERT(m.isNoOp());
    OCIO_CHECK_ASSERT(m.isUnityDiagonal());
    OCIO_CHECK_ASSERT(m.isDiagonal());
    OCIO_CHECK_ASSERT(!m.hasOffsets());
    OCIO_CHECK_NO_THROW(m.validate());

    m.setOffsetValue(2, 1.0f);
    OCIO_CHECK_ASSERT(!m.isNoOp());
    OCIO_CHECK_ASSERT(m.isUnityDiagonal());
    OCIO_CHECK_ASSERT(m.isDiagonal());
    OCIO_CHECK_ASSERT(m.hasOffsets());
    OCIO_CHECK_NO_THROW(m.validate());
    OCIO_CHECK_EQUAL(m.getOffsets()[2], 1.0f);
}

OCIO_ADD_TEST(MatrixOpData, offsets4)
{
    OCIO::MatrixOpData m;
    OCIO_CHECK_ASSERT(m.isNoOp());
    OCIO_CHECK_ASSERT(m.isUnityDiagonal());
    OCIO_CHECK_ASSERT(m.isDiagonal());
    OCIO_CHECK_ASSERT(!m.hasOffsets());
    OCIO_CHECK_NO_THROW(m.validate());

    m.setOffsetValue(3, -1e-6f);
    OCIO_CHECK_ASSERT(!m.isNoOp());
    OCIO_CHECK_ASSERT(m.isUnityDiagonal());
    OCIO_CHECK_ASSERT(m.isDiagonal());
    OCIO_CHECK_ASSERT(m.hasOffsets());
    OCIO_CHECK_NO_THROW(m.validate());
    OCIO_CHECK_EQUAL(m.getOffsets()[3], -1e-6f);
}

OCIO_ADD_TEST(MatrixOpData, diagonal)
{
    OCIO::MatrixOpDataRcPtr pM = OCIO::MatrixOpData::CreateDiagonalMatrix(0.5);

    OCIO_CHECK_ASSERT(pM->isDiagonal());
    OCIO_CHECK_ASSERT(!pM->hasOffsets());
    OCIO_CHECK_NO_THROW(pM->validate());
    OCIO_CHECK_EQUAL(pM->getArray().getValues()[0], 0.5);
    OCIO_CHECK_EQUAL(pM->getArray().getValues()[5], 0.5);
    OCIO_CHECK_EQUAL(pM->getArray().getValues()[10], 0.5);
    OCIO_CHECK_EQUAL(pM->getArray().getValues()[15], 0.5);
}

#define MATRIX_TEST_HAS_ALPHA(id, val)  \
{                                       \
    mat.getArray()[id] = val + 0.001;   \
    OCIO_CHECK_ASSERT(mat.hasAlpha());  \
    mat.getArray()[id] = val;           \
    OCIO_CHECK_ASSERT(!mat.hasAlpha()); \
}

OCIO_ADD_TEST(MatrixOpData, has_alpha)
{
    OCIO::MatrixOpData mat;
    OCIO_CHECK_ASSERT(!mat.hasAlpha());

    MATRIX_TEST_HAS_ALPHA(3, 0.0);
    MATRIX_TEST_HAS_ALPHA(7, 0.0);
    MATRIX_TEST_HAS_ALPHA(11, 0.0);
    MATRIX_TEST_HAS_ALPHA(12, 0.0);
    MATRIX_TEST_HAS_ALPHA(13, 0.0);
    MATRIX_TEST_HAS_ALPHA(14, 0.0);
    MATRIX_TEST_HAS_ALPHA(15, 1.0);

    mat.getOffsets()[3] = 0.001;
    OCIO_CHECK_ASSERT(mat.hasAlpha());
    mat.getOffsets()[3] = 0.0;
    OCIO_CHECK_ASSERT(!mat.hasAlpha());
}

#undef MATRIX_TEST_HAS_ALPHA

OCIO_ADD_TEST(MatrixOpData, clone)
{
    OCIO::MatrixOpData ref;
    ref.setOffsetValue(2, 1.0f);
    ref.setArrayValue(0, 2.0f);

    OCIO::MatrixOpDataRcPtr pClone = ref.clone();

    OCIO_CHECK_ASSERT(pClone.get());
    OCIO_CHECK_ASSERT(!pClone->isNoOp());
    OCIO_CHECK_ASSERT(!pClone->isUnityDiagonal());
    OCIO_CHECK_ASSERT(pClone->isDiagonal());
    OCIO_CHECK_NO_THROW(pClone->validate());
    OCIO_CHECK_EQUAL(pClone->getType(), OCIO::OpData::MatrixType);
    OCIO_CHECK_EQUAL(pClone->getOffsets()[0], 0.0f);
    OCIO_CHECK_EQUAL(pClone->getOffsets()[1], 0.0f);
    OCIO_CHECK_EQUAL(pClone->getOffsets()[2], 1.0f);
    OCIO_CHECK_EQUAL(pClone->getOffsets()[3], 0.0f);
    OCIO_CHECK_ASSERT(pClone->getArray() == ref.getArray());
}

OCIO_ADD_TEST(MatrixOpData, clone_offsets4)
{
    OCIO::MatrixOpData ref;
    ref.setOffsetValue(0, 1.0f);
    ref.setOffsetValue(1, 2.0f);
    ref.setOffsetValue(2, 3.0f);
    ref.setOffsetValue(3, 4.0f);
    ref.setArrayValue(0, 2.0f);

    OCIO::MatrixOpDataRcPtr pClone = ref.clone();

    OCIO_CHECK_ASSERT(pClone.get());
    OCIO_CHECK_ASSERT(!pClone->isNoOp());
    OCIO_CHECK_ASSERT(!pClone->isUnityDiagonal());
    OCIO_CHECK_ASSERT(pClone->isDiagonal());
    OCIO_CHECK_NO_THROW(pClone->validate());
    OCIO_CHECK_EQUAL(pClone->getType(), OCIO::OpData::MatrixType);
    OCIO_CHECK_EQUAL(pClone->getOffsets()[0], 1.0f);
    OCIO_CHECK_EQUAL(pClone->getOffsets()[1], 2.0f);
    OCIO_CHECK_EQUAL(pClone->getOffsets()[2], 3.0f);
    OCIO_CHECK_EQUAL(pClone->getOffsets()[3], 4.0f);
    OCIO_CHECK_ASSERT(pClone->getArray() == ref.getArray());
}

OCIO_ADD_TEST(MatrixOpData, test_construct)
{
    OCIO::MatrixOpData matOp;

    OCIO_CHECK_EQUAL(matOp.getID(), "");
    OCIO_CHECK_EQUAL(matOp.getType(), OCIO::OpData::MatrixType);
    OCIO_CHECK_ASSERT(matOp.getFormatMetadata().getChildrenElements().empty());
    OCIO_CHECK_EQUAL(matOp.getOffsets()[0], 0.0f);
    OCIO_CHECK_EQUAL(matOp.getOffsets()[1], 0.0f);
    OCIO_CHECK_EQUAL(matOp.getOffsets()[2], 0.0f);
    OCIO_CHECK_EQUAL(matOp.getOffsets()[3], 0.0f);
    OCIO_CHECK_EQUAL(matOp.getArray().getLength(), 4);
    OCIO_CHECK_EQUAL(matOp.getArray().getNumColorComponents(), 4);
    OCIO_CHECK_EQUAL(matOp.getArray().getNumValues(), 16);
    const OCIO::ArrayDouble::Values & val = matOp.getArray().getValues();
    OCIO_CHECK_EQUAL(val.size(), 16);
    OCIO_CHECK_EQUAL(val[0], 1.0f);
    OCIO_CHECK_EQUAL(val[1], 0.0f);
    OCIO_CHECK_EQUAL(val[2], 0.0f);
    OCIO_CHECK_EQUAL(val[3], 0.0f);

    OCIO_CHECK_EQUAL(val[4], 0.0f);
    OCIO_CHECK_EQUAL(val[5], 1.0f);
    OCIO_CHECK_EQUAL(val[6], 0.0f);
    OCIO_CHECK_EQUAL(val[7], 0.0f);

    OCIO_CHECK_EQUAL(val[8], 0.0f);
    OCIO_CHECK_EQUAL(val[9], 0.0f);
    OCIO_CHECK_EQUAL(val[10], 1.0f);
    OCIO_CHECK_EQUAL(val[11], 0.0f);

    OCIO_CHECK_EQUAL(val[12], 0.0f);
    OCIO_CHECK_EQUAL(val[13], 0.0f);
    OCIO_CHECK_EQUAL(val[14], 0.0f);
    OCIO_CHECK_EQUAL(val[15], 1.0f);

    OCIO_CHECK_NO_THROW(matOp.validate());

    matOp.getArray().resize(3, 3); // validate() will resize to 4x4

    OCIO_CHECK_EQUAL(matOp.getArray().getNumValues(), 9);
    OCIO_CHECK_EQUAL(matOp.getArray().getLength(), 3);
    OCIO_CHECK_EQUAL(matOp.getArray().getNumColorComponents(), 3);

    OCIO_CHECK_NO_THROW(matOp.validate());

    OCIO_CHECK_EQUAL(matOp.getArray().getNumValues(), 16);
    OCIO_CHECK_EQUAL(matOp.getArray().getLength(), 4);
    OCIO_CHECK_EQUAL(matOp.getArray().getNumColorComponents(), 4);
}

// Validate matrix composition.
OCIO_ADD_TEST(MatrixOpData, composition)
{
    // Create two test ops.
    const float mtxA[] = {  1, 2, 3, 4,
                            4, 5, 6, 7,
                            7, 8, 9, 10,
                            11, 12, 13, 14 };
    const float offsA[] = { 10, 11, 12, 13 };

    OCIO::MatrixOpData mA;
    mA.setFileInputBitDepth(OCIO::BIT_DEPTH_UINT8);
    mA.setFileOutputBitDepth(OCIO::BIT_DEPTH_F16);

    mA.setRGBA(mtxA);
    mA.setRGBAOffsets(offsA);

    const float mtxB[] = { 21, 22, 23, 24,
                           24, 25, 26, 27,
                           27, 28, 29, 30,
                           31, 32, 33, 34 };
    const float offsB[] = { 30, 31, 32, 33 };

    OCIO::MatrixOpDataRcPtr mB = std::make_shared<OCIO::MatrixOpData>();
    mB->setFileInputBitDepth(OCIO::BIT_DEPTH_F16);
    mB->setFileOutputBitDepth(OCIO::BIT_DEPTH_UINT10);

    mB->setRGBA(mtxB);
    mB->setRGBAOffsets(offsB);

    // Correct results.
    const float aim[] = { 534, 624, 714, 804,
                          603, 705, 807, 909,
                          672, 786, 900, 1014,
                          764, 894, 1024, 1154 };
    const float aim_offs[] = { 1040 + 30, 1178 + 31, 1316 + 32, 1500 + 33 };

    // Compose.
    OCIO::ConstMatrixOpDataRcPtr mBConst = mB;
    OCIO::MatrixOpDataRcPtr result(mA.compose(mBConst));

    // Check bit-depths copied correctly.
    OCIO_CHECK_EQUAL(result->getFileInputBitDepth(), OCIO::BIT_DEPTH_UINT8);
    OCIO_CHECK_EQUAL(result->getFileOutputBitDepth(), OCIO::BIT_DEPTH_UINT10);

    const OCIO::ArrayDouble::Values& newCoeff = result->getArray().getValues();

    // Sanity check on size.
    OCIO_CHECK_ASSERT(newCoeff.size() == 16);

    // Coefficient check.
    for (unsigned long i = 0; i < newCoeff.size(); i++)
    {
        OCIO_CHECK_EQUAL(aim[i], newCoeff[i]);
    }

    // Offset check.
    const unsigned long dim = result->getArray().getLength();
    for (unsigned long i = 0; i < dim; i++)
    {
        OCIO_CHECK_EQUAL(aim_offs[i], result->getOffsets()[i]);
    }
}

OCIO_ADD_TEST(MatrixOpData, equality)
{
    OCIO::MatrixOpData m1;
    m1.setArrayValue(0, 2);

    OCIO::MatrixOpData m2;
    m2.setID("invalid_u_id_test");
    m2.setArrayValue(0, 2);

    // id is part of metadata. FormatMetadataImpl is ignored for ==.
    OCIO_CHECK_ASSERT(m1 == m2);

    // File bit-depth is ignored for ==.
    m1.setFileInputBitDepth(OCIO::BIT_DEPTH_UINT8);
    OCIO_CHECK_ASSERT(m1 == m2);

    OCIO::MatrixOpData m3;
    m3.setArrayValue(0, 6);

    OCIO_CHECK_ASSERT(!(m1 == m3));

    OCIO::MatrixOpData m4;
    m4.setArrayValue(0, 2);

    OCIO_CHECK_ASSERT(m1 == m4);

    m4.setOffsetValue(3, 1e-5f);

    OCIO_CHECK_ASSERT(!(m1 == m4));
}

OCIO_ADD_TEST(MatrixOpData, rgb)
{
    OCIO::MatrixOpData m;

    const float rgb[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    m.setRGB(rgb);

    const OCIO::ArrayDouble::Values & v = m.getArray().getValues();
    OCIO_CHECK_EQUAL(v[0], rgb[0]);
    OCIO_CHECK_EQUAL(v[1], rgb[1]);
    OCIO_CHECK_EQUAL(v[2], rgb[2]);
    OCIO_CHECK_EQUAL(v[3], 0.0f);

    OCIO_CHECK_EQUAL(v[4], rgb[3]);
    OCIO_CHECK_EQUAL(v[5], rgb[4]);
    OCIO_CHECK_EQUAL(v[6], rgb[5]);
    OCIO_CHECK_EQUAL(v[7], 0.0f);

    OCIO_CHECK_EQUAL(v[8], rgb[6]);
    OCIO_CHECK_EQUAL(v[9], rgb[7]);
    OCIO_CHECK_EQUAL(v[10], rgb[8]);
    OCIO_CHECK_EQUAL(v[11], 0.0f);

    OCIO_CHECK_EQUAL(v[12], 0.0f);
    OCIO_CHECK_EQUAL(v[13], 0.0f);
    OCIO_CHECK_EQUAL(v[14], 0.0f);
    OCIO_CHECK_EQUAL(v[15], 1.0f);
}

OCIO_ADD_TEST(MatrixOpData, rgba)
{
    OCIO::MatrixOpData m;

    const float rgba[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15 };
    m.setRGBA(rgba);

    const OCIO::ArrayDouble::Values & v = m.getArray().getValues();
    for (unsigned long i = 0; i<16; ++i)
    {
        OCIO_CHECK_EQUAL(v[i], rgba[i]);
    }
}

OCIO_ADD_TEST(MatrixOpData, matrixInverse_identity)
{
    OCIO::MatrixOpData refMatrixOp;

    refMatrixOp.setFileInputBitDepth(OCIO::BIT_DEPTH_F32);
    refMatrixOp.setFileOutputBitDepth(OCIO::BIT_DEPTH_UINT12);

    OCIO_CHECK_ASSERT(refMatrixOp.isDiagonal());
    OCIO_CHECK_ASSERT(refMatrixOp.isIdentity());
    OCIO_CHECK_ASSERT(!refMatrixOp.hasOffsets());

    // Get inverse of reference matrix operation.
    OCIO::MatrixOpDataRcPtr invMatrixOp;
    OCIO_CHECK_ASSERT(!invMatrixOp);
    OCIO_CHECK_NO_THROW(invMatrixOp = refMatrixOp.inverse());
    OCIO_REQUIRE_ASSERT(invMatrixOp);

    // Inverse op should have its input/output bit-depth inverted.
    OCIO_CHECK_EQUAL(invMatrixOp->getFileInputBitDepth(),
                     refMatrixOp.getFileOutputBitDepth());
    OCIO_CHECK_EQUAL(invMatrixOp->getFileOutputBitDepth(),
                     refMatrixOp.getFileInputBitDepth());

    // But still be an identity matrix.
    OCIO_CHECK_ASSERT(invMatrixOp->isDiagonal());
    OCIO_CHECK_ASSERT(invMatrixOp->isIdentity());
    OCIO_CHECK_ASSERT(!invMatrixOp->hasOffsets());
}

OCIO_ADD_TEST(MatrixOpData, matrixInverse_singular)
{
    OCIO::MatrixOpData singularMatrixOp;

    // Set singular matrix values.
    const float mat[16] 
        = { 1.0f, 0.f, 0.f, 0.2f,
            0.0f, 0.f, 0.f, 0.0f,
            0.0f, 0.f, 0.f, 0.0f,
            0.2f, 0.f, 0.f, 1.0f };

    singularMatrixOp.setRGBA(mat);

    OCIO_CHECK_ASSERT(!singularMatrixOp.isNoOp());
    OCIO_CHECK_ASSERT(!singularMatrixOp.isUnityDiagonal());
    OCIO_CHECK_ASSERT(!singularMatrixOp.isDiagonal());
    OCIO_CHECK_ASSERT(!singularMatrixOp.isIdentity());
    OCIO_CHECK_ASSERT(!singularMatrixOp.hasOffsets());

    // Get inverse of singular matrix operation.
    OCIO_CHECK_THROW_WHAT(singularMatrixOp.inverse(),
                          OCIO::Exception,
                          "Singular Matrix can't be inverted");
}

OCIO_ADD_TEST(MatrixOpData, inverse)
{
    OCIO::MatrixOpData refMatrixOp;

    // Set arbitrary matrix and offset values.
    const float matrix[16] = { 0.9f,  0.8f, -0.7f, 0.6f,
                              -0.4f,  0.5f,  0.3f, 0.2f,
                               0.1f, -0.2f,  0.4f, 0.3f,
                              -0.5f,  0.6f,  0.7f, 0.8f };
    const float offsets[4] = { -0.1f, 0.2f, -0.3f, 0.4f };

    refMatrixOp.setRGBA(matrix);
    refMatrixOp.setRGBAOffsets(offsets);

    // Get inverse of reference matrix operation.
    OCIO::MatrixOpDataRcPtr invMatrixOp;
    OCIO_CHECK_NO_THROW(invMatrixOp = refMatrixOp.inverse());
    OCIO_REQUIRE_ASSERT(invMatrixOp);

    const float expectedMatrix[16] = {
        0.75f,                3.5f,               3.5f,              -2.75f,
        0.546296296296297f,   3.90740740740741f,  1.31481481481482f, -1.87962962962963f,
        0.12037037037037f,    4.75925925925926f,  4.01851851851852f, -2.78703703703704f,
       -0.0462962962962963f, -4.90740740740741f, -2.31481481481482f,  3.37962962962963f };

    const float expectedOffsets[4] = {
        1.525f, 0.419444444444445f, 1.38055555555556f, -1.06944444444444f };

    const OCIO::ArrayDouble::Values & invValues =
        invMatrixOp->getArray().getValues();
    const double* invOffsets = invMatrixOp->getOffsets().getValues();

    // Check matrix coeffs.
    for (unsigned long i = 0; i < 16; ++i)
    {
        OCIO_CHECK_CLOSE(invValues[i], expectedMatrix[i], 1e-6f);
    }

    // Check matrix offsets.
    for (unsigned long i = 0; i < 4; ++i)
    {
        OCIO_CHECK_CLOSE(invOffsets[i],expectedOffsets[i], 1e-6f);
    }
}

OCIO_ADD_TEST(MatrixOpData, channel)
{
    OCIO::MatrixOpData refMatrixOp;

    OCIO_CHECK_ASSERT(!refMatrixOp.hasChannelCrosstalk());

    const float offsets[4] = { -0.1f, 0.2f, -0.3f, 0.4f };
    refMatrixOp.setRGBAOffsets(offsets);
    // False: with offsets.
    OCIO_CHECK_ASSERT(!refMatrixOp.hasChannelCrosstalk());

    const float matrix[16] = { 0.9f, 0.0f,  0.0f, 0.0f,
                               0.0f, 0.5f,  0.0f, 0.0f,
                               0.0f, 0.0f, -0.4f, 0.0f,
                               0.0f, 0.0f,  0.0f, 0.8f };
    refMatrixOp.setRGBA(matrix);
    // False: with diagonal.
    OCIO_CHECK_ASSERT(!refMatrixOp.hasChannelCrosstalk());

    const float matrix2[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 1.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.000000001f,
                                0.0f, 0.0f, 0.0f, 1.0f };
    refMatrixOp.setRGBA(matrix2);
    // True: with off-diagonal.
    OCIO_CHECK_ASSERT(refMatrixOp.hasChannelCrosstalk());
}

#endif
