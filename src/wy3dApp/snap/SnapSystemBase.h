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

#ifndef WYAP_SNAP_SYSTEM_H
#define WYAP_SNAP_SYSTEM_H

#include <list>
#include <set>
#include <memory>

#include <wyVector3.h>
#include <wydbElement.h>
#include <wyapDefs.h>

NS_WYAP_BEG

class SnapSystemReactor;

// 捕捉上下文
class SnapContext
{
public:
    virtual ~SnapContext() {}
};
typedef std::shared_ptr<SnapContext> SnapContextSPtr;

// 捕捉对象
class SnapObject
{
public:
    explicit SnapObject(const wydb::ElementId& id) : _id(id) {}
    virtual ~SnapObject() {}

    // 获取元素ID
    inline const wydb::ElementId& getId() const
    {
        return _id;
    }

protected:
    wydb::ElementId _id;
};
typedef std::shared_ptr<SnapObject> SnapObjectSPtr;

// 捕捉结果
class SnapResult
{
public:
    explicit SnapResult(const wy::Vector3& position) : _position(position) {}
    virtual ~SnapResult() {}

    // 获取位置坐标
    const wy::Vector3& getPosition() const
    {
        return _position;
    }

protected:
    wy::Vector3 _position;
};
typedef std::shared_ptr<SnapResult> SnapResultSPtr;

// 元素捕捉对象工厂
class SnapObjectFactoryIF
{
public:
    virtual ~SnapObjectFactoryIF() {}

    // 创建元素的捕捉对象
    virtual std::list<wyap::SnapObjectSPtr> createSnapObjects(const wydb::Element* pElem) = 0;
};

class SnapSystem
{
public:
    SnapSystem() {}
    virtual ~SnapSystem() {}

    // 设置捕捉对象工厂
    void setSnapObjectFactory(std::shared_ptr<SnapObjectFactoryIF> pSnapObjectFactory)
    {
        _pSnapObjectFactory = pSnapObjectFactory;
    }

    // 重置
    virtual void reset() = 0;

    // 捕捉
    // x,y               --- 窗口坐标
    // pContext          --- 捕捉上下文
    // idsToExclude      --- 排除掉的元素ID
    // isResultPublished --- publish the result to the reactors (false: compute only)
    virtual wyap::SnapResultSPtr snap(double x, double y, wyap::SnapContextSPtr pContext, bool isResultPublished = true) = 0;
    virtual wyap::SnapResultSPtr snap(double x, double y, wyap::SnapContextSPtr pContext, const std::set<wydb::ElementId>& idsToExclude, bool isResultPublished = true) = 0;

    // 清空捕捉结果
    virtual void clearSnapResult() = 0;

    // 添加反应器
    virtual wy::ErrorStatus addReactor(SnapSystemReactor* pReactor) = 0;
    // 删除反应器
    virtual wy::ErrorStatus removeReactor(SnapSystemReactor* pReactor) = 0;

public:
    // 开始变更
    virtual void beginChange() = 0;
    // 结束变更
    virtual void endChange() = 0;
    // 添加常驻捕捉对象
    virtual bool addResidentSnapObject(wyap::SnapObjectSPtr pSnapObject) = 0;
    // 删除常驻捕捉对象
    virtual bool removeResidentSnapObject(wyap::SnapObjectSPtr pSnapObject) = 0;
    // 清空常驻捕捉对象
    virtual void clearResidentSnapObjects() = 0;

protected:
    std::shared_ptr<SnapObjectFactoryIF> _pSnapObjectFactory;
};

// 反应器
class SnapSystemReactor
{
public:
    SnapSystemReactor() {}
    virtual ~SnapSystemReactor() {}

    // 变更常驻捕捉对象
    virtual void onResidentSnapObjectsChanged(
        const std::set<wyap::SnapObjectSPtr>& addedSnapObjects,
        const std::set<wyap::SnapObjectSPtr>& removedSnapObjects,
        const std::set<wyap::SnapObjectSPtr>& curSnapObjects) {}

    // 捕捉结果变更
    virtual void onSnapResultChanged(
        wyap::SnapResultSPtr pCurSnapResult,
        wyap::SnapResultSPtr pLastSnapResult) {}
};

NS_WYAP_END

#endif // WYAP_SNAP_SYSTEM_H
