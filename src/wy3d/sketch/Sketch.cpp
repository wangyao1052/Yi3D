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

#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(Sketch)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(Sketch, _ownerId)
    REGISTER_FIELD(Sketch, _plane)
    REGISTER_FIELD(Sketch, _entities)
END_FIELD_REGISTRATION()

Sketch::Sketch() : wy3d::Feature(), _ownerId(wydb::ElementId::kNull),
    _plane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0))
{
    _entities.reserve(20);
}

Sketch::~Sketch()
{
}

wy::ErrorStatus Sketch::create(wydb::Transaction* pTrans, const wy3d::SketchPlane& plane, Sketch*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }

    Sketch* pSketch = new Sketch();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pSketch);
    if (error != wy::ErrorStatus::Ok)
    {
        wydb::deleteElement(pSketch);
        pSketch = nullptr;
        return error;
    }

    error = pSketch->setPlane(plane);
    CHECK_ERROR_FOR_CREATE(error, pSketch)

    pOut = pSketch;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sketch::setOwner(const wydb::ElementId& ownerId)
{
    if (ownerId == _ownerId) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kSketch_ownerId, wydb::ElementDataPieceType::Appearance);
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

wy::ErrorStatus Sketch::setPlane(const wy3d::SketchPlane& plane)
{
    if (plane == _plane) return wy::ErrorStatus::Ok;
    if (!plane.isValid()) return wy::ErrorStatus::InvalidInput;

    wy::ErrorStatus error = this->prepareForFieldChange(kSketch_plane);
    if (wy::ErrorStatus::Ok == error)
    {
        _plane = plane;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::Iterator<wydb::ElementId> Sketch::createIterator() const
{
    return wy::Iterator<wydb::ElementId>(std::make_unique<SketchElementIterator>(this));
}

wy::ErrorStatus Sketch::addEntity(wy3d::SketchEntity* pEntity)
{
    if (!pEntity) return wy::ErrorStatus::NullElementPointer;

    wydb::ElementId eo = pEntity->getParent();
    if (!eo.isNull())
    {
        if (eo == this->getId()) return wy::ErrorStatus::Ok;
        else return wy::ErrorStatus::InvalidInput;
    }

    wy::ErrorStatus error = pEntity->setOwner(this->getId());
    assert(error == wy::ErrorStatus::Ok);

    error = this->prepareForFieldChange(kSketch_entities);
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

wy::ErrorStatus Sketch::_setEntities(std::vector<wydb::ElementId>&& entities)
{
    if (_entities == entities) return wy::ErrorStatus::Ok;

    wy::ErrorStatus error = this->prepareForFieldChange(kSketch_entities);
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

bool Sketch::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kSketch_ownerId.value():
        value = _ownerId;
        return true;
    case kSketch_plane.value():
        value = _plane;
        return true;
    case kSketch_entities.value():
        value = _entities;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool Sketch::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kSketch_ownerId.value():
        _ownerId = std::any_cast<wydb::ElementId>(value);
        return true;
    case kSketch_plane.value():
        _plane = std::any_cast<const wy3d::SketchPlane&>(value);
        return true;
    case kSketch_entities.value():
        _entities = std::any_cast<const std::vector<wydb::ElementId>&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus Sketch::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);

    filer << _ownerId;

    wy::Vector3 planeOrigin = _plane.getOrigin();
    filer << planeOrigin.x() << planeOrigin.y() << planeOrigin.z();
    wy::Vector3 planeNormal = _plane.getNormal();
    filer << planeNormal.x() << planeNormal.y() << planeNormal.z();
    wy::Vector3 planeXDir = _plane.getXDir();
    filer << planeXDir.x() << planeXDir.y() << planeXDir.z();

    std::uint32_t numEntities = _entities.size();
    filer << numEntities;
    for (const wydb::ElementId& entityId : _entities) { filer << entityId; }

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus Sketch::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);

    filer >> _ownerId;

    double x(0.0), y(0.0), z(0.0);
    filer >> x >> y >> z;
    wy::Vector3 planeOrigin(x, y, z);
    filer >> x >> y >> z;
    wy::Vector3 planeNormal(x, y, z);
    filer >> x >> y >> z;
    wy::Vector3 planeXDir(x, y, z);
    _plane = wy3d::SketchPlane(planeOrigin, planeNormal, planeXDir);

    std::uint32_t numEntities(0);
    filer >> numEntities;
    _entities.resize(numEntities);
    for (std::uint32_t i = 0; i < numEntities; ++i) { filer >> _entities[i]; }

    return wy::ErrorStatus::Ok;
}

void Sketch::reportDependencies(std::set<wydb::ElementId>& dependencies) const
{
    __baseClass::reportDependencies(dependencies);
    if (!_ownerId.isNull()) dependencies.insert(_ownerId);
    for (const wydb::ElementId& entityId : _entities) { dependencies.insert(entityId); }
}

bool Sketch::onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies)
{
    bool responsed = __baseClass::onDependenciesErased(erasedDependencies);

    if (!_ownerId.isNull() && erasedDependencies.find(_ownerId) != erasedDependencies.cend())
    {
        // commented by wangyao 2026.08.06 {
        // Keep the sketch alive when its owner is deleted;
        // only clear the owner reference instead of erasing the sketch.
        //this->erase(true);
        // }
        this->setOwner(wydb::ElementId::kNull);
        return true;
    }

    std::vector<wydb::ElementId> newEntities;
    newEntities.reserve(_entities.size());
    for (const wydb::ElementId& entityId : _entities)
    {
        if (erasedDependencies.find(entityId) == erasedDependencies.cend()) newEntities.emplace_back(entityId);
    }
    if (newEntities.size() < _entities.size())
    {
        this->_setEntities(std::move(newEntities));
        return true;
    }
    else
    {
        return responsed;
    }
}

SketchElementIterator::SketchElementIterator(const Sketch* pSketch) : _pSketch(pSketch)
{
    assert(_pSketch);
    _iter = _pSketch->_entities.cbegin();
}

bool SketchElementIterator::isDone() const
{
    return _iter == _pSketch->_entities.cend();
}

void SketchElementIterator::moveNext()
{
    ++_iter;
}

wydb::ElementId SketchElementIterator::current() const
{
    return *_iter;
}

void Sketch::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

NS_WY3D_END
