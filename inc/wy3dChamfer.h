///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_CHAMFER_H
#define WY3D_CHAMFER_H

#include <vector>
#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dSolidModification.h>

NS_WY3D_BEG

class WY3D_EXPORT Chamfer : public wy3d::SolidModification
{
    WYDB_DECLARE_MEMBERS(Chamfer, wy3d::Chamfer, wy3d::SolidModification)

public:
    // 创建倒角
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Solid* pSolid,
        const std::vector<std::uint32_t>& faceIndices,
        const std::vector<std::uint32_t>& edgeIndices,
        double distance,
        Chamfer*& pOutChamfer);

    // 获取倒角距离
    double getDistance() const { return _distance; }
    // 设置倒角距离
    wy::ErrorStatus setDistance(double distance);

    // 获取边集合
    const TopoNameList& getEdges() const { return _edgeNames; }
    // 设置边集合
    wy::ErrorStatus setEdges(const TopoNameList& edges);

    // 获取面集合
    const TopoNameList& getFaces() const { return _faceNames; }
    // 设置面集合
    wy::ErrorStatus setFaces(const TopoNameList& faces);

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

protected:
    // 修改实体形体
    virtual std::pair<bool, TopoDS_Shape> modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    TopoNameList _edgeNames;
    TopoNameList _faceNames;
    double _distance;
};

NS_WY3D_END

#endif // WY3D_CHAMFER_H
