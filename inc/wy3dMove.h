///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_MOVE_H
#define WY3D_MOVE_H

#include <wyVector3.h>
#include <wy3dVector3.h>
#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dBodyModification.h>

NS_WY3D_BEG

class WY3D_EXPORT Move : public wy3d::BodyModification
{
    WYDB_DECLARE_MEMBERS(Move, wy3d::Move, wy3d::BodyModification)

public:
    // Parameter names
    static constexpr const char* PARAM_VECTOR_X = "VectorX";
    static constexpr const char* PARAM_VECTOR_Y = "VectorY";
    static constexpr const char* PARAM_VECTOR_Z = "VectorZ";

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Solid* pSolid,
        const wy::Vector3& moveVector,
        Move*& pOutMove);

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sheet* pSheet,
        const wy::Vector3& moveVector,
        Move*& pOutMove);

    const wy::Vector3& getVector() const { return _vector; }
    wy::ErrorStatus setVector(const wy::Vector3& vector);

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
        const wy::Vector3& moveVector,
        Move*& pOutMove);

private:
    wy::Vector3 _vector;
};

NS_WY3D_END

#endif // WY3D_MOVE_H