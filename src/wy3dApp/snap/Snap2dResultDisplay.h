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

#ifndef WY3DAPP_SNAP2D_RESULT_DISPLAY_H
#define WY3DAPP_SNAP2D_RESULT_DISPLAY_H

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <vector>
#include <wy3dApp/GuiCmdTransient.h>
#include <wy3dApp/Snap2dResult.h>
#include "SnapTipWidget.h"

namespace wy3dApp {

// Creates the highlight transient of one element (systems without element
// highlights return null).
using HighlightTransientCreator =
    std::function<GuiCmdTransientSPtr(const wydb::ElementId& id)>;

// Display state of the current snap result: element highlights, item markers
// (point / guide line) and tip widgets. Owned by the snap system, so the
// highlight cache and the item displays survive across results.
class Snap2dResultDisplay
{
public:
    explicit Snap2dResultDisplay(HighlightTransientCreator createHighlight);
    ~Snap2dResultDisplay();
    Snap2dResultDisplay(const Snap2dResultDisplay&) = delete;
    Snap2dResultDisplay& operator=(const Snap2dResultDisplay&) = delete;

    // Synchronize the display with pResult (null clears it). A marker whose
    // kind did not change is moved instead of rebuilt.
    void apply(const Snap2dResult* pResult);
    // Remove everything from the scene.
    void clear();

private:
    struct ItemDisplay
    {
        Snap2dType type = Snap2dType::Undefined;
        std::list<GuiCmdTransientSPtr> transients;
        SnapTipWidget* pSnapTipWidget = nullptr;
    };

    void buildItemDisplay(const Snap2dResult& result, const Snap2dHit& item,
        ItemDisplay& display);
    void updateItemDisplay(const Snap2dResult& result, const Snap2dHit& item,
        ItemDisplay& display);
    void refreshElementHighlights(const Snap2dResult& result);
    void rebuildTipMouseFollow();

    HighlightTransientCreator _createHighlight;
    // Element highlights only (deduped by id), reused across results.
    std::map<wydb::ElementId, GuiCmdTransientSPtr> _elementHighlights;
    // Same order as Snap2dResult::getHits().
    std::vector<ItemDisplay> _itemDisplays;
    std::shared_ptr<SnapTipMouseFollow> _pSnapTipMouseFollow;
};

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_RESULT_DISPLAY_H
