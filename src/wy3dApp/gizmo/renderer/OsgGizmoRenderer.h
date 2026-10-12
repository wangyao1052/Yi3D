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

#ifndef WY3DAPP_OSG_GIZMO_RENDERER_H
#define WY3DAPP_OSG_GIZMO_RENDERER_H

#include <vector>
#include <osg/Node>
#include <osg/observer_ptr>
#include <osgViewer/View>

#include "GizmoRenderer.h"
#include "gizmo/OsgGizmoNode.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class BaseGizmo;
class ElementNode;

class OsgGizmoRenderer : public GizmoRenderer
{
public:
    explicit OsgGizmoRenderer(BaseGizmo* pGizmo);
    virtual ~OsgGizmoRenderer() {}

    OsgGizmoNode* getOsgNode() const { return _osgNode.get(); }

    void setOsgView(osgViewer::View* pView) { _pOsgView = pView; }
    osgViewer::View* getOsgView() const { return _pOsgView.get(); }

    virtual void attachToScene(Scene* pScene) override;
    virtual void detachFromScene(Scene* pScene) override;

protected:
    osg::observer_ptr<osgViewer::View> _pOsgView;
    osg::ref_ptr<OsgGizmoNode> _osgNode;
};

}

#endif // WY3DAPP_OSG_GIZMO_RENDERER_H
