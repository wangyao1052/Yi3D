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
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketch3DParamNames.h>

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(SketchCurve3D)

BEGIN_FIELD_REGISTRATION()
END_FIELD_REGISTRATION()

SketchCurve3D::SketchCurve3D() : wy3d::SketchEntity3D()
{
}

SketchCurve3D::~SketchCurve3D()
{
}

void SketchCurve3D::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = Sketch3DParamNames::SKETCH_CURVE3D_ID;
        def.isReadonly = true;
        pParamSchema->addParameterDefinition(def);
    }
}

wydb::ParameterValueUPtr SketchCurve3D::getParameterValue(const std::string& className, const std::string& paramName) const
{
    if (className == SketchCurve3D::classInfo()->className())
    {
        if (Sketch3DParamNames::SKETCH_CURVE3D_ID == paramName)
        {
            return wydb::ParameterValue::createElementId(this->getId());
        }
        else
        {
            return nullptr;
        }
    }
    else
    {
        return __baseClass::getParameterValue(className, paramName);
    }
}

wy::ErrorStatus SketchCurve3D::setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue)
{
    if (className == SketchCurve3D::classInfo()->className())
    {
        if (Sketch3DParamNames::SKETCH_CURVE3D_ID == paramName)
        {
            return wy::ErrorStatus::ParameterReadonly;
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    else
    {
        return __baseClass::setParameterValue(className, paramName, paramValue);
    }
}

NS_WY3D_END
