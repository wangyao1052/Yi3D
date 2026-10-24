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

#ifndef WY3DAPP_SKETCH_DIMENSION_OSG_RENDERER_H
#define WY3DAPP_SKETCH_DIMENSION_OSG_RENDERER_H

#include <osg/BoundingSphere>
#include <osg/CullStack>
#include <osg/Geometry>
#include <osg/Matrixd>
#include <osg/Texture2D>
#include <osg/Transform>
#include <osg/Vec2d>
#include <osg/Vec3d>

#include <QString>

#include <wy3dApp/OsgGizmoRenderer.h>
#include "utils/SketchDimensionLabelMath.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

// Billboard that keeps a fixed on-screen size and faces the camera. Unlike
// osg::AutoTransform the matrix built at cull time is cached here: the
// IntersectionVisitor is not a CullStack, so it cannot rebuild the auto scale --
// without the cache the pick box would drift away from the drawn text as the
// view is zoomed.
class ScreenBillboardTransform : public osg::Transform
{
public:
    ScreenBillboardTransform() = default;
    ScreenBillboardTransform(const ScreenBillboardTransform& rhs,
                             const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);
    META_Node(wy3dApp, ScreenBillboardTransform);

    // Anchor in the parent's frame (world: the gizmos group has no transform)
    void setAnchor(const osg::Vec3d& anchor) { _anchor = anchor; dirtyBound(); }
    // On-screen size of the quad, in device pixels
    void setPixelSize(const osg::Vec2d& pixelSize) { _pixelSize = pixelSize; dirtyBound(); }
    // Where the label sits relative to the anchor
    void setPlacement(SketchDimensionPlacement placement)
    {
        _placement = placement;
        dirtyBound();
    }
    // World-space direction at the anchor; only used by the Radial placement
    void setRadialDirection(const osg::Vec3d& direction)
    {
        _radialDirection = direction;
        dirtyBound();
    }
    // Gap between the anchor and the near edge of the label box, in pixels
    void setGapPixels(double gapPx) { _gapPx = gapPx; dirtyBound(); }

    virtual bool computeLocalToWorldMatrix(osg::Matrix& matrix, osg::NodeVisitor* nv) const override;
    virtual bool computeWorldToLocalMatrix(osg::Matrix& matrix, osg::NodeVisitor*) const override;

    // The billboard matrix is only known during the cull traversal, so the bound
    // follows the last culled frame. Without it the base Transform bound stays a
    // unit sphere at the parent origin, and both frustum culling and the polytope
    // pick of the drawn text would test the wrong place.
    virtual osg::BoundingSphere computeBound() const override;

private:
    osg::Matrixd computeMatrix(const osg::CullStack& cullStack) const;

private:
    osg::Vec3d _anchor;
    osg::Vec2d _pixelSize = osg::Vec2d(1.0, 1.0);
    SketchDimensionPlacement _placement = SketchDimensionPlacement::Below;
    osg::Vec3d _radialDirection;
    double _gapPx = 0.0;
    mutable osg::Matrixd _matrix;
    mutable double _worldRadius = 0.0;
};

// Draws a dimension text as a QPainter-baked texture on a screen-sized quad
class SketchDimensionOsgRenderer : public OsgGizmoRenderer
{
public:
    explicit SketchDimensionOsgRenderer(BaseGizmo* pGizmo);
    virtual ~SketchDimensionOsgRenderer();

    virtual void refresh() override;
    virtual void onActivate() override {}
    virtual void onDeactivate() override {}

    // Click-only: it never drags
    virtual bool onBeginDrag(const wy::Vector3&) override { return false; }
    virtual bool onDragging(const wy::Vector3&) override { return false; }
    virtual void onEndDrag(const wy::Vector3&) override {}
    virtual void onCancelDrag() override {}

private:
    osg::Geometry* createQuadGeometry();
    void setLabelText(const QString& text);
    void setLabelVisible(bool visible);

private:
    osg::ref_ptr<ScreenBillboardTransform> _pBillboard;
    osg::ref_ptr<osg::Geometry> _pQuad;
    osg::ref_ptr<osg::Texture2D> _pTexture;
    QString _text;
    double _aspect = 1.0;
    bool _isVisible = true;
};

} // namespace wy3dApp

#endif // WY3DAPP_SKETCH_DIMENSION_OSG_RENDERER_H
