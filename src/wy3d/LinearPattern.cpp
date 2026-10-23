///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#include <BRepBuilderAPI_Transform.hxx>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dLinearPattern.h>
#include <wy3dSolid.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "topo/TopoNamingUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(LinearPattern)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(LinearPattern, _dir1st)
    REGISTER_FIELD(LinearPattern, _count1st)
    REGISTER_FIELD(LinearPattern, _spacing1st)
    REGISTER_FIELD(LinearPattern, _dir2nd)
    REGISTER_FIELD(LinearPattern, _count2nd)
    REGISTER_FIELD(LinearPattern, _spacing2nd)
END_FIELD_REGISTRATION()

LinearPattern::LinearPattern() : wy3d::Pattern(),
    _dir1st(wy::Vector3::kXAxis), _count1st(1), _spacing1st(0.0),
    _dir2nd(wy::Vector3::kYAxis), _count2nd(1), _spacing2nd(0.0)
{
}

LinearPattern::~LinearPattern()
{
}

wy::ErrorStatus LinearPattern::create(
    wydb::Transaction* pTrans,
    wy3d::Solid* pOwner, const wy3d::Solid* pSource,
    const wy::Vector3& dir1st, double count1st, double spacing1st,
    const wy::Vector3& dir2nd, double count2nd, double spacing2nd,
    LinearPattern*& pOutPattern)
{
    if (!pOwner || !pSource) { pOutPattern = nullptr; return wy::ErrorStatus::NullElementPointer; }
    if (pSource->getParent() != pOwner->getId() || pSource->getParent().isNull())
    { pOutPattern = nullptr; return wy::ErrorStatus::InvalidInput; }

    LinearPattern* pPattern = new LinearPattern();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pPattern);
    if (wy::ErrorStatus::Ok != error) { wydb::deleteElement(pPattern); pPattern = nullptr; return error; }

    error = pPattern->_setSource(pSource); CHECK_ERROR_FOR_CREATE(error, pPattern);
    error = pPattern->setDirection1st(dir1st); CHECK_ERROR_FOR_CREATE(error, pPattern);
    error = pPattern->setCount1st(count1st); CHECK_ERROR_FOR_CREATE(error, pPattern);
    error = pPattern->setSpacing1st(spacing1st); CHECK_ERROR_FOR_CREATE(error, pPattern);
    error = pPattern->setDirection2nd(dir2nd); CHECK_ERROR_FOR_CREATE(error, pPattern);
    error = pPattern->setCount2nd(count2nd); CHECK_ERROR_FOR_CREATE(error, pPattern);
    error = pPattern->setSpacing2nd(spacing2nd); CHECK_ERROR_FOR_CREATE(error, pPattern);

    error = pOwner->addModification(pPattern); CHECK_ERROR_FOR_CREATE(error, pPattern);

    pOutPattern = pPattern;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus LinearPattern::setDirection1st(const wy::Vector3& dir1st)
{
    if (dir1st.length() <= wy3d::EPS) return wy::ErrorStatus::InvalidInput;
    if (dir1st == _dir1st) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kLinearPattern_dir1st);
    if (wy::ErrorStatus::Ok == error)
    {
        _dir1st = dir1st;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus LinearPattern::setCount1st(std::uint32_t count1st)
{
    if (count1st == 0 || count1st > wy3d::kMaxLinearPatternCount) return wy::ErrorStatus::InvalidInput;
    if (count1st == _count1st) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kLinearPattern_count1st);
    if (wy::ErrorStatus::Ok == error)
    {
        _count1st = count1st;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus LinearPattern::setSpacing1st(double spacing1st)
{
    if (std::fabs(spacing1st - _spacing1st) <= wy3d::TOL) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kLinearPattern_spacing1st);
    if (wy::ErrorStatus::Ok == error)
    {
        _spacing1st = spacing1st;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus LinearPattern::setDirection2nd(const wy::Vector3& dir2nd)
{
    if (dir2nd.length() <= wy3d::EPS) return wy::ErrorStatus::InvalidInput;
    if (dir2nd == _dir2nd) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kLinearPattern_dir2nd);
    if (wy::ErrorStatus::Ok == error)
    {
        _dir2nd = dir2nd;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus LinearPattern::setCount2nd(std::uint32_t count2nd)
{
    if (count2nd == 0 || count2nd > wy3d::kMaxLinearPatternCount) return wy::ErrorStatus::InvalidInput;
    if (count2nd == _count2nd) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kLinearPattern_count2nd);
    if (wy::ErrorStatus::Ok == error)
    {
        _count2nd = count2nd;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus LinearPattern::setSpacing2nd(double spacing2nd)
{
    if (std::fabs(spacing2nd - _spacing2nd) <= wy3d::TOL) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kLinearPattern_spacing2nd);
    if (wy::ErrorStatus::Ok == error)
    {
        _spacing2nd = spacing2nd;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void LinearPattern::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = LinearPattern::PARAM_COUNT_1ST;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = LinearPattern::PARAM_COUNT_2ND;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = LinearPattern::PARAM_DIRECTION_1ST_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = LinearPattern::PARAM_DIRECTION_1ST_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = LinearPattern::PARAM_DIRECTION_1ST_Z;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = LinearPattern::PARAM_DIRECTION_2ND_X;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = LinearPattern::PARAM_DIRECTION_2ND_Y;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = LinearPattern::PARAM_DIRECTION_2ND_Z;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = LinearPattern::PARAM_SPACING_1ST;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = LinearPattern::PARAM_SPACING_2ND;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr LinearPattern::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == LinearPattern::classInfo()->className()) {
        if (LinearPattern::PARAM_DIRECTION_1ST_X == paramName)
            return wydb::ParameterValue::createDouble(_dir1st.x());
        if (LinearPattern::PARAM_DIRECTION_1ST_Y == paramName)
            return wydb::ParameterValue::createDouble(_dir1st.y());
        if (LinearPattern::PARAM_DIRECTION_1ST_Z == paramName)
            return wydb::ParameterValue::createDouble(_dir1st.z());
        if (LinearPattern::PARAM_COUNT_1ST == paramName)
            return wydb::ParameterValue::createInteger(_count1st);
        if (LinearPattern::PARAM_SPACING_1ST == paramName)
            return wydb::ParameterValue::createDouble(_spacing1st);
        if (LinearPattern::PARAM_DIRECTION_2ND_X == paramName)
            return wydb::ParameterValue::createDouble(_dir2nd.x());
        if (LinearPattern::PARAM_DIRECTION_2ND_Y == paramName)
            return wydb::ParameterValue::createDouble(_dir2nd.y());
        if (LinearPattern::PARAM_DIRECTION_2ND_Z == paramName)
            return wydb::ParameterValue::createDouble(_dir2nd.z());
        if (LinearPattern::PARAM_COUNT_2ND == paramName)
            return wydb::ParameterValue::createInteger(_count2nd);
        if (LinearPattern::PARAM_SPACING_2ND == paramName)
            return wydb::ParameterValue::createDouble(_spacing2nd);

        return nullptr;
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus LinearPattern::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == LinearPattern::classInfo()->className()) {
        if (LinearPattern::PARAM_DIRECTION_1ST_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setDirection1st(wy::Vector3(paramValue.asDouble(), _dir1st.y(), _dir1st.z())); }
        if (LinearPattern::PARAM_DIRECTION_1ST_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setDirection1st(wy::Vector3(_dir1st.x(), paramValue.asDouble(), _dir1st.z())); }
        if (LinearPattern::PARAM_DIRECTION_1ST_Z == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setDirection1st(wy::Vector3(_dir1st.x(), _dir1st.y(), paramValue.asDouble())); }
        if (LinearPattern::PARAM_COUNT_1ST == paramName)
        { if (!paramValue.isInteger()) return wy::ErrorStatus::InvalidInput; return setCount1st(std::abs(paramValue.asInteger())); }
        if (LinearPattern::PARAM_SPACING_1ST == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setSpacing1st(paramValue.asDouble()); }
        if (LinearPattern::PARAM_DIRECTION_2ND_X == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setDirection2nd(wy::Vector3(paramValue.asDouble(), _dir2nd.y(), _dir2nd.z())); }
        if (LinearPattern::PARAM_DIRECTION_2ND_Y == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setDirection2nd(wy::Vector3(_dir2nd.x(), paramValue.asDouble(), _dir2nd.z())); }
        if (LinearPattern::PARAM_DIRECTION_2ND_Z == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setDirection2nd(wy::Vector3(_dir2nd.x(), _dir2nd.y(), paramValue.asDouble())); }
        if (LinearPattern::PARAM_COUNT_2ND == paramName)
        { if (!paramValue.isInteger()) return wy::ErrorStatus::InvalidInput; return setCount2nd(std::abs(paramValue.asInteger())); }
        if (LinearPattern::PARAM_SPACING_2ND == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return setSpacing2nd(paramValue.asDouble()); }

        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool LinearPattern::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kLinearPattern_dir1st.value():
        value = _dir1st;
        return true;
    case kLinearPattern_count1st.value():
        value = _count1st;
        return true;
    case kLinearPattern_spacing1st.value():
        value = _spacing1st;
        return true;
    case kLinearPattern_dir2nd.value():
        value = _dir2nd;
        return true;
    case kLinearPattern_count2nd.value():
        value = _count2nd;
        return true;
    case kLinearPattern_spacing2nd.value():
        value = _spacing2nd;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool LinearPattern::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kLinearPattern_dir1st.value():
        _dir1st = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kLinearPattern_count1st.value():
        _count1st = std::any_cast<std::uint32_t>(value);
        return true;
    case kLinearPattern_spacing1st.value():
        _spacing1st = std::any_cast<double>(value);
        return true;
    case kLinearPattern_dir2nd.value():
        _dir2nd = std::any_cast<const wy::Vector3&>(value);
        return true;
    case kLinearPattern_count2nd.value():
        _count2nd = std::any_cast<std::uint32_t>(value);
        return true;
    case kLinearPattern_spacing2nd.value():
        _spacing2nd = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus LinearPattern::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _dir1st << _count1st << _spacing1st << _dir2nd << _count2nd << _spacing2nd;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus LinearPattern::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _dir1st >> _count1st >> _spacing1st >> _dir2nd >> _count2nd >> _spacing2nd;
    return wy::ErrorStatus::Ok;
}

std::pair<bool, TopoDS_Shape> LinearPattern::modifyOwnerShapeImpl(
    const TopoDS_Shape& shape, TopoNaming* pTopoNaming,
    const TopoDS_Shape& sourceShape, const wy3d::TopoNaming* pSourceNaming,
    bool isCut,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    assert(pSourceNaming);

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    TopoDS_Shape retShape = shape;
    unsigned int count1st = (0 == _count1st) ? 1 : _count1st;
    unsigned int count2nd = (0 == _count2nd) ? 1 : _count2nd;
    if (count1st <= 1 && count2nd <= 1)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(wy3d::ErrorCode::warnTOPOSHAPE_NullShape));
        return std::pair<bool, TopoDS_Shape>(false, retShape);
    }

    bool success(true);
    wy::Vector3 dir1 = _dir1st.normalized();
    wy::Vector3 dir2 = _dir2nd.normalized();

    for (unsigned int row = 1; row <= count2nd; ++row)
    {
        for (unsigned int col = 1; col <= count1st; ++col)
        {
            if (1 == row && 1 == col) continue;

            gp_Trsf trsf;
            wy::Vector3 vec = (col - 1) * dir1 * _spacing1st + (row - 1) * dir2 * _spacing2nd;
            trsf.SetTranslation(gp_Vec(vec.x(), vec.y(), vec.z()));
            BRepBuilderAPI_Transform transformer(sourceShape, trsf);
            TopoDS_Shape instShape = transformer.Shape();

            std::vector<std::uint32_t> suffix;
            suffix.emplace_back(this->getId().value());
            suffix.emplace_back(row);
            suffix.emplace_back(col);
            TopoNamingSPtr pInstTopoNaming = std::make_shared<TopoNaming>();
            bool namingRet = TopoNamingUtil::patternNaming(sourceShape, *pSourceNaming,
                suffix, transformer, *pInstTopoNaming);
            assert(namingRet);

            auto ret = this->modifyOwnerShapeByInstance(retShape, pTopoNaming,
                instShape, pInstTopoNaming.get(), isCut, feedbackCollector);
            retShape = ret.second;
            if (!ret.first)
            {
                success = false;
                break;
            }
        }
    }

    return std::pair<bool, TopoDS_Shape>(success, retShape);
}

NS_WY3D_END
