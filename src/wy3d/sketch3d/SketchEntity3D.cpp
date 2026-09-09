///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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
#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketchEntity3D.h>

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(SketchEntity3D)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(SketchEntity3D, _ownerId)
END_FIELD_REGISTRATION()

SketchEntity3D::SketchEntity3D() : wydb::Element(), _ownerId(wydb::ElementId::kNull)
{
}

SketchEntity3D::~SketchEntity3D()
{
}

wy::ErrorStatus SketchEntity3D::setOwner(const wydb::ElementId& ownerId)
{
    if (ownerId == _ownerId)
    {
        return wy::ErrorStatus::Ok;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kSketchEntity3D_ownerId, wydb::ElementDataPieceType::Appearance);
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

bool SketchEntity3D::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchEntity3D_ownerId.value():
        value = _ownerId;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool SketchEntity3D::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketchEntity3D_ownerId.value():
        _ownerId = std::any_cast<wydb::ElementId>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus SketchEntity3D::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    filer << _ownerId;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SketchEntity3D::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    filer >> _ownerId;
    return wy::ErrorStatus::Ok;
}

void SketchEntity3D::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_ownerId.isNull())
    {
        dependencies.insert(_ownerId);
    }
}

bool SketchEntity3D::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);
    if (!_ownerId.isNull() && erasedDependencies.find(_ownerId) != erasedDependencies.cend())
    {
        this->erase(true);
        this->setOwner(wydb::ElementId::kNull);
        return true;
    }
    return responsed;
}

void SketchEntity3D::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

NS_WY3D_END
