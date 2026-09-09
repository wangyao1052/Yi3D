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
#include <wydbDatabase.h>
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchEntity3D.h>
#include "utils/Util.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(Sketch3D)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Sketch3D, _parentId)
    REGISTER_FIELD(Sketch3D, _entities)
END_FIELD_REGISTRATION()

Sketch3D::Sketch3D() : wy3d::Feature(), _parentId(wydb::ElementId::kNull)
{
    _entities.reserve(10);
}

Sketch3D::~Sketch3D()
{
}

wy::ErrorStatus Sketch3D::create(wydb::Transaction* pTrans, Sketch3D*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullTransactionPointer;
    }

    Sketch3D* pSketch3D = new Sketch3D();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketch3D);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pSketch3D);
        pSketch3D = nullptr;
        return error;
    }

    pOut = pSketch3D;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sketch3D::setParent(const wydb::ElementId& parentId)
{
    if (parentId == _parentId) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kSketch3D_parentId, wydb::ElementDataPieceType::Appearance);
    if (wy::ErrorStatus::Ok == error)
    {
        _parentId = parentId;
        this->markDataPieceDirty(wydb::ElementDataPiece::hierarchy(this->getId()));
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::Iterator<wydb::ElementId> Sketch3D::createIterator() const
{
    return wy::Iterator<wydb::ElementId>(std::make_unique<Sketch3DElementIterator>(this));
}

wy::ErrorStatus Sketch3D::addEntity(wy3d::SketchEntity3D* pEntity)
{
    if (!pEntity) return wy::ErrorStatus::NullElementPointer;

    wydb::ElementId entityParentId = pEntity->getParent();
    if (!entityParentId.isNull())
    {
        if (entityParentId == this->getId()) return wy::ErrorStatus::Ok;
        else return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = pEntity->setOwner(this->getId());
    assert(wy::ErrorStatus::Ok == error);

    error = this->prepareForFieldChange(kSketch3D_entities);
    if (wy::ErrorStatus::Ok == error)
    {
        _entities.emplace_back(pEntity->getId());
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus Sketch3D::setEntitiesImpl(std::vector<wydb::ElementId>&& entities)
{
    if (_entities == entities) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kSketch3D_entities);
    if (wy::ErrorStatus::Ok == error)
    {
        _entities = std::move(entities);
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

bool Sketch3D::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketch3D_parentId.value():
        value = _parentId;
        return true;
    case kSketch3D_entities.value():
        value = _entities;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Sketch3D::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketch3D_parentId.value():
        _parentId = std::any_cast<wydb::ElementId>(value);
        return true;
    case kSketch3D_entities.value():
        _entities = std::any_cast<const std::vector<wydb::ElementId>&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Sketch3D::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);

    filer << _parentId;

    std::uint32_t numEntities = _entities.size();
    filer << numEntities;
    for (const wydb::ElementId& entityId : _entities) { filer << entityId; }

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sketch3D::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);

    filer >> _parentId;

    std::uint32_t numEntities(0);
    filer >> numEntities;
    _entities.resize(numEntities);
    for (std::uint32_t i = 0; i < numEntities; ++i) { filer >> _entities[i]; }

    return wy::ErrorStatus::Ok;
}

void Sketch3D::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_parentId.isNull()) dependencies.insert(_parentId);
    for (const wydb::ElementId& entityId : _entities) { dependencies.insert(entityId); }
}

bool Sketch3D::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    if (!_parentId.isNull() && erasedDependencies.find(_parentId) != erasedDependencies.cend())
    {
        this->setParent(wydb::ElementId::kNull);
        responsed = true;
    }

    std::vector<wydb::ElementId> newEntities;
    newEntities.reserve(_entities.size());
    for (const wydb::ElementId& entityId : _entities)
    {
        if (erasedDependencies.find(entityId) == erasedDependencies.cend()) newEntities.emplace_back(entityId);
    }
    if (newEntities.size() < _entities.size())
    {
        this->setEntitiesImpl(std::move(newEntities));
        responsed = true;
    }

    return responsed;
}

Sketch3DElementIterator::Sketch3DElementIterator(const Sketch3D* pSketch3D) : _pSketch3D(pSketch3D)
{
    assert(_pSketch3D);
    _iter = _pSketch3D->_entities.cbegin();
}

bool Sketch3DElementIterator::isDone() const
{
    return _iter == _pSketch3D->_entities.cend();
}

void Sketch3DElementIterator::moveNext()
{
    ++_iter;
}

wydb::ElementId Sketch3DElementIterator::current() const
{
    return *_iter;
}

void Sketch3D::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

NS_WY3D_END
