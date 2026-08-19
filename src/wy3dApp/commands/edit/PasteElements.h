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

#ifndef WY3DAPP_PASTE_ELEMENTS_H
#define WY3DAPP_PASTE_ELEMENTS_H

#include <list>
#include <set>

#include <osg/PositionAttitudeTransform>
#include <osg/ref_ptr>

#include <wyVector3.h>
#include <wydbElementId.h>
#include <wy3dSketchPlane.h>

#include "commands/GuiCommand.h"

class ElementNode;

class PasteElements : public GuiCmdMakeElement
{
public:
    PasteElements(wydb::Database* pDb, GuiCommand* pGuiCmd, GuiCmdEnvType mode, wydb::ElementId sketchId, const wy3d::SketchPlane& sketchPlane);
    ~PasteElements();

    enum class InitRet
    {
        Success_Continue = 0, // 成功并且需要继续指定粘贴的位置
        Success_End = 1,      // 成功并且结束
        Failed = 2,           // 失败
    };
    InitRet init(const wy::Vector3& pos);

    bool update(const wy::Vector3& pos);
    bool perform(const wy::Vector3& pos);

    // 获取新创建的元素ID集合
    inline const std::set<wydb::ElementId>& getNewlyCreatedElements() const
    {
        return _newlyCreatedIds;
    }

private:
    void freeCopyElements();

    // 粘贴的元素是否支持重定位
    static bool whetherSupportsRelocating_Modeling(const std::vector<wydb::Element*>& copyElements);

private:
    GuiCmdEnvType _mode;
    wydb::ElementId _sketchId;
    wy3d::SketchPlane _sketchPlane;

    // 拷贝的元素
    struct CopyElement
    {
        wydb::Element* pElem;
        wy::Vector3 initPosition;
        CopyElement() : pElem(nullptr) {}
    };
    std::list<CopyElement> _copyElements;
    std::set<wydb::ElementId> _newlyCreatedIds;
    // 初始位置
    wy::Vector3 _initOrigin;
    // 临时渲染节点
    osg::ref_ptr<osg::PositionAttitudeTransform> _pat;
    // 隐藏的元素渲染节点
    std::list<wydb::ElementId> _hiddenElemIds;
};

#endif // WY3DAPP_PASTE_ELEMENTS_H
