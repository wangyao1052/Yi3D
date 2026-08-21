///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SWEPT_SHEET_H
#define WY3D_SWEPT_SHEET_H

#include <wy3dDefs.h>
#include <wy3dSheet.h>

NS_WY3D_BEG

class Sketch;
class Curve;

class WY3D_EXPORT SweptSheet : public wy3d::Sheet
{
    WYDB_DECLARE_MEMBERS(SweptSheet, wy3d::SweptSheet, wy3d::Sheet)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, wy3d::Sketch* pPath, wy3d::Sketch* pProfile, SweptSheet*& pOutSheet);
    static wy::ErrorStatus create(wydb::Transaction* pTrans, wy3d::Curve* pPath, wy3d::Sketch* pProfile, SweptSheet*& pOutSheet);

    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children = __baseClass::getChildren();
        children.emplace_back(_pathId);
        children.emplace_back(_profileId);
        return children;
    }
    
    wydb::ElementId getPath() const { return _pathId; }
    wydb::ElementId getProfile() const { return _profileId; }

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;

    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;

    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies) override;

    virtual TopoDS_Shape generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    wy::ErrorStatus setPathImpl(const wydb::ElementId& pathId);
    wy::ErrorStatus setPathImpl(wy3d::Sketch* pPathSketch);
    wy::ErrorStatus setPathImpl(wy3d::Curve* pCurve);

    wy::ErrorStatus setProfileImpl(const wydb::ElementId& profileId);
    wy::ErrorStatus setProfileImpl(wy3d::Sketch* pProfileSketch);

private:
    wydb::ElementId _pathId;
    wydb::ElementId _profileId;
};

NS_WY3D_END

#endif // WY3D_SWEPT_SHEET_H
