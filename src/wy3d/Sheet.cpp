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

#include <wydbFiler.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSheet.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dParamNames.h>
#include <wydbFieldRegistry.h>

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Sheet)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Sheet, _shape)
    REGISTER_FIELD(Sheet, _pTopoNaming)
    REGISTER_FIELD(Sheet, _color)
END_FIELD_REGISTRATION()

Sheet::Sheet() :
    wy3d::Feature(),
    _color(140, 153, 165)
{
    _pTopoNaming = std::make_shared<TopoNaming>();
}

Sheet::~Sheet()
{
}

wy::ErrorStatus Sheet::setColor(const wy3d::Color& color)
{
    if (color == _color)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(
        kSheet_color, wydb::ElementDataPieceType::Appearance);
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

void Sheet::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ParamNames::SOLID_PARAM_COLOR;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr Sheet::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == Sheet::classInfo()->className())
    {
        if (ParamNames::SOLID_PARAM_COLOR == paramName)
            return wydb::ParameterValue::createAny(_color);
        return nullptr;
    }
    return __baseClass::getParameterValue(className, paramName);
}

wy::ErrorStatus Sheet::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == Sheet::classInfo()->className())
    {
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

wy::ErrorStatus Sheet::setShape(const TopoDS_Shape& shape)
{
    wy::ErrorStatus error = this->prepareForFieldChange(
        kSheet_shape, wydb::ElementDataPieceType::None);
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

wy::ErrorStatus Sheet::setTopoNaming(TopoNamingSPtr pTopoNaming)
{
    if (_pTopoNaming == pTopoNaming)
    {
        return wy::ErrorStatus::Ok;
    }
    wy::ErrorStatus error = this->prepareForFieldChange(
        kSheet_pTopoNaming, wydb::ElementDataPieceType::None);
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

bool Sheet::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSheet_shape.value():
        value = _shape;
        return true;
    case kSheet_pTopoNaming.value():
        value = _pTopoNaming;
        return true;
    case kSheet_color.value():
        value = _color;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Sheet::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSheet_shape.value():
        _shape = std::any_cast<const TopoDS_Shape&>(value);
        return true;
    case kSheet_pTopoNaming.value():
        _pTopoNaming = std::any_cast<const TopoNamingSPtr&>(value);
        return true;
    case kSheet_color.value():
        _color = std::any_cast<wy3d::Color>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Sheet::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);

    if (filer.getFileVersion() > wydb::FileVersion(0, 13))
    {
        filer << static_cast<std::uint32_t>(_color.red)
              << static_cast<std::uint32_t>(_color.green)
              << static_cast<std::uint32_t>(_color.blue);
    }

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sheet::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);

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

void Sheet::onChainUpdater_Completion(
    const wydb::ElementDataPiece& dirtyDataPiece,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    try
    {
        TopoNamingSPtr pTopoNaming = std::make_shared<TopoNaming>();
        TopoDS_Shape initShape = this->generateShape(pTopoNaming.get(), feedbackCollector);
        this->setShape(initShape);
        this->setTopoNaming(pTopoNaming);
    }
    catch (const Standard_Failure&)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<unsigned int>(wy3d::ErrorCode::TOPOSHAPE_GenerateShapeError));
        this->setShape(TopoDS_Shape());
        this->setTopoNaming(std::make_shared<TopoNaming>());
    }
}

TopoDS_Shape Sheet::generateShape(
    TopoNaming* pTopoNaming,
    wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    return TopoDS_Shape();
}

NS_WY3D_END
