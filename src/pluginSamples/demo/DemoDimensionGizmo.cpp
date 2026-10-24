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

#include "DemoDimensionGizmo.h"

#include <cmath>
#include <memory>

#include <QApplication>
#include <QCoreApplication>
#include <QInputDialog>
#include <QTimer>
#include <QWidget>

#include <osg/Array>
#include <osg/Geometry>
#include <osg/StateSet>

#include <wydbDatabase.h>
#include <wydbElement.h>
#include <wydbParameter.h>
#include <wydbTransaction.h>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dPlane.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>

#include <wyapApplication.h>

#include <wy3dApp/GuiCommand.h> // MouseEvent
#include <wy3dApp/OsgGizmoNode.h>
#include <wy3dApp/OsgGizmoRenderer.h>

namespace {

constexpr float kClickTolerancePx = 3.0f;

// Reads the line and its sketch plane; false when either is gone.
bool readLine(const wydb::ElementId& lineId,
              const wy3d::SketchLine*& pLine,
              const wy3d::Sketch*& pSketch)
{
    pLine = nullptr;
    pSketch = nullptr;

    const wydb::Database* pDb = wyap::getApplication()->getActiveDatabase();
    if (!pDb)
    {
        return false;
    }

    pLine = wy3d::SketchLine::cast(pDb->getElement(lineId));
    if (!pLine)
    {
        return false;
    }

    pSketch = wy3d::Sketch::cast(pDb->getElement(pLine->getParent()));
    return pSketch && pSketch->getPlane().isValid();
}

osg::Vec3 toOsg(const wy::Vector3& point)
{
    return osg::Vec3(static_cast<float>(point.x()),
                     static_cast<float>(point.y()),
                     static_cast<float>(point.z()));
}

// Draws a dimension line offset from the sketch line plus two extension lines.
// Everything is placed in world coordinates through the sketch plane, since the
// gizmo node itself carries no transform.
class DemoLineDimensionRenderer : public wy3dApp::OsgGizmoRenderer
{
public:
    explicit DemoLineDimensionRenderer(wy3dApp::BaseGizmo* pGizmo)
        : wy3dApp::OsgGizmoRenderer(pGizmo)
    {
        _pVertices = new osg::Vec3Array();
        _pVertices->resize(6);

        _pColors = new osg::Vec4Array();
        _pColors->push_back(wy3dApp::OsgGizmoNode::YELLOW_COLOR);

        _pGeometry = new osg::Geometry();
        _pGeometry->setUseDisplayList(false);
        _pGeometry->setUseVertexBufferObjects(true);
        _pGeometry->setVertexArray(_pVertices.get());
        _pGeometry->setColorArray(_pColors.get(), osg::Array::BIND_OVERALL);
        _pGeometry->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, 6));
        _pGeometry->getOrCreateStateSet()->setMode(
            GL_LIGHTING, osg::StateAttribute::OFF);

        this->getOsgNode()->addChild(_pGeometry.get());
    }

    virtual void refresh() override
    {
        const wy3d::SketchLine* pLine = nullptr;
        const wy3d::Sketch* pSketch = nullptr;
        if (!readLine(_pGizmo->getFirstElementId(), pLine, pSketch))
        {
            this->getOsgNode()->setNodeMask(0u);
            return;
        }

        const wy::Vector2 start = pLine->getStartPoint();
        const wy::Vector2 end = pLine->getEndPoint();
        wy::Vector2 direction = end - start;
        const double length = direction.length();
        if (length < 1e-9)
        {
            this->getOsgNode()->setNodeMask(0u);
            return;
        }

        direction = direction / length;
        // Offset the dimension to the left of the line in sketch UV space.
        const wy::Vector2 normal(-direction.y(), direction.x());
        const double offset = 0.2 * length;
        const wy::Vector2 dimStart = start + normal * offset;
        const wy::Vector2 dimEnd = end + normal * offset;

        const wy3d::Plane& plane = pSketch->getPlane();
        (*_pVertices)[0] = toOsg(plane.value(start));
        (*_pVertices)[1] = toOsg(plane.value(dimStart));
        (*_pVertices)[2] = toOsg(plane.value(dimStart));
        (*_pVertices)[3] = toOsg(plane.value(dimEnd));
        (*_pVertices)[4] = toOsg(plane.value(dimEnd));
        (*_pVertices)[5] = toOsg(plane.value(end));
        _pVertices->dirty();

        this->getOsgNode()->setNodeMask(~0u);
        this->getOsgNode()->dirtyBound();
    }

    virtual void onActivate() override
    {
        _pColors->front() = wy3dApp::OsgGizmoNode::RED_COLOR;
        _pColors->dirty();
    }

    virtual void onDeactivate() override
    {
        _pColors->front() = wy3dApp::OsgGizmoNode::YELLOW_COLOR;
        _pColors->dirty();
    }

    // Click-only: the demo dimension never drags.
    virtual bool onBeginDrag(const wy::Vector3&) override { return false; }
    virtual bool onDragging(const wy::Vector3&) override { return false; }
    virtual void onEndDrag(const wy::Vector3&) override {}
    virtual void onCancelDrag() override {}

private:
    osg::ref_ptr<osg::Geometry> _pGeometry;
    osg::ref_ptr<osg::Vec3Array> _pVertices;
    osg::ref_ptr<osg::Vec4Array> _pColors;
};

void editLineLength(const wydb::ElementId& lineId)
{
    const wy3d::SketchLine* pLine = nullptr;
    const wy3d::Sketch* pSketch = nullptr;
    if (!readLine(lineId, pLine, pSketch))
    {
        return;
    }

    bool ok = false;
    const double newLength = QInputDialog::getDouble(
        QApplication::activeWindow(),
        QCoreApplication::translate("DemoDimensionGizmo", "Edit Sketch Line"),
        QCoreApplication::translate("DemoDimensionGizmo", "Length"),
        pLine->getLength(), 0.001, 1.0e9, 3, &ok);
    if (!ok || !std::isfinite(newLength) || newLength <= 0.0)
    {
        return;
    }

    wydb::Database* pDb = wyap::getApplication()->getActiveDatabase();
    if (!pDb)
    {
        return;
    }

    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    if (!pTransMgr)
    {
        return;
    }

    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans)
    {
        return;
    }

    wydb::Element* pElem = pTrans->getElementForWrite(lineId);
    if (!pElem)
    {
        pTransMgr->abortTransaction();
        return;
    }

    const wy::ErrorStatus status = pElem->setParameterValue(
        wy3d::SketchLine::className(), wy3d::SketchLine::PARAM_LENGTH,
        *wydb::ParameterValue::createDouble(newLength));
    if (wy::ErrorStatus::Ok != status)
    {
        pTransMgr->abortTransaction();
        return;
    }

    pTransMgr->endTransaction();
}

} // namespace

DemoLineDimensionGizmo::DemoLineDimensionGizmo(const wydb::ElementId& lineId)
    : wy3dApp::BaseGizmo(lineId)
{
    _pRenderer = std::make_unique<DemoLineDimensionRenderer>(this);
}

DemoLineDimensionGizmo::~DemoLineDimensionGizmo()
{
}

void DemoLineDimensionGizmo::onLeftMouseDown(const wy3dApp::MouseEvent& event)
{
    _mouseDownX = event.x;
    _mouseDownY = event.y;
    _isMouseDown = true;
}

void DemoLineDimensionGizmo::onLeftMouseUp(const wy3dApp::MouseEvent& event)
{
    if (!_isMouseDown)
    {
        return;
    }
    _isMouseDown = false;

    if (std::fabs(event.x - _mouseDownX) > kClickTolerancePx ||
        std::fabs(event.y - _mouseDownY) > kClickTolerancePx)
    {
        return;
    }

    // Defer the modal editor until the OSG event callback has returned.
    const wydb::ElementId lineId = this->getFirstElementId();
    QTimer::singleShot(0, QCoreApplication::instance(), [lineId]() {
        editLineLength(lineId);
    });
}

bool DemoDimensionCreator::createGizmos(const wyap::SelectionSet& sels,
                                        std::list<wyap::GizmoSPtr>& gizmos) const
{
    if (1 != sels.getCount())
    {
        return false;
    }

    auto iter = sels.createIterator();
    if (iter.isDone())
    {
        return false;
    }

    const wyap::Selection& sel = iter.current();
    if (wyap::Selection::kWholeElement != sel.getSelectionType())
    {
        return false;
    }

    const wydb::Database* pDb = wyap::getApplication()->getActiveDatabase();
    if (!pDb)
    {
        return false;
    }

    const wy3d::SketchLine* pLine =
        wy3d::SketchLine::cast(pDb->getElement(sel.getElementId()));
    if (!pLine)
    {
        return false;
    }

    const wy3d::Sketch* pSketch =
        wy3d::Sketch::cast(pDb->getElement(pLine->getParent()));
    if (!pSketch || !pSketch->getPlane().isValid())
    {
        return false;
    }

    gizmos.emplace_back(std::make_shared<DemoLineDimensionGizmo>(pLine->getId()));
    return true;
}
