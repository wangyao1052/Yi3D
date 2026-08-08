///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_EXTRUSION_H
#define WY3D_EXTRUSION_H

#include <wy3dDefs.h>
#include <wy3dSolid.h>

NS_WY3D_BEG

class Sketch;

class WY3D_EXPORT Extrusion : public wy3d::Solid
{
    WYDB_DECLARE_MEMBERS(Extrusion, wy3d::Extrusion, wy3d::Solid)

public:
    // 创建拉伸体
    // 可能的返回值:Ok or NullDatabasePointer or NullElementPointer or InvalidInput.
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, double depth,
        Extrusion*& pOutExtrusion);

    // 创建拉伸切除
    // 可能的返回值:Ok or NullDatabasePointer or NullElementPointer or InvalidInput.
    static wy::ErrorStatus createCut(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, double depth,
        wy3d::Solid* pSolidToCut,
        Extrusion*& pOutExtrusion);

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
    const wydb::ElementId& getSketch() const { return _sketchId; }

    // 获取拉伸深度
    double getDepth() const { return _depth; }
    // 设置拉伸深度
    wy::ErrorStatus setDepth(double depth);

    // 获取起始偏移
    double getStartOffset() const { return _startOffset; }
    // 设置起始偏移
    wy::ErrorStatus setStartOffset(double startOffset);

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
    // 设置草图
    // only called by Extrusion static create function.
    wy::ErrorStatus _setSketch(wy3d::Sketch* pSketch);

protected:
    // 草图
    wydb::ElementId _sketchId;
    // 拉伸深度
    double _depth;
    // 起始偏移
    double _startOffset;
};

NS_WY3D_END

#endif // WY3D_EXTRUSION_H