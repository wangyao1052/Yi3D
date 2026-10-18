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

#ifndef WYAP_SNAP_SYSTEM_IMPL_H
#define WYAP_SNAP_SYSTEM_IMPL_H

#include <vector>
#include <list>
#include <map>
#include <set>
#include <queue>
#include <memory>

#include <wydbDatabase.h>
#include "SnapSystemBase.h"
#include <wyapDocManager.h>

NS_WYAP_BEG

// 元素捕捉对象缓存
class ElemSnapObjectsCache
{
public:
    ElemSnapObjectsCache(size_t capacity);

    // 是否为空
    bool isEmpty() const;
    // 是否包含元素
    bool contains(const wydb::ElementId& id) const;
    // 获取所有的元素
    std::vector<wydb::ElementId> getAllElementIds() const;
    // 获取捕捉对象
    const std::list<SnapObjectSPtr>& getSnapObjects(const wydb::ElementId& id) const;
    // 添加缓存
    bool addSnapObjects(const wydb::ElementId& id, std::list<SnapObjectSPtr>&& snapObjects);
    // 移除缓存
    bool removeSnapObjects(const wydb::ElementId& id);
    // 清空
    void clear();

private:
    size_t _capacity;
    std::queue<wydb::ElementId> _ids;
    std::map<wydb::ElementId, std::list<SnapObjectSPtr>> _id2SnapObjects;
};

class SnapSystemImpl : public wyap::SnapSystem, public wyap::DocManagerReactor, public wydb::DatabaseReactor
{
public:
    SnapSystemImpl();
    ~SnapSystemImpl();
    SnapSystemImpl(const SnapSystemImpl&) = delete;
    SnapSystemImpl& operator=(const SnapSystemImpl&) = delete;
    SnapSystemImpl(SnapSystemImpl&&) = delete;
    SnapSystemImpl& operator=(SnapSystemImpl&&) = delete;

    // 重置
    virtual void reset() override;

    // 捕捉
    virtual SnapResultSPtr snap(double x, double y, wyap::SnapContextSPtr pContext, bool isResultPublished = true) override;
    virtual wyap::SnapResultSPtr snap(double x, double y, wyap::SnapContextSPtr pContext, const std::set<wydb::ElementId>& idsToExclude, bool isResultPublished = true) override;
    // 清空捕捉结果
    virtual void clearSnapResult() override;

    // 添加反应器
    virtual wy::ErrorStatus addReactor(SnapSystemReactor* pReactor) override;
    // 删除反应器
    virtual wy::ErrorStatus removeReactor(SnapSystemReactor* pReactor) override;

    // 取消激活文档响应
    virtual void onDocumentToBeDeactivated(Document* pDocToDeactivate) override;
    // 激活文档响应
    virtual void onDocumentToBeActivated(Document* pDocToActivate) override;
    virtual void onDocumentActivated(Document* pActivatedDoc) override;

    // 数据库变更响应
    virtual void onDatabaseChanged(
        const wydb::Database* pDatabase,
        const wydb::Transaction* pTransaction,
        const wydb::DatabaseChangeInfo& changeInfo) override;

public:
    // 开始变更
    virtual void beginChange() override;
    // 结束变更
    virtual void endChange() override;
    // 添加常驻捕捉对象
    virtual bool addResidentSnapObject(wyap::SnapObjectSPtr pSnapObject) override;
    // 删除常驻捕捉对象
    virtual bool removeResidentSnapObject(wyap::SnapObjectSPtr pSnapObject) override;
    // 清空常驻捕捉对象
    virtual void clearResidentSnapObjects() override;

private:
    // 设置捕捉结果
    void setSnapResult(wyap::SnapResultSPtr pSnapResult);
    // 捕捉具体实现
    SnapResultSPtr snapImpl(double x, double y, wyap::SnapContextSPtr pContext, const std::set<wydb::ElementId>* pIdsToExclude = nullptr);

private:
    // 捕捉结果
    wyap::SnapResultSPtr _pCurrSnapResult;

    // 常驻的捕捉对象
    std::set<wyap::SnapObjectSPtr> _residentSnapObjects;
    // 添加的常驻捕捉对象
    // 缓存数据,在endChange()时才会提交修改;
    std::set<wyap::SnapObjectSPtr> _addedSnapObjects;
    // 移除的常驻捕捉对象
    // 缓存数据,在endChange()时才会提交修改;
    std::set<wyap::SnapObjectSPtr> _removedSnapObjects;

    // 反应器集合
    std::vector<SnapSystemReactor*> _reactors;

    // 捕捉对象缓存
    ElemSnapObjectsCache _snapObjectsCache;
};

NS_WYAP_END

#endif // WYAP_SNAP_SYSTEM_IMPL_H
