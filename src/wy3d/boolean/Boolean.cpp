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

#include <TopoDS_Compound.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wy3dBoolean.h>
#include <wydbFieldRegistry.h>
#include <wy3dSolid.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "topo/TopoShapeComparer.h"
#include "topo/BooleanTopoShapeComparer.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Boolean)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Boolean, _boolType)
    REGISTER_FIELD(Boolean, _target)
    REGISTER_FIELD(Boolean, _tools)
END_FIELD_REGISTRATION()

#define TOOLS_INIT_CAPACITY 5

Boolean::Boolean() : wy3d::Solid(), _boolType(BooleanType::Undefined), _target()
{
    _tools.reserve(TOOLS_INIT_CAPACITY);
}

Boolean::Boolean(BooleanType boolType) : wy3d::Solid(), _boolType(boolType), _target()
{
    _tools.reserve(TOOLS_INIT_CAPACITY);
}

Boolean::~Boolean()
{
}

std::vector<wydb::ElementId> Boolean::getChildren() const
{
    std::vector<wydb::ElementId> children;
    std::vector<wydb::ElementId> baseChildren = __baseClass::getChildren();
    children.reserve(_tools.size() + 1 + baseChildren.size());
    if (!_target.isNull()) children.emplace_back(_target);
    for (const wydb::ElementId& toolId : _tools)
    {
        if (!toolId.isNull()) children.emplace_back(toolId);
    }
    children.insert(children.cend(), baseChildren.cbegin(), baseChildren.cend());
    return children;
}

wy::ErrorStatus Boolean::_setTarget(wy3d::Solid* pTarget)
{
    assert(_target.isNull());
    assert(_tools.empty());

    if (!pTarget) return wy::ErrorStatus::NullElementPointer;
    if (!pTarget->getParent().isNull()) return wy::ErrorStatus::InvalidInput;

    const wydb::ElementId& targetId = pTarget->getId();
    assert(!targetId.isNull());

    wy::ErrorStatus fieldError = this->prepareForFieldChange(kBoolean_target);
    if (wy::ErrorStatus::Ok == fieldError)
    {
        _target = targetId;

        wy::ErrorStatus error = pTarget->setOwner(this->getId());
        if (wy::ErrorStatus::Ok != error) return error;

        return wy::ErrorStatus::Ok;
    }
    else
    {
        return fieldError;
    }
}

wy::ErrorStatus Boolean::_setTarget(const wydb::ElementId& targetId)
{
    if (_target == targetId) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kBoolean_target);
    if (wy::ErrorStatus::Ok == error)
    {
        _target = targetId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Boolean::_setTools(std::vector<wydb::ElementId>&& tools)
{
    if (_tools == tools) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kBoolean_tools);
    if (wy::ErrorStatus::Ok == error)
    {
        _tools = std::move(tools);
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Boolean::addTool(wy3d::Solid* pTool)
{
    if (!pTool) return wy::ErrorStatus::NullElementPointer;
    if (!pTool->getParent().isNull()) return wy::ErrorStatus::InvalidInput;

    const wydb::ElementId& toolId = pTool->getId();
    if (toolId == _target) return wy::ErrorStatus::InvalidInput;
    if (std::find(_tools.cbegin(), _tools.cend(), toolId) != _tools.cend()) return wy::ErrorStatus::Ok;

    wy::ErrorStatus fieldError = this->prepareForFieldChange(kBoolean_tools);
    if (wy::ErrorStatus::Ok == fieldError)
    {
        _tools.emplace_back(toolId);

        wy::ErrorStatus error = pTool->setOwner(this->getId());
        assert(wy::ErrorStatus::Ok == error);

        return wy::ErrorStatus::Ok;
    }
    else
    {
        return fieldError;
    }
}

wy::ErrorStatus Boolean::cancelBoolean()
{
    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    if (!pTrans) { assert(false); return wy::ErrorStatus::NoActiveTransaction; }

    wy::ErrorStatus error = wy::ErrorStatus::Ok;

    if (!_target.isNull())
    {
        wy3d::Feature* pFeat = wy3d::Feature::cast(pTrans->getElementForWrite(_target));
        if (!pFeat) { assert(false); error = wy::ErrorStatus::Error; }
        else
        {
            wy3d::Solid* pFeatSolid = dynamic_cast<wy3d::Solid*>(pFeat);
            pFeatSolid->setOwner(wydb::ElementId::kNull);
        }
    }

    for (const wydb::ElementId& toolId : _tools)
    {
        wy3d::Feature* pFeat = wy3d::Feature::cast(pTrans->getElementForWrite(toolId));
        if (!pFeat) { assert(false); error = wy::ErrorStatus::Error; continue; }
        wy3d::Solid* pFeatSolid = dynamic_cast<wy3d::Solid*>(pFeat);
        pFeatSolid->setOwner(wydb::ElementId::kNull);
    }

    this->_setTarget(wydb::ElementId::kNull);
    this->_setTools(std::vector<wydb::ElementId>());
    this->erase(true);

    return error;
}

bool Boolean::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kBoolean_boolType.value():
        value = _boolType;
        return true;
    case kBoolean_target.value():
        value = _target;
        return true;
    case kBoolean_tools.value():
        value = _tools;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Boolean::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kBoolean_boolType.value():
        _boolType = std::any_cast<BooleanType>(value);
        return true;
    case kBoolean_target.value():
        _target = std::any_cast<wydb::ElementId>(value);
        return true;
    case kBoolean_tools.value():
        _tools = std::any_cast<const std::vector<wydb::ElementId>&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Boolean::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << static_cast<std::uint16_t>(_boolType);
    filer << _target;
    std::uint32_t numMembers = _tools.size();
    filer << numMembers;
    for (const wydb::ElementId& tool : _tools) { filer << tool; }
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Boolean::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    std::uint16_t boolType(0);
    filer >> boolType;
    _boolType = static_cast<BooleanType>(boolType);
    filer >> _target;
    std::uint32_t numTools(0);
    filer >> numTools;
    _tools.resize(numTools);
    for (std::uint32_t i = 0; i < numTools; ++i) { filer >> _tools[i]; }
    return wy::ErrorStatus::Ok;
}

void Boolean::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    dependencies.insert(_target);
    dependencies.insert(_tools.cbegin(), _tools.cend());
}

bool Boolean::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    if (erasedDependencies.find(_target) != erasedDependencies.cend())
    {
        this->eraseOnResponse(erasedDependencies);
        return true;
    }

    std::vector<wydb::ElementId> newTools;
    newTools.reserve(_tools.size());
    for (const wydb::ElementId& tool : _tools)
    {
        if (erasedDependencies.find(tool) == erasedDependencies.cend()) newTools.emplace_back(tool);
    }
    if (newTools.size() == _tools.size()) return responsed;

    if (newTools.empty()) this->eraseOnResponse(erasedDependencies);
    this->_setTools(std::move(newTools));
    return true;
}

void Boolean::eraseOnResponse(const std::set<wydb::ElementId>& erasedDependencies)
{
    this->erase(true);

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    if (!pTrans) { assert(false); return; }

    auto setHostBooleanNull = [pTrans](const wydb::ElementId& id)
    {
        wydb::Element* pElem = pTrans->getElementForWrite(id);
        wy3d::Solid* pSolid = wy3d::Solid::cast(pElem);
        if (pSolid)
        {
            pSolid->setOwner(wydb::ElementId::kNull);
        }
    };
    setHostBooleanNull(_target);
    for (const wydb::ElementId& toolId : _tools) { setHostBooleanNull(toolId); }
}

static TopoDS_Compound makeCompound(const TopoDS_Shape& shape1, const TopoDS_Shape& shape2)
{
    BRep_Builder brepBuilder;
    TopoDS_Compound compound;
    brepBuilder.MakeCompound(compound);
    brepBuilder.Add(compound, shape1);
    brepBuilder.Add(compound, shape2);
    return compound;
}

TopoDS_Shape Boolean::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    assert(pTopoNaming->isEmpty());

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    auto getMemberSolid = [pDb](const wydb::ElementId& id) -> const wy3d::Solid*
    {
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        return pSolid;
    };

    const wy3d::Solid* pTargetSolid = getMemberSolid(_target);
    if (!pTargetSolid)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(wy3d::ErrorCode::BOOLEAN_InvalidTargetId));
        return TopoDS_Shape();
    }
    TopoDS_Shape resultShape = pTargetSolid->getShape();
    const wy3d::TopoNaming* pTargetNaming = pTargetSolid->getTopoNaming();
    assert(pTargetNaming);

    *pTopoNaming = *pTargetNaming;

    switch (_boolType)
    {
    case BooleanType::Union:
    {
        for (const wydb::ElementId& toolId : _tools)
        {
            const wy3d::Solid* pToolSolid = getMemberSolid(toolId);
            if (!pToolSolid)
            {
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<unsigned int>(wy3d::ErrorCode::BOOLEAN_InvalidToolId));
                continue;
            }

            TopoDS_Shape toolShape = pToolSolid->getShape();
            const wy3d::TopoNaming* pToolNaming = pToolSolid->getTopoNaming();
            assert(pToolNaming);

            pTopoNaming->merge(*pToolNaming, toolShape, toolShape);

            BRepAlgoAPI_Fuse fuseAlgo(resultShape, toolShape);
            fuseAlgo.Build();
            if (!fuseAlgo.IsDone())
            {
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<unsigned int>(wy3d::ErrorCode::TOPOSHAPE_GenerateShapeError));
                return TopoDS_Shape();
            }
            fuseAlgo.SimplifyResult();

            BooleanTopoShapeComparer topoComparer(fuseAlgo);
            topoComparer.perform();
            pTopoNaming->update(&topoComparer, this->getId().value());

            resultShape = fuseAlgo.Shape();
        }
    }
    break;

    case BooleanType::Difference:
    {
        for (const wydb::ElementId& toolId : _tools)
        {
            const wy3d::Solid* pToolSolid = getMemberSolid(toolId);
            if (!pToolSolid)
            {
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<unsigned int>(wy3d::ErrorCode::BOOLEAN_InvalidToolId));
                continue;
            }

            TopoDS_Shape toolShape = pToolSolid->getShape();
            const wy3d::TopoNaming* pToolNaming = pToolSolid->getTopoNaming();
            assert(pToolNaming);

            pTopoNaming->merge(*pToolNaming, toolShape, toolShape);

            BRepAlgoAPI_Cut cutAlgo(resultShape, toolShape);
            cutAlgo.Build();
            if (!cutAlgo.IsDone())
            {
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<unsigned int>(wy3d::ErrorCode::TOPOSHAPE_GenerateShapeError));
                return TopoDS_Shape();
            }

            BooleanTopoShapeComparer topoComparer(cutAlgo);
            topoComparer.perform();
            pTopoNaming->update(&topoComparer, this->getId().value());

            resultShape = cutAlgo.Shape();
        }
    }
    break;

    case BooleanType::Intersection:
    {
        for (const wydb::ElementId& toolId : _tools)
        {
            const wy3d::Solid* pToolSolid = getMemberSolid(toolId);
            if (!pToolSolid)
            {
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<unsigned int>(wy3d::ErrorCode::BOOLEAN_InvalidToolId));
                continue;
            }

            TopoDS_Shape toolShape = pToolSolid->getShape();
            const wy3d::TopoNaming* pToolNaming = pToolSolid->getTopoNaming();
            assert(pToolNaming);

            pTopoNaming->merge(*pToolNaming, toolShape, toolShape);

            BRepAlgoAPI_Common commonAlgo(resultShape, toolShape);
            commonAlgo.Build();
            if (!commonAlgo.IsDone())
            {
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<unsigned int>(wy3d::ErrorCode::TOPOSHAPE_GenerateShapeError));
                return TopoDS_Shape();
            }

            BooleanTopoShapeComparer topoComparer(commonAlgo);
            topoComparer.perform();
            pTopoNaming->update(&topoComparer, this->getId().value());

            resultShape = commonAlgo.Shape();
        }
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }

    TopExp_Explorer exp(resultShape, TopAbs_EDGE);
    bool hasEdge = exp.More();
    if (!hasEdge)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(wy3d::ErrorCode::warnTOPOSHAPE_NullShape));
    }

    return resultShape;
}

void Boolean::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

NS_WY3D_END
