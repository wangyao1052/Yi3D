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
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopTools_ListOfShape.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dChamfer.h>
#include <wy3dSolid.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dParamNames.h>
#include <wy3dParamEnumDef.h>
#include <wy3dMath.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "topo/TopoShapeComparer.h"
#include "topo/ChamferFilletTopoShapeComparer.h"
#include "topo/TopoNamingUtil.h"
#include "SolidModificationUtil.h"
#include "utils/FilerUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Chamfer)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Chamfer, _edgeNames)
    REGISTER_FIELD(Chamfer, _faceNames)
    REGISTER_FIELD(Chamfer, _chamferType)
    REGISTER_FIELD(Chamfer, _distance1)
    REGISTER_FIELD(Chamfer, _distance2)
    REGISTER_FIELD(Chamfer, _angle)
    REGISTER_FIELD(Chamfer, _isFlipped)
END_FIELD_REGISTRATION()

Chamfer::Chamfer() : wy3d::SolidModification(), _chamferType(ChamferType::EqualDistance),
    _distance1(0.0), _distance2(0.0), _angle(wy3d::PI_4), _isFlipped(false)
{
}

Chamfer::~Chamfer()
{
}

wy::ErrorStatus Chamfer::create(
    wydb::Transaction* pTrans,
    wy3d::Solid* pSolid,
    const std::vector<std::uint32_t>& faceIndices,
    const std::vector<std::uint32_t>& edgeIndices,
    double distance,
    Chamfer*& pOutChamfer)
{
    // Legacy signature: delegates with equal-distance defaults.
    return create(pTrans, pSolid, faceIndices, edgeIndices, ChamferType::EqualDistance,
        distance, distance, wy3d::PI_4, false, pOutChamfer);
}

wy::ErrorStatus Chamfer::create(
    wydb::Transaction* pTrans,
    wy3d::Solid* pSolid,
    const std::vector<std::uint32_t>& faceIndices,
    const std::vector<std::uint32_t>& edgeIndices,
    ChamferType chamferType,
    double distance1,
    double distance2,
    double angle,
    bool isFlipped,
    Chamfer*& pOutChamfer)
{
    if (!pTrans)
    {
        pOutChamfer = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }
    if (!pSolid)
    {
        pOutChamfer = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }
    if (faceIndices.empty() && edgeIndices.empty())
    {
        pOutChamfer = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }
    if (ChamferType::EqualDistance != chamferType &&
        ChamferType::DistanceDistance != chamferType &&
        ChamferType::DistanceAngle != chamferType)
    {
        pOutChamfer = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }
    if (distance1 < 0.0)
    {
        pOutChamfer = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }
    if (distance2 < 0.0)
    {
        pOutChamfer = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }
    if (angle <= 0.0 || angle >= wy3d::PI)
    {
        pOutChamfer = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }

    TopoDS_Shape shape = pSolid->getShape();
    TopoNaming* pTopoNaming = pSolid->getTopoNaming();
    if (!pTopoNaming)
    {
        pOutChamfer = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }

    TopoNameList faceNames;
    if (!faceIndices.empty())
    {
        if (!TopoNamingUtil::assemblyTopoNames(*pTopoNaming, shape,
            TopAbs_ShapeEnum::TopAbs_FACE, faceIndices, faceNames))
        {
            pOutChamfer = nullptr;
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
            pOutChamfer = nullptr;
            return wy::ErrorStatus::InvalidInput;
        }
        assert(!edgeNames.empty());
    }

    Chamfer* pChamfer = new Chamfer();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pChamfer);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pChamfer);
        pChamfer = nullptr;
        return error;
    }

    if (!edgeNames.empty())
    {
        error = pChamfer->setEdges(edgeNames);
        CHECK_ERROR_FOR_CREATE(error, pChamfer);
    }
    if (!faceNames.empty())
    {
        error = pChamfer->setFaces(faceNames);
        CHECK_ERROR_FOR_CREATE(error, pChamfer);
    }
    error = pChamfer->setChamferType(chamferType);
    CHECK_ERROR_FOR_CREATE(error, pChamfer);
    error = pChamfer->setDistance1(distance1);
    CHECK_ERROR_FOR_CREATE(error, pChamfer);
    error = pChamfer->setDistance2(distance2);
    CHECK_ERROR_FOR_CREATE(error, pChamfer);
    error = pChamfer->setAngle(angle);
    CHECK_ERROR_FOR_CREATE(error, pChamfer);
    error = pChamfer->setFlipped(isFlipped);
    CHECK_ERROR_FOR_CREATE(error, pChamfer);

    error = pSolid->addModification(pChamfer);
    CHECK_ERROR_FOR_CREATE(error, pChamfer);

    pOutChamfer = pChamfer;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Chamfer::setDistance1(double distance1)
{
    if (distance1 < wy3d::kMinValue || distance1 > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (distance1 == _distance1)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kChamfer_distance1);
    if (wy::ErrorStatus::Ok == error)
    {
        _distance1 = distance1;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Chamfer::setDistance2(double distance2)
{
    if (distance2 < wy3d::kMinValue || distance2 > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (distance2 == _distance2)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kChamfer_distance2);
    if (wy::ErrorStatus::Ok == error)
    {
        _distance2 = distance2;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Chamfer::setAngle(double angle)
{
    if (angle <= 0.0 || angle >= wy3d::PI)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (angle == _angle)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kChamfer_angle);
    if (wy::ErrorStatus::Ok == error)
    {
        _angle = angle;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Chamfer::setChamferType(ChamferType chamferType)
{
    if (ChamferType::EqualDistance != chamferType &&
        ChamferType::DistanceDistance != chamferType &&
        ChamferType::DistanceAngle != chamferType)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (chamferType == _chamferType)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kChamfer_chamferType);
    if (wy::ErrorStatus::Ok == error)
    {
        _chamferType = chamferType;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Chamfer::setFlipped(bool isFlipped)
{
    if (isFlipped == _isFlipped)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kChamfer_isFlipped);
    if (wy::ErrorStatus::Ok == error)
    {
        _isFlipped = isFlipped;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Chamfer::setEdges(const TopoNameList& edges)
{
    if (edges == _edgeNames)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kChamfer_edgeNames);
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

wy::ErrorStatus Chamfer::setFaces(const TopoNameList& faces)
{
    if (faces == _faceNames)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kChamfer_faceNames);
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


void Chamfer::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::CHAMFER_TYPE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::CHAMFER_DISTANCE1;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::CHAMFER_DISTANCE2;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::CHAMFER_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::CHAMFER_IS_FLIPPED;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr Chamfer::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Chamfer::classInfo()->className())
    {
        if (ParamNames::CHAMFER_DISTANCE1 == paramName)
        {
            return wydb::ParameterValue::createDouble(_distance1);
        }
        if (ParamNames::CHAMFER_DISTANCE2 == paramName)
        {
            return wydb::ParameterValue::createDouble(_distance2);
        }
        if (ParamNames::CHAMFER_ANGLE == paramName)
        {
            return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_angle));
        }
        if (ParamNames::CHAMFER_TYPE == paramName)
        {
            return wydb::ParameterValue::createAny(
                wy3d::ParamEnumDef(
                    {{static_cast<int>(ChamferType::EqualDistance), "Equal distance"},
                     {static_cast<int>(ChamferType::DistanceDistance), "Distance-Distance"},
                     {static_cast<int>(ChamferType::DistanceAngle), "Distance-Angle"}},
                    static_cast<int>(_chamferType)));
        }
        if (ParamNames::CHAMFER_IS_FLIPPED == paramName)
        {
            return wydb::ParameterValue::createBoolean(_isFlipped);
        }
        return nullptr;
    }
    return __baseClass::getParameterValue(className, paramName);
}

static int _extractEnumValue(const wydb::ParameterValue& paramValue)
{
    if (paramValue.isInteger())
        return paramValue.asInteger();
    if (paramValue.isAny())
    {
        const auto* pAnyVal = dynamic_cast<const wydb::AnyParameterValue*>(&paramValue);
        if (pAnyVal)
        {
            const wy3d::ParamEnumDef* pDef = pAnyVal->tryGet<wy3d::ParamEnumDef>();
            if (pDef) return pDef->currentValue;
        }
    }
    return -1;
}

wy::ErrorStatus Chamfer::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Chamfer::classInfo()->className())
    {
        if (ParamNames::CHAMFER_DISTANCE1 == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setDistance1(paramValue.asDouble());
        }
        if (ParamNames::CHAMFER_DISTANCE2 == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setDistance2(paramValue.asDouble());
        }
        if (ParamNames::CHAMFER_ANGLE == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setAngle(wy3d::degreesToRadians(paramValue.asDouble()));
        }
        if (ParamNames::CHAMFER_TYPE == paramName)
        {
            return this->setChamferType(static_cast<ChamferType>(_extractEnumValue(paramValue)));
        }
        if (ParamNames::CHAMFER_IS_FLIPPED == paramName)
        {
            if (!paramValue.isBoolean()) return wy::ErrorStatus::InvalidInput;
            return this->setFlipped(paramValue.asBoolean());
        }
        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Chamfer::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kChamfer_edgeNames.value():
        value = _edgeNames;
        return true;
    case kChamfer_faceNames.value():
        value = _faceNames;
        return true;
    case kChamfer_distance1.value():
        value = _distance1;
        return true;
    case kChamfer_distance2.value():
        value = _distance2;
        return true;
    case kChamfer_angle.value():
        value = _angle;
        return true;
    case kChamfer_chamferType.value():
        value = _chamferType;
        return true;
    case kChamfer_isFlipped.value():
        value = _isFlipped;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Chamfer::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kChamfer_edgeNames.value():
        _edgeNames = std::any_cast<const TopoNameList&>(value);
        return true;
    case kChamfer_faceNames.value():
        _faceNames = std::any_cast<const TopoNameList&>(value);
        return true;
    case kChamfer_distance1.value():
        _distance1 = std::any_cast<double>(value);
        return true;
    case kChamfer_distance2.value():
        _distance2 = std::any_cast<double>(value);
        return true;
    case kChamfer_angle.value():
        _angle = std::any_cast<double>(value);
        return true;
    case kChamfer_chamferType.value():
        _chamferType = std::any_cast<ChamferType>(value);
        return true;
    case kChamfer_isFlipped.value():
        _isFlipped = std::any_cast<bool>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Chamfer::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    FilerUtil::writeVector(filer, _edgeNames);
    FilerUtil::writeVector(filer, _faceNames);
    if (filer.getFileVersion() >= wydb::FileVersion(0, 19))
    {
        filer << static_cast<std::int32_t>(_chamferType);
        filer << _distance1 << _distance2 << _angle << _isFlipped;
    }
    else
    {
        filer << _distance1;
    }
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Chamfer::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);

    FilerUtil::readTopoNameList(filer, _edgeNames);
    FilerUtil::readTopoNameList(filer, _faceNames);
    if (filer.getFileVersion() >= wydb::FileVersion(0, 19))
    {
        std::int32_t chamferTypeInt(0);
        filer >> chamferTypeInt;
        _chamferType = static_cast<ChamferType>(chamferTypeInt);
        filer >> _distance1 >> _distance2 >> _angle >> _isFlipped;
    }
    else
    {
        filer >> _distance1;
        _distance2 = _distance1;
    }

    return wy::ErrorStatus::Ok;
}

std::pair<bool, TopoDS_Shape> Chamfer::modifyOwnerShape(
    const TopoDS_Shape& shape,
    TopoNaming* pTopoNaming,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    std::vector<TopoDS_Edge> topoEdges;
    ErrorCode errorCode = SolidModificationUtil::getTopoEdgesByTopoNamings<
        ErrorCode::CHAMFER_InvalidData,
        ErrorCode::CHAMFER_EdgeNotExists,
        ErrorCode::CHAMFER_FaceNotExists>(*pTopoNaming, _edgeNames, _faceNames, topoEdges);
    if (ErrorCode::NoError != errorCode)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<unsigned int>(errorCode));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }
    if (topoEdges.empty())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<std::uint32_t>(ErrorCode::CHAMFER_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    if (_distance1 < wy3d::kMinValue || _distance1 > wy3d::kMaxValue ||
        (ChamferType::DistanceDistance == _chamferType &&
            (_distance2 < wy3d::kMinValue || _distance2 > wy3d::kMaxValue)) ||
        (ChamferType::DistanceAngle == _chamferType &&
            (_angle <= 0.0 || _angle >= wy3d::PI)))
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::CHAMFER_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    try
    {
        BRepFilletAPI_MakeChamfer chamfer(shape);
        if (ChamferType::EqualDistance == _chamferType)
        {
            for (const TopoDS_Edge& topoEdge : topoEdges)
            {
                chamfer.Add(_distance1, topoEdge);
            }
        }
        else
        {
            TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
            TopExp::MapShapesAndAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);

            for (const TopoDS_Edge& topoEdge : topoEdges)
            {
                const int ancestorIndex = edgeFaceMap.FindIndex(topoEdge);
                if (0 == ancestorIndex)
                {
                    wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                        static_cast<std::uint32_t>(ErrorCode::CHAMFER_InvalidData));
                    return std::pair<bool, TopoDS_Shape>(false, shape);
                }
                const TopTools_ListOfShape& ancestorFaces = edgeFaceMap.FindFromIndex(ancestorIndex);
                const TopoDS_Face refFace = TopoDS::Face(_isFlipped ? ancestorFaces.Last() : ancestorFaces.First());

                if (ChamferType::DistanceDistance == _chamferType)
                {
                    chamfer.Add(_distance1, _distance2, topoEdge, refFace);
                }
                else
                {
                    chamfer.AddDA(_distance1, _angle, topoEdge, refFace);
                }
            }
        }
        chamfer.Build();
        if (chamfer.IsDone())
        {
            TopoDS_Shape retShape = chamfer.Shape();

            ChamferFilletTopoShapeComparer topoComparer(chamfer, shape);
            topoComparer.perform();
#ifdef _DEBUG
            {
                char szFileName[100] = { 0 };
                sprintf_s(szFileName, 100, "D:/logs/%d_topoComparer.txt", this->getId().value());
                topoComparer.print(szFileName);
            }
#endif
            pTopoNaming->update(&topoComparer, this->getId().value());

#ifdef _DEBUG
            char szFileName[100] = { 0 };
            sprintf_s(szFileName, 100, "D:/logs/%d.txt", this->getId().value());
            pTopoNaming->print(szFileName, retShape);
#endif

            this->recordNewFaces(topoComparer.getFaceDelta(), pTopoNaming);

            return std::pair<bool, TopoDS_Shape>(true, retShape);
        }
    }
    catch (const Standard_Failure&)
    {
    }

    wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
        static_cast<std::uint32_t>(ErrorCode::CHAMFER_GenerateChamferError));
    return std::pair<bool, TopoDS_Shape>(false, shape);
}

NS_WY3D_END
