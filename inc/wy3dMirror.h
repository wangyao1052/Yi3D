///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_MIRROR_H
#define WY3D_MIRROR_H

#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dSolidModification.h>
#include <wy3dSketchPlane.h>

NS_WY3D_BEG

class WY3D_EXPORT Mirror : public wy3d::SolidModification
{
    WYDB_DECLARE_MEMBERS(Mirror, wy3d::Mirror, wy3d::SolidModification)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Solid* pOwner,
        const wy3d::Solid* pSource,
        const wy3d::SketchPlane& mirrorPlane,
        Mirror*& pOutMirror);

    wydb::ElementId getSource() const { return _source; }

    const wy3d::SketchPlane& getPlane() const { return _plane; }
    wy::ErrorStatus setPlane(const wy3d::SketchPlane& plane);

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies) override;
    virtual std::pair<bool, TopoDS_Shape> modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

    wy::ErrorStatus setSourceId(const wydb::ElementId& source);

private:
    wydb::ElementId _source;
    wy3d::SketchPlane _plane;
};

NS_WY3D_END

#endif // WY3D_MIRROR_H