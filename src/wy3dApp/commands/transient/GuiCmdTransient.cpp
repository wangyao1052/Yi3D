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

#include <wy3dApp/GuiCmdTransient.h>
#include <osg/StateSet>
#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include <wy3dApp/Scene.h>
#include "scene/RenderConst.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

GuiCmdTransient::GuiCmdTransient()
{
    _root = new osg::Group();
    // 关闭光照
    _root->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    // 关闭深度测试
    _root->getOrCreateStateSet()->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    // 后绘制
    _root->getOrCreateStateSet()->setRenderBinDetails(RenderBinNumers::GuiTransientObject, "RenderBin");

    // 添加到当前场景中
    Scene* pActiveScene = ApplicationImpl::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->addTransient(_root.get());
    }
}

GuiCmdTransient::~GuiCmdTransient()
{
    // 从当前场景中移除
    Scene* pActiveScene = ApplicationImpl::instance().getActiveScene();
    if (pActiveScene)
    {
        pActiveScene->removeTransient(_root.get());
    }
}

} // namespace wy3dApp
