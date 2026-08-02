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
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <GeomAbs_JoinType.hxx>
#include <BRepOffset_Mode.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dShell.h>
#include <wy3dSolid.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dParamNames.h>
#include <wy3dParamEnumDef.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "topo/TopoShapeComparer.h"
#include "topo/ShellTopoShapeComparer.h"
#include "topo/TopoNamingUtil.h"
#include "SolidModificationUtil.h"
#include "utils/FilerUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Shell)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Shell, _faceNames)
    REGISTER_FIELD(Shell, _thickness)
    REGISTER_FIELD(Shell, _direction)
    REGISTER_FIELD(Shell, _joinType)
    REGISTER_FIELD(Shell, _offsetMode)
    REGISTER_FIELD(Shell, _intersection)
END_FIELD_REGISTRATION()

Shell::Shell() : wy3d::SolidModification(), _thickness(0.0), _direction(ShellDirection::Inward),
    _joinType(ShellJoinType::Intersection), _offsetMode(ShellOffsetMode::Skin), _intersection(false)
{
}

Shell::~Shell()
{
}

wy::ErrorStatus Shell::create(
    wydb::Transaction* pTrans,
    wy3d::Solid* pSolid,
    const std::vector<std::uint32_t>& faceIndices,
    double thickness,
    ShellDirection direction,
    Shell*& pOutShell)
{
    if (!pTrans) { pOutShell = nullptr; return wy::ErrorStatus::NullDatabasePointer; }
    if (!pSolid) { pOutShell = nullptr; return wy::ErrorStatus::NullElementPointer; }
    if (faceIndices.empty()) { pOutShell = nullptr; return wy::ErrorStatus::InvalidInput; }
    if (thickness < 0.0) { pOutShell = nullptr; return wy::ErrorStatus::InvalidInput; }

    TopoDS_Shape shape = pSolid->getShape();
    TopoNaming* pTopoNaming = pSolid->getTopoNaming();
    if (!pTopoNaming) { pOutShell = nullptr; return wy::ErrorStatus::InvalidInput; }

    TopoNameList faceNames;
    if (!TopoNamingUtil::assemblyTopoNames(*pTopoNaming, shape,
        TopAbs_ShapeEnum::TopAbs_FACE, faceIndices, faceNames))
    {
        pOutShell = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }
    if (faceNames.empty()) { assert(false); pOutShell = nullptr; return wy::ErrorStatus::InvalidInput; }

    Shell* pShell = new Shell();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pShell);
    if (wy::ErrorStatus::Ok != error) { wydb::deleteElement(pShell); pShell = nullptr; return error; }

    error = pShell->setFaces(faceNames); CHECK_ERROR_FOR_CREATE(error, pShell);
    error = pShell->setThickness(thickness); CHECK_ERROR_FOR_CREATE(error, pShell);
    error = pShell->setDirection(direction); CHECK_ERROR_FOR_CREATE(error, pShell);

    error = pSolid->addModification(pShell); CHECK_ERROR_FOR_CREATE(error, pShell);

    pOutShell = pShell;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Shell::setThickness(double thickness)
{
    if (thickness < wy3d::kMinValue || thickness > wy3d::kMaxValue) return wy::ErrorStatus::InvalidInput;
    if (thickness == _thickness) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kShell_thickness);
    if (wy::ErrorStatus::Ok == error)
    {
        _thickness = thickness;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Shell::setDirection(ShellDirection direction)
{
    if (direction == _direction) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kShell_direction);
    if (wy::ErrorStatus::Ok == error)
    {
        _direction = direction;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Shell::setJoinType(ShellJoinType joinType)
{
    if (ShellJoinType::Arc != joinType && ShellJoinType::Intersection != joinType)
        return wy::ErrorStatus::InvalidInput;
    if (joinType == _joinType) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kShell_joinType);
    if (wy::ErrorStatus::Ok == error)
    {
        _joinType = joinType;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Shell::setOffsetMode(ShellOffsetMode offsetMode)
{
    if (ShellOffsetMode::Skin != offsetMode && ShellOffsetMode::Pipe != offsetMode && ShellOffsetMode::RectoVerso != offsetMode)
        return wy::ErrorStatus::InvalidInput;
    if (offsetMode == _offsetMode) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kShell_offsetMode);
    if (wy::ErrorStatus::Ok == error)
    {
        _offsetMode = offsetMode;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Shell::setIntersection(bool intersection)
{
    if (intersection == _intersection) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kShell_intersection);
    if (wy::ErrorStatus::Ok == error)
    {
        _intersection = intersection;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Shell::setFaces(const TopoNameList& faces)
{
    if (faces.empty()) return wy::ErrorStatus::InvalidInput;
    if (faces == _faceNames) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kShell_faceNames);
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


void Shell::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::SHELL_DIRECTION;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::SHELL_THICKNESS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::SHELL_JOIN_TYPE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::SHELL_OFFSET_MODE;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::SHELL_INTERSECTION;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Shell::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Shell::classInfo()->className()) {
        if (ParamNames::SHELL_THICKNESS == paramName) return wydb::ParameterValue::createDouble(_thickness);
        if (ParamNames::SHELL_DIRECTION == paramName) return wydb::ParameterValue::createBoolean(_direction == ShellDirection::Inward);
        if (ParamNames::SHELL_JOIN_TYPE == paramName)
        {
            return wydb::ParameterValue::createAny(
                wy3d::ParamEnumDef(
                    {{static_cast<int>(ShellJoinType::Arc), "Arc"},
                     {static_cast<int>(ShellJoinType::Intersection), "Intersection"}},
                    static_cast<int>(_joinType)));
        }
        if (ParamNames::SHELL_OFFSET_MODE == paramName)
        {
            return wydb::ParameterValue::createAny(
                wy3d::ParamEnumDef(
                    {{static_cast<int>(ShellOffsetMode::Skin), "Skin"},
                     {static_cast<int>(ShellOffsetMode::Pipe), "Pipe"},
                     {static_cast<int>(ShellOffsetMode::RectoVerso), "RectoVerso"}},
                    static_cast<int>(_offsetMode)));
        }
        if (ParamNames::SHELL_INTERSECTION == paramName) return wydb::ParameterValue::createBoolean(_intersection);
        return nullptr;
    }
    return __baseClass::getParameterValue(className, paramName);
}

// 从 ParameterValue 中提取枚举整数值 (兼容 Integer 和 Any<ParamEnumDef>)
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
    return -1; // 无效
}

wy::ErrorStatus Shell::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Shell::classInfo()->className()) {
        if (ParamNames::SHELL_THICKNESS == paramName)
        { if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput; return this->setThickness(paramValue.asDouble()); }
        if (ParamNames::SHELL_DIRECTION == paramName)
        { if (!paramValue.isBoolean()) return wy::ErrorStatus::InvalidInput; return this->setDirection(paramValue.asBoolean() ? ShellDirection::Inward : ShellDirection::Outward); }
        if (ParamNames::SHELL_JOIN_TYPE == paramName)
        { return this->setJoinType(static_cast<ShellJoinType>(_extractEnumValue(paramValue))); }
        if (ParamNames::SHELL_OFFSET_MODE == paramName)
        { return this->setOffsetMode(static_cast<ShellOffsetMode>(_extractEnumValue(paramValue))); }
        if (ParamNames::SHELL_INTERSECTION == paramName)
        { if (!paramValue.isBoolean()) return wy::ErrorStatus::InvalidInput; return this->setIntersection(paramValue.asBoolean()); }
        return wy::ErrorStatus::ParameterNotFound;
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Shell::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kShell_faceNames.value():
        value = _faceNames;
        return true;
    case kShell_thickness.value():
        value = _thickness;
        return true;
    case kShell_direction.value():
        value = _direction;
        return true;
    case kShell_joinType.value():
        value = _joinType;
        return true;
    case kShell_offsetMode.value():
        value = _offsetMode;
        return true;
    case kShell_intersection.value():
        value = _intersection;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Shell::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kShell_faceNames.value():
        _faceNames = std::any_cast<const TopoNameList&>(value);
        return true;
    case kShell_thickness.value():
        _thickness = std::any_cast<double>(value);
        return true;
    case kShell_direction.value():
        _direction = std::any_cast<ShellDirection>(value);
        return true;
    case kShell_joinType.value():
        _joinType = std::any_cast<ShellJoinType>(value);
        return true;
    case kShell_offsetMode.value():
        _offsetMode = std::any_cast<ShellOffsetMode>(value);
        return true;
    case kShell_intersection.value():
        _intersection = std::any_cast<bool>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Shell::writeToFiler(wydb::OutFiler& f) const
{ __baseClass::writeToFiler(f);
    FilerUtil::writeVector(f, _faceNames);
    f << _thickness << static_cast<std::int32_t>(_direction)
      << static_cast<std::int32_t>(_joinType) << static_cast<std::int32_t>(_offsetMode)
      << _intersection;
    return wy::ErrorStatus::Ok; }

wy::ErrorStatus Shell::readFromFiler(wydb::InFiler& f)
{
    __baseClass::readFromFiler(f);
    FilerUtil::readTopoNameList(f, _faceNames);
    f >> _thickness;
    std::int32_t directionInt(0); f >> directionInt;
    _direction = (0 == directionInt) ? ShellDirection::Inward : ShellDirection::Outward;
    // 版本门控: v0.17+ 才有 joinType 和 offsetMode
    if (f.getFileVersion() >= wydb::FileVersion(0, 17))
    {
        std::int32_t joinTypeInt(0); f >> joinTypeInt;
        _joinType = static_cast<ShellJoinType>(joinTypeInt);
        std::int32_t offsetModeInt(0); f >> offsetModeInt;
        _offsetMode = static_cast<ShellOffsetMode>(offsetModeInt);
        f >> _intersection;
    }
    // else: 保持构造函数默认值 (Intersection + Skin + false), 向后兼容旧文件
    return wy::ErrorStatus::Ok;
}

std::pair<bool, TopoDS_Shape> Shell::modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    wydb::Database* pDb = this->getDatabase(); assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction(); assert(pTrans); assert(!pTrans->isGroup());

    std::vector<TopoDS_Face> topoFaces;
    ErrorCode errorCode = SolidModificationUtil::getTopoFacesByTopoNamings<
        ErrorCode::SHELL_InvalidData, ErrorCode::SHELL_FaceNotExists>(*pTopoNaming, _faceNames, topoFaces);
    if (ErrorCode::NoError != errorCode) { wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<unsigned int>(errorCode)); return {false, shape}; }
    if (topoFaces.empty()) { assert(false); return {false, shape}; }

    try
    {
        BRepOffsetAPI_MakeThickSolid shellMaker;
        TopTools_ListOfShape shapeList;
        for (const TopoDS_Face& face : topoFaces) { shapeList.Append(face); }

        // 映射 wy3d 枚举 → OCCT 枚举
        GeomAbs_JoinType occtJoinType;
        switch (_joinType)
        {
        case ShellJoinType::Arc:          occtJoinType = GeomAbs_Arc; break;
        case ShellJoinType::Intersection: occtJoinType = GeomAbs_Intersection; break;
        default: assert(false); occtJoinType = GeomAbs_Intersection; break;
        }

        BRepOffset_Mode occtOffsetMode;
        switch (_offsetMode)
        {
        case ShellOffsetMode::Skin:       occtOffsetMode = BRepOffset_Skin; break;
        case ShellOffsetMode::Pipe:       occtOffsetMode = BRepOffset_Pipe; break;
        case ShellOffsetMode::RectoVerso: occtOffsetMode = BRepOffset_RectoVerso; break;
        default: assert(false); occtOffsetMode = BRepOffset_Skin; break;
        }

        if (_direction == ShellDirection::Inward)
            shellMaker.MakeThickSolidByJoin(shape, shapeList, -_thickness, wy3d::TOL * 10, occtOffsetMode, _intersection, false, occtJoinType);
        else
            shellMaker.MakeThickSolidByJoin(shape, shapeList, _thickness, wy3d::TOL * 10, occtOffsetMode, _intersection, false, occtJoinType);
        shellMaker.Build();
        if (shellMaker.IsDone())
        {
            TopoDS_Shape retShape = shellMaker.Shape();
            ShellTopoShapeComparer topoComparer(shellMaker, shape);
            topoComparer.perform();

#ifdef _DEBUG
            { // 打印拓扑命名的日志
                char szFileName[100] = { 0 };
                sprintf_s(szFileName, 100, "D:/logs/%d.txt", this->getId().value());
                pTopoNaming->print(szFileName, retShape);
            }
#endif

            pTopoNaming->update(&topoComparer, this->getId().value());
            this->recordNewFaces(topoComparer.getFaceDelta(), pTopoNaming);
            return {true, retShape};
        }
    }
    catch (const Standard_Failure&) {}

    wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<std::uint32_t>(ErrorCode::SHELL_GenerateShellError));
    return {false, shape};
}

NS_WY3D_END
