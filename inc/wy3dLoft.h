///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_LOFT_H
#define WY3D_LOFT_H

#include <vector>
#include <wy3dDefs.h>
#include <wy3dSolid.h>

NS_WY3D_BEG

class Sketch;

class WY3D_EXPORT Loft : public wy3d::Solid
{
    WYDB_DECLARE_MEMBERS(Loft, wy3d::Loft, wy3d::Solid)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        const std::vector<wy3d::Sketch*>& profiles,
        Loft*& pOutLoft);

    static wy::ErrorStatus createCut(
        wydb::Transaction* pTrans,
        const std::vector<wy3d::Sketch*>& profiles,
        wy3d::Solid* pSolidToCut,
        Loft*& pOutLoft);

    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children;
        std::vector<wydb::ElementId> baseChildren = __baseClass::getChildren();
        children.reserve(baseChildren.size() + _profileIds.size());
        children.insert(children.cend(), _profileIds.cbegin(), _profileIds.cend());
        children.insert(children.cend(), baseChildren.cbegin(), baseChildren.cend());
        return children;
    }

    const std::vector<wydb::ElementId>& getProfiles() const { return _profileIds; }

public:

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies) override;
    virtual TopoDS_Shape generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    wy::ErrorStatus _setProfiles(const std::vector<wydb::ElementId>& profileIds);
    wy::ErrorStatus _setProfiles(const std::vector<wy3d::Sketch*>& profiles);

protected:
    std::vector<wydb::ElementId> _profileIds;
};

NS_WY3D_END

#endif // WY3D_LOFT_H