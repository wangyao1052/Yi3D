///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_ROTATE_H
#define WY3D_ROTATE_H

#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dBodyModification.h>

NS_WY3D_BEG

class WY3D_EXPORT Rotate : public wy3d::BodyModification
{
    WYDB_DECLARE_MEMBERS(Rotate, wy3d::Rotate, wy3d::BodyModification)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Solid* pSolid,
        const wy::Vector3& centerPoint,
        const wy::Vector3& axisDirection,
        double angle,
        Rotate*& pOutRotate);

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sheet* pSheet,
        const wy::Vector3& centerPoint,
        const wy::Vector3& axisDirection,
        double angle,
        Rotate*& pOutRotate);

    const wy::Vector3& getCenterPoint() const { return _center; }
    wy::ErrorStatus setCenterPoint(const wy::Vector3& center);

    const wy::Vector3& getAxisDirection() const { return _axisDir; }
    wy::ErrorStatus setAxisDirection(const wy::Vector3& direction);

    double getAngle() const { return _angle; }
    wy::ErrorStatus setAngle(double angle);

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
    static wy::ErrorStatus createImpl(
        wydb::Transaction* pTrans,
        const wy::Vector3& centerPoint,
        const wy::Vector3& axisDirection,
        double angle,
        Rotate*& pOutRotate);

private:
    wy::Vector3 _center;
    wy::Vector3 _axisDir;
    double _angle;
};

NS_WY3D_END

#endif // WY3D_ROTATE_H