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

#include <gp_Quaternion.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <wydbFiler.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSolid.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dParamNames.h>
#include <wy3dBodyModification.h>
#include <wy3dExtrusion.h>
#include <wy3dRevolution.h>
#include <wy3dSweep.h>
#include <wydbFieldRegistry.h>

#include "topo/TopoNamingUtil.h"
#include "topo/TopoShapeUtil.h"
#include "topo/BooleanTopoShapeComparer.h"
#include "BodyModificationUtil.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Solid)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Solid, _solidFlags)
    REGISTER_FIELD(Solid, _ownerId)
    REGISTER_FIELD(Solid, _shape)
    REGISTER_FIELD(Solid, _pTopoNaming)
    REGISTER_FIELD(Solid, _modifications)
    REGISTER_FIELD(Solid, _color)
    REGISTER_FIELD(Solid, _newFaces)
END_FIELD_REGISTRATION()

Solid::Solid() :
    wy3d::Feature(),
    _solidFlags(0),
    _ownerId(wydb::ElementId::kNull),
    _color(140, 153, 165)
{
    _pTopoNaming = std::make_shared<TopoNaming>();
}

Solid::~Solid()
{
}

wy::ErrorStatus Solid::setOwner(const wydb::ElementId& ownerId)
{
    if (ownerId == _ownerId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kSolid_ownerId, wydb::ElementDataPieceType::Appearance);
    if (wy::ErrorStatus::Ok == error)
    {
        _ownerId = ownerId;
        this->markDataPieceDirty(wydb::ElementDataPiece::hierarchy(this->getId()));
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Solid::setCut(bool isCut)
{
    if (this->isCut() == isCut)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kSolid_solidFlags);
    if (wy::ErrorStatus::Ok == error)
    {
        if (isCut)
            _solidFlags |= static_cast<std::uint32_t>(SolidFlag::Cut);
        else
            _solidFlags &= ~static_cast<std::uint32_t>(SolidFlag::Cut);
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Solid::setColor(const wy3d::Color& color)
{
    if (color == _color)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kSolid_color, wydb::ElementDataPieceType::Appearance);
    if (wy::ErrorStatus::Ok == error)
    {
        _color = color;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void Solid::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::SOLID_PARAM_COLOR;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Solid::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Solid::classInfo()->className())
    {
        if (ParamNames::SOLID_PARAM_COLOR == paramName)
            return wydb::ParameterValue::createAny(_color);
        return nullptr;
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus Solid::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Solid::classInfo()->className()) {
        if (ParamNames::SOLID_PARAM_COLOR == paramName)
        {
            if (!paramValue.isAny()) return wy::ErrorStatus::InvalidInput;
            const auto* pAnyVal = dynamic_cast<const wydb::AnyParameterValue*>(&paramValue);
            if (!pAnyVal) return wy::ErrorStatus::InvalidInput;
            const auto* pColor = pAnyVal->tryGet<wy3d::Color>();
            if (!pColor) return wy::ErrorStatus::InvalidInput;
            return this->setColor(*pColor);
        }
        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

wy::ErrorStatus Solid::setShape(const TopoDS_Shape& shape)
{
    wy::ErrorStatus error = this->prepareForFieldChange(kSolid_shape, wydb::ElementDataPieceType::None);
    if (wy::ErrorStatus::Ok == error)
    {
        _shape = shape;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Solid::setTopoNaming(TopoNamingSPtr pTopoNaming)
{
    if (_pTopoNaming == pTopoNaming)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kSolid_pTopoNaming, wydb::ElementDataPieceType::None);
    if (wy::ErrorStatus::Ok == error)
    {
        _pTopoNaming = pTopoNaming;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Solid::addModification(wy3d::BodyModification* pModification)
{
    if (!pModification)
    {
        return wy::ErrorStatus::NullElementPointer;
    }
    if (!pModification->getParent().isNull())
    {
        return wy::ErrorStatus::InvalidInput;
    }
    wydb::ElementId modificationId = pModification->getId();
    if (std::find(_modifications.cbegin(), _modifications.cend(), modificationId) != _modifications.cend()) // already exists
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kSolid_modifications);
    if (wy::ErrorStatus::Ok == error)
    {
        _modifications.emplace_back(modificationId);
        wy::ErrorStatus ownerError = pModification->_setOwner(this->getId()); // always return Ok
        assert(wy::ErrorStatus::Ok == ownerError);
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Solid::addModification(wy3d::Solid* pCutSolid)
{
    if (!pCutSolid)
    {
        return wy::ErrorStatus::NullElementPointer;
    }
    if (pCutSolid->getId() == this->getId())
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (!pCutSolid->getParent().isNull())
    {
        return wy::ErrorStatus::InvalidInput;
    }
    wydb::ElementId modificationId = pCutSolid->getId();
    if (std::find(_modifications.cbegin(), _modifications.cend(), modificationId) != _modifications.cend()) // already exists
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kSolid_modifications);
    if (wy::ErrorStatus::Ok == error)
    {
        _modifications.emplace_back(modificationId);
        wy::ErrorStatus ownerError = pCutSolid->setOwner(this->getId()); // always return Ok
        assert(wy::ErrorStatus::Ok == ownerError);
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Solid::_setModifications(const std::vector<wydb::ElementId>& modifications)
{
    if (modifications == _modifications)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kSolid_modifications);
    if (wy::ErrorStatus::Ok == error)
    {
        _modifications = modifications;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Solid::_setNewFaces(const TopoNameList& newFaces)
{
    if (newFaces == _newFaces)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kSolid_newFaces, wydb::ElementDataPieceType::None);
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

void Solid::recordNewFaces(const ShapeDelta& faceDelta, TopoNaming* pTopoNaming)
{
    assert(pTopoNaming);
    TopoNameList newFaceNames = TopoNamingUtil::computeFacesFromShape(faceDelta, *pTopoNaming, this->getShape());
    this->_setNewFaces(newFaceNames);
}

std::vector<std::uint32_t> Solid::getNewFaceIndices() const
{
    return BodyModificationUtil::computeNewFaceIndices(this->getDatabase(), _newFaces, _ownerId);
}

bool Solid::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSolid_solidFlags.value():
        value = _solidFlags;
        return true;
    case kSolid_ownerId.value():
        value = _ownerId;
        return true;
    case kSolid_shape.value():
        value = _shape;
        return true;
    case kSolid_pTopoNaming.value():
        value = _pTopoNaming;
        return true;
    case kSolid_modifications.value():
        value = _modifications;
        return true;
    case kSolid_color.value():
        value = _color;
        return true;
    case kSolid_newFaces.value():
        value = _newFaces;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Solid::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSolid_solidFlags.value():
        _solidFlags = std::any_cast<std::uint32_t>(value);
        return true;
    case kSolid_ownerId.value():
        _ownerId = std::any_cast<wydb::ElementId>(value);
        return true;
    case kSolid_shape.value():
        _shape = std::any_cast<const TopoDS_Shape&>(value);
        return true;
    case kSolid_pTopoNaming.value():
        _pTopoNaming = std::any_cast<const TopoNamingSPtr&>(value);
        return true;
    case kSolid_modifications.value():
        _modifications = std::any_cast<const std::vector<wydb::ElementId>&>(value);
        return true;
    case kSolid_color.value():
        _color = std::any_cast<wy3d::Color>(value);
        return true;
    case kSolid_newFaces.value():
        _newFaces = std::any_cast<const TopoNameList&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Solid::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _solidFlags;
    filer << _ownerId;
    std::uint32_t numModification = static_cast<std::uint32_t>(_modifications.size());
    filer << numModification;
    for (const wydb::ElementId& modification : _modifications)
    {
        filer << modification;
    }
    filer << static_cast<std::uint32_t>(_color.red)
          << static_cast<std::uint32_t>(_color.green)
          << static_cast<std::uint32_t>(_color.blue);

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Solid::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);

    if (filer.getFileVersion() > wydb::FileVersion(0, 3))
    {
        filer >> _solidFlags;
    }
    else
    {
        _solidFlags = 0;
    }
    filer >> _ownerId;
    std::uint32_t numModifications(0);
    filer >> numModifications;
    _modifications.resize(numModifications);
    for (std::uint32_t i = 0; i < numModifications; ++i)
    {
        filer >> _modifications[i];
    }

    if (filer.getFileVersion() > wydb::FileVersion(0, 13))
    {
        std::uint32_t red(0);
        std::uint32_t green(0);
        std::uint32_t blue(0);
        filer >> red >> green >> blue;
        Color color(red, green, blue);
        _color = color;
    }

    return wy::ErrorStatus::Ok;
}

void Solid::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_ownerId.isNull())
    {
        dependencies.insert(_ownerId);
    }
    dependencies.insert(_modifications.cbegin(), _modifications.cend());
}

bool Solid::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    // 如果主体被删除了则删除自身
    if (!_ownerId.isNull() && erasedDependencies.find(_ownerId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->setOwner(wydb::ElementId::kNull);
        return true;
    }
    else
    {
        // 参与体
        std::vector<wydb::ElementId> newModifications;
        newModifications.reserve(_modifications.size());
        for (const wydb::ElementId& modification : _modifications)
        {
            if (erasedDependencies.find(modification) == erasedDependencies.cend()) // 没有删除
            {
                newModifications.emplace_back(modification);
            }
        }
        if (newModifications.size() == _modifications.size())
        {
            return responsed;
        }

        this->_setModifications(newModifications);
        return true;
    }
}

void Solid::reportChainUpdateDataPieces(wydb::ElementDataPieceCollector& dps) const
{
    __baseClass::reportChainUpdateDataPieces(dps);
    if (!_ownerId.isNull())
    {
        dps.append(
            wydb::ElementDataPiece::create(this->getId(), wydb::ElementDataPieceType::UserDefinedExt4, wydb::TypedValue()),
            wydb::ElementDataPiece::shape(_ownerId));
    }
}

void Solid::onChainUpdater_Completion(
    const wydb::ElementDataPiece& dirtyDataPiece,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    try
    {
        TopoNamingSPtr pTopoNaming = std::make_shared<TopoNaming>();
        TopoDS_Shape initShape = this->generateShape(pTopoNaming.get(), feedbackCollector);
        auto modifyRet = this->modifyShape(initShape, pTopoNaming.get(), feedbackCollector); // 无论成功与失败,返回的Shape都确保是对的
        this->setShape(modifyRet.second);
        this->setTopoNaming(pTopoNaming);
#ifdef _DEBUG
        char szFileName[100] = { 0 };
        sprintf_s(szFileName, 100, "D:/logs/%d.txt", this->getId().value());
        pTopoNaming->print(szFileName, modifyRet.second);
#endif // _DEBUG
    }
    catch (const Standard_Failure&)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(wy3d::ErrorCode::TOPOSHAPE_GenerateShapeError));
        this->setShape(TopoDS_Shape());
        this->setTopoNaming(std::make_shared<TopoNaming>());
    }
}

TopoDS_Shape Solid::generateShape(
    TopoNaming* pTopoNaming,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    return TopoDS_Shape();
}

std::pair<bool, TopoDS_Shape> Solid::modifyShape(
    const TopoDS_Shape& shape,
    TopoNaming* pTopoNaming,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    if (_modifications.empty())
    {
        return std::pair<bool, TopoDS_Shape>(true, shape);
    }
    if (!pTopoNaming || shape.IsNull())
    {
        assert(false);
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    if (!pTrans)
    {
        assert(false);
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    TopoDS_Shape retShape = shape;
    for (const wydb::ElementId& modificationId : _modifications)
    {
        wydb::Element* pModElem = pTrans->getElementForWrite(modificationId);
        if (wy3d::BodyModification* pSolidMod = wy3d::BodyModification::cast(pModElem))
        {
            auto modifyRet = pSolidMod->modifyOwnerShape(retShape, pTopoNaming, feedbackCollector);
            // added by wangyao 2025.05.13 {
            // 强制MarkDirty,无论成功与失败都需要MarkDirty以刷新实体修改的场景节点;
            // 比如:对拉伸体做了倒角;现修改拉伸体的深度;则会执行拉伸体的SolidImpl::onChainUpdater_Completion
            // -->执行到此处,假若不markDirty,则在事务框架中,该实体修改元素并没有被修改从而不会刷新对应的场景节点;
            // 此时选中倒角,显示的高亮边线还是之前倒角的;
            //pSolidMod->markDataPieceDirty(wydb::ElementDataPiece::shape(this->getId()));
            // }
            retShape = modifyRet.second;
        }
        else if (wy3d::Solid* pCutSolid = wy3d::Solid::cast(pModElem))
        {
            auto modifyRet = pCutSolid->modifyOwnerShape(retShape, pTopoNaming, feedbackCollector);
            // added by wangyao 2025.05.13 {
            // 强制MarkDirty,无论成功与失败都需要MarkDirty以刷新实体修改的场景节点;
            // 比如:对拉伸体做了倒角;现修改拉伸体的深度;则会执行拉伸体的SolidImpl::onChainUpdater_Completion
            // -->执行到此处,假若不markDirty,则在事务框架中,该实体修改元素并没有被修改从而不会刷新对应的场景节点;
            // 此时选中倒角,显示的高亮边线还是之前倒角的;
            //pCutSolid->markDataPieceDirty(wydb::ElementDataPiece::shape(this->getId()));
            // }
            retShape = modifyRet.second;
        }
    }

    return std::pair<bool, TopoDS_Shape>(true, retShape);
}

std::pair<bool, TopoDS_Shape> Solid::modifyOwnerShape(
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

    // 参与体(自身)的形体及拓扑命名
    TopoDS_Shape toolShape = this->getShape();
    const wy3d::TopoNaming* pToolNaming = this->getTopoNaming();
    assert(pToolNaming);

    // 合并拓扑命名
    pTopoNaming->merge(*pToolNaming, toolShape, toolShape);

#ifdef _DEBUG
    {
        // 打印拓扑命名的合并结果
        char szFileName[100] = { 0 };
        sprintf_s(szFileName, 100, "D:/logs/%d_merge.txt", this->getId().value());
        pTopoNaming->print(szFileName, TopoShapeUtil::makeCompound(shape, toolShape));
    }
#endif // _DEBUG

    try
    {
        TopoDS_Shape retShape;
        std::shared_ptr<BooleanTopoShapeComparer> pTopoComparer;
        if (this->isCut())
        {
            // 布尔减
            BRepAlgoAPI_Cut cutAlgo(shape, toolShape);
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
            BRepAlgoAPI_Fuse fuseAlgo(shape, toolShape);
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
        this->recordNewFaces(pTopoComparer->getFaceDelta(), pTopoNaming);

        return std::pair<bool, TopoDS_Shape>(true, retShape);
    }
    catch (const Standard_Failure&)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_GenerateShapeError));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }
}

NS_WY3D_END
