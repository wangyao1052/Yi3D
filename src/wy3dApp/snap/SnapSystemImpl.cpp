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

#include "SnapSystemImpl.h"
#include <cassert>
#include <wydbDatabase.h>
#include <wyapApplication.h>
#include <wyapViewManager.h>
#include <wyapSceneManager.h>
#include <wyapView.h>
#include "view/View.h"

NS_WYAP_BEG

SnapSystemImpl::SnapSystemImpl() : _snapObjectsCache(10)
{
}

SnapSystemImpl::~SnapSystemImpl()
{
}

wyap::SnapResultSPtr SnapSystemImpl::snap(double x, double y, wyap::SnapContextSPtr pContext, bool isResultPublished)
{
    wyap::SnapResultSPtr pSnapResult = this->snapImpl(x, y, pContext);
    if (isResultPublished) this->setSnapResult(pSnapResult);
    return pSnapResult;
}

wyap::SnapResultSPtr SnapSystemImpl::snap(double x, double y, wyap::SnapContextSPtr pContext, const std::set<wydb::ElementId>& idsToExclude, bool isResultPublished)
{
    wyap::SnapResultSPtr pSnapResult = this->snapImpl(x, y, pContext, &idsToExclude);
    if (isResultPublished) this->setSnapResult(pSnapResult);
    return pSnapResult;
}

SnapResultSPtr SnapSystemImpl::snapImpl(double x, double y, wyap::SnapContextSPtr pContext, const std::set<wydb::ElementId>* pIdsToExclude)
{
    if (!pContext)
    {
        assert(false);
        return nullptr;
    }

    Application* pApp = getApplication();
    if (!pApp)
    {
        assert(false);
        return nullptr;
    }

    // 当前文档
    wyap::DocManager* pDocMgr = pApp->getDocManager();
    assert(pDocMgr);
    wyap::Document* pActiveDoc = pDocMgr->getActiveDocument();
    if (!pActiveDoc)
    {
        assert(false);
        return nullptr;
    }
    wydb::Database* pDb = pActiveDoc->getDatabase();
    assert(pDb);

    // 当前视图
    ViewManager* pViewMgr = pApp->getViewManager();
    assert(pViewMgr);
    wyap::View* pView = pViewMgr->getView(pActiveDoc);
    if (!pView)
    {
        assert(false);
        return nullptr;
    }

    // 优先捕捉常驻对象
    wy3dApp::View* pBaseView = static_cast<wy3dApp::View*>(pView);
    wyap::SnapResultSPtr pSnapResult = pBaseView->snapObject(x, y, _residentSnapObjects);
    if (pSnapResult)
    {
        return pSnapResult;
    }

    // 捕捉到的元素
    std::list<wydb::ElementId> snappedElemIds = pBaseView->snapElements(x, y);
    if (snappedElemIds.empty())
    {
        return nullptr;
    }

    // 遍历
    size_t idx(0);
    for (const wydb::ElementId& id : snappedElemIds)
    {
        if (pIdsToExclude && pIdsToExclude->find(id) != pIdsToExclude->cend())
        {
            continue;
        }

        ++idx;
        if (idx >= 20) // 限制最多捕捉20个元素
        {
            break;
        }

        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem)
        {
            assert(false);
            continue;
        }

        // 不在缓存中则添加进缓存
        if (!_snapObjectsCache.contains(id))
        {
            std::list<wyap::SnapObjectSPtr> snapObjects = _pSnapObjectFactory->createSnapObjects(pElem);
            _snapObjectsCache.addSnapObjects(id, std::move(snapObjects));
        }
        assert(_snapObjectsCache.contains(id));

        // 捕捉
        const std::list<SnapObjectSPtr>& snapObjects = _snapObjectsCache.getSnapObjects(id);
        wyap::SnapResultSPtr pSnapResult2 = pBaseView->snapObject(x, y, snapObjects);
        if (pSnapResult2)
        {
            return pSnapResult2;
        }
    }

    return nullptr;
}

void SnapSystemImpl::clearSnapResult()
{
    this->setSnapResult(nullptr);
}

void SnapSystemImpl::beginChange()
{
    // 清空数据
    _addedSnapObjects.clear();
    _removedSnapObjects.clear();
}

// 校验数据的有效性
static bool _checkForAssert(
    const std::set<wyap::SnapObjectSPtr>& snapObjects,
    const std::set<wyap::SnapObjectSPtr>& addedSnapObjects,
    const std::set<wyap::SnapObjectSPtr>& removedSnapObjects)
{
    // 所有指针都不为空
    for (const wyap::SnapObjectSPtr& pSnapObject : snapObjects)
    {
        if (!pSnapObject) return false;
    }
    for (const wyap::SnapObjectSPtr& pSnapObject : addedSnapObjects)
    {
        if (!pSnapObject) return false;
    }
    for (const wyap::SnapObjectSPtr& pSnapObject : removedSnapObjects)
    {
        if (!pSnapObject) return false;
    }

    // 待添加的已经在snapObjects中则return false
    for (auto iter = addedSnapObjects.cbegin(); iter != addedSnapObjects.cend(); ++iter)
    {
        if (snapObjects.find(*iter) != snapObjects.cend())
        {
            return false;
        }
    }

    // 待删除的不在snapObjects中return false
    for (auto iter = removedSnapObjects.cbegin(); iter != removedSnapObjects.cend(); ++iter)
    {
        if (snapObjects.find(*iter) == snapObjects.cend())
        {
            return false;
        }
    }

    return true;
}

void SnapSystemImpl::endChange()
{
    // assert校验数据
    assert(_checkForAssert(_residentSnapObjects, _addedSnapObjects, _removedSnapObjects));

    // 没有任何修改则直接返回
    if (_addedSnapObjects.empty() && _removedSnapObjects.empty())
    {
        return;
    }

    // 先移除
    for (auto iter = _removedSnapObjects.cbegin(); iter != _removedSnapObjects.cend(); ++iter)
    {
        _residentSnapObjects.erase(*iter);
    }

    // 再添加
    for (auto iter = _addedSnapObjects.cbegin(); iter != _addedSnapObjects.cend(); ++iter)
    {
        _residentSnapObjects.insert(*iter);
    }

    // 反应器
    for (wyap::SnapSystemReactor* pReactor : _reactors)
    {
        assert(pReactor);
        pReactor->onResidentSnapObjectsChanged(_addedSnapObjects, _removedSnapObjects, _residentSnapObjects);
    }

    // 清空数据
    _addedSnapObjects.clear();
    _removedSnapObjects.clear();
}

bool SnapSystemImpl::addResidentSnapObject(wyap::SnapObjectSPtr pSnapObject)
{
    if (!pSnapObject)
    {
        assert(false);
        return false;
    }

    // 已经在集合中
    if (_residentSnapObjects.find(pSnapObject) != _residentSnapObjects.cend())
    {
        assert(false);
        return false;
    }

    auto pair = _addedSnapObjects.insert(pSnapObject);
    return pair.second;
}

bool SnapSystemImpl::removeResidentSnapObject(wyap::SnapObjectSPtr pSnapObject)
{
    if (!pSnapObject)
    {
        assert(false);
        return false;
    }

    // 不在集合中
    if (_residentSnapObjects.find(pSnapObject) == _residentSnapObjects.cend())
    {
        assert(false);
        return false;
    }

    auto pair = _removedSnapObjects.insert(pSnapObject);
    return pair.second;
}

void SnapSystemImpl::clearResidentSnapObjects()
{
    _addedSnapObjects.clear();
    _removedSnapObjects = _residentSnapObjects;
}

wy::ErrorStatus SnapSystemImpl::addReactor(SnapSystemReactor* pReactor)
{
    if (!pReactor)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    auto iter = std::find(_reactors.cbegin(), _reactors.cend(), pReactor);
    if (iter != _reactors.cend())
    {
        return wy::ErrorStatus::DuplicateKey;
    }
    _reactors.emplace_back(pReactor);

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus SnapSystemImpl::removeReactor(SnapSystemReactor* pReactor)
{
    if (!pReactor)
    {
        return wy::ErrorStatus::InvalidInput;
    }
    auto iter = std::find(_reactors.cbegin(), _reactors.cend(), pReactor);
    if (iter == _reactors.cend())
    {
        return wy::ErrorStatus::KeyNotFound;
    }
    _reactors.erase(iter);

    return wy::ErrorStatus::Ok;
}

void SnapSystemImpl::setSnapResult(wyap::SnapResultSPtr pSnapResult)
{
    if (_pCurrSnapResult == pSnapResult)
    {
        return;
    }

    wyap::SnapResultSPtr pLastSnapResult = _pCurrSnapResult;
    _pCurrSnapResult = pSnapResult;

    for (SnapSystemReactor* pReactor : _reactors)
    {
        assert(pReactor);
        pReactor->onSnapResultChanged(_pCurrSnapResult, pLastSnapResult);
    }
}

void SnapSystemImpl::reset()
{
    this->setSnapResult(nullptr);
    _addedSnapObjects.clear();
    _removedSnapObjects.clear();
    _snapObjectsCache.clear();
    return;
}

void SnapSystemImpl::onDocumentToBeDeactivated(Document* pDocToDeactivate)
{
    assert(pDocToDeactivate);

    // 重置
    this->reset();

    // remove database reactor
    wydb::Database* pDb = pDocToDeactivate->getDatabase();
    assert(pDb);
    pDb->removeReactor(this);
}

void SnapSystemImpl::onDocumentToBeActivated(Document* pDocToActivate)
{
}

void SnapSystemImpl::onDocumentActivated(Document* pActivatedDoc)
{
    assert(pActivatedDoc);

    // 重置
    this->reset();

    // add database reactor
    wydb::Database* pDb = pActivatedDoc->getDatabase();
    assert(pDb);
    pDb->addReactor(this);
}

void SnapSystemImpl::onDatabaseChanged(
    const wydb::Database* pDatabase,
    const wydb::Transaction* pTransaction,
    const wydb::DatabaseChangeInfo& changeInfo)
{
    if (_snapObjectsCache.isEmpty())
    {
        return;
    }

    std::vector<wydb::ElementId> ids = _snapObjectsCache.getAllElementIds();
    for (const wydb::ElementId& id : ids)
    {
        if (changeInfo.erasedIds.find(id) != changeInfo.erasedIds.cend())
        {
            _snapObjectsCache.removeSnapObjects(id);
        }
        else if (changeInfo.modifiedIds.find(id) != changeInfo.modifiedIds.cend())
        {
            // added by wangyao 2025.01.18 {
            // TODO 暂时先暴力地移除
            _snapObjectsCache.removeSnapObjects(id);
            //if (details.isShapeChanged(id) || details.isTransformChanged(id))
            //{
            //    _snapObjectsCache.removeSnapObjects(id);
            //}
            // }
        }
    }
}

ElemSnapObjectsCache::ElemSnapObjectsCache(size_t capacity)
    : _capacity(capacity)
{
    assert(_capacity > 0);
}

bool ElemSnapObjectsCache::isEmpty() const
{
    assert(_ids.size() == _id2SnapObjects.size());
    return _id2SnapObjects.empty();
}

bool ElemSnapObjectsCache::contains(const wydb::ElementId& id) const
{
    assert(_ids.size() == _id2SnapObjects.size());
    return _id2SnapObjects.find(id) != _id2SnapObjects.cend();
}

std::vector<wydb::ElementId> ElemSnapObjectsCache::getAllElementIds() const
{
    assert(_ids.size() == _id2SnapObjects.size());
    std::vector<wydb::ElementId> ids;
    ids.reserve(_id2SnapObjects.size());
    for (const auto& kvp : _id2SnapObjects)
    {
        ids.push_back(kvp.first);
    }
    return ids;
}

const std::list<SnapObjectSPtr>& ElemSnapObjectsCache::getSnapObjects(const wydb::ElementId& id) const
{
    static const std::list<SnapObjectSPtr> emptySnapObjects;

    auto iter = _id2SnapObjects.find(id);
    if (iter == _id2SnapObjects.cend())
    {
        return emptySnapObjects;
    }
    return iter->second;
}

bool ElemSnapObjectsCache::addSnapObjects(const wydb::ElementId& id, std::list<SnapObjectSPtr>&& snapObjects)
{
    assert(_ids.size() == _id2SnapObjects.size());
    if (id.isNull()) return false;

    if (_id2SnapObjects.find(id) != _id2SnapObjects.cend())
    {
        _id2SnapObjects[id] = std::move(snapObjects);
        return true;
    }

    // 达到容量上限先移除一个
    assert(_ids.size() <= _capacity);
    if (_ids.size() == _capacity)
    {
        if (_ids.empty()) return false;
        wydb::ElementId id = _ids.front();
        _ids.pop();
        _id2SnapObjects.erase(id);
        assert(_ids.size() == _id2SnapObjects.size());
    }

    // 添加
    _ids.push(id);
    _id2SnapObjects[id] = std::move(snapObjects);
    assert(_ids.size() == _id2SnapObjects.size());
    return true;
}

bool ElemSnapObjectsCache::removeSnapObjects(const wydb::ElementId& id)
{
    assert(_ids.size() == _id2SnapObjects.size());
    if (id.isNull()) return false;
    if (_id2SnapObjects.find(id) == _id2SnapObjects.cend()) return false;

    std::queue<wydb::ElementId> newIds;
    while (!_ids.empty())
    {
        if (_ids.front() != id)
        {
            newIds.push(_ids.front());
        }
        _ids.pop();
    }
    _ids.swap(newIds);
    _id2SnapObjects.erase(id);
    assert(_ids.size() == _id2SnapObjects.size());
    return true;
}

void ElemSnapObjectsCache::clear()
{
    std::queue<wydb::ElementId> emptyIds;
    _ids.swap(emptyIds);
    _id2SnapObjects.clear();
}

NS_WYAP_END
