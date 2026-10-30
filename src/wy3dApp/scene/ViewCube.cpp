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

#include "ViewCube.h"
#include <wy3dApp/ElementNodeType.h>

#include "view/CameraManipulator3d.h"
#include "view/ViewUtil.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include <osg/BlendFunc>
#include <osg/GraphicsContext>
#include <osg/Group>
#include <osg/Image>
#include <osg/LineWidth>
#include <osg/PolygonOffset>
#include <osg/PrimitiveSet>
#include <osg/TexEnv>
#include <osg/Texture2D>

#include <osgViewer/View>

#include <QCoreApplication>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QString>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

namespace
{
    // Side length of the cube's own square, in device pixels. The whole widget scales with it
    // -- the fixed orthographic projection maps this square to 2 eye units -- so this is the
    // knob for the cube's overall size. The overlay viewport is wider by the home strip.
    const double kCubeSize = 142.0;
    // Gap to the viewport's right and top edges, in device pixels
    const double kMargin = 16.0;
    // The home button: a square glyph left of the cube, its top edge flush with the cube's top
    // edge, with a small gap between them. The overlay viewport is wider than the cube by this
    // strip, so the cube keeps its own square on the right of it and its rectangle on screen
    // does not move.
    const double kHomeGap = 4.0;
    // Icon size in device pixels; the strip width, the overlay width and every eye-space
    // coordinate follow from it.
    const double kHomeButton = 30.0;
    const double kHomeStrip = kHomeGap + kHomeButton;
    const double kOverlayWidth = kCubeSize + kHomeStrip;
    // Overlay eye units per device pixel. The cube square is kCubeSize across and the
    // projection spans [-1, 1] across it, so this is the same on both axes even though the
    // viewport is wider than it is tall: the cube's pixels stay square and its size does not
    // change.
    const double kEyePerPixel = 2.0 / kCubeSize;
    // Projection left: the right kCubeSize pixels are the cube's [-1, 1] square, the strip to
    // their left takes the rest of the eye range. Without this the projection would still be
    // square while the viewport is not, and the cube would stretch horizontally.
    const double kOverlayLeft = -1.0 - kHomeStrip * kEyePerPixel;
    // The home glyph is baked at twice its drawn size, like the labels. The quad is screen
    // aligned and always minified by exactly 2, so LINEAR without mipmaps is an exact 2x2
    // box filter; a mip filter would only blend in a softer level for no gain.
    const double kHomeBakeScale = 2.0;
    // Rounding radius (cube half-size is 1). The body is the Minkowski sum of the inner box
    // [-(1-r), 1-r]^3 with a ball of radius r, so the faces stay flat, the edges are
    // cylindrical arcs and the corners spherical patches, all tangent continuous. This is
    // what makes the cube read as rounded: at 0.4 the flat middle is only 0.6 of the half
    // size and the rounding band, seen from an isometric view, is a wide soft frame.
    const double kRounding = 0.40;
    // Cells across one quarter arc of the rounding. A face axis needs 2*this+1 cells: the
    // flat middle is a single one. All six faces share the same cell boundaries, so
    // neighbouring faces meet exactly on their common edge.
    const int kRoundingSegments = 5;
    // Bounding sphere as a fraction of the half viewport, with slack so that no
    // orientation pushes the cube outside
    const double kFitRatio = 0.92;
    // The farthest point of the body is a corner: (1-r)*sqrt(3) + r
    const double kScale = kFitRatio / ((1.0 - kRounding) * std::sqrt(3.0) + kRounding);
    // Label textures: the width follows the text aspect, the height is a cap, and the margin
    // and the width cap keep the glyph inside the texture. The height is only a cap: each
    // label is baked at the size it is drawn at (see kLabelBakeScale), so a long Latin word
    // gets a small texture rather than a 96 px one minified into a loss.
    const int kLabelTexSize = 96;
    const int kLabelTexMinSize = 16;
    // The margin is a fraction of the texture height, not a pixel count (kLabelTexSize is the
    // reference), and it is not floored: at the sizes these labels are baked at the fraction
    // lands under one texel, and a one-texel floor used to eat it whole -- 3% of the glyph
    // height spent on a border the ink never reached anyway.
    const int kLabelMargin = 2;
    const int kLabelTexMaxWidth = 384;
    // The label texture is baked this many times taller than the label is drawn, so the GPU
    // only ever minifies it by this much (mip level 1). A fixed 96 px bake was 8x to 12x
    // oversized for "Front"/"Bottom", and GL_LINEAR without mipmaps then dropped their thin
    // strokes -- dashes on the plate instead of letters.
    const double kLabelBakeScale = 2.0;
    // Label box, in cube-local units: a label is at most 2*kLabelHalfLong wide and exactly
    // 2*kLabelHalfShort tall -- one size on every face, the way SolidWorks' cube reads. The
    // shared height is the largest that fits the widest label (kLabelHalfLong / widest
    // aspect, capped by kLabelHalfShort), so "BOTTOM" spans the plate while "TOP" sits
    // narrower instead of each word filling its own card. The rule used to pin the long edge
    // per label, which left a long word small in both directions: "Bottom" came out 7.8 px
    // tall against 32 px for a CJK glyph. The box grew with the plate (0.55 x 0.45 on a
    // 0.575 plate) and kept the same share of it (0.9565) until it was finally pushed out to
    // the plate's own width, which is as large as the text can read.
    //
    // The box has to sit inside the plate: the plate's straight band runs out to
    // kPlateHalf - kPlateCorner = 0.52, and the corner of the label box is inside the
    // rounded rectangle exactly when
    // sqrt(max(0, u - 0.52)^2 + max(0, v - 0.52)^2) <= kPlateCorner. The half height never
    // reaches 0.52, so its term is always zero and the width cap is the plain kPlateHalf:
    // 0.70 is exactly that cap, so the box's corner lands on the plate's edge and the ink
    // stops a hair short of it -- the card carries no padding left or right.
    const double kLabelHalfLong = 0.70;
    const double kLabelHalfShort = 0.50;
    // The label face: a condensed sans, the way SolidWorks' cube reads but larger. Because the
    // shared height is kLabelHalfLong / widest aspect, the width of "BOTTOM" sets the size of
    // every label, so a narrower face is a *bigger* label: aspect 3.49 here against 4.80 for
    // Arial Narrow Bold, which is 16.0 px of ink against 11.6 px (Segoe UI Bold is 5.97 and
    // 9.3 px -- a wider face is a smaller label, never a bigger one). Bahnschrift ships with
    // Windows 10/11 only; elsewhere Qt substitutes a default face and the label simply lands
    // smaller, because the aspect is measured from the resolved font below.
    const char kLabelFontFamily[] = "Bahnschrift SemiBold Condensed";
    // The CJK face, pinned rather than left to the platform's fallback: Bahnschrift carries no
    // Han glyphs, and what Qt picks instead is not ours to choose -- measured 2026-09-27, every
    // name that does not exist on this machine (and the generic "serif"/"sans-serif") resolves
    // to SimSun through the Windows font-link table, while a Linux box would answer with
    // whatever its fontconfig default is. SimSun is Windows-only; on a system without it the
    // list simply falls through to the platform default again. It carries no bold face, so the
    // label's Bold comes from Qt's synthesis -- that is the look the cube has had all along.
    // A bundled OFL Song face (Source Han Serif / Noto Serif SC) is the cross-platform route.
    const char kLabelCjkFamily[] = "SimSun";
    // How far a label is pushed along its normal, to keep it off its own face
    const double kLabelOffset = 0.012;
    // Distance from the origin to the picking ray's start plane; it has to exceed the
    // cube's largest half extent, (1-r)*sqrt(3) + r = 1.44
    const double kRayFar = 4.0;

    // Half-length of the label plate drawn on each face and the radius of its corners, in
    // cube-local units. The plate is a flat card, so one of its points floats above the body
    // by kPlateOffset + kRounding - sqrt(kRounding^2 - d^2), where d is the distance from
    // that point to the flat middle of the face, the square [-(1-r), 1-r]^2 -- the drop
    // follows the distance to that square, measured per axis, not the distance from the face
    // centre, so the plate's corners are the low end of it rather than its high end. At
    // kPlateHalf 0.70 the worst points are the middle of each straight edge and the ends of
    // the corner arcs, where d = 0.70 - 0.60 = 0.10 and the float is 0.019 units, 0.85 px at
    // kCubeSize 142; the corners float 0.53 px. A pixel is the budget: d <= 0.11, so
    // kPlateHalf <= 0.71 at kRounding 0.40. Past that it reads as hovering (0.75 -> 1.6 px,
    // 0.80 -> 2.9 px, the worst point by then moved to the corners) and a plate that hugs
    // the surface instead would be needed. The grey frame a user sees is what is left of the
    // face around this card.
    // The plate is filled, not just outlined: the flat middle is a square with sharp
    // corners, so a white body below the plate would show its corners past the plate's
    // rounded ones and read as white threads on the rounding.
    const double kPlateHalf = 0.70;
    const double kPlateCorner = 0.18;
    // Segments across one quarter of a plate corner
    const int kPlateCornerSegments = 6;
    // How far a plate is pushed along its normal, to keep it off the body. Has to stay
    // below kLabelOffset, or the label loses the depth test against its own plate
    const double kPlateOffset = 0.006;
    const double kPi = 3.14159265358979323846;

    // One flat grey body with a light grey card on each face, neither of them lit: the form
    // reads from the silhouette alone, the way CrownCAD's cube reads. Shading the body towards
    // the silhouette instead painted the rounding bands and the near corner darker, and the eye
    // took the flat middle of each face for the cube with the rounding hidden behind its
    // shading. A directional light on top of that made the three faces differ (209 / 192 / 161,
    // which the eye took for the cube's own shading) and lost against the flat body when the
    // two were compared (2026-09-26); the flat body also keeps the card a fixed 31 levels above
    // it in every view.
    //
    // Both greys are unlit, so each holds its value in every orientation. The card is grey
    // rather than white, and darker than the viewport behind the cube, for the same reason: a
    // white card merges with the background. Measured 2026-09-26, the background is a light
    // grey -- 234 at the dark end of its gradient, 240 at the bright one -- and the old 0.92
    // card (235) sat Delta-L* 0.4 to 1.7 from it, one flat field. 0.89 (227) sits 2.4 / 4.6
    // away instead -- the lightest the card can go and still read as a card, at the price of
    // CrownCAD's 7.0 of card on white (0.90 (230) is 1.3, 0.92 (235) is 0.4, both back to one
    // flat field). The body drops with it to 0.73 (186), which keeps the card a 14.7 step of
    // its own (CrownCAD 14.3), and the black ink lands at 16.4:1 on it.
    const osg::Vec4 kPlateColor(0.89f, 0.89f, 0.89f, 1.0f);
    const osg::Vec4 kBodyColor(0.73f, 0.73f, 0.73f, 1.0f);
    const osg::Vec4 kHoverColor(0.353f, 0.627f, 0.902f, 1.0f);
    // Home button ink. The glyph texture is white and this is the colour that actually shows,
    // so it has to read on the light viewport background; the hover state matches the cube's.
    const osg::Vec4 kHomeColor(0.30f, 0.30f, 0.30f, 1.0f);
    // Pure black ink, as asked (2026-09-26); it used to be 0x1A1A1A, which measured a touch
    // lighter than CrownCAD's ink (about 50)
    const QColor kLabelColor(0x00, 0x00, 0x00);

    // Outward normals of the six main faces; this order is facet 0..5 and maps one to
    // one onto the named ViewUtil views
    const osg::Vec3d kMainNormals[6] = {
        osg::Vec3d(0.0, -1.0, 0.0), // Front
        osg::Vec3d(0.0, 1.0, 0.0),  // Back
        osg::Vec3d(-1.0, 0.0, 0.0), // Left
        osg::Vec3d(1.0, 0.0, 0.0),  // Right
        osg::Vec3d(0.0, 0.0, 1.0),  // Top
        osg::Vec3d(0.0, 0.0, -1.0), // Bottom
    };

    // Horizontal basis of each face label (screen right); always u x v = normal
    const osg::Vec3d kLabelU[6] = {
        osg::Vec3d(1.0, 0.0, 0.0),
        osg::Vec3d(-1.0, 0.0, 0.0),
        osg::Vec3d(0.0, -1.0, 0.0),
        osg::Vec3d(0.0, 1.0, 0.0),
        osg::Vec3d(1.0, 0.0, 0.0),
        osg::Vec3d(1.0, 0.0, 0.0),
    };

    // Vertical basis of each face label (screen up)
    const osg::Vec3d kLabelV[6] = {
        osg::Vec3d(0.0, 0.0, 1.0),
        osg::Vec3d(0.0, 0.0, 1.0),
        osg::Vec3d(0.0, 0.0, 1.0),
        osg::Vec3d(0.0, 0.0, 1.0),
        osg::Vec3d(0.0, 1.0, 0.0),
        osg::Vec3d(0.0, -1.0, 0.0),
    };

    // Region of the surface point whose outward normal is d (= the point minus its
    // projection onto the inner box). Faces are 0..5 in kMainNormals order, then the 12
    // edges (6..17) and the 8 corners (18..25), the same numbering the chamfered facets
    // used; a point in the flat middle of a face has a single non-zero component, an edge
    // point two and a corner point three. Returns -1 for the degenerate all-zero case.
    int regionOf(const osg::Vec3d& d)
    {
        const bool on[3] = {std::abs(d.x()) > 1e-7, std::abs(d.y()) > 1e-7,
                            std::abs(d.z()) > 1e-7};
        const bool pos[3] = {d.x() > 0.0, d.y() > 0.0, d.z() > 0.0};
        const int count = (on[0] ? 1 : 0) + (on[1] ? 1 : 0) + (on[2] ? 1 : 0);

        if (count == 0)
        {
            return -1;
        }

        if (count == 1)
        {
            // Axis 0/1/2 and sign map onto kMainNormals: -Y, +Y, -X, +X, +Z, -Z
            static const int face[3][2] = {{2, 3}, {0, 1}, {5, 4}};
            for (int i = 0; i < 3; ++i)
            {
                if (on[i])
                {
                    return face[i][pos[i] ? 1 : 0];
                }
            }
        }

        if (count == 2)
        {
            const int i = on[0] ? 0 : 1;
            const int j = on[2] ? 2 : (on[1] ? 1 : 2);
            const int pair = (i == 0) ? ((j == 1) ? 0 : 1) : 2;
            return 6 + pair * 4 + (pos[i] ? 2 : 0) + (pos[j] ? 1 : 0);
        }

        return 18 + (pos[0] ? 4 : 0) + (pos[1] ? 2 : 0) + (pos[2] ? 1 : 0);
    }

    // Outward normal at the middle of an edge or corner region, used as the oblique view
    // direction. Note that a face region is never passed in: those have named views.
    osg::Vec3d regionNormal(int region)
    {
        if (region < 18)
        {
            const int pair = (region - 6) / 4;
            const int sign = (region - 6) % 4;
            const int i = (pair == 2) ? 1 : 0;
            const int j = (pair == 0) ? 1 : 2;

            osg::Vec3d n(0.0, 0.0, 0.0);
            n[i] = (sign >= 2) ? 1.0 : -1.0;
            n[j] = (sign % 2 != 0) ? 1.0 : -1.0;
            n.normalize();
            return n;
        }

        const int sign = region - 18;
        osg::Vec3d n((sign & 4) != 0 ? 1.0 : -1.0, (sign & 2) != 0 ? 1.0 : -1.0,
                     (sign & 1) != 0 ? 1.0 : -1.0);
        n.normalize();
        return n;
    }

    // The font every label is measured and baked with. The family list keeps the Latin face for
    // the Latin labels and hands the Han ones to the pinned CJK face, instead of leaving that
    // pick to whatever the platform falls back to.
    QFont labelFont(int pixelSize)
    {
        QFont font(QString::fromLatin1(kLabelFontFamily));
        font.setFamilies({QString::fromLatin1(kLabelFontFamily), QString::fromLatin1(kLabelCjkFamily)});
        font.setPixelSize(pixelSize);
        font.setWeight(QFont::Bold);
        return font;
    }

    // Ink box of a label, measured with a 100 px font. Both the aspect (which sizes every
    // label) and the point size of the bake come from it, and neither depends on the pixel
    // size the label is finally drawn at. The labels of a run are a fixed set -- one language,
    // one font family -- so each box is measured once and kept.
    QRect labelInk(const QString& text)
    {
        static QHash<QString, QRect> measured;
        const auto found = measured.constFind(text);
        if (measured.constEnd() != found)
            return found.value();
        const QFontMetrics metrics{labelFont(100)};
        const QRect ink = metrics.tightBoundingRect(text);
        measured.insert(text, ink);
        return ink;
    }

    // The baked bitmap of one label, already flipped for osg (QImage row 0 is the top row,
    // osg::Image starts at the bottom left). The labels of a run -- one language, one font
    // family -- are the same in every document, and rasterising them is most of a cube's
    // build, so the pixels are made once per process and handed back from here. The key
    // carries the text and the texture size; point size, margin and flip all follow from those.
    QImage labelBitmap(const QString& text, const QRect& ink, int texWidth, int texHeight)
    {
        static QHash<QString, QImage> baked;
        const QString key = QStringLiteral("%1|%2x%3").arg(text).arg(texWidth).arg(texHeight);
        const auto found = baked.constFind(key);
        if (baked.constEnd() != found)
            return found.value();

        const double inkWidth = std::max(1, ink.width());
        const double inkHeight = std::max(1, ink.height());
        const double margin = kLabelMargin * texHeight / static_cast<double>(kLabelTexSize);
        // The point size is proportional to the ink, so a single scale makes it fill the
        // texture's inner box exactly
        const double scale = std::min((texWidth - 2.0 * margin) / inkWidth,
                                      (texHeight - 2.0 * margin) / inkHeight);
        const QFont font = labelFont(std::max(8, static_cast<int>(std::lround(100.0 * scale))));

        // RGBA8888 is not premultiplied, which is what GL_RGBA with
        // SRC_ALPHA/ONE_MINUS_SRC_ALPHA expects; Format_ARGB32 is BGRA and premultiplied,
        // so it cannot be used
        QImage image(texWidth, texHeight, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::TextAntialiasing, true);
            painter.setFont(font);
            painter.setPen(kLabelColor);
            painter.drawText(image.rect(), Qt::AlignCenter, text);
        }
        const QImage flipped = image.mirrored(false, true);
        baked.insert(key, flipped);
        return flipped;
    }

    // Bake the text into a texture with QPainter and hand back the texture aspect (w/h).
    // The repository ships no font file, so osgText could only fall back to a default
    // font without CJK glyphs -- hence this route.
    //
    // The texture is sized to the ink aspect and the label quad uses the same ratio: a
    // single CJK glyph is nearly square while a long Latin word is flat, so one fixed
    // square texture would squash one of the two beyond readability.
    osg::Texture2D* createLabelTexture(const QString& text, double pixelHeight, const QRect& ink,
                                       double& aspect)
    {
        // The caller asks for the height the label is drawn at; the margin has to scale with
        // it, or a 20 px texture would be all margin. It may come out below one texel, which
        // is fine: the font size below is fitted to the inner box, so the glyph stops there.
        const int texHeight = std::clamp(static_cast<int>(std::lround(pixelHeight)),
                                         kLabelTexMinSize, kLabelTexSize);
        const double inkWidth = std::max(1, ink.width());
        const double inkHeight = std::max(1, ink.height());
        const int texWidth =
            std::min(kLabelTexMaxWidth, std::max(texHeight,
                     static_cast<int>(std::lround(texHeight * inkWidth / inkHeight))));
        aspect = static_cast<double>(texWidth) / texHeight;

        const QImage flipped = labelBitmap(text, ink, texWidth, texHeight);
        const std::size_t bytes = static_cast<std::size_t>(flipped.sizeInBytes());

        osg::ref_ptr<osg::Image> pImage = new osg::Image;
        pImage->setImage(flipped.width(), flipped.height(), 1,
                         GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE,
                         new unsigned char[bytes], osg::Image::USE_NEW_DELETE);
        std::memcpy(pImage->data(), flipped.constBits(), bytes);

        osg::ref_ptr<osg::Texture2D> pTexture = new osg::Texture2D;
        pTexture->setImage(pImage.get());
        // Keep the baked size: osg's default power-of-two hint rescales the image before upload
        // -- measured, the 82x82 Han labels went up as 64x64, which cuts the 2x bake to 1.6x.
        // Only honoured where the driver can mipmap a non-power-of-two texture.
        pTexture->setResizeNonPowerOfTwoHint(false);
        // A mipmap min filter, not plain LINEAR: the label is drawn smaller than it is baked
        // wherever its face is tilted, and LINEAR then samples only the 2x2 texels around the
        // pixel centre, which drops thin glyph strokes outright (the dashed "Top" of
        // 333.png). Texture2D has hardware mipmap generation on by default, so asking for a
        // mipmap filter is enough.
        pTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
        pTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        // A label quad is compressed whenever its face is at an angle -- 1.73x in the resting
        // isometric view -- and a plain mip filter takes its level from that compressed axis,
        // which blurs the reading direction with it: a 12.7 px label came out of a ~7 texel
        // tall mip. Anisotropic filtering takes the level from the un-compressed axis instead,
        // where the 2x bake lands at about one texel per pixel -- that pair is the reason
        // kLabelBakeScale stays at 2.0; a 1x bake would only shimmer as the cube turns.
        pTexture->setMaxAnisotropy(16.0f);
        pTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        pTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        pTexture->setUnRefImageDataAfterApply(true);
        return pTexture.release();
    }

    // The home glyph: a white house on a transparent white ground. White rather than black
    // because the quad tints it (MODULATE), and transparent *white* because a transparent
    // black ground would darken the antialiased edges when filtered -- only alpha should vary.
    QImage homeIconBitmap()
    {
        const int size = std::max(kLabelTexMinSize,
                                  static_cast<int>(std::lround(kHomeButton * kHomeBakeScale)));
        QImage image(size, size, QImage::Format_RGBA8888);
        image.fill(QColor(255, 255, 255, 0));

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(Qt::white);
        pen.setWidthF(size * 0.10);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        // Roof, walls and an open door, as fractions of the texture; the stroke is centred on
        // the path, so the margins leave room for half its width.
        const double s = size;
        QPainterPath house;
        house.moveTo(0.14 * s, 0.50 * s);
        house.lineTo(0.50 * s, 0.16 * s);
        house.lineTo(0.86 * s, 0.50 * s);
        house.moveTo(0.24 * s, 0.40 * s);
        house.lineTo(0.24 * s, 0.86 * s);
        house.lineTo(0.76 * s, 0.86 * s);
        house.lineTo(0.76 * s, 0.40 * s);
        house.moveTo(0.38 * s, 0.86 * s);
        house.lineTo(0.38 * s, 0.62 * s);
        house.lineTo(0.62 * s, 0.62 * s);
        house.lineTo(0.62 * s, 0.86 * s);
        painter.drawPath(house);

        // QImage row 0 is the top row; osg::Image starts at the bottom left
        return image.mirrored(false, true);
    }

    // Bake the glyph into a texture, the same route the labels take. No mipmaps here, unlike
    // the labels: the quad is screen aligned and exactly 2x minified, so a plain LINEAR min
    // filter averages the 2x2 block under each pixel -- the supersample the bake was made for.
    osg::Texture2D* createHomeIconTexture()
    {
        const QImage flipped = homeIconBitmap();
        const std::size_t bytes = static_cast<std::size_t>(flipped.sizeInBytes());

        osg::ref_ptr<osg::Image> pImage = new osg::Image;
        pImage->setImage(flipped.width(), flipped.height(), 1,
                         GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE,
                         new unsigned char[bytes], osg::Image::USE_NEW_DELETE);
        std::memcpy(pImage->data(), flipped.constBits(), bytes);

        osg::ref_ptr<osg::Texture2D> pTexture = new osg::Texture2D;
        pTexture->setImage(pImage.get());
        // 52 is not a power of two; without this osg rescales the image to 64 and the exact
        // 2x bake the filter above relies on is gone
        pTexture->setResizeNonPowerOfTwoHint(false);
        pTexture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        pTexture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        pTexture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        pTexture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        pTexture->setUnRefImageDataAfterApply(true);
        return pTexture.release();
    }
}

// Refresh the cube's orientation and viewport every frame, during cull.
//
// A cull callback rather than an update callback: in Viewer::updateTraversal, updateCamera
// runs after updateSceneGraph, so an update callback would read a stale manipulator pose.
// The camera's cull callback runs after updateTraversal but before the children are culled,
// which is still early enough to change the model matrix.
class ViewCube::CubeCullCallback : public osg::NodeCallback
{
public:
    CubeCullCallback(ViewCube* pCube) : _pCube(pCube) {}

    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv) override
    {
        if (_pCube)
        {
            _pCube->refresh();
        }
        traverse(node, nv);
    }

private:
    // Raw pointer: the camera owns the callback, a ref_ptr would cycle with it
    ViewCube* _pCube = nullptr;
};

ViewCube::ViewCube(osgViewer::View* pView, std::function<osg::BoundingSphere()> getFitSphere)
    : osg::Camera(), _pView(pView), _getFitSphere(std::move(getFitSphere))
{
    // Overlay setup, the same as select/BoxSelectRectangle
    setRenderOrder(osg::Camera::POST_RENDER);
    setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    setAllowEventFocus(false);
    setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    // Depth only: clearing colour would wipe out the main scene
    setClearMask(GL_DEPTH_BUFFER_BIT);

    // The viewport is wider than the cube: its right kCubeSize pixels are the cube's square
    // and the strip to their left carries the home button. The projection's left is extended
    // by the same strip, so one device pixel still covers kEyePerPixel on both axes and the
    // cube keeps its size and square pixels; the zoom stays folded into the _pCubeXform
    // matrix.
    _pViewport = new osg::Viewport(0.0, 0.0, kOverlayWidth, kCubeSize);
    setViewport(_pViewport.get());
    setProjectionMatrixAsOrtho2D(kOverlayLeft, 1.0, -1.0, 1.0);
    // Never let the cull compute this camera's near/far from the scene. A nested camera inherits
    // its parent's mode (the inheritance mask defaults to every variable), so the main camera's
    // setting would otherwise rule here -- and with it on, osg discards a drawable whose bounding
    // box lies entirely behind the eye (updateCalculatedNearFar). The eye of this overlay is the
    // world origin, which is the cube's own centre, so the boxes of the three visible faces all
    // straddle that plane by a hair: measured 2026-09-27, the front label spans eye-space z
    // [0.004, 0.743] and was dropped outright, while "right" [-0.002, ...] and "top" [-0.014, ...]
    // survived -- one face losing its label depended on the view being perspective. Setting the
    // mode also clears the matching inheritance bit, so a later change of the main camera's view
    // cannot bring it back.
    setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);

    // Transparent to element picking. PointPick's intersection callback returns false -- not
    // continue -- on geometry without an ElementId, so one hit fails the whole pick, and
    // element picking traverses the entire tree from the main camera, overlay included.
    setNodeMask(~ElementNodeType::All);

    osg::StateSet* pStateSet = getOrCreateStateSet();
    // Explicitly off: the overlay shares the main scene's GL state, and lighting that is on
    // with no light enabled would paint the flat vertex colours black
    pStateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    pStateSet->setMode(GL_CULL_FACE, osg::StateAttribute::OFF);

    _pCubeXform = new osg::MatrixTransform;
    addChild(_pCubeXform.get());

    buildCube();
    buildLabels();
    buildHomeButton();

    setCullCallback(new CubeCullCallback(this));
}

bool ViewCube::handleEvent(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa)
{
    if (!_pView.valid())
    {
        return false;
    }

    switch (ea.getEventType())
    {
    case osgGA::GUIEventAdapter::PUSH:
    {
        if (ea.getButton() != osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        {
            return false;
        }
        // A left press always starts a new gesture. Clearing first drops state left behind by
        // a RELEASE that never arrived (Alt+Tab, release outside the widget).
        _isPressed = false;
        _pressedFacet = -1;
        _homePressed = false;

        if (hitHome(ea.getX(), ea.getY()))
        {
            _homePressed = true;
            const bool facetChanged = setHoverFacet(-1);
            const bool homeChanged = setHomeHovered(true);
            if (facetChanged || homeChanged)
            {
                aa.requestRedraw();
            }
            // Consumed: no facet toggle, gizmo, selection or orbit. The view changes on
            // release, not here, so a press dragged away can still be cancelled.
            return true;
        }

        const int facet = pickFacet(ea.getX(), ea.getY());
        if (facet < 0)
        {
            return false;
        }
        _pressedFacet = facet;
        _isPressed = true;
        setHomeHovered(false);
        if (setHoverFacet(facet))
        {
            aa.requestRedraw();
        }
        return true;
    }

    case osgGA::GUIEventAdapter::DRAG:
        // The button's feedback follows the cursor, so dragging off it reads as cancelled
        // before the release decides; the cube keeps its press highlight for the whole drag
        if (_homePressed && setHomeHovered(hitHome(ea.getX(), ea.getY())))
        {
            aa.requestRedraw();
        }
        // Swallow only what this overlay received a PUSH for, or an in-progress gizmo drag or
        // box selection would be cut short
        return _isPressed || _homePressed;

    case osgGA::GUIEventAdapter::RELEASE:
    {
        if (!_isPressed && !_homePressed)
        {
            return false;
        }
        // The gesture belongs to the left button. Another button's release neither ends it
        // nor acts on the overlay; it used to switch views when a right release arrived
        // while a left press was still held.
        if (ea.getButton() != osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        {
            return true;
        }

        const bool homePressed = _homePressed;
        const bool cubePressed = _isPressed;
        const int pressed = _pressedFacet;
        _homePressed = false;
        _isPressed = false;
        _pressedFacet = -1;

        const bool onHome = hitHome(ea.getX(), ea.getY());
        const int released = pickFacet(ea.getX(), ea.getY());
        // A view switches only when the release lands on the control that was pressed
        if (homePressed && onHome)
        {
            applyHomeView();
        }
        if (cubePressed && pressed >= 0 && pressed == released)
        {
            applyView(pressed);
        }

        const bool facetChanged = setHoverFacet(released);
        const bool homeChanged = setHomeHovered(onHome);
        if (facetChanged || homeChanged)
        {
            aa.requestRedraw();
        }
        // Even a cancelled press must not leak its release to selection or the manipulator:
        // the press was consumed, so its release has to be as well
        return true;
    }

    case osgGA::GUIEventAdapter::MOVE:
        // Never swallow MOVE: doing so leaves SelectHandler's hover highlight stuck
        if (updateHover(ea.getX(), ea.getY()))
        {
            aa.requestRedraw();
        }
        return false;

    default:
        // OSG 3.6 has no LEAVE event: a cursor that leaves the viewport is reported through
        // clearHover instead
        return false;
    }
}

void ViewCube::clearHover()
{
    const bool facetChanged = setHoverFacet(-1);
    const bool homeChanged = setHomeHovered(false);
    if (!facetChanged && !homeChanged)
    {
        return;
    }
    if (_pView.valid())
    {
        _pView->requestRedraw();
    }
}

void ViewCube::refresh()
{
    if (!_pView.valid())
    {
        return;
    }

    osg::Camera* pCamera = _pView->getCamera();
    if (!pCamera)
    {
        return;
    }

    const osg::GraphicsContext* pContext = pCamera->getGraphicsContext();
    const osg::GraphicsContext::Traits* pTraits = pContext ? pContext->getTraits() : nullptr;
    if (pTraits && _pViewport.valid())
    {
        // Traits are in device pixels and the viewport origin is bottom left, so pinning to
        // the top right means folding y downwards. The viewport is wider than the cube: its
        // right and top edges stay at kMargin and the home strip extends to the left, which
        // leaves the cube's own rectangle exactly where it was before the strip existed.
        _pViewport->setViewport(pTraits->width - kMargin - kOverlayWidth,
                                pTraits->height - kMargin - kCubeSize,
                                kOverlayWidth, kCubeSize);
    }

    if (!computeBasis())
    {
        return;
    }

    // World -> overlay view space. The eye sits at the world origin: the overlay is an
    // orthographic projection so the eye distance does not affect the image, and the cube is
    // centred on the world origin anyway, which puts it dead centre in the cube square at the
    // right of the overlay viewport. Use OSG's own lookAt rather than a hand built matrix: the
    // row/column convention is then the library's, as everywhere else in this project.
    const osg::Matrix view = osg::Matrix::lookAt(osg::Vec3d(0.0, 0.0, 0.0), _forward, _up);
    const osg::Matrix matrix = osg::Matrix::scale(kScale, kScale, kScale) * view;
    if (_pCubeXform.valid() && _pCubeXform->getMatrix() != matrix)
    {
        _pCubeXform->setMatrix(matrix);
    }
}

bool ViewCube::computeBasis()
{
    if (!_pView.valid())
    {
        return false;
    }

    CameraManipulator3d* pManipulator =
        dynamic_cast<CameraManipulator3d*>(_pView->getCameraManipulator());
    if (!pManipulator)
    {
        return false;
    }

    // Read the authoritative pose from the manipulator: camera->getViewMatrix is written by
    // updateTraversal and lags one frame behind
    osg::Vec3d eye, center, upHint;
    pManipulator->getTransformation(eye, center, upHint);

    _forward = center - eye;
    if (_forward.length2() < 1e-12)
    {
        return false;
    }
    _forward.normalize();

    _right = _forward ^ upHint;
    if (_right.length2() < 1e-12)
    {
        return false;
    }
    _right.normalize();

    // Same two steps as osg::Matrixd::makeLookAt does internally
    _up = _right ^ _forward;

    return true;
}

void ViewCube::applyView(int facet)
{
    if (facet < 0 || facet > 25 || !_pView.valid())
    {
        return;
    }

    osgViewer::View* pView = _pView.get();
    // ViewUtil::viewTo has an unguarded assert inside: check the manipulator type first
    if (!dynamic_cast<CameraManipulator3d*>(pView->getCameraManipulator()))
    {
        return;
    }

    // Main faces go through the named functions: the generic up formula degenerates exactly
    // at Top/Bottom and would roll differently from the toolbar buttons
    switch (facet)
    {
    case 0: ViewUtil::viewToFront(pView); return;
    case 1: ViewUtil::viewToBack(pView); return;
    case 2: ViewUtil::viewToLeft(pView); return;
    case 3: ViewUtil::viewToRight(pView); return;
    case 4: ViewUtil::viewToTop(pView); return;
    case 5: ViewUtil::viewToBottom(pView); return;
    default: break;
    }

    // Edge and corner regions: the view direction is the reversed outward normal at the
    // middle of the region. Its |z| peaks at 1/sqrt(2), so the direction is never parallel
    // to Z and passing Z as up cannot degenerate in lookAt's orthogonalisation.
    ViewUtil::viewTo(pView, -regionNormal(facet), osg::Vec3d(0.0, 0.0, 1.0));
}

void ViewCube::applyHomeView()
{
    if (!_pView.valid())
    {
        return;
    }

    osgViewer::View* pView = _pView.get();
    // ViewUtil::viewTo has an unguarded assert inside: check the manipulator type first
    if (!dynamic_cast<CameraManipulator3d*>(pView->getCameraManipulator()))
    {
        return;
    }

    // The default view of a document is the isometric direction fitted to the model (the
    // sequence FileCmdsUtil::fitViewToAll runs). Fit after the orientation: viewAll keeps the
    // look direction and handles ortho and perspective itself, so the current projection
    // mode is preserved.
    ViewUtil::viewToISO(pView);
    if (_getFitSphere)
    {
        ViewUtil::viewAll(pView, _getFitSphere());
    }
    // On-demand rendering: applying a view may not change any hover state, so the frame that
    // shows it has to be asked for here
    pView->requestRedraw();
}

osg::Vec2d ViewCube::windowToEye(float winX, float winY) const
{
    // Window pixels (origin bottom left) -> overlay eye space. The cube square is kCubeSize
    // across with the projection spanning [-1, 1] across it, so one device pixel is
    // kEyePerPixel on both axes. The cube's square is the rightmost kCubeSize pixels of the
    // wider viewport and the home strip extends to its left, which is what kOverlayLeft
    // shifts for.
    return osg::Vec2d(kOverlayLeft + 2.0 * (winX - _pViewport->x()) / kCubeSize,
                      -1.0 + 2.0 * (winY - _pViewport->y()) / kCubeSize);
}

bool ViewCube::hitHome(float winX, float winY) const
{
    if (!_pViewport.valid())
    {
        return false;
    }

    // The same rectangle buildHomeButton draws, from the same constants, so the glyph and its
    // hit area cannot drift apart. The outer edges (left, top) are inclusive; the edges that
    // face the gap are not. The top edge maps to eye y 1.0 exactly.
    const osg::Vec2d eye = windowToEye(winX, winY);
    const double size = kHomeButton * kEyePerPixel;
    return eye.x() >= kOverlayLeft && eye.x() < kOverlayLeft + size &&
           eye.y() > 1.0 - size && eye.y() <= 1.0;
}

int ViewCube::pickFacet(float winX, float winY)
{
    if (!_pViewport.valid() || !computeBasis())
    {
        return -1;
    }

    // Window pixels -> overlay eye space. Only the cube's own square is a valid pick target:
    // the home strip to its left has eyeX < -1 and falls out through the same test.
    const osg::Vec2d eye = windowToEye(winX, winY);
    if (eye.x() < -1.0 || eye.x() > 1.0 || eye.y() < -1.0 || eye.y() > 1.0)
    {
        return -1;
    }

    // Under an orthographic projection a screen point is a ray along the view direction;
    // the origin is placed in front of the cube
    const osg::Vec3d origin =
        _right * (eye.x() / kScale) + _up * (eye.y() / kScale) - _forward * kRayFar;

    // The body is a rounded box rather than a polyhedron, so the entry point comes from the
    // distance field: gap2(p) is the squared distance from p to the inner box, it is convex
    // along the ray, and the body is exactly the set where gap2 <= r². At the entry to the
    // outer box gap2 >= r² already, so the ray crosses the surface at most once and a
    // rejected ray is simply one whose closest approach stays outside the rounding.
    const double b = 1.0 - kRounding;
    const double r2 = kRounding * kRounding;

    auto gap2 = [b](const osg::Vec3d& p)
    {
        double sum = 0.0;
        for (int i = 0; i < 3; ++i)
        {
            const double a = std::abs(p[i]) - b;
            if (a > 0.0)
            {
                sum += a * a;
            }
        }
        return sum;
    };

    // Slab test against the outer box: no crossing means the ray cannot hit the body
    double tEnter = -std::numeric_limits<double>::max();
    double tExit = std::numeric_limits<double>::max();
    for (int i = 0; i < 3; ++i)
    {
        if (std::abs(_forward[i]) < 1e-12)
        {
            if (std::abs(origin[i]) > 1.0)
            {
                return -1;
            }
            continue;
        }

        double t0 = (-1.0 - origin[i]) / _forward[i];
        double t1 = (1.0 - origin[i]) / _forward[i];
        if (t0 > t1)
        {
            std::swap(t0, t1);
        }
        tEnter = std::max(tEnter, t0);
        tExit = std::min(tExit, t1);
    }
    if (tEnter > tExit)
    {
        return -1;
    }

    // Ternary search for the closest approach: a minimum further away than the rounding
    // radius means the ray only grazes the outer box, past a rounded corner
    double lo = tEnter;
    double hi = tExit;
    for (int i = 0; i < 100; ++i)
    {
        const double m1 = lo + (hi - lo) / 3.0;
        const double m2 = hi - (hi - lo) / 3.0;
        if (gap2(origin + _forward * m1) < gap2(origin + _forward * m2))
        {
            hi = m2;
        }
        else
        {
            lo = m1;
        }
    }
    const double tNear = (lo + hi) * 0.5;
    if (gap2(origin + _forward * tNear) > r2)
    {
        return -1;
    }

    // gap2 falls from tEnter to tNear, so plain bisection finds the first crossing
    double tIn = tEnter;
    double tOut = tNear;
    for (int i = 0; i < 60; ++i)
    {
        const double mid = (tIn + tOut) * 0.5;
        if (gap2(origin + _forward * mid) > r2)
        {
            tIn = mid;
        }
        else
        {
            tOut = mid;
        }
    }
    const osg::Vec3d hit = origin + _forward * ((tIn + tOut) * 0.5);

    // The outward normal is the hit point minus its projection onto the inner box; how many
    // of its components are non-zero tells a face from an edge from a corner
    osg::Vec3d out;
    for (int i = 0; i < 3; ++i)
    {
        out[i] = (std::abs(hit[i]) > b) ? hit[i] - ((hit[i] > 0.0) ? b : -b) : 0.0;
    }
    return regionOf(out);
}

bool ViewCube::setHoverFacet(int facet)
{
    if (facet == _hoverFacet)
    {
        return false;
    }
    _hoverFacet = facet;
    refreshColors();
    return true;
}

void ViewCube::refreshColors()
{
    if (!_pFillColors.valid() || _baseColors.size() != _pFillColors->size() ||
        _plateColors.size() != 6)
    {
        return;
    }

    // A hovered face shows on its plate and a hovered edge or corner on the body cells of
    // that region. The flat middle never takes the highlight: a plate is smaller than the
    // cell below it, so a blue ring would show around the plate.
    const std::size_t count = _pFillColors->size();
    for (std::size_t i = 0; i < count; ++i)
    {
        const bool hovered = (_hoverFacet >= 6) && (_vertexRegions[i] == _hoverFacet);
        (*_pFillColors)[i] = hovered ? kHoverColor : _baseColors[i];
    }
    _pFillColors->dirty();

    for (int face = 0; face < 6; ++face)
    {
        (*_plateColors[face])[0] = (_hoverFacet == face) ? kHoverColor : kPlateColor;
        _plateColors[face]->dirty();
    }
}

bool ViewCube::setHomeHovered(bool hovered)
{
    if (hovered == _homeHovered)
    {
        return false;
    }
    _homeHovered = hovered;
    refreshHomeColor();
    return true;
}

void ViewCube::refreshHomeColor()
{
    if (!_pHomeColors.valid() || _pHomeColors->empty())
    {
        return;
    }
    (*_pHomeColors)[0] = _homeHovered ? kHoverColor : kHomeColor;
    _pHomeColors->dirty();
}

bool ViewCube::updateHover(float winX, float winY)
{
    // The button and the cube are disjoint, so only one of the two can light up
    if (hitHome(winX, winY))
    {
        const bool homeChanged = setHomeHovered(true);
        const bool facetChanged = setHoverFacet(-1);
        return homeChanged || facetChanged;
    }
    const bool homeChanged = setHomeHovered(false);
    const bool facetChanged = setHoverFacet(pickFacet(winX, winY));
    return homeChanged || facetChanged;
}

void ViewCube::buildCube()
{
    _pVertices = new osg::Vec3Array;
    _pFillColors = new osg::Vec4Array;
    _pFillColors->setBinding(osg::Array::BIND_PER_VERTEX);
    _vertexRegions.clear();
    _baseColors.clear();

    const double b = 1.0 - kRounding;

    // Cell boundaries along one axis of a face: the flat middle is the single cell between
    // -b and b, and each quarter arc of the rounding gets kRoundingSegments of them. All
    // six faces use the same boundaries and sample their common edge at the same points,
    // so the surface has no cracks.
    std::vector<double> bounds;
    for (int i = 0; i <= kRoundingSegments; ++i)
    {
        bounds.push_back(-1.0 + kRounding * i / kRoundingSegments); // -1 .. -b
    }
    for (int i = 0; i <= kRoundingSegments; ++i)
    {
        bounds.push_back(b + kRounding * i / kRoundingSegments); // b .. 1
    }
    const int cells = static_cast<int>(bounds.size()) - 1;

    osg::ref_ptr<osg::DrawElementsUInt> pIndices = new osg::DrawElementsUInt(GL_TRIANGLES);
    pIndices->reserve(static_cast<std::size_t>(cells) * cells * 6 * 6);

    for (int face = 0; face < 6; ++face)
    {
        const osg::Vec3d& n = kMainNormals[face];
        const osg::Vec3d& u = kLabelU[face];
        const osg::Vec3d& v = kLabelV[face];

        // Surface point and outward normal at the in-plane coordinates (uu, vv): the point
        // on the inner box plus the rounding radius along the residual direction
        auto surface = [&](double uu, double vv, osg::Vec3d& point, osg::Vec3d& normal)
        {
            const double cu = std::max(-b, std::min(b, uu));
            const double cv = std::max(-b, std::min(b, vv));
            normal = n * kRounding + u * (uu - cu) + v * (vv - cv);
            normal.normalize();
            point = n * b + u * cu + v * cv + normal * kRounding;
        };

        for (int ci = 0; ci < cells; ++ci)
        {
            for (int cj = 0; cj < cells; ++cj)
            {
                // Every cell takes the same grey: one flat body colour, so the rounding
                // shows up in the silhouette only, never as a seam across the surface
                const double uc = (bounds[ci] + bounds[ci + 1]) * 0.5;
                const double vc = (bounds[cj] + bounds[cj + 1]) * 0.5;
                osg::Vec3d cellNormal;
                osg::Vec3d cellCenter;
                surface(uc, vc, cellCenter, cellNormal);

                const osg::Vec4 color = kBodyColor;

                const int region = regionOf(cellNormal);

                const double corners[4][2] = {{bounds[ci], bounds[cj]},
                                             {bounds[ci + 1], bounds[cj]},
                                             {bounds[ci + 1], bounds[cj + 1]},
                                             {bounds[ci], bounds[cj + 1]}};
                const unsigned int first = static_cast<unsigned int>(_pVertices->size());
                for (const double* corner : corners)
                {
                    osg::Vec3d point;
                    osg::Vec3d normal; // surface() hands one back; nothing shades with it
                    surface(corner[0], corner[1], point, normal);
                    _pVertices->push_back(point);
                    _baseColors.push_back(color);
                    _vertexRegions.push_back(region);
                }
                pIndices->push_back(first);
                pIndices->push_back(first + 1);
                pIndices->push_back(first + 2);
                pIndices->push_back(first);
                pIndices->push_back(first + 2);
                pIndices->push_back(first + 3);
            }
        }
    }

    osg::ref_ptr<osg::Geometry> pFills = new osg::Geometry;
    pFills->setUseDisplayList(false);
    pFills->setUseVertexBufferObjects(true);
    pFills->setVertexArray(_pVertices.get());
    pFills->setColorArray(_pFillColors.get());
    pFills->addPrimitiveSet(pIndices.get());
    // Positive: push the fills away from the viewer so the plates and the face labels
    // (offset 0.012 in front) win the depth test. A negative offset pulls the
    // fills toward the viewer instead and buries the labels as soon as the cube is
    // tilted off a standard view.
    pFills->getOrCreateStateSet()->setAttributeAndModes(new osg::PolygonOffset(1.0f, 1.0f),
                                                        osg::StateAttribute::ON);

    // The label plate of each face: a filled rounded rectangle lifted off the flat middle of
    // the face, with no outline. An outline around a card draws a square frame that the eye
    // reads as part of a box, and CrownCAD's cards have none either.
    // Perimeter of one face's plate, at the given distance from the origin
    auto platePerimeter = [&](int face, double radius, osg::Vec3Array* pOut)
    {
        const osg::Vec3d& n = kMainNormals[face];
        const osg::Vec3d& u = kLabelU[face];
        const osg::Vec3d& v = kLabelV[face];
        const double offset = kPlateHalf - kPlateCorner; // where a corner arc starts

        // Four arcs, counter clockwise starting from the lower left corner; the straight
        // sides are the segments that join one arc's end to the next arc's start
        for (int corner = 0; corner < 4; ++corner)
        {
            const double su = (corner == 0 || corner == 3) ? -offset : offset;
            const double sv = (corner < 2) ? -offset : offset;
            const osg::Vec3d center = n * radius + u * su + v * sv;
            const double start = (180.0 + 90.0 * corner) * kPi / 180.0;
            for (int i = 0; i <= kPlateCornerSegments; ++i)
            {
                const double angle = start + kPi * 0.5 * i / kPlateCornerSegments;
                pOut->push_back(center + u * (kPlateCorner * std::cos(angle)) +
                                v * (kPlateCorner * std::sin(angle)));
            }
        }
    };

    // Draw order follows the child order: body fills, then the plates on top of them
    _pCubeXform->addChild(pFills.get());

    _plateColors.clear();
    for (int face = 0; face < 6; ++face)
    {
        // Centre first, then the perimeter closed back onto its first point: one fan
        osg::ref_ptr<osg::Vec3Array> pVertices = new osg::Vec3Array;
        pVertices->push_back(kMainNormals[face] * (1.0 + kPlateOffset));
        platePerimeter(face, 1.0 + kPlateOffset, pVertices.get());
        pVertices->push_back((*pVertices)[1]);

        osg::ref_ptr<osg::Vec4Array> pColors = new osg::Vec4Array;
        pColors->push_back(kPlateColor);
        _plateColors.push_back(pColors);

        osg::ref_ptr<osg::Geometry> pPlate = new osg::Geometry;
        pPlate->setUseDisplayList(false);
        pPlate->setUseVertexBufferObjects(true);
        pPlate->setVertexArray(pVertices.get());
        pPlate->setColorArray(pColors.get());
        pPlate->setColorBinding(osg::Geometry::BIND_OVERALL);
        pPlate->addPrimitiveSet(
            new osg::DrawArrays(GL_TRIANGLE_FAN, 0, static_cast<int>(pVertices->size())));
        // Unlit, like the labels: the card holds its own colour on every face instead of
        // brightening towards the light and clipping to white there
        pPlate->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        _pCubeXform->addChild(pPlate.get());
    }

    _pFillColors->resize(_pVertices->size());
    refreshColors();
}

void ViewCube::buildLabels()
{
    // lupdate only extracts literals, so these have to be written out one by one
    const QString texts[6] = {
        QCoreApplication::translate("ViewCube", "Front"),
        QCoreApplication::translate("ViewCube", "Back"),
        QCoreApplication::translate("ViewCube", "Left"),
        QCoreApplication::translate("ViewCube", "Right"),
        QCoreApplication::translate("ViewCube", "Top"),
        QCoreApplication::translate("ViewCube", "Bottom"),
    };

    // Upper case for display, the way SolidWorks shows them; a CJK label is unaffected. The
    // shared label height is the largest one that still fits the widest label across the
    // plate, so every face carries the same size text instead of each word filling its own
    // card -- "TOP" is drawn smaller than "BOTTOM" rather than both reaching the card edge.
    QString labels[6];
    QRect inks[6];
    double aspects[6] = {0.0};
    double widest = 1.0;
    for (int i = 0; i < 6; ++i)
    {
        labels[i] = texts[i].toUpper();
        inks[i] = labelInk(labels[i]);
        aspects[i] = std::max(1, inks[i].width()) /
                     static_cast<double>(std::max(1, inks[i].height()));
        widest = std::max(widest, aspects[i]);
    }
    const double halfV = std::min(kLabelHalfShort, kLabelHalfLong / widest);
    // Height a label is drawn at, in device pixels; the projection is fixed, so a cube-local
    // unit is kScale * kCubeSize / 2 px
    const double pxPerUnit = kScale * kCubeSize * 0.5;
    const double pixelHeight = kLabelBakeScale * 2.0 * halfV * pxPerUnit;

    osg::ref_ptr<osg::Group> pLabelGroup = new osg::Group;
    for (int i = 0; i < 6; ++i)
    {
        const osg::Vec3d& u = kLabelU[i];
        const osg::Vec3d& v = kLabelV[i];
        // Push out along the normal a little, to keep the label off its own face
        const osg::Vec3d center = kMainNormals[i] * (1.0 + kLabelOffset);

        double aspect = aspects[i];
        osg::ref_ptr<osg::Texture2D> pTexture =
            createLabelTexture(labels[i], pixelHeight, inks[i], aspect);
        // The aspect comes back from the texture, which the width cap can widen past the ink
        // aspect; the quad keeps the texture's ratio so the text is never stretched
        const double halfU = std::min(kLabelHalfLong, halfV * aspect);
        const double halfVHere = halfU / aspect;

        osg::ref_ptr<osg::Vec3Array> pVertices = new osg::Vec3Array;
        pVertices->push_back(center - u * halfU - v * halfVHere);
        pVertices->push_back(center + u * halfU - v * halfVHere);
        pVertices->push_back(center + u * halfU + v * halfVHere);
        pVertices->push_back(center - u * halfU + v * halfVHere);

        osg::ref_ptr<osg::Vec2Array> pTexCoords = new osg::Vec2Array;
        pTexCoords->push_back(osg::Vec2(0.0f, 0.0f));
        pTexCoords->push_back(osg::Vec2(1.0f, 0.0f));
        pTexCoords->push_back(osg::Vec2(1.0f, 1.0f));
        pTexCoords->push_back(osg::Vec2(0.0f, 1.0f));

        osg::ref_ptr<osg::Geometry> pGeom = new osg::Geometry;
        pGeom->setUseDisplayList(false);
        pGeom->setUseVertexBufferObjects(true);
        pGeom->setVertexArray(pVertices.get());
        pGeom->setTexCoordArray(0, pTexCoords.get());
        pGeom->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLE_FAN, 0, 4));

        osg::StateSet* pStateSet = pGeom->getOrCreateStateSet();
        pStateSet->setTextureAttributeAndModes(0, pTexture.get(), osg::StateAttribute::ON);
        // REPLACE: the fragment colour comes straight from the texture (alpha included),
        // which is then blended by that alpha. It also discards the lighting, which is what
        // keeps the glyphs one flat ink colour on every face however the cube is turned.
        pStateSet->setAttribute(new osg::TexEnv(osg::TexEnv::REPLACE));
        pStateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        pStateSet->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
                                        osg::StateAttribute::ON);

        pLabelGroup->addChild(pGeom.get());
    }

    // Added last, drawn last
    _pCubeXform->addChild(pLabelGroup.get());
}

void ViewCube::buildHomeButton()
{
    // The quad sits flush with the top left of the overlay viewport: its top edge lines up
    // with the cube square's top edge and its right edge is kHomeGap left of the cube square.
    const double left = kOverlayLeft;
    const double right = kOverlayLeft + kHomeButton * kEyePerPixel;
    const double top = 1.0;
    const double bottom = 1.0 - kHomeButton * kEyePerPixel;

    osg::ref_ptr<osg::Vec3Array> pVertices = new osg::Vec3Array;
    pVertices->push_back(osg::Vec3d(left, bottom, 0.0));
    pVertices->push_back(osg::Vec3d(right, bottom, 0.0));
    pVertices->push_back(osg::Vec3d(right, top, 0.0));
    pVertices->push_back(osg::Vec3d(left, top, 0.0));

    osg::ref_ptr<osg::Vec2Array> pTexCoords = new osg::Vec2Array;
    pTexCoords->push_back(osg::Vec2(0.0f, 0.0f));
    pTexCoords->push_back(osg::Vec2(1.0f, 0.0f));
    pTexCoords->push_back(osg::Vec2(1.0f, 1.0f));
    pTexCoords->push_back(osg::Vec2(0.0f, 1.0f));

    _pHomeColors = new osg::Vec4Array;
    _pHomeColors->push_back(kHomeColor);

    osg::ref_ptr<osg::Geometry> pGeom = new osg::Geometry;
    pGeom->setUseDisplayList(false);
    pGeom->setUseVertexBufferObjects(true);
    pGeom->setVertexArray(pVertices.get());
    pGeom->setTexCoordArray(0, pTexCoords.get());
    pGeom->setColorArray(_pHomeColors.get());
    pGeom->setColorBinding(osg::Geometry::BIND_OVERALL);
    pGeom->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLE_FAN, 0, 4));

    osg::ref_ptr<osg::Texture2D> pTexture = createHomeIconTexture();
    osg::StateSet* pStateSet = pGeom->getOrCreateStateSet();
    pStateSet->setTextureAttributeAndModes(0, pTexture.get(), osg::StateAttribute::ON);
    // MODULATE, not REPLACE: the texture is a white mask and the visible ink is the quad's
    // overall colour, so a hover only rewrites that colour and never re-bakes the glyph
    pStateSet->setAttribute(new osg::TexEnv(osg::TexEnv::MODULATE));
    pStateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    // A flat screen-space glyph: it has no depth relationship with the cube or the scene
    pStateSet->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    pStateSet->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
                                    osg::StateAttribute::ON);

    // Added to the camera directly, not under _pCubeXform: the glyph must stay upright while
    // the cube turns
    addChild(pGeom.get());
}

} // namespace wy3dApp
