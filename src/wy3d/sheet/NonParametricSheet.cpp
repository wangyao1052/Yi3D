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
#include <wydbDatabase.h>
#include <wydbFieldRegistry.h>
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wy3dNonParametricSheet.h>
#include "topo/TopoNamingUtil.h"
#include "utils/StringUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(NonParametricSheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(NonParametricSheet, _sourceShape)
END_FIELD_REGISTRATION()

NonParametricSheet::NonParametricSheet() : wy3d::Sheet()
{
}

NonParametricSheet::~NonParametricSheet()
{
}

wy::ErrorStatus NonParametricSheet::create(wydb::Transaction* pTrans, const TopoDS_Shape& shape, NonParametricSheet*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullTransactionPointer;
    }
    if (shape.IsNull())
    {
        pOut = nullptr;
        return wy::ErrorStatus::InvalidInput;
    }

    NonParametricSheet* pSheet = new NonParametricSheet();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSheet);
    if (error != wy::ErrorStatus::Ok)
    {
        assert(false);
        wydb::deleteElement(pSheet);
        pSheet = nullptr;
        return error;
    }

    error = pSheet->setSourceShape(shape);
    CHECK_ERROR_FOR_CREATE(error, pSheet)
    error = pSheet->setShapeImpl(shape);
    CHECK_ERROR_FOR_CREATE(error, pSheet)

    pOut = pSheet;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus NonParametricSheet::setSourceShape(const TopoDS_Shape& shape)
{
    if (shape.IsNull()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->prepareForFieldChange(
        kNonParametricSheet_sourceShape, wydb::ElementDataPieceType::Shape);
    if (wy::ErrorStatus::Ok == error)
    {
        _sourceShape = shape;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

bool NonParametricSheet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kNonParametricSheet_sourceShape.value():
        value = _sourceShape;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool NonParametricSheet::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kNonParametricSheet_sourceShape.value():
        _sourceShape = std::any_cast<const TopoDS_Shape&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

void NonParametricSheet::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

wy::ErrorStatus NonParametricSheet::writeToFiler(wydb::OutFiler& f) const
{
    __baseClass::writeToFiler(f);

    std::string b64ShapeData;
    try { b64ShapeData = StringUtil::shapeToBase64(_sourceShape); }
    catch (...) { assert(false); b64ShapeData = ""; }
    f << b64ShapeData;

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus NonParametricSheet::readFromFiler(wydb::InFiler& f)
{
    __baseClass::readFromFiler(f);

    std::string b64ShapeData;
    f >> b64ShapeData;
    try { _sourceShape = StringUtil::base64ToShape(b64ShapeData); }
    catch (...) { _sourceShape = TopoDS_Shape(); }

    return wy::ErrorStatus::Ok;
}

TopoDS_Shape NonParametricSheet::generateShape(
    TopoNaming* pTopoNaming,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);
    TopoNamingUtil::primitiveNaming(_sourceShape, this->getId().value(), *pTopoNaming);
    return _sourceShape;
}

NS_WY3D_END
