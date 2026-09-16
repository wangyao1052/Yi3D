///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SOLID_MODIFICATION_H
#define WY3D_SOLID_MODIFICATION_H

#include <TopoDS_Shape.hxx>
#include <wy3dDefs.h>
#include <wy3dFeature.h>
#include <wy3dTopoNaming.h>

NS_WY3D_BEG

class Solid;
class Sheet;

// 就地修改一个形体。宿主是实体还是片体由数据决定, 不由类型决定: 实体和片体各有自己的修改链,
// 都按 _ownerId 找到这里调用 modifyOwnerShape, 而 modifyOwnerShape 只认 TopoDS_Shape。
class WY3D_EXPORT BodyModification : public wy3d::Feature
{
    WYDB_DECLARE_ABSTRACT_MEMBERS(BodyModification, wy3d::BodyModification, wy3d::Feature)

public:
    // 获取主体
    virtual wydb::ElementId getParent() const override { return _ownerId; }

    // 获取新生成的面在宿主Shape面中的索引
    std::vector<std::uint32_t> getNewFaceIndices() const;

public:

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

protected:
    // 修改宿主形体
    virtual std::pair<bool, TopoDS_Shape> modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
    {
        this->clearNewFaces();
        return std::pair<bool, TopoDS_Shape>(true, shape);
    }

    // 用实例修改形体
    virtual std::pair<bool, TopoDS_Shape> modifyOwnerShapeByInstance(
        const TopoDS_Shape& shape, TopoNaming* pTopoNaming,
        const TopoDS_Shape& instShape, TopoNaming* pInstNaming,
        bool isCut,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector);

    const wy3d::Solid* getSolidHost() const;
    const wy3d::Sheet* getSheetHost() const;

    // 获取新生成的面
    const TopoNameList& getNewFaces() const { return _newFaces; }
    // 设置新生成的面
    wy::ErrorStatus setNewFaces(const TopoNameList& newFaces);
    // 记录新生成的面
    void recordNewFaces(const ShapeDelta& faceDelta, TopoNaming* pTopoNaming);

    // added by wangyao 2025.09.10 {
    // 清空新生成的面
    wy::ErrorStatus clearNewFaces();
    // 添加新生成的面
    void appendNewFaces(const ShapeDelta& faceDelta, TopoNaming* pTopoNaming, const TopoDS_Shape& instShape);
    // }

private:
    // 设置主体
    wy::ErrorStatus _setOwner(const wydb::ElementId& ownerId);

private:
    // 所属的宿主(实体或片体)
    wydb::ElementId _ownerId;

    // 新生成的面也即关联的面
    TopoNameList _newFaces;

    friend class Solid;
    friend class Sheet;
};

NS_WY3D_END

#endif // WY3D_SOLID_MODIFICATION_H
