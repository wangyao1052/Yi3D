///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_LOFTED_SHEET_H
#define WY3D_LOFTED_SHEET_H

#include <vector>
#include <wy3dDefs.h>
#include <wy3dSheet.h>

NS_WY3D_BEG

class Sketch;

class WY3D_EXPORT LoftedSheet : public wy3d::Sheet
{
    WYDB_DECLARE_MEMBERS(LoftedSheet, wy3d::LoftedSheet, wy3d::Sheet)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        const std::vector<wy3d::Sketch*>& profiles,
        LoftedSheet*& pOutSheet);

    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children = __baseClass::getChildren();
        children.insert(children.cend(), _profileIds.cbegin(), _profileIds.cend());
        return children;
    }

    const std::vector<wydb::ElementId>& getProfiles() const { return _profileIds; }

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;

    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;

    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies) override;

    virtual TopoDS_Shape generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    wy::ErrorStatus setProfilesImpl(const std::vector<wydb::ElementId>& profileIds);
    wy::ErrorStatus setProfilesImpl(const std::vector<wy3d::Sketch*>& profiles);

private:
    std::vector<wydb::ElementId> _profileIds;
};

NS_WY3D_END

#endif // WY3D_LOFTED_SHEET_H
