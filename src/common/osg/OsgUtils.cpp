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

#include "OsgUtils.h"

#include <cassert>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/ShapeDrawable>
#include <osg/Shape>
#include <osg/PositionAttitudeTransform>
#include <osg/LineWidth>
#include <osg/Point>
#include <osg/MatrixTransform>

#include <BRepMesh_IncrementalMesh.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <BRep_Tool.hxx>
#include <TopExp.hxx>

// 顶点坐标
const float boxVertices[][3] = {
	// Top
	{ 1.0f, 1.0f, -1.0f },
	{ -1.0f, 1.0f, -1.0f },
	{ -1.0f, 1.0f, 1.0f },
	{ 1.0f, 1.0f, 1.0f },
	// Bottom
	{ 1.0f, -1.0f, 1.0f },
	{ -1.0f, -1.0f, 1.0f },
	{ -1.0f, -1.0f, -1.0f },
	{ 1.0f, -1.0f, -1.0f },
	// Front
	{ 1.0f, 1.0f, 1.0f },
	{ -1.0f, 1.0f, 1.0f },
	{ -1.0f, -1.0f, 1.0f },
	{ 1.0f, -1.0f, 1.0f },
	// Back
	{ 1.0f, -1.0f, -1.0f },
	{ -1.0f, -1.0f, -1.0f },
	{ -1.0f, 1.0f, -1.0f },
	{ 1.0f, 1.0f, -1.0f },
	// Left
	{ -1.0f, 1.0f, 1.0f },
	{ -1.0f, 1.0f, -1.0f },
	{ -1.0f, -1.0f, -1.0f },
	{ -1.0f, -1.0f, 1.0f },
	// Right
	{ 1.0f, 1.0f, -1.0f },
	{ 1.0f, 1.0f, 1.0f },
	{ 1.0f, -1.0f, 1.0f },
	{ 1.0f, -1.0f, -1.0f }
};

// 法线
const float boxNormals[][3] = {
    {  0.0f,  1.0f,  0.0f },
    {  0.0f, -1.0f,  0.0f },
    {  0.0f,  0.0f,  1.0f },
    {  0.0f,  0.0f, -1.0f },
    { -1.0f,  0.0f,  0.0f },
    {  1.0f,  0.0f,  0.0f }
};

// 颜色
const float boxColors[][3] = {
	{ 0.0f, 1.0f, 0.0f },
	{ 1.0f, 0.5f, 0.0f },
	{ 1.0f, 0.0f, 0.0f },
	{ 1.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f },
	{ 1.0f, 0.0f, 1.0f }
};

osg::Node* OsgUtils::createBox()
{
	osg::Geode* geode = new osg::Geode();

	osg::ref_ptr<osg::Geometry> boxGeometry = new osg::Geometry();
	boxGeometry->setUseDisplayList(false);
	{
		// vertices
		osg::Vec3Array* vertices = new osg::Vec3Array();
		int nVertices = sizeof(boxVertices) / sizeof(boxVertices[0]);
		for (int i = 0; i < nVertices; ++i)
		{
			vertices->push_back(osg::Vec3(boxVertices[i][0] * 10, boxVertices[i][1] * 10, boxVertices[i][2] * 10));
		}
		boxGeometry->setVertexArray(vertices);

        // normals
        // 法向设置对光照效果才是好的
        osg::Vec3Array* normals = new osg::Vec3Array();
        int nNormals = sizeof(boxNormals) / sizeof(boxNormals[0]);
        for (int i = 0; i < nNormals; ++i)
        {
            normals->push_back(osg::Vec3(boxNormals[i][0], boxNormals[i][1], boxNormals[i][2]));
        }
        boxGeometry->setNormalArray(normals, osg::Array::BIND_PER_PRIMITIVE_SET);

		// colors
		osg::Vec4Array* colors = new osg::Vec4Array();
		int nColors = sizeof(boxColors) / sizeof(boxColors[0]);
		for (int i = 0; i < nColors; ++i)
		{
			colors->push_back(osg::Vec4(boxColors[i][0], boxColors[i][1], boxColors[i][2], 1.0f));
		}
		boxGeometry->setColorArray(colors, osg::Array::BIND_PER_PRIMITIVE_SET);

		// primitive set
		int nVertsPerPrimitiveSet = 4;
		for (int i = 0, pos = 0; i < nVertices / nVertsPerPrimitiveSet; ++i, pos += nVertsPerPrimitiveSet)
		{
			boxGeometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, pos, nVertsPerPrimitiveSet));
		}
	}
	geode->addDrawable(boxGeometry);

	return geode;
}

osg::Node* OsgUtils::BuildShapeMesh(
	const TopoDS_Shape& aShape,
	double deflection)
{
	osg::ref_ptr<osg::Geode> geode = new osg::Geode();
	osg::ref_ptr<osg::Geometry> triGeom = new osg::Geometry();
	osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
	osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();

	BRepMesh_IncrementalMesh(aShape, deflection);
	for (TopExp_Explorer faceExplorer(aShape, TopAbs_FACE); faceExplorer.More(); faceExplorer.Next())
	{
		TopLoc_Location loc;
		TopoDS_Face aFace = TopoDS::Face(faceExplorer.Current());
		Handle_Poly_Triangulation triFace = BRep_Tool::Triangulation(aFace, loc);
		if (triFace.IsNull())
		{
			// The face has no triangulation, e.g. meshing failed.
			assert(false);
			continue;
		}
		Standard_Integer nTriangles = triFace->NbTriangles();

		gp_Pnt vertex1;
		gp_Pnt vertex2;
		gp_Pnt vertex3;
		Standard_Integer nVertexIndex1 = 0;
		Standard_Integer nVertexIndex2 = 0;
		Standard_Integer nVertexIndex3 = 0;

		TColgp_Array1OfPnt nodes(1, triFace->NbNodes());
		Poly_Array1OfTriangle triangles(1, triFace->NbTriangles());
		triangles = triFace->Triangles();

		for (Standard_Integer i = 1; i <= nTriangles; i++)
		{
			Poly_Triangle aTriangle = triangles.Value(i);
			aTriangle.Get(nVertexIndex1, nVertexIndex2, nVertexIndex3);
			vertex1 = triFace->Node(nVertexIndex1);
			vertex2 = triFace->Node(nVertexIndex2);
			vertex3 = triFace->Node(nVertexIndex3);

            // TODO  优化
            {
                gp_XYZ xyz = vertex1.XYZ();
                aShape.Location().Transformation().Transforms(xyz);
                vertex1.SetXYZ(xyz);
            }
            {
                gp_XYZ xyz = vertex2.XYZ();
                aShape.Location().Transformation().Transforms(xyz);
                vertex2.SetXYZ(xyz);
            }
            {
                gp_XYZ xyz = vertex3.XYZ();
                aShape.Location().Transformation().Transforms(xyz);
                vertex3.SetXYZ(xyz);
            }


			gp_XYZ vector12(vertex2.XYZ() - vertex1.XYZ());
			gp_XYZ vector13(vertex3.XYZ() - vertex1.XYZ());
			gp_XYZ normal = vector12.Crossed(vector13);
			Standard_Real rModulus = normal.Modulus();

			if (rModulus > gp::Resolution())
			{
				normal.Normalize();
			}
			else
			{
				normal.SetCoord(0., 0., 0.);
			}

			vertices->push_back(osg::Vec3(vertex1.X(), vertex1.Y(), vertex1.Z()));
			vertices->push_back(osg::Vec3(vertex2.X(), vertex2.Y(), vertex2.Z()));
			vertices->push_back(osg::Vec3(vertex3.X(), vertex3.Y(), vertex3.Z()));
			normals->push_back(osg::Vec3(normal.X(), normal.Y(), normal.Z()));
		}
	}
	triGeom->setUseDisplayList(false);
	triGeom->setVertexArray(vertices.get());
	triGeom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, vertices->size()));
	triGeom->setNormalArray(normals);
	triGeom->setNormalBinding(osg::Geometry::BIND_PER_PRIMITIVE_SET);

	geode->addDrawable(triGeom);

	return geode.release();
}

osg::Node* OsgUtils::BuildShapeEdges(
    const TopoDS_Shape& shape,
    double deflection)
{
    if (shape.IsNull())
    {
        return nullptr;
    }

    osg::ref_ptr<osg::Group> pGroup = new osg::Group();
    TopTools_IndexedMapOfShape indexMapOfShape;
    TopExp::MapShapes(shape, TopAbs_EDGE, indexMapOfShape);
    for (int i = 1; i <= indexMapOfShape.Extent(); ++i)
    {
        const TopoDS_Shape& shape = indexMapOfShape.FindKey(i);
        TopoDS_Edge edge = TopoDS::Edge(shape);
        if (edge.IsNull()) continue;

        osg::Node* pNode = BuildEdgeLine(edge, deflection);
        if (pNode)
        {
            pGroup->addChild(pNode);
        }
    }

    return pGroup.release();
}

osg::Node* OsgUtils::BuildEdgeLine(
	const TopoDS_Edge& edge,
	double deflection)
{
	BRepMesh_IncrementalMesh(edge, deflection);
	TopLoc_Location loc;
	Handle(Poly_Polygon3D) aPoly = BRep_Tool::Polygon3D(edge, loc);
	if (aPoly.IsNull())
	{
		return NULL;
	}

	osg::ref_ptr<osg::Geode> geode = new osg::Geode();
	osg::ref_ptr<osg::Geometry> triGeom = new osg::Geometry();
	osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
	osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();

	gp_Trsf transf = loc.Transformation();
	Standard_Integer nbNodes = aPoly->NbNodes();
	vertices->reserve(nbNodes);
	const TColgp_Array1OfPnt& Nodes = aPoly->Nodes();
	gp_Pnt V;
	for (Standard_Integer i = 1; i <= nbNodes; i++)
	{
		V = Nodes(i);
		V.Transform(transf);
		vertices->push_back(osg::Vec3(V.X(), V.Y(), V.Z()));
	}
	colors->push_back(osg::Vec4(0.0, 0.0, 0.0, 1.0));

	triGeom->setUseDisplayList(false);
	triGeom->setVertexArray(vertices.get());
	triGeom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP, 0, vertices->size()));
	triGeom->setColorArray(colors.get(), osg::Array::BIND_OVERALL);
	geode->addDrawable(triGeom);

	return geode.release();
}

bool OsgUtils::setNodeColor(osg::Node* pNode, const osg::Vec4& color)
{
    osg::Group* pGroup = dynamic_cast<osg::Group*>(pNode);
    if (pGroup)
    {
        unsigned int num = pGroup->getNumChildren();
        for (unsigned int i = 0; i < num; ++i)
        {
            osg::Node* pChild = pGroup->getChild(i);
            if (!pChild) continue;
            OsgUtils::setNodeColor(pChild, color);
        }

        return true;
    }
    else
    {
        osg::Geometry* pGeom = dynamic_cast<osg::Geometry*>(pNode);
        if (!pGeom)
        {
            return false;
        }
        osg::Vec4Array* pColorVec4Arr = dynamic_cast<osg::Vec4Array*>(pGeom->getColorArray());
        if (pColorVec4Arr)
        {
            pColorVec4Arr->assign(pColorVec4Arr->size(), color);
            pGeom->dirtyGLObjects();
        }
        else
        {
            osg::Vec4Array* colors = new osg::Vec4Array();
            colors->push_back(color);
            pGeom->setColorArray(colors);
            pGeom->setColorBinding(osg::Geometry::BIND_OVERALL);
        }

        return true;
    }
}

osg::Node* OsgUtils::createShapeDrawable_Box(
    float length, float width, float height, const osg::Vec4& color)
{
	osg::ShapeDrawable* pShapeDrawable = new osg::ShapeDrawable();
	osg::Box* pBox = new osg::Box(osg::Vec3(length/2, width/2, height/2), length, width, height);
    pShapeDrawable->setShape(pBox);
    pShapeDrawable->setColor(color);
	return pShapeDrawable;
}

osg::Node* OsgUtils::createShapeDrawable_Cylinder(
	float radius, float height, const osg::Vec4& color)
{
	osg::ShapeDrawable* pShapeDrawable = new osg::ShapeDrawable();
	osg::Cylinder* pCylinder = new osg::Cylinder(osg::Vec3(0, 0, height/2), radius, height);
    pShapeDrawable->setShape(pCylinder);
    pShapeDrawable->setColor(color);
	return pShapeDrawable;
}

osg::Node* OsgUtils::createShapeDrawable_Cone(
	float radius, float height, const osg::Vec4& color)
{
	osg::ShapeDrawable* pShapeDrawable = new osg::ShapeDrawable();
	osg::Cone* pCone = new osg::Cone(osg::Vec3(0, 0, 0), radius, height);
    pCone->setCenter(osg::Vec3(0, 0, -pCone->getBaseOffset()));
    pShapeDrawable->setShape(pCone);
    pShapeDrawable->setColor(color);
	return pShapeDrawable;
}

osg::Node* OsgUtils::createShapeDrawable_Sphere(
	float radius, const osg::Vec4& color)
{
	osg::ShapeDrawable* pShapeDrawable = new osg::ShapeDrawable();
	osg::Sphere* pSphere = new osg::Sphere(osg::Vec3(0, 0, 0), radius);
    pShapeDrawable->setShape(pSphere);
    pShapeDrawable->setColor(color);
	return pShapeDrawable;
}

osg::Node* OsgUtils::createGeometry_Line(
	const osg::Vec3d& startPoint,
	const osg::Vec3d& endPoint,
	const osg::Vec4& color,
	float lineWidth)
{
	osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
	//geom->setDataVariance(osg::Object::DYNAMIC);
	geom->setUseDisplayList(false);
	geom->setUseVertexBufferObjects(true);
	{
		osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
		vertices->push_back(startPoint);
		vertices->push_back(endPoint);
		geom->setVertexArray(vertices);
		osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
		colors->push_back(color);
		geom->setColorArray(colors);
		geom->setColorBinding(osg::Geometry::BIND_OVERALL);
	}
	geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINES, 0, 2));
	geom->getOrCreateStateSet()->setAttribute(new osg::LineWidth(lineWidth));

	osg::ref_ptr<osg::Geode> geode = new osg::Geode();
	//geode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
	geode->addDrawable(geom);

	return geode.release();
}

osg::Node* OsgUtils::createGeometry_Point(
	const osg::Vec3d& position,
	const osg::Vec4& color,
	float pointSize)
{
	osg::ref_ptr<osg::Geometry> geom = new osg::Geometry();
	//geom->setDataVariance(osg::Object::DYNAMIC);
	geom->setUseDisplayList(false);
	geom->setUseVertexBufferObjects(true);
	{
		osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
		vertices->push_back(position);
		geom->setVertexArray(vertices);
		osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
		colors->push_back(color);
		geom->setColorArray(colors);
		geom->setColorBinding(osg::Geometry::BIND_OVERALL);
	}
	geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, 1));
	geom->getOrCreateStateSet()->setAttribute(new osg::Point(pointSize));

	osg::ref_ptr<osg::Geode> geode = new osg::Geode();
	//geode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
	geode->addDrawable(geom);

	return geode.release();
}

osgText::Text* OsgUtils::create_Text(
	const osg::Vec3d& position)
{
	// 微软雅黑
	osg::ref_ptr<osgText::Font> font = osgText::readFontFile("fonts/msyh.ttc");

	// 文本
	osg::ref_ptr<osgText::Text> text = new osgText::Text();
	text->setFont(font.get());
	text->setCharacterSize(10.0f);
	text->setAxisAlignment(osgText::TextBase::XY_PLANE);
	text->setAlignment(osgText::TextBase::LEFT_BOTTOM);
	text->setColor(osg::Vec4d(1.0, 0.0, 0.0, 1.0));
	text->setPosition(position);
	text->setText(L"中国武汉 1234.5678");

	return text.release();
}

osg::Geode* OsgUtils::create_MultiLinesTexts(
	const osg::Vec3d& position)
{
	// 仿宋
	osg::ref_ptr<osgText::Font> font = osgText::readFontFile("fonts/simfang.ttf");

	// 多行文本
	//
	osg::ref_ptr<osgText::Text> text1 = new osgText::Text();
	text1->setFont(font.get());
	text1->setCharacterSize(10.0f);
	text1->setAxisAlignment(osgText::TextBase::XZ_PLANE);
	text1->setAlignment(osgText::TextBase::LEFT_BOTTOM);
	text1->setColor(osg::Vec4d(1.0, 0.0, 0.0, 1.0));
	text1->setPosition(position);
	text1->setText(L"中国武汉 1234.5678");
	//
	osg::ref_ptr<osgText::Text> text2 = new osgText::Text();
	text2->setFont(font.get());
	text2->setCharacterSize(10.0f);
	text2->setAxisAlignment(osgText::TextBase::XZ_PLANE);
	text2->setAlignment(osgText::TextBase::LEFT_BOTTOM);
	text2->setColor(osg::Vec4d(1.0, 0.0, 0.0, 1.0));
	text2->setPosition(position + osg::Vec3d(0.0, 0.0, 15.0));
	text2->setText(L"Hello Wolrd!");
	//
	osg::ref_ptr<osgText::Text> text3 = new osgText::Text();
	text3->setFont(font.get());
	text3->setCharacterSize(15.0f);
	text3->setAxisAlignment(osgText::TextBase::XZ_PLANE);
	text3->setAlignment(osgText::TextBase::LEFT_BOTTOM);
	text3->setColor(osg::Vec4d(0.0, 0.0, 0.0, 1.0));
	text3->setPosition(position + osg::Vec3d(0.0, 0.0, 40.0));
	text3->setText(L"中国武汉 1234.5678");

	//
	osg::ref_ptr<osg::Geode> geode = new osg::Geode();
	geode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
	geode->addDrawable(text1.get());
	geode->addDrawable(text2.get());
	geode->addDrawable(text3.get());

	return geode.release();
}

// 抬头显示文本
osg::Camera* OsgUtils::create_HUDTexts()
{
	// 文本范围
	double left(0.0), right(200.0), bottom(0.0), top(60.0);

	// 头显摄像机
	osg::ref_ptr<osg::Camera> camera = new osg::Camera();
	camera->setRenderOrder(osg::Camera::POST_RENDER);
	camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
	camera->setAllowEventFocus(false);
	camera->setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
	camera->setClearMask(GL_DEPTH_BUFFER_BIT);
	camera->setViewport(5.0, 5.0, (right - left)*3, (top - bottom)*3);
	camera->setProjectionMatrixAsOrtho2D(left, right, bottom, top);

	// 多行文本
	osg::ref_ptr<osg::Geode> geode = new osg::Geode();
	geode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
	{
		// 仿宋
		osg::ref_ptr<osgText::Font> font = osgText::readFontFile("fonts/simfang.ttf");

		//
		osg::ref_ptr<osgText::Text> text1 = new osgText::Text();
		text1->setFont(font.get());
		text1->setCharacterSize(10.0f);
		text1->setAxisAlignment(osgText::TextBase::XY_PLANE);
		text1->setAlignment(osgText::TextBase::LEFT_BOTTOM);
		text1->setColor(osg::Vec4d(1.0, 0.0, 0.0, 1.0));
		text1->setPosition(osg::Vec3d(0.0, 0.0, 0.0));
		text1->setText(L"中国武汉 1234.5678");
		//
		osg::ref_ptr<osgText::Text> text2 = new osgText::Text();
		text2->setFont(font.get());
		text2->setCharacterSize(10.0f);
		text2->setAxisAlignment(osgText::TextBase::XY_PLANE);
		text2->setAlignment(osgText::TextBase::LEFT_BOTTOM);
		text2->setColor(osg::Vec4d(0.0, 0.0, 0.0, 1.0));
		text2->setPosition(osg::Vec3d(0.0, 15.0, 0.0));
		text2->setText(L"Hello Wolrd!");
		//
		osg::ref_ptr<osgText::Text> text3 = new osgText::Text();
		text3->setFont(font.get());
		text3->setCharacterSize(10.0f);
		text3->setAxisAlignment(osgText::TextBase::XY_PLANE);
		text3->setAlignment(osgText::TextBase::LEFT_BOTTOM);
		text3->setColor(osg::Vec4d(0.0, 0.0, 1.0, 1.0));
		text3->setPosition(osg::Vec3d(0.0, 30.0, 0.0));
		text3->setText(L"中国武汉 1234.5678");

		//
		geode->addDrawable(text1.get());
		geode->addDrawable(text2.get());
		geode->addDrawable(text3.get());
	}
	camera->addChild(geode.get());

	return camera.release();
}

class HudTexts : public osg::Camera
{
public:
	class HudCameraDrawCallback : public osg::Camera::DrawCallback
	{
	public:
		HudCameraDrawCallback(HudTexts* hud)
			: osg::Camera::DrawCallback(), hud_(hud)
		{}

		virtual void operator () (osg::RenderInfo& renderInfo) const
		{
			const osg::GraphicsContext::Traits* traits = hud_->view_->getCamera()->getGraphicsContext()->getTraits();
			double space(5.0);
			double vpWidth = hud_->width_ * 3;
			double vpHeight = hud_->height_ * 3;
			hud_->setViewport(space, traits->height - space - vpHeight, vpWidth, vpHeight);
			hud_->setProjectionMatrixAsOrtho2D(0.0, hud_->width_, 0.0, hud_->height_);
		}

	private:
		osg::ref_ptr<HudTexts> hud_;
	};

public:
	HudTexts(osg::View* view) : osg::Camera(),
		view_(view), width_(200.0), height_(60.0)
	{
		const osg::GraphicsContext::Traits* traits = view_->getCamera()->getGraphicsContext()->getTraits();

		this->setRenderOrder(osg::Camera::POST_RENDER);
		this->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
		this->setAllowEventFocus(false);
		this->setClearColor(osg::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
		this->setClearMask(GL_DEPTH_BUFFER_BIT);
		double space(5.0);
		double vpWidth = width_ * 3;
		double vpHeight = height_ * 3;
		this->setViewport(space, traits->height - space - vpHeight, vpWidth, vpHeight);
		this->setProjectionMatrixAsOrtho2D(0.0, width_, 0.0, height_);
		this->setPreDrawCallback(new HudCameraDrawCallback(this));

		// 文本
		// 多行文本
		osg::ref_ptr<osg::Geode> geode = new osg::Geode();
		geode->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
		this->addChild(geode.get());
		{
			// 仿宋
			osg::ref_ptr<osgText::Font> font = osgText::readFontFile("fonts/simfang.ttf");

			//
			osg::Vec3d pos(0, height_, 0);
			osg::ref_ptr<osgText::Text> text1 = new osgText::Text();
			text1->setFont(font.get());
			text1->setCharacterSize(10.0f);
			text1->setAxisAlignment(osgText::TextBase::XY_PLANE);
			text1->setAlignment(osgText::TextBase::LEFT_TOP);
			text1->setColor(osg::Vec4d(0.0, 0.0, 0.0, 1.0));
			text1->setPosition(pos);
			text1->setText(L"中国武汉 1234.5678");
			//
			pos -= osg::Vec3d(0, 15, 0);
			osg::ref_ptr<osgText::Text> text2 = new osgText::Text();
			text2->setFont(font.get());
			text2->setCharacterSize(10.0f);
			text2->setAxisAlignment(osgText::TextBase::XY_PLANE);
			text2->setAlignment(osgText::TextBase::LEFT_TOP);
			text2->setColor(osg::Vec4d(0.0, 0.0, 0.0, 1.0));
			text2->setPosition(pos);
			text2->setText(L"Hello Wolrd!");
			//
			pos -= osg::Vec3d(0, 15, 0);
			osg::ref_ptr<osgText::Text> text3 = new osgText::Text();
			text3->setFont(font.get());
			text3->setCharacterSize(10.0f);
			text3->setAxisAlignment(osgText::TextBase::XY_PLANE);
			text3->setAlignment(osgText::TextBase::LEFT_TOP);
			text3->setColor(osg::Vec4d(0.0, 0.0, 0.0, 1.0));
			text3->setPosition(pos);
            text3->setText(L"中国武汉 1234.5678");

			//
			geode->addDrawable(text1.get());
			geode->addDrawable(text2.get());
			geode->addDrawable(text3.get());
		}
	}

public:
	osg::View* view_;
	double width_;
	double height_;
};

osg::Camera* OsgUtils::create_LeftTopHUDTexts(osg::View* view)
{
	return new HudTexts(view);
}