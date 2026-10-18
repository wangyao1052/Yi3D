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

#ifndef WY3DAPP_SNAP2D_RULE_H
#define WY3DAPP_SNAP2D_RULE_H

#include <memory>
#include <set>
#include <typeinfo>
#include <wyVector2.h>
#include <wydbDatabase.h>
#include <wy3dApp/Snap2dType.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class Snap2dContext;

// A rule's snap hit: the snapped plane position, the kind of snap (drives the
// tip icon; Undefined draws a plain point), an optional element to highlight
// (null id: no highlight), and the reference point of an alignment point.
struct Snap2dRuleHit
{
    wy::Vector2 point;
    wydb::ElementId highlightId;
    Snap2dType snapType = Snap2dType::Undefined;
    // Required for AlignX/AlignY (draws the guide line), ignored otherwise.
    wy::Vector2 refPoint;
};

class Snap2dRule;
typedef std::shared_ptr<Snap2dRule> Snap2dRuleSPtr;

// External snap rule; register with Snap2dSystem::addRule (one rule per exact
// context type). Both faces return true on a hit. The screen-scale parameters
// are plane units per screen pixel at the cursor; use them to convert a pixel
// tolerance into a plane tolerance.
class WY3DAPP_EXPORT Snap2dRule
{
public:
    // Context type this rule handles, for example typeid(MyContext).
    virtual const std::type_info& getContextType() const = 0;

    // Snap with no picked element.
    virtual bool snapNoPicked(
        const Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY,
        const std::set<wydb::ElementId>& excludeIds,
        Snap2dRuleHit& hit)
    {
        return false;
    }

    // Snap after an element was picked.
    virtual bool snapPicked(
        const Snap2dContext* pContext,
        const wydb::Database* pDb,
        const wydb::ElementId& pickedId,
        const wy::Vector2& pos,
        double scaleScreenX, double scaleScreenY,
        const std::set<wydb::ElementId>& excludeIds,
        Snap2dRuleHit& hit)
    {
        return false;
    }

protected:
    Snap2dRule();
    virtual ~Snap2dRule();
    Snap2dRule(const Snap2dRule&) = delete;
    Snap2dRule& operator=(const Snap2dRule&) = delete;
    Snap2dRule(Snap2dRule&&) = delete;
    Snap2dRule& operator=(Snap2dRule&&) = delete;
};

} // namespace wy3dApp

#endif // WY3DAPP_SNAP2D_RULE_H
