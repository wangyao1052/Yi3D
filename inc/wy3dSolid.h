///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SOLID_H
#define WY3D_SOLID_H

#include <TopoDS_Shape.hxx>
#include <vector>
#include <set>

#include <wy3dVector3.h>
#include <wy3dDefs.h>
#include <wy3dFeature.h>
#include <wy3dTopoNaming.h>
#include <wy3dSolidModification.h>
#include <wy3dColor.h>

NS_WY3D_BEG

enum class SolidFlag : std::uint32_t
{
    Cut = 0x00000001, // 实体切除
};

class WY3D_EXPORT Solid : public wy3d::Feature
{
    WYDB_DECLARE_ABSTRACT_MEMBERS(Solid, wy3d::Solid, wy3d::Feature)

public:
    // 获取主体
    virtual wydb::ElementId getParent() const override { return _ownerId; }
    // 设置主体
    wy::ErrorStatus setOwner(const wydb::ElementId& ownerId);

    // 获取子元素
    virtual std::vector<wydb::ElementId> getChildren() const override { return _modifications; }

    // 获取是否是切除材料
    bool isCut() const { return _solidFlags & static_cast<std::uint32_t>(SolidFlag::Cut); }
    // 设置是否切除材料
    // 目前仅仅在创建拉伸体时调用,不支持创建完成后再调用该函数更改是否切除材料;
    wy::ErrorStatus setCut(bool isCut);

    // 获取颜色
    wy3d::Color getColor() const { return _color; }
    // 设置颜色
    wy::ErrorStatus setColor(const wy3d::Color& color);

    // 获取拓扑表达
    virtual const TopoDS_Shape& getShape() const { return _shape; }

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

public:
    // 获取拓扑命名
    const TopoNaming* getTopoNaming() const
    {
        return _pTopoNaming.get();
    }
    TopoNaming* getTopoNaming()
    {
        return _pTopoNaming.get();
    }
    // 设置拓扑命名
    wy::ErrorStatus setTopoNaming(TopoNamingSPtr pTopoNaming);

    // 添加实体修改
    wy::ErrorStatus addModification(wy3d::SolidModification* pModification);
    wy::ErrorStatus addModification(wy3d::Solid* pCutSolid);
    // 获取实体修改
    const std::vector<wydb::ElementId>& getModifications() const { return _modifications; }

    // 获取新生成的面
    const TopoNameList& getNewFaces() const { return _newFaces; }
    // 获取新生成的面在Solid面中的索引
    std::vector<std::uint32_t> getNewFaceIndices() const;

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

    // 报告所有的级联更新数据块以及它们的依赖项
    virtual void reportChainUpdateDataPieces(wydb::ElementDataPieceCollector& dps) const override;

    // 级联更新响应函数
    virtual void onChainUpdater_Completion(
        const wydb::ElementDataPiece& dirtyDataPiece,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector);
    virtual TopoDS_Shape generateShape(
        TopoNaming* pTopoNaming,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector);

    // 记录新生成的面
    void recordNewFaces(const ShapeDelta& faceDelta, TopoNaming* pTopoNaming);

protected:
    // 设置拓扑表达
    // can only be called by onChainUpdater_Completion
    wy::ErrorStatus setShape(const TopoDS_Shape& shape);

    // 修改形体
    std::pair<bool, TopoDS_Shape> modifyShape(
        const TopoDS_Shape& shape,
        TopoNaming* pTopoNaming,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector);

    // 修改主体形体
    std::pair<bool, TopoDS_Shape> modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector);

private:
    // 设置实体修改特征集合(内部调用)
    wy::ErrorStatus _setModifications(const std::vector<wydb::ElementId>& modifications);

    // 设置新生成的面(内部调用)
    wy::ErrorStatus _setNewFaces(const TopoNameList& newFaces);

protected:
    // 实体选项
    std::uint32_t _solidFlags; // 和ElementImpl中的_flags区分开
    // 所属的主体
    wydb::ElementId _ownerId;
    // 拓扑命名
    TopoNamingSPtr _pTopoNaming;
    // 实体修改特征集合
    std::vector<wydb::ElementId> _modifications;
    // 颜色
    wy3d::Color _color;

    // 当实体本身用于修改(切除&合并)其它实体时,以下数据有效;否则为空.
    // 新生成的面也即关联的面
    TopoNameList _newFaces;

    // 最终的形状
    TopoDS_Shape _shape;
};

NS_WY3D_END

#endif // WY3D_SOLID_H
