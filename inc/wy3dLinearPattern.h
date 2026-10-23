///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_LINEAR_PATTERN_H
#define WY3D_LINEAR_PATTERN_H

#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dPattern.h>

NS_WY3D_BEG

class WY3D_EXPORT LinearPattern : public wy3d::Pattern
{
    WYDB_DECLARE_MEMBERS(LinearPattern, wy3d::LinearPattern, wy3d::Pattern)

public:
    // Parameter names
    static constexpr const char* PARAM_DIRECTION_1ST_X = "Direction1stX";
    static constexpr const char* PARAM_DIRECTION_1ST_Y = "Direction1stY";
    static constexpr const char* PARAM_DIRECTION_1ST_Z = "Direction1stZ";
    static constexpr const char* PARAM_COUNT_1ST       = "Count1st";
    static constexpr const char* PARAM_SPACING_1ST     = "Spacing1st";
    static constexpr const char* PARAM_DIRECTION_2ND_X = "Direction2ndX";
    static constexpr const char* PARAM_DIRECTION_2ND_Y = "Direction2ndY";
    static constexpr const char* PARAM_DIRECTION_2ND_Z = "Direction2ndZ";
    static constexpr const char* PARAM_COUNT_2ND       = "Count2nd";
    static constexpr const char* PARAM_SPACING_2ND     = "Spacing2nd";

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Solid* pOwner, const wy3d::Solid* pSource,
        const wy::Vector3& dir1st, double count1st, double spacing1st,
        const wy::Vector3& dir2nd, double count2nd, double spacing2nd,
        LinearPattern*& pOutPattern);

    wy::Vector3 getDirection1st() const { return _dir1st; }
    wy::ErrorStatus setDirection1st(const wy::Vector3& dir1st);

    std::uint32_t getCount1st() const { return _count1st; }
    wy::ErrorStatus setCount1st(std::uint32_t count1st);

    double getSpacing1st() const { return _spacing1st; }
    wy::ErrorStatus setSpacing1st(double spacing1st);

    wy::Vector3 getDirection2nd() const { return _dir2nd; }
    wy::ErrorStatus setDirection2nd(const wy::Vector3& dir2nd);

    std::uint32_t getCount2nd() const { return _count2nd; }
    wy::ErrorStatus setCount2nd(std::uint32_t count2nd);

    double getSpacing2nd() const { return _spacing2nd; }
    wy::ErrorStatus setSpacing2nd(double spacing2nd);

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
    wy::Vector3 _dir1st;
    std::uint32_t _count1st;
    double _spacing1st;
    wy::Vector3 _dir2nd;
    std::uint32_t _count2nd;
    double _spacing2nd;
};

NS_WY3D_END

#endif // WY3D_LINEAR_PATTERN_H