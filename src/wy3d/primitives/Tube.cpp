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
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dTube.h>
#include <wy3dImpl.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Tube)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Tube, _outerRadius)
    REGISTER_FIELD(Tube, _innerRadius)
    REGISTER_FIELD(Tube, _height)
END_FIELD_REGISTRATION()

Tube::Tube() : wy3d::Primitive(), _outerRadius(0.0), _innerRadius(0.0), _height(0.0)
{
}

Tube::~Tube()
{
}

wy::ErrorStatus Tube::create(wydb::Transaction* pTrans, double outerRadius, double innerRadius, double height, Tube*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }

    Tube* pTube = new Tube();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pTube);
    if (error != wy::ErrorStatus::Ok)
    {
        wydb::deleteElement(pTube);
        pTube = nullptr;
        return error;
    }

    error = pTube->setOuterRadius(outerRadius);
    CHECK_ERROR_FOR_CREATE(error, pTube)
    error = pTube->setInnerRadius(innerRadius);
    CHECK_ERROR_FOR_CREATE(error, pTube)
    error = pTube->setHeight(height);
    CHECK_ERROR_FOR_CREATE(error, pTube)

    pOut = pTube;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Tube::setOuterRadius(double r)
{
    if (r < wy3d::kMinValue || r > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (r < _innerRadius + wy3d::kMinValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kTube_outerRadius);
    if (wy::ErrorStatus::Ok == error)
    {
        _outerRadius = r;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Tube::setInnerRadius(double r)
{
    if (r < wy3d::kMinValue || r > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    if (r > _outerRadius - wy3d::kMinValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kTube_innerRadius);
    if (wy::ErrorStatus::Ok == error)
    {
        _innerRadius = r;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Tube::setHeight(double h)
{
    if (h < wy3d::kMinValue || h > wy3d::kMaxValue)
    {
        return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kTube_height);
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


void Tube::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = Tube::PARAM_HEIGHT;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Tube::PARAM_INNER_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
    {
        wydb::ParameterDefinitionData def;
        def.name = Tube::PARAM_OUTER_RADIUS;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr Tube::getParameterValue(const std::string& className, const std::string& n) const
{
    if (className == Tube::classInfo()->className())
    {
        if (Tube::PARAM_OUTER_RADIUS == n)
        {
            return wydb::ParameterValue::createDouble(_outerRadius);
        }
        else if (Tube::PARAM_INNER_RADIUS == n)
        {
            return wydb::ParameterValue::createDouble(_innerRadius);
        }
        else if (Tube::PARAM_HEIGHT == n)
        {
            return wydb::ParameterValue::createDouble(_height);
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, n);
}

wy::ErrorStatus Tube::setParameterValue(const std::string& className, const std::string& n, const wydb::ParameterValue& v)
{
    if (className == Tube::classInfo()->className())
    {
        if (!v.isDouble())
        {
            return wy::ErrorStatus::InvalidInput;
        }

        double d = v.asDouble();
        if (Tube::PARAM_OUTER_RADIUS == n)
        {
            return this->setOuterRadius(d);
        }
        else if (Tube::PARAM_INNER_RADIUS == n)
        {
            return this->setInnerRadius(d);
        }
        else if (Tube::PARAM_HEIGHT == n)
        {
            return this->setHeight(d);
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, n, v);
}

bool Tube::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kTube_outerRadius.value():
        value = _outerRadius;
        return true;
    case kTube_innerRadius.value():
        value = _innerRadius;
        return true;
    case kTube_height.value():
        value = _height;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Tube::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kTube_outerRadius.value():
        _outerRadius = std::any_cast<double>(value);
        return true;
    case kTube_innerRadius.value():
        _innerRadius = std::any_cast<double>(value);
        return true;
    case kTube_height.value():
        _height = std::any_cast<double>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Tube::writeToFiler(wydb::OutFiler& f) const
{
    __baseClass::writeToFiler(f);
    f << _outerRadius << _innerRadius << _height;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Tube::readFromFiler(wydb::InFiler& f)
{
    __baseClass::readFromFiler(f);
    f >> _outerRadius >> _innerRadius >> _height;
    return wy::ErrorStatus::Ok;
}

TopoDS_Shape Tube::generateOriginalShape() const
{
    TopoDS_Wire outerWire;
    {
        BRepBuilderAPI_MakeWire makeOuterWire;
        BRepBuilderAPI_MakeEdge mkEdge(gp_Circ(gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0)), _outerRadius));
        makeOuterWire.Add(mkEdge.Edge());
        outerWire = makeOuterWire.Wire();
    }

    TopoDS_Wire innerWire;
    {
        BRepBuilderAPI_MakeWire makeInnerWire;
        BRepBuilderAPI_MakeEdge mkEdge(gp_Circ(gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0, 0, -1), gp_Dir(1, 0, 0)), _innerRadius));
        makeInnerWire.Add(mkEdge.Edge());
        innerWire = makeInnerWire.Wire();
    }

    BRepBuilderAPI_MakeFace mkFace(gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)));
    mkFace.Add(outerWire);
    mkFace.Add(innerWire);

    BRepPrimAPI_MakePrism makePrism(mkFace, gp_Vec(0.0, 0.0, _height));
    TopoDS_Shape shape = makePrism.Shape();
    return shape;
}

NS_WY3D_END
