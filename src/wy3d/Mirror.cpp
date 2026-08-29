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

#include <algorithm>
#include <cassert>

#include <gp_Pln.hxx>
#include <BRepBuilderAPI_Transform.hxx>

#include <wyVector3.h>
#include <wy3dMirror.h>
#include <wy3dSolid.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wy3dParamNames.h>
#include <wydbFieldRegistry.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

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
#include <wy3dBoolean.h>

#include "topo/TopoShapeUtil.h"
#include "topo/TopoNamingUtil.h"
#include "topo/MoveRotateTopoShapeComparer.h"
#include "SolidModificationUtil.h"
#include "utils/OccUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Mirror)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Mirror, _source)
    REGISTER_FIELD(Mirror, _plane)
END_FIELD_REGISTRATION()

Mirror::Mirror() : wy3d::SolidModification(), _source(wydb::ElementId::kNull), _plane()
{
}

Mirror::~Mirror()
{
}

wy::ErrorStatus Mirror::create(
    wydb::Transaction* pTrans,
    wy3d::Solid* pOwner,
    const wy3d::Solid* pSource,
    const wy3d::SketchPlane& mirrorPlane,
    Mirror*& pOutMirror)
{
    pOutMirror = nullptr;
    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!pOwner) return wy::ErrorStatus::NullElementPointer;
    if (!pSource) return wy::ErrorStatus::NullElementPointer;

    if (pSource != pOwner)
    {
        if (pSource->getParent() != pOwner->getId()) return wy::ErrorStatus::InvalidInput;
        if (!pSource->isCut()) return wy::ErrorStatus::InvalidInput;
    }

    Mirror* pMirror = new Mirror();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pMirror);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pMirror);
        pMirror = nullptr;
        return error;
    }

    error = pMirror->setPlane(mirrorPlane);
    CHECK_ERROR_FOR_CREATE(error, pMirror);
    if (pSource == pOwner)
    {
        error = pMirror->setSourceId(wydb::ElementId::kNull);
        CHECK_ERROR_FOR_CREATE(error, pMirror);
    }
    else
    {
        error = pMirror->setSourceId(pSource->getId());
        CHECK_ERROR_FOR_CREATE(error, pMirror);
    }
    error = pOwner->addModification(pMirror);
    CHECK_ERROR_FOR_CREATE(error, pMirror);

    pOutMirror = pMirror;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Mirror::setSourceId(const wydb::ElementId& source)
{
    if (source == _source)
    {
        return wy::ErrorStatus::Ok;
    }

    wydb::ElementId parentId = this->getParent();
    if (parentId.isNull())
    {
        wydb::Database* pDb = this->getDatabase();
        if (!pDb)
        {
            assert(false);
            return wy::ErrorStatus::Error;
        }
        const wydb::Element* pSourceElem = pDb->getElement(source);
        if (!pSourceElem)
        {
            return wy::ErrorStatus::InvalidInput;
        }
        const wy3d::Solid* pSourceSolid = wy3d::Solid::cast(pSourceElem);
        if (!pSourceSolid)
        {
            return wy::ErrorStatus::InvalidInput;
        }
        if (pSourceSolid->getParent().isNull())
        {
            return wy::ErrorStatus::InvalidInput;
        }
        if (!pSourceSolid->isCut())
        {
            return wy::ErrorStatus::InvalidInput;
        }

        wy::ErrorStatus error = this->prepareForFieldChange(kMirror_source);
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
    else
    {
        if (source.isNull() || source == parentId)
        {
            if (wydb::ElementId::kNull == _source)
            {
                return wy::ErrorStatus::Ok;
            }
            wy::ErrorStatus error = this->prepareForFieldChange(kMirror_source);
            if (wy::ErrorStatus::Ok == error)
            {
                _source = wydb::ElementId::kNull;
                return wy::ErrorStatus::Ok;
            }
            else
            {
                return error;
            }
        }

        wydb::Database* pDb = this->getDatabase();
        if (!pDb)
        {
            assert(false);
            return wy::ErrorStatus::Error;
        }
        const wy3d::Solid* pParentSolid = wy3d::Solid::cast(pDb->getElement(parentId));
        if (!pParentSolid)
        {
            assert(false);
            return wy::ErrorStatus::Error;
        }
        const wydb::Element* pSourceElem = pDb->getElement(source);
        if (!pSourceElem)
        {
            return wy::ErrorStatus::InvalidInput;
        }
        const wy3d::Solid* pSourceSolid = wy3d::Solid::cast(pSourceElem);
        if (!pSourceSolid)
        {
            return wy::ErrorStatus::InvalidInput;
        }
        if (pSourceSolid->getParent() != parentId)
        {
            return wy::ErrorStatus::InvalidInput;
        }
        if (!pSourceSolid->isCut())
        {
            return wy::ErrorStatus::InvalidInput;
        }

        const std::vector<wydb::ElementId>& modifications = pParentSolid->getModifications();
        const auto selfIter = std::find(modifications.cbegin(), modifications.cend(), this->getId());
        if (selfIter == modifications.cend())
        {
            assert(false);
            return wy::ErrorStatus::Error;
        }
        const auto srcIter = std::find(modifications.cbegin(), modifications.cend(), source);
        if (srcIter == modifications.cend())
        {
            return wy::ErrorStatus::InvalidInput;
        }
        if (srcIter >= selfIter)
        {
            return wy::ErrorStatus::InvalidInput;
        }

        wy::ErrorStatus error = this->prepareForFieldChange(kMirror_source);
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
}

wy::ErrorStatus Mirror::setPlane(const wy3d::SketchPlane& plane)
{
    if (plane == _plane) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kMirror_plane);
    if (wy::ErrorStatus::Ok == error)
    {
        _plane = plane;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

void Mirror::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::MIRROR_PARAM_PLANE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::MIRROR_SOURCE;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr Mirror::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Mirror::classInfo()->className())
    {
        if (ParamNames::MIRROR_SOURCE == paramName)
        {
            return wydb::ParameterValue::createElementId(_source);
        }
        else if (ParamNames::MIRROR_PARAM_PLANE == paramName)
        {
            return wydb::ParameterValue::createAny(_plane);
        }
        else
        {
            return nullptr;
        }
    }
    else
    {
        return __baseClass::getParameterValue(className, paramName);
    }
}

wy::ErrorStatus Mirror::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Mirror::classInfo()->className())
    {
        if (ParamNames::MIRROR_SOURCE == paramName)
        {
            if (!paramValue.isElementId()) return wy::ErrorStatus::InvalidInput;
            return this->setSourceId(paramValue.asElementId());
        }
        else if (ParamNames::MIRROR_PARAM_PLANE == paramName)
        {
            if (!paramValue.isAny()) return wy::ErrorStatus::InvalidInput;
            const auto* pAnyVal = dynamic_cast<const wydb::AnyParameterValue*>(&paramValue);
            if (!pAnyVal) return wy::ErrorStatus::InvalidInput;
            const wy3d::SketchPlane* pPlane = pAnyVal->tryGet<wy3d::SketchPlane>();
            if (!pPlane) return wy::ErrorStatus::InvalidInput;
            return this->setPlane(*pPlane);
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    else
    {
        return __baseClass::setParameterValue(className, paramName, paramValue);
    }
}

bool Mirror::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kMirror_source.value():
        value = _source;
        return true;
    case kMirror_plane.value():
        value = _plane;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Mirror::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kMirror_source.value():
        _source = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kMirror_plane.value():
        _plane = std::any_cast<const wy3d::SketchPlane&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Mirror::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _source;

    wy::Vector3 planeOrigin = _plane.getOrigin();
    filer << planeOrigin.x() << planeOrigin.y() << planeOrigin.z();
    wy::Vector3 planeNormal = _plane.getNormal();
    filer << planeNormal.x() << planeNormal.y() << planeNormal.z();
    wy::Vector3 planeXDir = _plane.getXDir();
    filer << planeXDir.x() << planeXDir.y() << planeXDir.z();

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Mirror::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);

    if (filer.getFileVersion() < wydb::FileVersion(0, 10))
    {
        _source = wydb::ElementId::kNull;
    }
    else
    {
        filer >> _source;
    }

    double x(0.0), y(0.0), z(0.0);
    filer >> x >> y >> z;
    wy::Vector3 planeOrigin(x, y, z);
    filer >> x >> y >> z;
    wy::Vector3 planeNormal(x, y, z);
    filer >> x >> y >> z;
    wy::Vector3 planeXDir(x, y, z);
    _plane = wy3d::SketchPlane(planeOrigin, planeNormal, planeXDir);

    return wy::ErrorStatus::Ok;
}

void Mirror::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_source.isNull())
    {
        dependencies.insert(_source);
    }
}

bool Mirror::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);
    if (!_source.isNull() && erasedDependencies.find(_source) != erasedDependencies.cend())
    {
        this->erase(true);
        return true;
    }
    return responsed;
}

std::pair<bool, TopoDS_Shape> Mirror::modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);

    this->clearNewFaces();

    TopoDS_Shape sourceShape;
    const TopoNaming* pSourceNaming = nullptr;
    bool isCut(false);
    if (_source.isNull())
    {
        sourceShape = shape;
        pSourceNaming = pTopoNaming;
        isCut = false;
    }
    else
    {
        const wy3d::Solid* pSourceSolid = wy3d::Solid::cast(pDb->getElement(_source));
        if (!pSourceSolid || pSourceSolid->getParent() != this->getParent() || !pSourceSolid->isCut())
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::ELEMENT_InvalidData));
            return std::pair<bool, TopoDS_Shape>(false, shape);
        }
        sourceShape = pSourceSolid->getShape();
        pSourceNaming = pSourceSolid->getTopoNaming();
        isCut = true;
    }
    assert(pSourceNaming);

    gp_Trsf mirrorTrsf;
    try
    {
        gp_Ax2 ax2 = OccUtil::toAx2(_plane);
        mirrorTrsf.SetMirror(ax2);
    }
    catch (const Standard_Failure&)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(wy3d::ErrorCode::ELEMENT_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    BRepBuilderAPI_Transform transformer(sourceShape, mirrorTrsf);
    TopoDS_Shape instShape = transformer.Shape();

    std::vector<unsigned int> suffix;
    suffix.emplace_back(this->getId().value());
    TopoNamingSPtr pInstTopoNaming = std::make_shared<TopoNaming>();
    bool namingRet = TopoNamingUtil::patternNaming(sourceShape, *pSourceNaming,
        suffix, transformer, *pInstTopoNaming);
    assert(namingRet);

    return this->modifyOwnerShapeByInstance(shape, pTopoNaming,
        instShape, pInstTopoNaming.get(), isCut, feedbackCollector);
}

NS_WY3D_END
