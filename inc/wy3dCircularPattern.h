///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_CIRCULAR_PATTERN_H
#define WY3D_CIRCULAR_PATTERN_H

#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dPattern.h>

NS_WY3D_BEG

class WY3D_EXPORT CircularPattern : public wy3d::Pattern
{
    WYDB_DECLARE_MEMBERS(CircularPattern, wy3d::CircularPattern, wy3d::Pattern)

public:
    // Parameter names
    static constexpr const char* PARAM_CENTER_POINT_X   = "CenterPointX";
    static constexpr const char* PARAM_CENTER_POINT_Y   = "CenterPointY";
    static constexpr const char* PARAM_CENTER_POINT_Z   = "CenterPointZ";
    static constexpr const char* PARAM_AXIS_DIRECTION_X = "AxisDirectionX";
    static constexpr const char* PARAM_AXIS_DIRECTION_Y = "AxisDirectionY";
    static constexpr const char* PARAM_AXIS_DIRECTION_Z = "AxisDirectionZ";
    static constexpr const char* PARAM_TOTAL_ANGLE      = "TotalAngle";
    static constexpr const char* PARAM_INSTANCE_COUNT   = "InstanceCount";
    static constexpr const char* PARAM_IS_CLOCKWISE     = "IsClockWise";

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Solid* pOwner, const wy3d::Solid* pSource,
        const wy::Vector3& centerPoint, const wy::Vector3& axisDirection,
        double totalAngle, std::uint32_t instanceCount, bool isClockWise,
        CircularPattern*& pOutPattern);

    wy::Vector3 getCenterPoint() const { return _centerPoint; }
    wy::ErrorStatus setCenterPoint(const wy::Vector3& centerPoint);

    wy::Vector3 getAxisDirection() const { return _axisDirection; }
    wy::ErrorStatus setAxisDirection(const wy::Vector3& axisDirection);

    double getTotalAngle() const { return _totalAngle; }
    wy::ErrorStatus setTotalAngle(double totalAngle);

    std::uint32_t getInstanceCount() const { return _instanceCount; }
    wy::ErrorStatus setInstanceCount(std::uint32_t instanceCount);

    bool isClockWise() const { return _isClockWise; }
    wy::ErrorStatus setClockWise(bool value);

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    std::pair<bool, TopoDS_Shape> modifyOwnerShapeImpl(
        const TopoDS_Shape& shape, TopoNaming* pTopoNaming,
        const TopoDS_Shape& sourceShape, const wy3d::TopoNaming* pSourceNaming,
        bool isCut,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    wy::Vector3 _centerPoint;
    wy::Vector3 _axisDirection;
    double _totalAngle;
    std::uint32_t _instanceCount;
    bool _isClockWise;
};

NS_WY3D_END

#endif // WY3D_CIRCULAR_PATTERN_H