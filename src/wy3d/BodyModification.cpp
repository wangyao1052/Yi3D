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

#include <wy3dBodyModification.h>
#include <cassert>
#include <wy3dSolid.h>
#include <wy3dSheet.h>
#include <wydbFiler.h>
#include <TopExp.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <iterator>
#include "BodyModificationUtil.h"
#include "topo/TopoNamingUtil.h"
#include "topo/TopoShapeUtil.h"
#include "topo/BooleanTopoShapeComparer.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(BodyModification)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(BodyModification, _ownerId)
    REGISTER_FIELD(BodyModification, _newFaces)
END_FIELD_REGISTRATION()

BodyModification::BodyModification() : wy3d::Feature()
{
}

BodyModification::~BodyModification()
{
}

const wy3d::Solid* BodyModification::getSolidHost() const
{
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    return wy3d::Solid::cast(pDb->getElement(_ownerId));
}

const wy3d::Sheet* BodyModification::getSheetHost() const
{
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    return wy3d::Sheet::cast(pDb->getElement(_ownerId));
}

wy::ErrorStatus BodyModification::_setOwner(const wydb::ElementId& ownerId)
{
    if (ownerId == _ownerId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kBodyModification_ownerId);
    if (wy::ErrorStatus::Ok == error)
    {
        _ownerId = ownerId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus BodyModification::setNewFaces(const TopoNameList& newFaces)
{
    if (newFaces == _newFaces)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kBodyModification_newFaces, wydb::ElementDataPieceType::None);
    if (wy::ErrorStatus::Ok == error)
    {
        _newFaces = newFaces;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus BodyModification::clearNewFaces()
{
    return this->setNewFaces({});
}

std::vector<std::uint32_t> BodyModification::getNewFaceIndices() const
{
    return BodyModificationUtil::computeNewFaceIndices(this->getDatabase(), _newFaces, _ownerId);
}

bool BodyModification::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kBodyModification_ownerId.value():
        value = _ownerId;
        return true;
    case kBodyModification_newFaces.value():
        value = _newFaces;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool BodyModification::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kBodyModification_ownerId.value():
        _ownerId = std::any_cast<wydb::ElementId>(value);
        return true;
    case kBodyModification_newFaces.value():
        _newFaces = std::any_cast<const TopoNameList&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus BodyModification::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _ownerId;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus BodyModification::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _ownerId;
    return wy::ErrorStatus::Ok;
}

void BodyModification::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_ownerId.isNull())
    {
        dependencies.insert(_ownerId);
    }
}

bool BodyModification::onDependenciesErased(
    const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    // 如果主体被删除了则删除自身
    if (!_ownerId.isNull() && erasedDependencies.find(_ownerId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->_setOwner(wydb::ElementId::kNull);
        return true;
    }

    return responsed;
}

void BodyModification::reportChainUpdateDataPieces(wydb::ElementDataPieceCollector& dps) const
{
    __baseClass::reportChainUpdateDataPieces(dps);

    if (!_ownerId.isNull())
    {
        dps.append(
            wydb::ElementDataPiece::create(this->getId(), wydb::ElementDataPieceType::UserDefinedExt4, wydb::TypedValue()),
            wydb::ElementDataPiece::shape(_ownerId));
    }
}

void BodyModification::recordNewFaces(const ShapeDelta& faceDelta, TopoNaming* pTopoNaming)
{
    assert(pTopoNaming);
    TopoNameList newFaceNames = TopoNamingUtil::computeNewFaces(faceDelta, *pTopoNaming);
    this->setNewFaces(newFaceNames);
}

void BodyModification::appendNewFaces(const ShapeDelta& faceDelta, TopoNaming* pTopoNaming, const TopoDS_Shape& instShape)
{
    assert(pTopoNaming);
    TopoNameList newFaceNames = TopoNamingUtil::computeFacesFromShape(faceDelta, *pTopoNaming, instShape);
    if (newFaceNames.empty())
    {
        return;
    }
    TopoNameList retNewFaceNames = _newFaces;
    retNewFaceNames.insert(retNewFaceNames.cend(),
        std::make_move_iterator(newFaceNames.begin()),
        std::make_move_iterator(newFaceNames.end()));
    this->setNewFaces(retNewFaceNames);
}

std::pair<bool, TopoDS_Shape> BodyModification::modifyOwnerShapeByInstance(
    const TopoDS_Shape& shape, TopoNaming* pTopoNaming,
    const TopoDS_Shape& instShape, TopoNaming* pInstNaming,
    bool isCut,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(!shape.IsNull());
    assert(pTopoNaming);
    assert(!instShape.IsNull());
    assert(pInstNaming);

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    // 合并拓扑命名
    pTopoNaming->merge(*pInstNaming, instShape, instShape);

#ifdef _DEBUG
    {
        // 打印拓扑命名的合并结果
        char szFileName[100] = { 0 };
        sprintf_s(szFileName, 100, "D:/logs/%d_merge.txt", this->getId().value());
        pTopoNaming->print(szFileName, TopoShapeUtil::makeCompound(shape, instShape));
    }
#endif // _DEBUG

    try
    {
        TopoDS_Shape retShape;
        std::shared_ptr<BooleanTopoShapeComparer> pTopoComparer;
        if (isCut)
        {
            // 布尔减
            BRepAlgoAPI_Cut cutAlgo(shape, instShape);
            cutAlgo.Build();
            if (!cutAlgo.IsDone())
            {
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<unsigned int>(wy3d::ErrorCode::TOPOSHAPE_GenerateShapeError));
                return std::pair<bool, TopoDS_Shape>(false, shape);
            }
            retShape = cutAlgo.Shape();

            pTopoComparer = std::make_shared<BooleanTopoShapeComparer>(cutAlgo);
            pTopoComparer->perform();
        }
        else
        {
            // 布尔并
            BRepAlgoAPI_Fuse fuseAlgo(shape, instShape);
            fuseAlgo.Build();
            if (!fuseAlgo.IsDone())
            {
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<unsigned int>(wy3d::ErrorCode::TOPOSHAPE_GenerateShapeError));
                return std::pair<bool, TopoDS_Shape>(false, shape);
            }
            fuseAlgo.SimplifyResult();
            retShape = fuseAlgo.Shape();

            pTopoComparer = std::make_shared<BooleanTopoShapeComparer>(fuseAlgo);
            pTopoComparer->perform();
        }

        // 刷新拓扑命名
#ifdef _DEBUG
        {
            // 打印拓扑比较结果
            char szFileName[100] = { 0 };
            sprintf_s(szFileName, 100, "D:/logs/%d_topoComparer.txt", this->getId().value());
            pTopoComparer->print(szFileName);
        }
#endif // _DEBUG
        pTopoNaming->update(pTopoComparer.get(), this->getId().value());

#ifdef _DEBUG
        // 打印拓扑命名的日志
        char szFileName[100] = { 0 };
        sprintf_s(szFileName, 100, "D:/logs/%d.txt", this->getId().value());
        pTopoNaming->print(szFileName, retShape);
#endif // _DEBUG

        // 记录新生成的面
        this->appendNewFaces(pTopoComparer->getFaceDelta(), pTopoNaming, instShape);

        return std::pair<bool, TopoDS_Shape>(true, retShape);
    }
    catch (const Standard_Failure&)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_GenerateShapeError));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }
}

void BodyModification::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

NS_WY3D_END
