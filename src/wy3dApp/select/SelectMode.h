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

#ifndef WY3DAPP_SELECT_MODE_H
#define WY3DAPP_SELECT_MODE_H

#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

/// Selection mode
/// Reference: selection behavior of SolidWorks & Creo
enum class SelectMode
{
    /// Replace mode
    /// 1. Ctrl key not pressed:
    ///    Point‑select and box‑select replace the current selection set (overwrite).
    /// 2. Ctrl key pressed:
    ///    Point‑select and box‑select toggle element selection state.
    /// Primarily used in generic selection commands.
    Replace = 0,

    /// Add mode
    /// 1. Ctrl key not pressed:
    ///    Point‑select and box‑select add elements to the existing selection set.
    /// 2. Ctrl key pressed:
    ///    Point‑select and box‑select toggle element selection state.
    /// Primarily used in commands requiring element selection, such as move and mirror.
    Add = 1,
};

} // namespace wy3dApp

#endif // WY3DAPP_SELECT_MODE_H
