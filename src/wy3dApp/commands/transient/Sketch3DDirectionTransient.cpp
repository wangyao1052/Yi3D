///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include "Sketch3DDirectionTransient.h"

#include <osg/AutoTransform>
#include <osg/Math>
#include <osg/PositionAttitudeTransform>
#include <osg/Quat>
#include <osg/Shape>
#include <osg/ShapeDrawable>
#include <osg/StateSet>

#include <wy3dMath.h>

#include "scene/Colors.h"
#include "scene/RenderConst.h"

namespace
{

// AutoTransform's auto scale multiplies the whole subtree by (the world size of one pixel at the
// anchor) / 0.48, so anything built of N local units comes out N / 0.48 pixels across, the same
// number of pixels whatever the model is measured in and however far away it is. The sizes below
// are therefore written in pixels and converted at that one rate, which is what makes them
// readable and directly tweakable.
const double kLocalUnitsPerPixel = 0.48;

const double kArmPixels = 83.0;        // one arm, root to tip
const double kShaftRadiusPixels = 7.0;
const double kHeadRadiusPixels = 16.0;
const double kHeadLengthPixels = 24.0;

const double kArmLength = kArmPixels * kLocalUnitsPerPixel;
const double kShaftRadius = kShaftRadiusPixels * kLocalUnitsPerPixel;
const double kHeadRadius = kHeadRadiusPixels * kLocalUnitsPerPixel;
const double kHeadLength = kHeadLengthPixels * kLocalUnitsPerPixel;
const double kShaftLength = kArmLength - kHeadLength;

} // namespace

Sketch3DDirectionTransient::Sketch3DDirectionTransient(const wy::Vector3& origin,
    const wy::Vector3& direction, bool bidirectional)
    : GuiCmdTransient()
{
    if (direction.length() < wy::EPS) return;

    const wy::Vector3 dir = direction.normalized();

    osg::ref_ptr<osg::AutoTransform> pScaler = new osg::AutoTransform();
    pScaler->setAutoRotateMode(osg::AutoTransform::NO_ROTATION);
    pScaler->setAutoScaleToScreen(true);
    pScaler->setPosition(osg::Vec3d(origin.x(), origin.y(), origin.z()));
    // Nothing here is pickable: PointPick would otherwise find the arrow instead of the face
    // under it.
    pScaler->setNodeMask(~PICK_MASK);

    // The preview root switches lighting off, which suits the lines every other preview is drawn
    // with but leaves these solids as flat silhouettes. Lighting is turned back on here, for the
    // arrow alone, together with the two modes a solid under this node needs:
    //
    //   normalize   the auto scale is part of the modelview, so without it the normals - and with
    //               them the shading - would be scaled by the zoom.
    //   cull face   depth testing is off here, so the far side of the shaft would otherwise be
    //               painted over the near side. osg tessellates the shapes as front faces only,
    //               which is to say it expects culling to be on.
    osg::StateSet* pStates = pScaler->getOrCreateStateSet();
    pStates->setMode(GL_LIGHTING, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
    pStates->setMode(GL_NORMALIZE, osg::StateAttribute::ON);
    pStates->setMode(GL_CULL_FACE, osg::StateAttribute::ON);

    // The arrow is built along +Z and the whole of it is then turned onto the direction, so the
    // head is shaped once, upright, instead of being rebuilt for every direction.
    osg::ref_ptr<osg::PositionAttitudeTransform> pAxis = new osg::PositionAttitudeTransform();
    osg::Quat attitude;
    attitude.makeRotate(osg::Vec3d(0.0, 0.0, 1.0), osg::Vec3d(dir.x(), dir.y(), dir.z()));
    pAxis->setAttitude(attitude);
    pScaler->addChild(pAxis.get());

    // A cone is placed by its centre, and that centre sits a quarter of the height above its
    // base, so this lands the base on the end of the shaft and leaves the tip pointing outward.
    // Each end gets a head of its own: a node cannot have two parents.
    const auto makeHead = [](bool flipped) -> osg::ref_ptr<osg::Node>
    {
        osg::ref_ptr<osg::Cone> pCone = new osg::Cone(osg::Vec3(0.0, 0.0, 0.0), kHeadRadius, kHeadLength);
        pCone->setCenter(osg::Vec3(0.0, 0.0, -pCone->getBaseOffset() + kShaftLength));

        osg::ref_ptr<osg::ShapeDrawable> pDrawable = new osg::ShapeDrawable(pCone.get());
        pDrawable->setColor(Colors::kDirectionArrow);
        if (!flipped) return pDrawable;

        // Half a turn about X puts the same head on the other side, pointing the other way.
        osg::ref_ptr<osg::PositionAttitudeTransform> pPat = new osg::PositionAttitudeTransform();
        pPat->setAttitude(osg::Quat(osg::PI, osg::Vec3d(1.0, 0.0, 0.0)));
        pPat->addChild(pDrawable.get());
        return pPat;
    };

    // One arm reaches from the root to the tip; a bidirectional arrow is the same bar grown
    // through the root, with a head on each end of it.
    const double shaftLength = bidirectional ? kShaftLength * 2.0 : kShaftLength;
    const double shaftCenterZ = bidirectional ? 0.0 : kShaftLength / 2.0;

    osg::ref_ptr<osg::Cylinder> pShaft =
        new osg::Cylinder(osg::Vec3(0.0, 0.0, shaftCenterZ), kShaftRadius, shaftLength);
    osg::ref_ptr<osg::ShapeDrawable> pShaftDrawable = new osg::ShapeDrawable(pShaft.get());
    pShaftDrawable->setColor(Colors::kDirectionArrow);
    pAxis->addChild(pShaftDrawable.get());

    pAxis->addChild(makeHead(false).get());
    if (bidirectional) pAxis->addChild(makeHead(true).get());

    _root->addChild(pScaler.get());
}

Sketch3DDirectionTransient::~Sketch3DDirectionTransient()
{
}
