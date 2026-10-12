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

#ifndef WY3DAPP_SELECTION_SET_HIGHLIGHTOR_H
#define WY3DAPP_SELECTION_SET_HIGHLIGHTOR_H

#include <memory>
#include <wyapSelection.h>
#include <osg/Vec4>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class SelectionSetHighlightor
{
public:
    explicit SelectionSetHighlightor(const wyap::SelectionSet& ss = wyap::SelectionSet());
    SelectionSetHighlightor(const wyap::SelectionSet& ss, const osg::Vec4& color);
    ~SelectionSetHighlightor();

    // 添加选择集
    bool addSelection(const wyap::Selection& sel);
    // 移除选择集
    bool removeSelection(const wyap::Selection& sel);
    // 清空选择集
    void clearSelections();
    // 是否包含选择集
    bool containsSelection(const wyap::Selection& sel) const;
    // 获取选择集
    const wyap::SelectionSet& getSelectionSet() const { return _ss; }

private:
    void showSelectionSet(const wyap::SelectionSet& ss, bool value);
    void showSelection(const wyap::Selection& sel, bool value);

private:
    wyap::SelectionSet _ss;
    bool _useCustomColor;
    osg::Vec4 _color;

};
typedef std::shared_ptr<SelectionSetHighlightor> SelectionSetHighlightorSPtr;

}

#endif // WY3DAPP_SELECTION_SET_HIGHLIGHTOR_H
