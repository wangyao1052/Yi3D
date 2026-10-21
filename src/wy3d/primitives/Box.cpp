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
#include <BRepPrimAPI_MakeBox.hxx>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dBox.h>
#include <wy3dImpl.h>
#include "utils/Util.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(Box)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Box, _length)
    REGISTER_FIELD(Box, _width)
    REGISTER_FIELD(Box, _height)
END_FIELD_REGISTRATION()

Box::Box() : wy3d::Primitive(), _length(0.0), _width(0.0), _height(0.0)
{
}

Box::~Box()
{
}

wy::ErrorStatus Box::create(wydb::Transaction* pTrans, double length, double width, double height, Box*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }

    Box* pBox = new Box();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pBox);
    if (error != wy::ErrorStatus::Ok)
    {
        wydb::deleteElement(pBox);
        pBox = nullptr;
        return error;
    }

    error = pBox->setLength(length);
    CHECK_ERROR_FOR_CREATE(error, pBox)
    error = pBox->setWidth(width);
    CHECK_ERROR_FOR_CREATE(error, pBox)
    error = pBox->setHeight(height);
    CHECK_ERROR_FOR_CREATE(error, pBox)

    pOut = pBox;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Box::setLength(double l)
{
    if (l < wy3d::kMinValue || l > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kBox_length);
    if (wy::ErrorStatus::Ok == error)
    {
        _length = l;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Box::setWidth(double w)
{
    if (w < wy3d::kMinValue || w > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kBox_width);
    if (wy::ErrorStatus::Ok == error)
    {
        _width = w;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Box::setHeight(double h)
{
    if (h < wy3d::kMinValue || h > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kBox_height);
    if (wy::ErrorStatus::Ok == error)
    {
        _height = h;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void Box::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = Box::PARAM_HEIGHT;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Box::PARAM_LENGTH;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Box::PARAM_WIDTH;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Box::getParameterValue(const std::string& className, const std::string& n) const
{
    if (className == Box::classInfo()->className())
    {
        if (Box::PARAM_LENGTH == n)
        {
            return wydb::ParameterValue::createDouble(_length);
        }
        else if (Box::PARAM_WIDTH == n)
        {
            return wydb::ParameterValue::createDouble(_width);
        }
        else if (Box::PARAM_HEIGHT == n)
        {
            return wydb::ParameterValue::createDouble(_height);
        }
    }
    return __baseClass::getParameterValue(className, n);
}

wy::ErrorStatus Box::setParameterValue(const std::string& className, const std::string& n, const wydb::ParameterValue& v)
{
    if (className == Box::classInfo()->className())
    {
        if (Box::PARAM_LENGTH == n)
        {
            if (!v.isDouble())
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setLength(v.asDouble());
        }
        else if (Box::PARAM_WIDTH == n)
        {
            if (!v.isDouble())
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setWidth(v.asDouble());
        }
        else if (Box::PARAM_HEIGHT == n)
        {
            if (!v.isDouble())
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setHeight(v.asDouble());
        }
    }
    return __baseClass::setParameterValue(className, n, v);
}

bool Box::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kBox_length.value():
        value = _length;
        return true;
    case kBox_width.value():
        value = _width;
        return true;
    case kBox_height.value():
        value = _height;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Box::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kBox_length.value():
        _length = std::any_cast<double>(value);
        return true;
    case kBox_width.value():
        _width = std::any_cast<double>(value);
        return true;
    case kBox_height.value():
        _height = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Box::writeToFiler(wydb::OutFiler& f) const
{
    __baseClass::writeToFiler(f);
    f << _length << _width << _height;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Box::readFromFiler(wydb::InFiler& f)
{
    __baseClass::readFromFiler(f);
    f >> _length >> _width >> _height;
    return wy::ErrorStatus::Ok;
}

TopoDS_Shape Box::generateOriginalShape() const
{
    BRepPrimAPI_MakeBox makeBox(_length, _width, _height);
    TopoDS_Shape shape = makeBox.Shape();
    return shape;
}

NS_WY3D_END
