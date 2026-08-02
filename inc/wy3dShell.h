///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SHELL_H
#define WY3D_SHELL_H

#include <vector>
#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dSolidModification.h>
#include <wy3dTableIndex.h>

NS_WY3D_BEG

// 抽壳方向
enum class ShellDirection : std::int32_t
{
    Inward  = 0,  // 向内抽壳
    Outward = 1,  // 向外抽壳
};

// 抽壳连接类型 — 仅暴露 Arc 和 Intersection
enum class ShellJoinType : std::int32_t
{
    Arc          = 0,  // 圆弧连接 (GeomAbs_Arc)
    Intersection = 1,  // 求交连接 (GeomAbs_Intersection)
};

// 抽壳偏移模式 — 1:1 映射 BRepOffset_Mode
enum class ShellOffsetMode : std::int32_t
{
    Skin       = 0,  // 沿表面偏移 (BRepOffset_Skin)
    Pipe       = 1,  // 管道偏移 (BRepOffset_Pipe, 仅用于曲线偏移, 不适用抽壳)
    RectoVerso = 2,  // 双面偏移 (BRepOffset_RectoVerso)
};

class WY3D_EXPORT Shell : public wy3d::SolidModification
{
    WYDB_DECLARE_MEMBERS(Shell, wy3d::Shell, wy3d::SolidModification)

public:
    // 创建抽壳
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Solid* pSolid,
        const std::vector<std::uint32_t>& faceIndices,
        double thickness,
        ShellDirection direction,
        Shell*& pOutShell);

    // 获取抽壳厚度
    double getThickness() const { return _thickness; }
    // 设置抽壳厚度
    wy::ErrorStatus setThickness(double thickness);

    // 获取抽壳方向
    ShellDirection getDirection() const { return _direction; }
    // 设置抽壳方向
    wy::ErrorStatus setDirection(ShellDirection direction);

    // 获取面集合
    const TopoNameList& getFaces() const { return _faceNames; }
    // 设置面集合
    wy::ErrorStatus setFaces(const TopoNameList& faces);

    // 获取抽壳连接类型
    ShellJoinType getJoinType() const { return _joinType; }
    // 设置抽壳连接类型
    wy::ErrorStatus setJoinType(ShellJoinType joinType);

    // 获取抽壳偏移模式
    ShellOffsetMode getOffsetMode() const { return _offsetMode; }
    // 设置抽壳偏移模式
    wy::ErrorStatus setOffsetMode(ShellOffsetMode offsetMode);

    // 获取是否启用全局求交处理
    bool getIntersection() const { return _intersection; }
    // 设置是否启用全局求交处理 (对应 OCCT Intersection, 未完整实现, 不推荐启用)
    wy::ErrorStatus setIntersection(bool intersection);

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual std::pair<bool, TopoDS_Shape> modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    TopoNameList _faceNames;
    double _thickness;
    ShellDirection _direction;
    ShellJoinType _joinType;
    ShellOffsetMode _offsetMode;
    bool _intersection;
};

NS_WY3D_END

#endif // WY3D_SHELL_H
