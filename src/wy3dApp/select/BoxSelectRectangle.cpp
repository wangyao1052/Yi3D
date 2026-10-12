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

#include "BoxSelectRectangle.h"
#include <osg/Geometry>
#include <osg/LineStipple>
#include <osg/LineWidth>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SelectBoxDrawCallback : public osg::Camera::DrawCallback
{
public:
    SelectBoxDrawCallback(BoxSelectRectangle* selBox)
        : osg::Camera::DrawCallback(), selBox_(selBox)
    {}

    virtual void operator () (osg::RenderInfo& renderInfo) const
    {
        selBox_->setProjectionMatrixAsOrtho2D(0.0, selBox_->getViewport()->width(),
            0.0, selBox_->getViewport()->height());
    }

private:
    osg::ref_ptr<BoxSelectRectangle> selBox_;
};

BoxSelectRectangle::BoxSelectRectangle(osg::View* view)
    : osg::Camera(), view_(view)
{
    //
    osg::Camera* mainCamera = view_->getCamera();
    osg::Viewport* viewport = mainCamera->getViewport();

    //
    this->setRenderOrder(osg::Camera::POST_RENDER);
    this->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    this->setAllowEventFocus(false);
    this->setClearColor(osg::Vec4(0.4f, 0.26f, 0.44f, 1.0f));
    this->setClearMask(GL_DEPTH_BUFFER_BIT);
    this->setViewport(viewport);
    this->setProjectionMatrixAsOrtho2D(0.0, viewport->width(), 0.0, viewport->height());
    this->setPreDrawCallback(new SelectBoxDrawCallback(this));

    //
    geom_ = new osg::Geometry();
    geom_->setDataVariance(osg::Object::DYNAMIC);
    geom_->setUseDisplayList(false);
    geom_->setUseVertexBufferObjects(true);
    {
        //
        vertices_ = new osg::Vec3Array(4);
        vertices_->at(0).set(pnt1_.x(), pnt1_.y(), 0.0f);
        vertices_->at(1).set(pnt2_.x(), pnt1_.y(), 0.0f);
        vertices_->at(2).set(pnt2_.x(), pnt2_.y(), 0.0f);
        vertices_->at(3).set(pnt1_.x(), pnt2_.y(), 0.0f);
        geom_->setVertexArray(vertices_.get());
        //
        osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array(1);
        normals->at(0).set(0.0f, 0.0f, 1.0f);
        geom_->setNormalArray(normals.get());
        geom_->setNormalBinding(osg::Geometry::BIND_OVERALL);
        //
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array(1);
        colors->at(0).set(0.0f, 0.0f, 0.0f, 1.0f);
        geom_->setColorArray(colors.get());
        geom_->setColorBinding(osg::Geometry::BIND_OVERALL);

        //
        geom_->addPrimitiveSet(new osg::DrawArrays(GL_LINE_LOOP, 0, 4));
        lineStipple_ = new osg::LineStipple(2, 0xF0F0);
        geom_->getOrCreateStateSet()->setAttributeAndModes(lineStipple_, osg::StateAttribute::OFF);
        geom_->getOrCreateStateSet()->setAttribute(new osg::LineWidth(1.6f));
    }
    geom_->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
}

void BoxSelectRectangle::show()
{
    if (0 == this->getNumChildren())
        this->addChild(geom_.get());
}

void BoxSelectRectangle::hide()
{
    this->removeChild(geom_.get());
}

void BoxSelectRectangle::update(const osg::Vec2d& pnt1, const osg::Vec2d& pnt2)
{
    pnt1_ = pnt1;
    pnt2_ = pnt2;

    bool lineStippleOn = pnt1_.x() > pnt2_.x();
    geom_->getOrCreateStateSet()->setAttributeAndModes(lineStipple_, lineStippleOn);

    vertices_->at(0).set(pnt1_.x(), pnt1_.y(), 0.0f);
    vertices_->at(1).set(pnt2_.x(), pnt1_.y(), 0.0f);
    vertices_->at(2).set(pnt2_.x(), pnt2_.y(), 0.0f);
    vertices_->at(3).set(pnt1_.x(), pnt2_.y(), 0.0f);
    vertices_->dirty();
    geom_->dirtyBound();
}

}
