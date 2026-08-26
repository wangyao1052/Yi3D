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

#pragma once

#include <osg/Node>
#include <osg/NodeCallback>
#include <osg/ShapeDrawable>
#include <osgText/Text>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Edge.hxx>

// OSG工具类
class OsgUtils
{
public:
	static osg::Node* createBox();

	static osg::Node* BuildShapeMesh(
		const TopoDS_Shape& aShape,
		double deflection = 0.1);

    static osg::Node* BuildShapeEdges(
        const TopoDS_Shape& shape,
        double deflection = 0.1);

	static osg::Node* BuildEdgeLine(
		const TopoDS_Edge& edge,
		double deflection = 0.1);

	static bool setNodeColor(osg::Node* node, const osg::Vec4& color);

	// 创建Box
	static osg::Node* createShapeDrawable_Box(
        float length, float width, float height, const osg::Vec4& color);
    // 创建Cylinder
	static osg::Node* createShapeDrawable_Cylinder(
		float radius, float height, const osg::Vec4& color);
    // 创建Cone
	static osg::Node* createShapeDrawable_Cone(
		float radius, float height, const osg::Vec4& color);
    // 创建Sphere
	static osg::Node* createShapeDrawable_Sphere(
		float radius, const osg::Vec4& color);
	// 直线段
	static osg::Node* createGeometry_Line(
		const osg::Vec3d& startPoint,
		const osg::Vec3d& endPoint,
		const osg::Vec4& color = osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
		float lineWidth = 1.0);
	// 点
	static osg::Node* createGeometry_Point(
		const osg::Vec3d& position,
		const osg::Vec4& color = osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f),
		float pointSize = 1.0);

	// 单行文本
	static osgText::Text* create_Text(
		const osg::Vec3d& position);
	// 多行文本
	static osg::Geode* create_MultiLinesTexts(
		const osg::Vec3d& position);
	// 抬头显示文本
	static osg::Camera* create_HUDTexts();
	static osg::Camera* create_LeftTopHUDTexts(osg::View* view);

    // 渲染时跳过该节点及其子树(cull)，拾取等其他遍历(IntersectionVisitor不咨询cull回调)仍正常访问
    // 用于"着色无边框"模式下隐藏边但保持边可拾取
    // 必须挂在Group等Node节点上:CullVisitor的Node路径(handle_cull_callbacks_and_traverse)不traverse即不渲染,
    // 已用探针验证;本OSG fork中Drawable路径的cull回调行为不可见,不要挂在Geometry上
    class SkipRenderCallback : public osg::NodeCallback
    {
    public:
        virtual bool run(osg::Object* object, osg::Object* data) override
        {
            osg::NodeVisitor* nv = dynamic_cast<osg::NodeVisitor*>(data);
            if (nv && nv->getVisitorType() == osg::NodeVisitor::CULL_VISITOR)
                return true; // 不traverse → 不渲染
            return traverse(object, data);
        }
    };
};