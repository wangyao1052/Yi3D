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
#include <BRepFilletAPI_MakeFillet.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dFillet.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dParamNames.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "topo/TopoShapeComparer.h"
#include "topo/ChamferFilletTopoShapeComparer.h"
#include "topo/TopoNamingUtil.h"
#include "BodyModificationUtil.h"
#include "utils/FilerUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Fillet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Fillet, _edgeNames)
    REGISTER_FIELD(Fillet, _faceNames)
    REGISTER_FIELD(Fillet, _radius)
END_FIELD_REGISTRATION()

Fillet::Fillet() : wy3d::BodyModification(), _radius(0.0)
{
}

Fillet::~Fillet()
{
}

wy::ErrorStatus Fillet::create(
    wydb::Transaction* pTrans,
    wy3d::Solid* pSolid,
    const std::vector<std::uint32_t>& faceIndices,
    const std::vector<std::uint32_t>& edgeIndices,
    double radius,
    Fillet*& pOutFillet)
{
    pOutFillet = nullptr;

    if (!pTrans) return wy::ErrorStatus::NullDatabasePointer;
    if (!pSolid) return wy::ErrorStatus::NullElementPointer;

    wy::ErrorStatus error = createImpl(pTrans, pSolid->getShape(), pSolid->getTopoNaming(),
        faceIndices, edgeIndices, radius, pOutFillet);
    if (wy::ErrorStatus::Ok != error) return error;

    error = pSolid->addModification(pOutFillet);
    CHECK_ERROR_FOR_CREATE(error, pOutFillet);
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Fillet::create(
    wydb::Transaction* pTrans,
    wy3d::Sheet* pSheet,
    const std::vector<std::uint32_t>& faceIndices,
    const std::vector<std::uint32_t>& edgeIndices,
    double radius,
    Fillet*& pOutFillet)
{
    pOutFillet = nullptr;

    if (!pTrans) return wy::ErrorStatus::NullDatabasePointer;
    if (!pSheet) return wy::ErrorStatus::NullElementPointer;

    wy::ErrorStatus error = createImpl(pTrans, pSheet->getShape(), pSheet->getTopoNaming(),
        faceIndices, edgeIndices, radius, pOutFillet);
    if (wy::ErrorStatus::Ok != error) return error;

    error = pSheet->addModification(pOutFillet);
    CHECK_ERROR_FOR_CREATE(error, pOutFillet);
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Fillet::createImpl(
    wydb::Transaction* pTrans,
    const TopoDS_Shape& shape,
    TopoNaming* pTopoNaming,
    const std::vector<std::uint32_t>& faceIndices,
    const std::vector<std::uint32_t>& edgeIndices,
    double radius,
    Fillet*& pOutFillet)
{
    pOutFillet = nullptr;
    assert(pTrans);

    if (faceIndices.empty() && edgeIndices.empty())
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (radius < 0.0)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    if (!pTopoNaming)
    {
        assert(false);
        return wy::ErrorStatus::InvalidInput;
    }

    TopoNameList faceNames;
    if (!faceIndices.empty())
    {
        if (!TopoNamingUtil::assemblyTopoNames(*pTopoNaming, shape,
            TopAbs_ShapeEnum::TopAbs_FACE, faceIndices, faceNames))
        {
            pOutFillet = nullptr;
            return wy::ErrorStatus::InvalidInput;
        }
        assert(!faceNames.empty());
    }

    TopoNameList edgeNames;
    if (!edgeIndices.empty())
    {
        if (!TopoNamingUtil::assemblyTopoNames(*pTopoNaming, shape,
            TopAbs_ShapeEnum::TopAbs_EDGE, edgeIndices, edgeNames))
        {
            pOutFillet = nullptr;
            return wy::ErrorStatus::InvalidInput;
        }
        assert(!edgeNames.empty());
    }

    Fillet* pFillet = new Fillet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pFillet);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pFillet);
        pFillet = nullptr;
        return error;
    }

    if (!edgeNames.empty())
    {
        error = pFillet->setEdges(edgeNames);
        CHECK_ERROR_FOR_CREATE(error, pFillet);
    }
    if (!faceNames.empty())
    {
        error = pFillet->setFaces(faceNames);
        CHECK_ERROR_FOR_CREATE(error, pFillet);
    }
    error = pFillet->setRadius(radius);
    CHECK_ERROR_FOR_CREATE(error, pFillet);

    pOutFillet = pFillet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Fillet::setRadius(double radius)
{
    if (radius < wy3d::kMinValue || radius > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (radius == _radius)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kFillet_radius);
    if (wy::ErrorStatus::Ok == error)
    {
        _radius = radius;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Fillet::setEdges(const TopoNameList& edges)
{
    if (edges == _edgeNames)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kFillet_edgeNames);
    if (wy::ErrorStatus::Ok == error)
    {
        _edgeNames = edges;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Fillet::setFaces(const TopoNameList& faces)
{
    if (faces == _faceNames)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kFillet_faceNames);
    if (wy::ErrorStatus::Ok == error)
    {
        _faceNames = faces;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void Fillet::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::FILLET_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Fillet::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Fillet::classInfo()->className()) {
        if (ParamNames::FILLET_RADIUS == paramName) return wydb::ParameterValue::createDouble(_radius);
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus Fillet::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Fillet::classInfo()->className()) {
        if (ParamNames::FILLET_RADIUS == paramName)
        {
            if (!paramValue.isDouble())
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setRadius(paramValue.asDouble());
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Fillet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kFillet_edgeNames.value():
        value = _edgeNames;
        return true;
    case kFillet_faceNames.value():
        value = _faceNames;
        return true;
    case kFillet_radius.value():
        value = _radius;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Fillet::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kFillet_edgeNames.value():
        _edgeNames = std::any_cast<const TopoNameList&>(value);
        return true;
    case kFillet_faceNames.value():
        _faceNames = std::any_cast<const TopoNameList&>(value);
        return true;
    case kFillet_radius.value():
        _radius = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Fillet::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    FilerUtil::writeVector(filer, _edgeNames);
    FilerUtil::writeVector(filer, _faceNames);
    filer << _radius;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Fillet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    FilerUtil::readTopoNameList(filer, _edgeNames);
    FilerUtil::readTopoNameList(filer, _faceNames);
    filer >> _radius;
    return wy::ErrorStatus::Ok;
}

std::pair<bool, TopoDS_Shape> Fillet::modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans && !pTrans->isGroup());

    std::vector<TopoDS_Edge> topoEdges;
    ErrorCode errorCode = BodyModificationUtil::getTopoEdgesByTopoNamings<
        ErrorCode::FILLET_InvalidData, ErrorCode::FILLET_EdgeNotExists, ErrorCode::FILLET_FaceNotExists>(
        *pTopoNaming, _edgeNames, _faceNames, topoEdges);
    if (ErrorCode::NoError != errorCode)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<unsigned int>(errorCode));
        return {false, shape};
    }
    if (topoEdges.empty())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<std::uint32_t>(ErrorCode::FILLET_InvalidData));
        return {false, shape};
    }

    // sheet host: every selected edge needs exactly two adjacent faces
    if (this->getSheetHost())
    {
        TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
        TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);
        for (const TopoDS_Edge& topoEdge : topoEdges)
        {
            const int ancestorIndex = edgeFaceMap.FindIndex(topoEdge);
            if (0 == ancestorIndex || 2 != edgeFaceMap.FindFromIndex(ancestorIndex).Extent())
            {
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<std::uint32_t>(ErrorCode::FILLET_EdgeNotTwoFaces));
                return {false, shape};
            }
        }
    }

    try
    {
        BRepFilletAPI_MakeFillet fillet(shape);
        for (const TopoDS_Edge& topoEdge : topoEdges) fillet.Add(_radius, topoEdge);
        fillet.Build();
        if (fillet.IsDone())
        {
            TopoDS_Shape retShape = fillet.Shape();
            ChamferFilletTopoShapeComparer topoComparer(fillet, shape);
            topoComparer.perform();
            pTopoNaming->update(&topoComparer, this->getId().value());
            this->recordNewFaces(topoComparer.getFaceDelta(), pTopoNaming);
            return {true, retShape};
        }
    } catch (const Standard_Failure&)
    {
    }

    wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<std::uint32_t>(ErrorCode::FILLET_GenerateFilletError));
    return {false, shape};
}

NS_WY3D_END
