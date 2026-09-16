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

#include <TopoDS.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>

#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dPattern.h>
#include <wy3dParamNames.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dSolid.h>

#include <wy3dExtrusion.h>
#include <wy3dRevolution.h>
#include <wy3dSweep.h>
#include <wy3dLoft.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dCone.h>
#include <wy3dTorus.h>
#include <wy3dTube.h>

#include "utils/OccUtil.h"
#include "topo/TopoNamingUtil.h"
#include "topo/TopoShapeUtil.h"
#include "topo/BooleanTopoShapeComparer.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Pattern)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Pattern, _source)
END_FIELD_REGISTRATION()

bool Pattern::isValidSource(const wy3d::Solid* pSolid)
{
    if (!pSolid) return false;

    wydb::ElementId ownerId = pSolid->getParent();
    if (ownerId.isNull()) return false;

    const wydb::Database* pDb = pSolid->getDatabase();
    assert(pDb);
    const wy3d::Solid* pOwnerSolid = wy3d::Solid::cast(pDb->getElement(ownerId));
    if (!pOwnerSolid) { assert(false); return false; }

    wyrx::ClassInfo* ownerClassInfo = pOwnerSolid->getClassInfo();
    if (ownerClassInfo == wy3d::Extrusion::classInfo() ||
        ownerClassInfo == wy3d::Revolution::classInfo() ||
        ownerClassInfo == wy3d::Sweep::classInfo() ||
        ownerClassInfo == wy3d::Loft::classInfo() ||
        ownerClassInfo == wy3d::Box::classInfo() ||
        ownerClassInfo == wy3d::Cylinder::classInfo() ||
        ownerClassInfo == wy3d::Sphere::classInfo() ||
        ownerClassInfo == wy3d::Cone::classInfo() ||
        ownerClassInfo == wy3d::Torus::classInfo() ||
        ownerClassInfo == wy3d::Tube::classInfo())
    {
        return true;
    }
    else
    {
        return false;
    }
}

Pattern::Pattern() : wy3d::BodyModification(), _source(wydb::ElementId::kNull)
{
}

Pattern::~Pattern()
{
}

wy::ErrorStatus Pattern::_setSource(const wy3d::Solid* pSource)
{
    if (!pSource) return wy::ErrorStatus::NullElementPointer;
    return this->setSourceId(pSource->getId());
}

wy::ErrorStatus Pattern::setSourceId(const wydb::ElementId& source)
{
    if (source == _source) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kPattern_source);
    if (wy::ErrorStatus::Ok == error)
    {
        _source = source;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void Pattern::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::PATTERN_SOURCE;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Pattern::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Pattern::classInfo()->className()) {
        if (ParamNames::PATTERN_SOURCE == paramName) return wydb::ParameterValue::createInteger(_source.value());
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus Pattern::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Pattern::classInfo()->className()) {
        if (ParamNames::PATTERN_SOURCE == paramName) { assert(false); return wy::ErrorStatus::ParameterReadonly; }
        return wy::ErrorStatus::Ok;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Pattern::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kPattern_source.value():
        value = _source;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Pattern::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kPattern_source.value():
        _source = std::any_cast<wydb::ElementId>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Pattern::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _source;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Pattern::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _source;
    return wy::ErrorStatus::Ok;
}

void Pattern::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_source.isNull())
    {
        dependencies.insert(_source);
    }
}

bool Pattern::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    if (!_source.isNull() && erasedDependencies.find(_source) != erasedDependencies.cend())
    {
        this->erase(true);
        this->setSourceId(wydb::ElementId::kNull);
        return true;
    }

    return responsed;
}

std::pair<bool, TopoDS_Shape> Pattern::modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    this->clearNewFaces();

    const wy3d::Solid* pSourceSolid = wy3d::Solid::cast(pDb->getElement(_source));
    if (!pSourceSolid)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::ELEMENT_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }
    if (!this->isValidSource(pSourceSolid))
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::ELEMENT_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }
    if (this->getParent() != pSourceSolid->getParent())
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::ELEMENT_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    TopoDS_Shape sourceShape = pSourceSolid->getShape();
    if (sourceShape.IsNull())
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::warnTOPOSHAPE_NullShape));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }
    const wy3d::TopoNaming* pSourceNaming = pSourceSolid->getTopoNaming();
    assert(pSourceNaming);

    return this->modifyOwnerShapeImpl(shape, pTopoNaming,
        sourceShape, pSourceNaming, pSourceSolid->isCut(), feedbackCollector);
}

std::pair<bool, TopoDS_Shape> Pattern::modifyOwnerShapeImpl(
    const TopoDS_Shape& shape, TopoNaming* pTopoNaming,
    const TopoDS_Shape& sourceShape, const wy3d::TopoNaming* pSourceNaming,
    bool isCut,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    return std::pair<bool, TopoDS_Shape>(true, shape);
}

NS_WY3D_END
