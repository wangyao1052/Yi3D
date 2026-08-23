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
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>

#include <TopExp.hxx>
#include <BRep_Tool.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dExtrusion.h>
#include <wy3dParamNames.h>
#include <wy3dParamEnumDef.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchProfile.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "utils/Util.h"
#include "topo/SketchTopoBuilder.h"
#include "topo/TopoNamingUtil.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Extrusion)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Extrusion, _sketchId)
    REGISTER_FIELD(Extrusion, _direction)
    REGISTER_FIELD(Extrusion, _depth)
    REGISTER_FIELD(Extrusion, _startOffset)
END_FIELD_REGISTRATION()

Extrusion::Extrusion()
    : wy3d::Solid()
    , _sketchId(wydb::ElementId::kNull)
    , _direction(ExtrusionDirection::OneSide)
    , _depth(0.0)
    , _startOffset(0.0)
{
}

Extrusion::~Extrusion()
{
}

wy::ErrorStatus Extrusion::create(
    wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch,
    double depth,
    Extrusion*& pOutExtrusion)
{
    return Extrusion::create(pTrans, pSketch, ExtrusionDirection::OneSide, depth, pOutExtrusion);
}

wy::ErrorStatus Extrusion::create(
    wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch,
    ExtrusionDirection direction,
    double depth,
    Extrusion*& pOutExtrusion)
{
    if (!pTrans)
    {
        pOutExtrusion = nullptr;
        return wy::ErrorStatus::NullTransactionPointer;
    }
    if (!pSketch)
    {
        pOutExtrusion = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }

    Extrusion* pExtrusion = new Extrusion();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pExtrusion);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pExtrusion);
        pExtrusion = nullptr;
        return error;
    }

    error = pExtrusion->_setSketch(pSketch);
    CHECK_ERROR_FOR_CREATE(error, pExtrusion);
    error = pExtrusion->setDirection(direction);
    CHECK_ERROR_FOR_CREATE(error, pExtrusion);
    error = pExtrusion->setDepth(depth);
    CHECK_ERROR_FOR_CREATE(error, pExtrusion);

    pOutExtrusion = pExtrusion;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Extrusion::createCut(
    wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch,
    double depth,
    wy3d::Solid* pSolidToCut,
    Extrusion*& pOutExtrusion)
{
    return Extrusion::createCut(pTrans, pSketch, ExtrusionDirection::OneSide, depth, pSolidToCut, pOutExtrusion);
}

wy::ErrorStatus Extrusion::createCut(
    wydb::Transaction* pTrans,
    wy3d::Sketch* pSketch,
    ExtrusionDirection direction,
    double depth,
    wy3d::Solid* pSolidToCut,
    Extrusion*& pOutExtrusion)
{
    if (!pTrans)
    {
        pOutExtrusion = nullptr;
        return wy::ErrorStatus::NullTransactionPointer;
    }
    if (!pSketch)
    {
        pOutExtrusion = nullptr;
        return wy::ErrorStatus::NullElementPointer;
    }

    Extrusion* pExtrusion = new Extrusion();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pExtrusion);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pExtrusion);
        pExtrusion = nullptr;
        return error;
    }

    error = pExtrusion->_setSketch(pSketch);
    CHECK_ERROR_FOR_CREATE(error, pExtrusion);
    error = pExtrusion->setDirection(direction);
    CHECK_ERROR_FOR_CREATE(error, pExtrusion);
    error = pExtrusion->setDepth(depth);
    CHECK_ERROR_FOR_CREATE(error, pExtrusion);
    error = pExtrusion->setCut(true);
    CHECK_ERROR_FOR_CREATE(error, pExtrusion);

    if (pSolidToCut)
    {
        error = pSolidToCut->addModification(pExtrusion);
        CHECK_ERROR_FOR_CREATE(error, pExtrusion);
    }

    pOutExtrusion = pExtrusion;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Extrusion::_setSketch(const wydb::ElementId& sketchId)
{
    if (sketchId == _sketchId)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kExtrusion_sketchId);
    if (wy::ErrorStatus::Ok == error)
    {
        _sketchId = sketchId;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Extrusion::_setSketch(wy3d::Sketch* pSketch)
{
    // only called by Extrusion static create function.
    assert(_sketchId.isNull());

    if (!pSketch)
    {
        return wy::ErrorStatus::NullElementPointer;
    }
    if (!pSketch->getParent().isNull()) // 草图有owner
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error(wy::ErrorStatus::Ok);
    assert(!pSketch->getId().isNull());
    error = this->_setSketch(pSketch->getId());
    if (wy::ErrorStatus::Ok != error)
    {
        return error;
    }

    error = pSketch->setOwner(this->getId()); //may return NotOpenedForWrite
    if (wy::ErrorStatus::Ok != error)
    {
        return error;
    }

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Extrusion::setDepth(double depth)
{
    if (std::fabs(depth) < wy3d::kMinValue || std::fabs(depth) > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (depth == _depth)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kExtrusion_depth);
    if (wy::ErrorStatus::Ok == error)
    {
        _depth = depth;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Extrusion::setStartOffset(double startOffset)
{
    if (startOffset > wy3d::kMaxValue || startOffset < -wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (startOffset == _startOffset)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kExtrusion_startOffset);
    if (wy::ErrorStatus::Ok == error)
    {
        _startOffset = startOffset;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Extrusion::setDirection(ExtrusionDirection direction)
{
    if (direction < ExtrusionDirection::OneSide || direction > ExtrusionDirection::Symmetric)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (direction == _direction)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(kExtrusion_direction);
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

void Extrusion::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::EXTRUSION_PARAM_DIRECTION;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::EXTRUSION_PARAM_DEPTH;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::EXTRUSION_PARAM_START_OFFSET;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Extrusion::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Extrusion::classInfo()->className())
    {
        if (ParamNames::EXTRUSION_PARAM_DEPTH == paramName)
        {
            return wydb::ParameterValue::createDouble(_depth);
        }
        else if (ParamNames::EXTRUSION_PARAM_START_OFFSET == paramName)
        {
            return wydb::ParameterValue::createDouble(_startOffset);
        }
        else if (ParamNames::EXTRUSION_PARAM_DIRECTION == paramName)
        {
            return wydb::ParameterValue::createAny(
                wy3d::ParamEnumDef(
                    {{static_cast<int>(ExtrusionDirection::OneSide), "One Side"},
                     {static_cast<int>(ExtrusionDirection::Symmetric), "Symmetric"}},
                    static_cast<int>(_direction)));
        }
        else
        {
            return nullptr;
        }
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

wy::ErrorStatus Extrusion::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Extrusion::classInfo()->className())
    {
        if (ParamNames::EXTRUSION_PARAM_DEPTH == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setDepth(paramValue.asDouble());
        }
        else if (ParamNames::EXTRUSION_PARAM_START_OFFSET == paramName)
        {
            if (!paramValue.isDouble()) return wy::ErrorStatus::InvalidInput;
            return this->setStartOffset(paramValue.asDouble());
        }
        else if (ParamNames::EXTRUSION_PARAM_DIRECTION == paramName)
        {
            int enumValue = _extractEnumValue(paramValue);
            if (enumValue < static_cast<int>(ExtrusionDirection::OneSide) ||
                enumValue > static_cast<int>(ExtrusionDirection::Symmetric))
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setDirection(static_cast<ExtrusionDirection>(enumValue));
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, paramName, paramValue);
}

bool Extrusion::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kExtrusion_sketchId.value():
        value = _sketchId;
        return true;
    case kExtrusion_depth.value():
        value = _depth;
        return true;
    case kExtrusion_startOffset.value():
        value = _startOffset;
        return true;
    case kExtrusion_direction.value():
        value = _direction;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Extrusion::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kExtrusion_sketchId.value():
        _sketchId = std::any_cast<const wydb::ElementId&>(value);
        return true;
    case kExtrusion_depth.value():
        _depth = std::any_cast<double>(value);
        return true;
    case kExtrusion_startOffset.value():
        _startOffset = std::any_cast<double>(value);
        return true;
    case kExtrusion_direction.value():
        _direction = std::any_cast<ExtrusionDirection>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Extrusion::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);

    filer << _sketchId;
    if (filer.getFileVersion() > wydb::FileVersion(0, 18))
    {
        filer << static_cast<std::int32_t>(_direction);
    }
    filer << _depth << _startOffset;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Extrusion::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);

    filer >> _sketchId;
    if (filer.getFileVersion() > wydb::FileVersion(0, 18))
    {
        std::int32_t directionInt(0);
        filer >> directionInt;
        _direction = static_cast<ExtrusionDirection>(directionInt);
    }
    filer >> _depth >> _startOffset;
    return wy::ErrorStatus::Ok;
}

void Extrusion::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_sketchId.isNull())
    {
        dependencies.insert(_sketchId);
    }
}

bool Extrusion::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    // 如果草图被删除了则删除拉伸体
    if (!_sketchId.isNull() && erasedDependencies.find(_sketchId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->_setSketch(wydb::ElementId::kNull);
        return true;
    }
    else
    {
        return __baseClass::onDependenciesErased(erasedDependencies);
    }
}

TopoDS_Shape Extrusion::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);

    wydb::Database* pDb = this->getDatabase();
    assert(pDb);
    wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
    assert(pTrans);
    assert(false == pTrans->isGroup());

    // 草图
    const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pDb->getElement(_sketchId));
    if (!pSketch)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }

    // 草图平面
    const wy3d::SketchPlane& sketchPlane = pSketch->getPlane();
    if (!sketchPlane.isValid())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        return TopoDS_Shape();
    }
    wy::Vector3 sketchNormal = sketchPlane.getNormal();
    assert(sketchNormal.length() > 0.5);
    gp_Vec sketchNormalVec(sketchNormal.x(), sketchNormal.y(), sketchNormal.z());
    gp_Dir sketchNormalDir(sketchNormalVec);
    // Symmetric: the face is moved by -|depth|/2 along the normal and extruded
    // by the full |depth|, so the body spans [-|depth|/2, +|depth|/2] (a single
    // prism keeps the topo naming identical to the one-sided case)
    const bool isSymmetric = (ExtrusionDirection::Symmetric == _direction);
    const double halfDepth = isSymmetric ? std::fabs(_depth) / 2.0 : 0.0;
    const double faceOffsetAlongNormal = _startOffset - halfDepth;
    const double extrudeLen = isSymmetric ? std::fabs(_depth) : _depth;
    gp_Trsf sketchTranslation;
    sketchTranslation.SetTranslation(sketchNormalVec * faceOffsetAlongNormal);
    wy::Vector3 sketchOrigin = sketchPlane.getOrigin();
    gp_Pln sketchPln(gp_Pnt(sketchOrigin.x(), sketchOrigin.y(), sketchOrigin.z()), sketchNormalDir);

    // 草图轮廓
    SketchProfile sketchProfile(pSketch);
    if (!sketchProfile.check())
    {
        std::shared_ptr<SketchError> pError = sketchProfile.getError();
        if (pError)
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<unsigned int>(pError->type));
        }
        else
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::PROFILE_InvalidProfile));
        }
        return TopoDS_Shape();
    }

    // 遍历创建形体
    std::vector<TopoDS_Shape> shapes;
    const std::vector<SketchProfile::FaceSPtr>& sketchFaces = sketchProfile.getFaces();
    unsigned int profileIndex = 0;
    const bool isMultiProfile = (sketchFaces.size() > 1);
    for (const SketchProfile::FaceSPtr& pSketchFace : sketchFaces)
    {
        if (isMultiProfile) { ++profileIndex; }
        std::vector<TopoUtil::EdgeNamingInfo> edgeNameInfos;
        auto makeFaceRet = TopoUtil::makeFace(pSketch, pSketchFace, edgeNameInfos);
        if (makeFaceRet.first != ErrorCode::NoError)
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<unsigned int>(makeFaceRet.first));
            return TopoDS_Shape();
        }

        TopoDS_Face face = makeFaceRet.second;
        assert(!face.IsNull());
        if (faceOffsetAlongNormal != 0.0)
        {
            BRepBuilderAPI_Transform transformer(face, sketchTranslation);
            face = TopoDS::Face(transformer.Shape());
            if (face.IsNull())
            {
                assert(false);
                wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                    static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_NullShapeError));
                return TopoDS_Shape();
            }
            for (TopoUtil::EdgeNamingInfo& edgeNameInfo : edgeNameInfos)
            {
                TopoDS_Shape modifiedShape = transformer.ModifiedShape(edgeNameInfo.edge);
                assert(!modifiedShape.IsNull());
                edgeNameInfo.edge = TopoDS::Edge(modifiedShape);
            }
        }

        // 拉伸成体
        BRepPrimAPI_MakePrism makePrism(face, sketchNormalVec * extrudeLen, Standard_True);
        makePrism.Build();
        if (makePrism.IsDone())
        {
            TopoDS_Shape prism = makePrism.Shape();
            shapes.emplace_back(prism);

            // 拓扑命名
            unsigned int idValue = this->getId().value();
            TopoNamingUtil::naming(face, makePrism, edgeNameInfos, idValue, *pTopoNaming,
                profileIndex);
        }
        else
        {
            assert(false);
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_GenerateShapeError));
            return TopoDS_Shape();
        }
    }

    if (shapes.empty())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_NullShapeError));
        return TopoDS_Shape();
    }
    else if (shapes.size() == 1)
    {
#ifdef _DEBUG
        char szFileName[100] = { 0 };
        sprintf_s(szFileName, 100, "D:/logs/%d.txt", this->getId().value());
        pTopoNaming->print(szFileName, shapes.front());
#endif // _DEBUG

        return shapes.front();
    }
    else
    {
        TopoDS_Compound compound;
        BRep_Builder brepBuilder;
        brepBuilder.MakeCompound(compound);
        for (const TopoDS_Shape& shape : shapes)
        {
            brepBuilder.Add(compound, shape);
        }

#ifdef _DEBUG
        char szFileName[100] = { 0 };
        sprintf_s(szFileName, 100, "D:/logs/%d.txt", this->getId().value());
        pTopoNaming->print(szFileName, compound);
#endif // _DEBUG

        return compound;
    }
}

NS_WY3D_END
