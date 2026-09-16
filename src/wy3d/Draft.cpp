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

#include <gp_Ax3.hxx>
#include <gp_Pln.hxx>
#include <TopoDS.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dDraft.h>
#include <wy3dSolid.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dParamNames.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "topo/DraftTopoShapeComparer.h"
#include "topo/TopoShapeUtil.h"
#include "topo/TopoNamingUtil.h"
#include "BodyModificationUtil.h"
#include "utils/FilerUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Draft)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Draft, _neutralFaceName)
    REGISTER_FIELD(Draft, _faceNames)
    REGISTER_FIELD(Draft, _angle)
END_FIELD_REGISTRATION()

Draft::Draft() : wy3d::BodyModification(), _angle(0.0)
{
}

Draft::~Draft()
{
}

wy::ErrorStatus Draft::create(
    wydb::Transaction* pTrans,
    wy3d::Solid* pSolid,
    std::uint32_t neutralFaceIndex,
    const std::vector<std::uint32_t>& faceIndices,
    double angle,
    Draft*& pOutDraft)
{
    if (!pTrans) { pOutDraft = nullptr; return wy::ErrorStatus::NullDatabasePointer; }
    if (!pSolid) { pOutDraft = nullptr; return wy::ErrorStatus::NullElementPointer; }
    if (UINT_MAX == neutralFaceIndex) { pOutDraft = nullptr; return wy::ErrorStatus::InvalidInput; }
    if (faceIndices.empty()) { pOutDraft = nullptr; return wy::ErrorStatus::InvalidInput; }
    if (std::fabs(angle) > wy3d::kMaxDraftAngle) { pOutDraft = nullptr; return wy::ErrorStatus::InvalidInput; }

    TopoDS_Shape shape = pSolid->getShape();
    TopoNaming* pTopoNaming = pSolid->getTopoNaming();
    if (!pTopoNaming) { pOutDraft = nullptr; return wy::ErrorStatus::InvalidInput; }

    TopoName neutralFaceName;
    if (!TopoNamingUtil::getTopoName(*pTopoNaming, shape,
        TopAbs_ShapeEnum::TopAbs_FACE, neutralFaceIndex, neutralFaceName))
    { pOutDraft = nullptr; return wy::ErrorStatus::InvalidInput; }
    if (neutralFaceName.empty()) { assert(false); pOutDraft = nullptr; return wy::ErrorStatus::InvalidInput; }

    TopoNameList faceNames;
    if (!TopoNamingUtil::assemblyTopoNames(*pTopoNaming, shape,
        TopAbs_ShapeEnum::TopAbs_FACE, faceIndices, faceNames))
    { pOutDraft = nullptr; return wy::ErrorStatus::InvalidInput; }
    if (faceNames.empty()) { assert(false); pOutDraft = nullptr; return wy::ErrorStatus::InvalidInput; }

    Draft* pDraft = new Draft();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pDraft);
    if (wy::ErrorStatus::Ok != error) { wydb::deleteElement(pDraft); pDraft = nullptr; return error; }

    error = pDraft->setNeutralFace(neutralFaceName); CHECK_ERROR_FOR_CREATE(error, pDraft);
    error = pDraft->setFaces(faceNames); CHECK_ERROR_FOR_CREATE(error, pDraft);
    error = pDraft->setAngle(angle); CHECK_ERROR_FOR_CREATE(error, pDraft);

    error = pSolid->addModification(pDraft); CHECK_ERROR_FOR_CREATE(error, pDraft);

    pOutDraft = pDraft;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Draft::setAngle(double angle)
{
    if (std::fabs(angle) > wy3d::kMaxDraftAngle) return wy::ErrorStatus::InvalidInput;
    if (angle == _angle) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kDraft_angle);
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

wy::ErrorStatus Draft::setNeutralFace(const TopoName& neutralFaceName)
{
    if (neutralFaceName.empty()) return wy::ErrorStatus::InvalidInput;
    if (neutralFaceName == _neutralFaceName) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kDraft_neutralFaceName);
    if (wy::ErrorStatus::Ok == error)
    {
        _neutralFaceName = neutralFaceName;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Draft::setFaces(const TopoNameList& faces)
{
    if (faces.empty()) return wy::ErrorStatus::InvalidInput;
    if (faces == _faceNames) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kDraft_faceNames);
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


void Draft::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::DRAFT_ANGLE;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Draft::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Draft::classInfo()->className()) {
        if (ParamNames::DRAFT_ANGLE == paramName) return wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(_angle));
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus Draft::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Draft::classInfo()->className()) {
        if (ParamNames::DRAFT_ANGLE == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setAngle(wy3d::degreesToRadians(paramValue.asDouble())); }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Draft::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kDraft_neutralFaceName.value():
        value = _neutralFaceName;
        return true;
    case kDraft_faceNames.value():
        value = _faceNames;
        return true;
    case kDraft_angle.value():
        value = _angle;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Draft::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kDraft_neutralFaceName.value():
        _neutralFaceName = std::any_cast<const TopoName&>(value);
        return true;
    case kDraft_faceNames.value():
        _faceNames = std::any_cast<const TopoNameList&>(value);
        return true;
    case kDraft_angle.value():
        _angle = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Draft::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _neutralFaceName;
    FilerUtil::writeVector(filer, _faceNames);
    filer << _angle;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Draft::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _neutralFaceName;
    FilerUtil::readTopoNameList(filer, _faceNames);
    if (wydb::MemoryFiler* pMemoryFiler = dynamic_cast<wydb::MemoryFiler*>(&filer))
    {
        const bool remapped = TopoNameCodec::remapIds(_neutralFaceName, pMemoryFiler->getIdMapping());
        assert(remapped);
    }
    filer >> _angle;
    return wy::ErrorStatus::Ok;
}

static inline gp_Pnt _toPnt(const wy::Vector3& pnt)
{
    return gp_Pnt(pnt.x(), pnt.y(), pnt.z());
}

static inline gp_Dir _toDir(const wy::Vector3& dir)
{
    return gp_Dir(dir.x(), dir.y(), dir.z());
}

std::pair<bool, TopoDS_Shape> Draft::modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    if (_neutralFaceName.empty())
    {
        assert(false);
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    std::vector<TopoDS_Face> neutralTopoFaces;
    ErrorCode errorCode = BodyModificationUtil::getTopoFacesByTopoNamings<
        ErrorCode::DRAFT_InvalidData,
        ErrorCode::DRAFT_FaceNotExists>(*pTopoNaming, TopoNameList{_neutralFaceName}, neutralTopoFaces);
    if (ErrorCode::NoError != errorCode)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<unsigned int>(errorCode));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }
    if (neutralTopoFaces.size() != 1)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<std::uint32_t>(ErrorCode::DRAFT_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    TopoDS_Face neutralFace = neutralTopoFaces[0];
    wy3d::SketchPlane neutralPlane;
    if (!TopoShapeUtil::getFacePlane(neutralFace, neutralPlane))
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<std::uint32_t>(ErrorCode::DRAFT_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    std::vector<TopoDS_Face> topoFaces;
    errorCode = BodyModificationUtil::getTopoFacesByTopoNamings<
        ErrorCode::DRAFT_InvalidData,
        ErrorCode::DRAFT_FaceNotExists>(*pTopoNaming, _faceNames, topoFaces);
    if (ErrorCode::NoError != errorCode)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<unsigned int>(errorCode));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }
    if (topoFaces.empty())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<std::uint32_t>(ErrorCode::DRAFT_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    try
    {
        BRepOffsetAPI_DraftAngle makeDraft(shape);
        gp_Dir draftDir = _toDir(neutralPlane.getNormal());
        gp_Ax3 ax3(_toPnt(neutralPlane.getOrigin()), draftDir, _toDir(neutralPlane.getXDir()));
        gp_Pln pln(ax3);
        for (const TopoDS_Face& face : topoFaces)
        {
            makeDraft.Add(face, draftDir, _angle, pln);
        }

        makeDraft.Build();
        if (makeDraft.IsDone())
        {
            TopoDS_Shape retShape = makeDraft.Shape();

            DraftTopoShapeComparer topoComparer(makeDraft, shape);
            topoComparer.perform();
            pTopoNaming->update(&topoComparer, this->getId().value());

            this->recordNewFaces(topoComparer.getFaceDelta(), pTopoNaming);

            return std::pair<bool, TopoDS_Shape>(true, retShape);
        }
    }
    catch (const Standard_Failure&)
    {
    }

    wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
        static_cast<std::uint32_t>(ErrorCode::DRAFT_GenerateDraftError));
    return std::pair<bool, TopoDS_Shape>(false, shape);
}

NS_WY3D_END
