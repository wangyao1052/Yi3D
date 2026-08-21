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
#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSweptSheet.h>
#include <wy3dSketch.h>
#include <wy3dCurve.h>
#include <wy3dSketchProfileForSheet.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "utils/Util.h"
#include "topo/SketchTopoBuilder.h"
#include "topo/TopoNamingUtil.h"
#include "topo/SweepTopoUtil.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(SweptSheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SweptSheet, _pathId)
    REGISTER_FIELD(SweptSheet, _profileId)
END_FIELD_REGISTRATION()

SweptSheet::SweptSheet() : wy3d::Sheet(),
    _pathId(wydb::ElementId::kNull),
    _profileId(wydb::ElementId::kNull)
{
}

SweptSheet::~SweptSheet()
{
}

wy::ErrorStatus SweptSheet::create(wydb::Transaction* pTrans, wy3d::Sketch* pPath, wy3d::Sketch* pProfile, SweptSheet*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullTransactionPointer;
    }

    if (!pPath || !pProfile)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }

    if (pPath->getId() == pProfile->getId())
    {
        pOut = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }

    SweptSheet* pSheet = new SweptSheet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSheet);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSheet);
        pSheet = nullptr;
        return error;
    }

    error = pSheet->setPathImpl(pPath);
    CHECK_ERROR_FOR_CREATE(error, pSheet);
    error = pSheet->setProfileImpl(pProfile);
    CHECK_ERROR_FOR_CREATE(error, pSheet);

    pOut = pSheet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SweptSheet::create(wydb::Transaction* pTrans, wy3d::Curve* pPath, wy3d::Sketch* pProfile, SweptSheet*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullTransactionPointer;
    }

    if (!pPath || !pProfile)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }

    SweptSheet* pSheet = new SweptSheet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSheet);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSheet);
        pSheet = nullptr;
        return error;
    }

    error = pSheet->setPathImpl(pPath);
    CHECK_ERROR_FOR_CREATE(error, pSheet);
    error = pSheet->setProfileImpl(pProfile);
    CHECK_ERROR_FOR_CREATE(error, pSheet);

    pOut = pSheet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SweptSheet::setPathImpl(const wydb::ElementId& pathId)
{
    if (pathId == _pathId) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kSweptSheet_pathId);
    if (wy::ErrorStatus::Ok == error)
    {
        _pathId = pathId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SweptSheet::setPathImpl(wy3d::Sketch* pSketch)
{
    assert(_pathId.isNull());

    if (!pSketch) return wy::ErrorStatus::NullElementPointer;
    if (!pSketch->getParent().isNull() && pSketch->getParent() != this->getId())
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (pSketch->getDatabase() != this->getDatabase()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->setPathImpl(pSketch->getId());
    if (wy::ErrorStatus::Ok != error) return error;

    error = pSketch->setOwner(this->getId());
    return error;
}

wy::ErrorStatus SweptSheet::setPathImpl(wy3d::Curve* pCurve)
{
    assert(_pathId.isNull());

    if (!pCurve) return wy::ErrorStatus::NullElementPointer;
    if (!pCurve->getParent().isNull() && pCurve->getParent() != this->getId())
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (pCurve->getDatabase() != this->getDatabase()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->setPathImpl(pCurve->getId());
    if (wy::ErrorStatus::Ok != error) return error;

    error = pCurve->setOwner(this->getId());
    return error;
}

wy::ErrorStatus SweptSheet::setProfileImpl(const wydb::ElementId& profileId)
{
    if (profileId == _profileId) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kSweptSheet_profileId);
    if (wy::ErrorStatus::Ok == error)
    {
        _profileId = profileId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus SweptSheet::setProfileImpl(wy3d::Sketch* pSketch)
{
    assert(_profileId.isNull());

    if (!pSketch) return wy::ErrorStatus::NullElementPointer;
    if (!pSketch->getParent().isNull() && pSketch->getParent() != this->getId())
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (pSketch->getDatabase() != this->getDatabase()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->setProfileImpl(pSketch->getId());
    if (wy::ErrorStatus::Ok != error) return error;

    error = pSketch->setOwner(this->getId());
    return error;
}

void SweptSheet::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

bool SweptSheet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSweptSheet_pathId.value():
        value = _pathId;
        return true;
    case kSweptSheet_profileId.value():
        value = _profileId;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SweptSheet::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSweptSheet_pathId.value():
        _pathId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kSweptSheet_profileId.value():
        _profileId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SweptSheet::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _pathId << _profileId;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SweptSheet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _pathId >> _profileId;
    return wy::ErrorStatus::Ok;
}

void SweptSheet::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_pathId.isNull()) dependencies.insert(_pathId);
    if (!_profileId.isNull()) dependencies.insert(_profileId);
}

bool SweptSheet::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    if (!_pathId.isNull() && erasedDependencies.find(_pathId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->setPathImpl(wydb::ElementId::kNull);
        responsed = true;
    }

    if (!_profileId.isNull() && erasedDependencies.find(_profileId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->setProfileImpl(wydb::ElementId::kNull);
        responsed = true;
    }

    return responsed;
}

TopoDS_Shape SweptSheet::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);

    wy3d::SketchPlane pathPlane;
    const wydb::Element* pPathElem = pDb->getElement(_pathId);
    const wy3d::Sketch* pPathSketch = wy3d::Sketch::cast(pPathElem);
    const wy3d::Curve* pPathCurve = wy3d::Curve::cast(pPathElem);
    if (pPathSketch && pPathSketch->getPlane().isValid())
    {
        pathPlane = pPathSketch->getPlane();
    }
    else if (!pPathCurve)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PATH_InvalidPath));
        return TopoDS_Shape();
    }

    const wy3d::Sketch* pProfileSketch = wy3d::Sketch::cast(pDb->getElement(_profileId));
    if (!pProfileSketch || !pProfileSketch->getPlane().isValid())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }
    const wy3d::SketchPlane& profilePlane = pProfileSketch->getPlane();
    assert(profilePlane.isValid());

    if (pPathSketch)
    {
        if (profilePlane.getNormal().dot(pathPlane.getNormal()) > 1e-3)
        {
            assert(false);
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::SWEEP_PathPlaneAndProfilePlaneAreNotOrthogonal));
            return TopoDS_Shape();
        }
    }

    TopoUtil::WireInfo pathWireInfo;
    wy::Vector3 pathStartPos;
    wy::Vector3 pathStartDir;
    ErrorCode errorCreatePathWire(ErrorCode::PATH_InvalidPath);
    if (pPathSketch)
    {
        errorCreatePathWire = SweepTopoUtil::createPathWire(*pPathSketch, pathWireInfo, pathStartPos, pathStartDir);
    }
    else if (pPathCurve)
    {
        errorCreatePathWire = SweepTopoUtil::createPathWire(*pPathCurve, pathWireInfo, pathStartPos, pathStartDir);
    }
    if (ErrorCode::NoError != errorCreatePathWire)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(errorCreatePathWire));
        return TopoDS_Shape();
    }
    assert(pathStartDir.length() > 0.5);

    SketchProfileForSheet profileForSheet(pProfileSketch);
    if (!profileForSheet.check())
    {
        std::shared_ptr<SketchError> pError = profileForSheet.getError();
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            pError ? static_cast<unsigned int>(pError->type)
                   : static_cast<unsigned int>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }
    const std::vector<SketchProfile::LoopSPtr>& loops = profileForSheet.getLoops();
    if (loops.empty())
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }

    std::vector<TopoDS_Shape> shapes;
    for (const SketchProfile::LoopSPtr& pLoop : loops)
    {
        std::vector<TopoUtil::WireInfo> wireInfos;
        gp_Trsf trsf; // 单位变换：扫掠无偏移
        ErrorCode errorCode = TopoUtil::makeWires(pProfileSketch, {pLoop}, trsf, wireInfos);
        if (ErrorCode::NoError != errorCode || wireInfos.size() != 1)
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<unsigned int>(ErrorCode::NoError != errorCode
                    ? errorCode : ErrorCode::SWEPTSHEET_GenerateError));
            return TopoDS_Shape();
        }
        const TopoUtil::WireInfo& profileWireInfo = wireInfos.front();

        TopoDS_Shape shape;
        ErrorCode error = SweepTopoUtil::makePipeShell(
            this->getId().value(), pathWireInfo, profileWireInfo,
            false, shape, *pTopoNaming);
        if (ErrorCode::NoError != error)
        {
            assert(false);
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::SWEPTSHEET_GenerateError));
            return TopoDS_Shape();
        }
        shapes.emplace_back(shape);
    }

    if (shapes.empty())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_NullShapeError));
        return TopoDS_Shape();
    }

    TopoDS_Compound compound;
    BRep_Builder brepBuilder;
    brepBuilder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : shapes)
    {
        brepBuilder.Add(compound, shape);
    }

#ifdef _DEBUG
    unsigned int idValue = this->getId().value();
    char szFileName[100] = { 0 };
    sprintf_s(szFileName, 100, "D:/logs/%d.txt", idValue);
    pTopoNaming->print(szFileName, compound);
#endif

    return compound;
}

NS_WY3D_END
