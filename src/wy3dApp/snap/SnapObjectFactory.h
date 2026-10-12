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

#ifndef WY3DAPP_SNAP_OBJECT_FACTORY_H
#define WY3DAPP_SNAP_OBJECT_FACTORY_H

#include <string>
#include <list>
#include <map>
#include <memory>
#include "SnapSystemBase.h"

#include "elements/ElemSnapObjectCreator.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// 元素捕捉对象工厂
class SnapObjectFactory : public wyap::SnapObjectFactoryIF
{
public:
    SnapObjectFactory();

    // 创建元素的捕捉对象
    virtual std::list<wyap::SnapObjectSPtr> createSnapObjects(const wydb::Element* pElem) override;

private:
    std::map<std::string, std::unique_ptr<ElemSnapObjectCreator>> _className2SnapObjCreator;
};

} // namespace wy3dApp

#endif // WY3DAPP_SNAP_OBJECT_FACTORY_H
