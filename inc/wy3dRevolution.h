///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_REVOLUTION_H
#define WY3D_REVOLUTION_H

#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>
#include <wy3dTableIndex.h>

NS_WY3D_BEG

class WY3D_EXPORT Revolution : public wy3d::Solid
{
    WYDB_DECLARE_MEMBERS(Revolution, wy3d::Revolution, wy3d::Solid)

public:
    // 创建旋转体
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, const wy3d::SketchCurve* pAxis, double startAngle, double endAngle,
        Revolution*& pOutRevolution);

    // 创建旋转切除
    static wy::ErrorStatus createCut(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, const wy3d::SketchCurve* pAxis, double startAngle, double endAngle,
        wy3d::Solid* pSolidToCut,
        Revolution*& pOutRevolution);

    // 获取子元素
    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children;
        std::vector<wydb::ElementId> baseChildren = __baseClass::getChildren();
        children.reserve(baseChildren.size() + 1);
        children.push_back(_sketchId);
        children.insert(children.cend(), baseChildren.cbegin(), baseChildren.cend());
        return children;
    }

    // 获取草图
    wydb::ElementId getSketch() const { return _sketchId; }

    // 获取旋转轴
    wydb::ElementId getAxis() const { return _axisId; }
    // 设置旋转轴
    wy::ErrorStatus setAxis(const wy3d::SketchCurve* pAxis);

    // 获取起始角度
    double getStartAngle() const { return _startAngle; }
    // 设置起始角度
    wy::ErrorStatus setStartAngle(double startAngle);

    // 获取终止角度
    double getEndAngle() const { return _endAngle; }
    // 设置终止角度
    wy::ErrorStatus setEndAngle(double endAngle);

public:
    // 参数
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    // 事务
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;

    // 序列化
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;

    // 依赖
    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies) override;

    // 生成形体
    virtual TopoDS_Shape generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    // 设置草图
    wy::ErrorStatus _setSketch(const wydb::ElementId& sketchId);
    wy::ErrorStatus _setSketch(wy3d::Sketch* pSketch);
    // 设置旋转轴
    wy::ErrorStatus _setAxis(const wydb::ElementId& axisId);

protected:
    wydb::ElementId _sketchId;
    wydb::ElementId _axisId;
    double _startAngle;
    double _endAngle;
};

NS_WY3D_END

#endif // WY3D_REVOLUTION_H