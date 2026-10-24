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

#include "SketchRectangleDimensionOsgRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QStringLiteral>

#include <osg/BlendFunc>
#include <osg/Image>
#include <osg/TexEnv>
#include <osg/Vec4d>
#include <osg/Viewport>

#include <wyMath.h>
#include <wyVector3.h>

#include <wy3dApp/Application.h>
#include "application/ApplicationImpl.h"
#include "gizmo/element/sketchrectangle/SketchRectangleDimensionGizmo.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace {

// Drawn size of the label, in device pixels
constexpr double kLabelHeightPx = 20.0;
// Gap between the rectangle edge and the near side of the label box
constexpr double kLabelGapPx = 8.0;

// The label is drawn at a fixed size, so the texture is baked a little above it
constexpr int kLabelBakeHeightPx = 48;
constexpr int kLabelBakeMinWidthPx = 16;
constexpr int kLabelBakeMaxWidthPx = 512;
constexpr int kLabelMarginPx = 2;
constexpr int kLabelMeasureFontPx = 100;

// The repository ships no font file, so osgText could only fall back to a font
// without CJK glyphs -- the text is baked with QPainter instead, the same route
// the ViewCube labels take.
QFont labelFont(int pixelSize)
{
    QFont font(QStringLiteral("Microsoft YaHei"));
    font.setFamilies({ QStringLiteral("Microsoft YaHei"), QStringLiteral("SimSun") });
    font.setPixelSize(pixelSize);
    return font;
}

osg::Texture2D* createLabelTexture(const QString& text, double& outAspect)
{
    const QFontMetrics measureMetrics(labelFont(kLabelMeasureFontPx));
    const QRect ink = measureMetrics.tightBoundingRect(text);
    const double inkWidth = std::max(1, ink.width());
    const double inkHeight = std::max(1, ink.height());

    const double margin = kLabelMarginPx;
    const double scale = (kLabelBakeHeightPx - 2.0 * margin) / inkHeight;
    const int texHeight = kLabelBakeHeightPx;
    const int texWidth = std::clamp(
        static_cast<int>(std::lround(inkWidth * scale + 2.0 * margin)),
        kLabelBakeMinWidthPx, kLabelBakeMaxWidthPx);
    outAspect = static_cast<double>(texWidth) / texHeight;

    QImage image(texWidth, texHeight, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setFont(labelFont(std::max(8, static_cast<int>(std::lround(kLabelMeasureFontPx * scale)))));
        painter.setPen(QColor(0, 128, 179)); // OsgGizmoNode::SKETCH_ENTITY_COLOR
        painter.drawText(image.rect(), Qt::AlignCenter, text);
    }

    // QImage row 0 is the top row, osg::Image starts at the bottom left
    const QImage flipped = image.mirrored(false, true);
    const std::size_t bytes = static_cast<std::size_t>(flipped.sizeInBytes());

    osg::ref_ptr<osg::Image> pImage = new osg::Image;
    pImage->setImage(flipped.width(), flipped.height(), 1,
                     GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE,
                     new unsigned char[bytes], osg::Image::USE_NEW_DELETE);
    std::memcpy(pImage->data(), flipped.constBits(), bytes);

    osg::ref_ptr<osg::Texture2D> pTexture = new osg::Texture2D;
    pTexture->setImage(pImage.get());
    // Keep the baked size: osg's default power-of-two hint would rescale the image
    // before upload and blur the glyphs
    pTexture->setResizeNonPowerOfTwoHint(false);
    pTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
    pTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    pTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
    pTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
    pTexture->setUnRefImageDataAfterApply(true);
    return pTexture.release();
}

} // namespace

ScreenBillboardTransform::ScreenBillboardTransform(
    const ScreenBillboardTransform& rhs, const osg::CopyOp& copyop)
    : osg::Transform(rhs, copyop)
    , _anchor(rhs._anchor)
    , _pixelSize(rhs._pixelSize)
    , _pixelOffset(rhs._pixelOffset)
    , _matrix(rhs._matrix)
    , _worldRadius(rhs._worldRadius)
{
}

osg::Matrixd ScreenBillboardTransform::computeMatrix(const osg::CullStack& cullStack) const
{
    const osg::RefMatrix* pModelView = cullStack.getModelViewMatrix();
    const osg::RefMatrix* pProjection = cullStack.getProjectionMatrix();
    const osg::Viewport* pViewport = cullStack.getViewport();
    if (!pModelView || !pProjection || !pViewport ||
        0.0 >= pViewport->width() || 0.0 >= pViewport->height())
    {
        return osg::Matrixd();
    }

    osg::Vec3d translation;
    osg::Vec3d scale;
    osg::Quat rotation;
    osg::Quat scaleOrientation;
    pModelView->decompose(translation, rotation, scale, scaleOrientation);
    const osg::Quat billboard = rotation.inverse();

    // Device pixels per world unit at the anchor. CullStack::pixelSize() divides by
    // a perspective depth term that degenerates for the orthographic camera used by
    // the sketch views, so the projected length of a unit step is measured instead.
    const osg::Matrixd mvp = (*pModelView) * (*pProjection);
    auto toPixels = [&mvp, pViewport](const osg::Vec3d& point) -> osg::Vec2d
    {
        const osg::Vec4d clip = osg::Vec4d(point, 1.0) * mvp;
        const double w = std::fabs(clip.w()) > 1e-12 ? clip.w() : 1e-12;
        return osg::Vec2d(0.5 * clip.x() / w * pViewport->width(),
                          0.5 * clip.y() / w * pViewport->height());
    };
    const osg::Vec2d anchorPixels = toPixels(_anchor);
    const osg::Vec2d stepPixels =
        toPixels(_anchor + billboard * osg::Vec3d(1.0, 0.0, 0.0));
    const double pixelsPerUnit = (stepPixels - anchorPixels).length();
    if (1e-12 >= pixelsPerUnit)
    {
        return osg::Matrixd();
    }
    const double worldPerPixel = 1.0 / pixelsPerUnit;

    const double halfWidth = 0.5 * _pixelSize.x() * worldPerPixel;
    const double halfHeight = 0.5 * _pixelSize.y() * worldPerPixel;
    const double worldRadius = std::sqrt(halfWidth * halfWidth + halfHeight * halfHeight)
        + osg::Vec3d(_pixelOffset.x() * worldPerPixel,
                     _pixelOffset.y() * worldPerPixel, 0.0).length();
    if (std::fabs(worldRadius - _worldRadius) > 1e-6 + 0.02 * std::max(worldRadius, _worldRadius))
    {
        _worldRadius = worldRadius;
        const_cast<ScreenBillboardTransform*>(this)->dirtyBound();
    }

    const osg::Vec3d center = _anchor + billboard * osg::Vec3d(
        _pixelOffset.x() * worldPerPixel, _pixelOffset.y() * worldPerPixel, 0.0);

    // Row-vector convention, same order as AutoTransform::computeMatrix: the
    // vertices are scaled, then billboarded, then moved onto the anchor
    osg::Matrixd matrix;
    matrix.makeRotate(billboard);
    matrix.postMultTranslate(center);
    matrix.preMultScale(osg::Vec3d(
        _pixelSize.x() * worldPerPixel, _pixelSize.y() * worldPerPixel, 1.0));
    return matrix;
}

bool ScreenBillboardTransform::computeLocalToWorldMatrix(
    osg::Matrix& matrix, osg::NodeVisitor* nv) const
{
    const osg::CullStack* pCullStack = nv ? nv->asCullStack() : nullptr;
    if (pCullStack)
    {
        _matrix = this->computeMatrix(*pCullStack);
    }
    matrix.preMult(_matrix);
    return true;
}

bool ScreenBillboardTransform::computeWorldToLocalMatrix(
    osg::Matrix& matrix, osg::NodeVisitor*) const
{
    matrix.postMult(osg::Matrixd::inverse(_matrix));
    return true;
}

osg::BoundingSphere ScreenBillboardTransform::computeBound() const
{
    // Centre in this node's parent frame, radius as last measured at cull time
    return osg::BoundingSphere(_anchor, std::max(_worldRadius, wy::EPS));
}

SketchRectangleDimensionOsgRenderer::SketchRectangleDimensionOsgRenderer(BaseGizmo* pGizmo)
    : OsgGizmoRenderer(pGizmo)
{
    _pBillboard = new ScreenBillboardTransform();
    _pQuad = this->createQuadGeometry();
    _pBillboard->addChild(_pQuad.get());
    this->getOsgNode()->addChild(_pBillboard.get());
}

SketchRectangleDimensionOsgRenderer::~SketchRectangleDimensionOsgRenderer()
{
}

osg::Geometry* SketchRectangleDimensionOsgRenderer::createQuadGeometry()
{
    osg::ref_ptr<osg::Geometry> pGeom = new osg::Geometry;
    pGeom->setUseDisplayList(false);
    pGeom->setUseVertexBufferObjects(true);

    osg::ref_ptr<osg::Vec3Array> pVertices = new osg::Vec3Array;
    pVertices->push_back(osg::Vec3(-0.5f, -0.5f, 0.0f));
    pVertices->push_back(osg::Vec3(0.5f, -0.5f, 0.0f));
    pVertices->push_back(osg::Vec3(0.5f, 0.5f, 0.0f));
    pVertices->push_back(osg::Vec3(-0.5f, 0.5f, 0.0f));
    pGeom->setVertexArray(pVertices.get());

    osg::ref_ptr<osg::Vec2Array> pTexCoords = new osg::Vec2Array;
    pTexCoords->push_back(osg::Vec2(0.0f, 0.0f));
    pTexCoords->push_back(osg::Vec2(1.0f, 0.0f));
    pTexCoords->push_back(osg::Vec2(1.0f, 1.0f));
    pTexCoords->push_back(osg::Vec2(0.0f, 1.0f));
    pGeom->setTexCoordArray(0, pTexCoords.get());

    pGeom->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLE_FAN, 0, 4));

    osg::StateSet* pStateSet = pGeom->getOrCreateStateSet();
    // The fragment colour comes straight from the texture and is blended by its alpha
    pStateSet->setTextureAttribute(0, new osg::TexEnv(osg::TexEnv::REPLACE));
    pStateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    pStateSet->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
                                    osg::StateAttribute::ON);
    return pGeom.release();
}

void SketchRectangleDimensionOsgRenderer::setLabelText(const QString& text)
{
    double aspect = 1.0;
    osg::ref_ptr<osg::Texture2D> pTexture = createLabelTexture(text, aspect);
    if (!pTexture)
    {
        return;
    }

    _text = text;
    _aspect = aspect;
    _pTexture = pTexture;
    _pQuad->getOrCreateStateSet()->setTextureAttributeAndModes(
        0, _pTexture.get(), osg::StateAttribute::ON);
}

void SketchRectangleDimensionOsgRenderer::refresh()
{
    auto* pGizmo = static_cast<SketchRectangleDimensionGizmo*>(_pGizmo);
    if (!pGizmo || !_pBillboard)
    {
        return;
    }

    const SketchRectangleInfo& rect = pGizmo->getRectangleInfo();

    // Live geometry first; the snapshot is only a fallback for a transient failure
    std::array<wy::Vector2, 4> corners = rect.corners;
    const wydb::Database* pDb = ApplicationImpl::instance().getActiveDatabase();
    std::array<wy::Vector2, 4> liveCorners{};
    if (pDb && readSketchRectangleCorners(pDb, pGizmo->getLineIds(), liveCorners))
    {
        corners = liveCorners;
    }

    const RectangleDimensionLabel label =
        computeRectangleDimensionLabel(corners, pGizmo->getDimension());

    const QString text = QStringLiteral("%1=%2")
        .arg(pGizmo->getDimensionName())
        .arg(QString::number(label.value, 'f', 2));
    if (text != _text)
    {
        this->setLabelText(text);
    }

    const wy::Vector3 worldPos = pGizmo->getSketchPlane().value(label.edgeMid);
    _pBillboard->setAnchor(osg::Vec3d(worldPos.x(), worldPos.y(), worldPos.z()));

    const double widthPx = kLabelHeightPx * _aspect;
    const osg::Vec2d pixelOffset = SketchRectangleDimension::Width == pGizmo->getDimension()
        ? osg::Vec2d(0.0, -(kLabelGapPx + 0.5 * kLabelHeightPx))
        : osg::Vec2d(kLabelGapPx + 0.5 * widthPx, 0.0);
    _pBillboard->setPixelOffset(pixelOffset);
    _pBillboard->setPixelSize(osg::Vec2d(widthPx, kLabelHeightPx));
}

} // namespace wy3dApp
