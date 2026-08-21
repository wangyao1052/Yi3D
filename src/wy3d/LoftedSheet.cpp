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

#include <cassert>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dLoftedSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchPoint.h>
#include <wy3dSketchProfileForSheet.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "utils/Util.h"
#include "topo/SketchTopoBuilder.h"
#include "topo/TopoNamingUtil.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(LoftedSheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(LoftedSheet, _profileIds)
END_FIELD_REGISTRATION()

LoftedSheet::LoftedSheet() : wy3d::Sheet()
{
}

LoftedSheet::~LoftedSheet()
{
}

wy::ErrorStatus LoftedSheet::create(
    wydb::Transaction* pTrans,
    const std::vector<wy3d::Sketch*>& profiles,
    LoftedSheet*& pOutSheet)
{
    if (!pTrans)
    {
        pOutSheet = nullptr;
        return wy::ErrorStatus::NullTransactionPointer;
    }
    if (profiles.empty())
    {
        pOutSheet = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }

    LoftedSheet* pSheet = new LoftedSheet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSheet);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSheet);
        pSheet = nullptr;
        return error;
    }

    error = pSheet->setProfilesImpl(profiles);
    CHECK_ERROR_FOR_CREATE(error, pSheet);

    pOutSheet = pSheet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus LoftedSheet::setProfilesImpl(const std::vector<wydb::ElementId>& profileIds)
{
    if (profileIds.size() < 2) return wy::ErrorStatus::InvalidInput;
    if (profileIds == _profileIds) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kLoftedSheet_profileIds);
    if (wy::ErrorStatus::Ok == error)
    {
        _profileIds = profileIds;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus LoftedSheet::setProfilesImpl(const std::vector<wy3d::Sketch*>& profiles)
{
    if (profiles.size() < 2) return wy::ErrorStatus::InvalidInput;

    std::vector<wydb::ElementId> profileIds;
    profileIds.reserve(profiles.size());
    for (wy3d::Sketch* pSketch : profiles)
    {
        if (!pSketch) return wy::ErrorStatus::NullElementPointer;
        if (!pSketch->getParent().isNull() && pSketch->getParent() != this->getId())
        {
            return wy::ErrorStatus::InvalidInput;
        }
        if (pSketch->getDatabase() != this->getDatabase()) return wy::ErrorStatus::InvalidInput;
        profileIds.emplace_back(pSketch->getId());
    }

    wy::ErrorStatus error = this->setProfilesImpl(profileIds);
    if (wy::ErrorStatus::Ok != error) return error;

    for (wy3d::Sketch* pSketch : profiles)
    {
        assert(pSketch);
        error = pSketch->setOwner(this->getId());
        if (wy::ErrorStatus::Ok != error) return error;
    }

    return wy::ErrorStatus::Ok;
}

void LoftedSheet::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

bool LoftedSheet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kLoftedSheet_profileIds.value():
        value = _profileIds;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool LoftedSheet::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kLoftedSheet_profileIds.value():
        _profileIds = std::any_cast<const std::vector<wydb::ElementId>&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus LoftedSheet::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    std::uint32_t numProfiles = _profileIds.size();
    filer << numProfiles;
    for (const wydb::ElementId& profileId : _profileIds) { filer << profileId; }
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus LoftedSheet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    std::uint32_t numProfiles(0);
    filer >> numProfiles;
    _profileIds.resize(numProfiles);
    for (std::uint32_t i = 0; i < numProfiles; ++i) { filer >> _profileIds[i]; }
    return wy::ErrorStatus::Ok;
}

void LoftedSheet::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    for (const wydb::ElementId& profileId : _profileIds)
    {
        if (!profileId.isNull()) dependencies.insert(profileId);
    }
}

bool LoftedSheet::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    std::vector<wydb::ElementId> newProfileIds;
    newProfileIds.reserve(_profileIds.size());
    for (const wydb::ElementId& profileId : _profileIds)
    {
        if (erasedDependencies.find(profileId) == erasedDependencies.cend())
        {
            newProfileIds.emplace_back(profileId);
        }
    }
    if (newProfileIds.size() < _profileIds.size())
    {
        if (newProfileIds.size() < 2)
        {
            this->erase(true);
            this->setProfilesImpl(std::vector<wydb::ElementId>());
        }
        else
        {
            this->setProfilesImpl(newProfileIds);
        }
        return true;
    }
    else
    {
        return responsed;
    }
}

static ErrorCode createProfileWiresForSheet(
    wydb::Database* pDb,
    const std::vector<wydb::ElementId>& profileIds,
    std::vector<TopoUtil::WireInfo>& outProfileWires,
    std::vector<wy::Vector3>& vertices)
{
    assert(pDb);
    if (profileIds.size() < 2) { assert(false); return ErrorCode::LOFTSHEET_InvalidData; }

    auto whetherSketchHasOnlyOneSketchPoint = [pDb](const wy3d::Sketch* pSketch, wy::Vector3& position) -> bool
    {
        assert(pDb);
        assert(pSketch);

        unsigned int numOfEntities(0);
        for (auto iter = pSketch->createIterator(); !iter.isDone(); iter.moveNext())
        {
            ++numOfEntities;
            if (numOfEntities > 1) return false;
        }
        if (1 != numOfEntities) return false;

        const wy3d::SketchPoint* pSketchPoint = wy3d::SketchPoint::cast(
            pDb->getElement(pSketch->createIterator().current()));
        if (!pSketchPoint) return false;
        position = pSketch->getPlane().value(pSketchPoint->getPosition());
        return true;
    };

    std::vector<TopoUtil::WireInfo> profileWires;
    profileWires.reserve(profileIds.size());
    for (const wydb::ElementId& profileId : profileIds)
    {
        const wy3d::Sketch* pProfileSketch = wy3d::Sketch::cast(pDb->getElement(profileId));
        if (!pProfileSketch) { assert(false); return ErrorCode::PROFILE_InvalidProfile; }

        wy::Vector3 point;
        if (whetherSketchHasOnlyOneSketchPoint(pProfileSketch, point))
        {
            vertices.emplace_back(point);
            TopoUtil::WireInfo nullWireInfo;
            profileWires.emplace_back(nullWireInfo);
            continue;
        }

        // Sheet 轮廓: 开放链+闭合链
        SketchProfileForSheet sketchProfileForSheet(pProfileSketch);
        if (!sketchProfileForSheet.check())
        {
            std::shared_ptr<SketchError> pError = sketchProfileForSheet.getError();
            if (pError) return pError->type;
            else return ErrorCode::PROFILE_InvalidProfile;
        }
        const std::vector<SketchProfile::LoopSPtr>& loops = sketchProfileForSheet.getLoops();
        if (loops.empty()) return ErrorCode::PROFILE_InvalidProfile;

        // 与实体放样"只取第一个面"一致: 多链草图只取第一条链
        const SketchProfile::LoopSPtr& pLoop = loops.front();
        assert(pLoop);
        std::vector<TopoUtil::WireInfo> wireInfos;
        ErrorCode errorCode = TopoUtil::makeWires(pProfileSketch, {pLoop}, gp_Trsf(), wireInfos);
        if (ErrorCode::NoError != errorCode) return errorCode;
        if (wireInfos.empty()) return ErrorCode::PROFILE_InvalidProfile;
        else { assert(wireInfos.size() == 1); profileWires.emplace_back(wireInfos[0]); }
    }

    outProfileWires = std::move(profileWires);
    return ErrorCode::NoError;
}

TopoDS_Shape LoftedSheet::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);

    std::vector<TopoUtil::WireInfo> profileWires;
    std::vector<wy::Vector3> vertices;
    ErrorCode error = createProfileWiresForSheet(pDb, _profileIds, profileWires, vertices);
    if (ErrorCode::NoError != error)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(error));
        return TopoDS_Shape();
    }
    if (profileWires.size() < 2)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::LOFTSHEET_InvalidData));
        return TopoDS_Shape();
    }

    bool isSolid = false;
    BRepOffsetAPI_ThruSections makeLoft(isSolid);
    unsigned int vertexIndex(0);
    for (const TopoUtil::WireInfo& wireInfo : profileWires)
    {
        if (wireInfo.wire.IsNull())
        {
            if (vertexIndex < vertices.size())
            {
                const wy::Vector3& pos = vertices[vertexIndex++];
                TopoDS_Vertex vertex = BRepBuilderAPI_MakeVertex(gp_Pnt(pos.x(), pos.y(), pos.z()));
                makeLoft.AddVertex(vertex);
            }
            else
            {
                assert(false);
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<std::uint32_t>(ErrorCode::LOFTSHEET_InvalidData));
                return TopoDS_Shape();
            }
        }
        else
        {
            makeLoft.AddWire(wireInfo.wire);
        }
    }
    makeLoft.Build();
    if (makeLoft.IsDone())
    {
        TopoDS_Shape resultShape = makeLoft.Shape();
        unsigned int idValue = this->getId().value();
        TopoNamingUtil::naming(profileWires, makeLoft, idValue, *pTopoNaming, false);
        return resultShape;
    }
    else
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::LOFTSHEET_GenerateError));
        return TopoDS_Shape();
    }
}

NS_WY3D_END
