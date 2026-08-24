///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SWEEP_H
#define WY3D_SWEEP_H

#include <wy3dDefs.h>
#include <wy3dSolid.h>

NS_WY3D_BEG

class Sketch;
class Curve;

class WY3D_EXPORT Sweep : public wy3d::Solid
{
    WYDB_DECLARE_MEMBERS(Sweep, wy3d::Sweep, wy3d::Solid)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, wy3d::Sketch* pPath, wy3d::Sketch* pProfile, Sweep*& pOutSweep);
    static wy::ErrorStatus create(wydb::Transaction* pTrans, wy3d::Curve* pPath, wy3d::Sketch* pProfile, Sweep*& pOutSweep);
    static wy::ErrorStatus createCut(wydb::Transaction* pTrans, wy3d::Sketch* pPath, wy3d::Sketch* pProfile, wy3d::Solid* pSolidToCut, Sweep*& pOutSweep);
    static wy::ErrorStatus createCut(wydb::Transaction* pTrans, wy3d::Curve* pPath, wy3d::Sketch* pProfile, wy3d::Solid* pSolidToCut, Sweep*& pOutSweep);

    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children;
        std::vector<wydb::ElementId> baseChildren = __baseClass::getChildren();
        children.reserve(2 + baseChildren.size());
        if (!_profileId.isNull()) children.emplace_back(_profileId);
        if (!_pathId.isNull()) children.emplace_back(_pathId);
        children.insert(children.cend(), baseChildren.cbegin(), baseChildren.cend());
        return children;
    }

    wydb::ElementId getPath() const { return _pathId; }
    wydb::ElementId getProfile() const { return _profileId; }

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
    wy::ErrorStatus _setPath(const wydb::ElementId& pathId);
    wy::ErrorStatus _setPath(wy3d::Sketch* pPathSketch);
    wy::ErrorStatus _setPath(wy3d::Curve* pCurve);
    wy::ErrorStatus _setProfile(const wydb::ElementId& profileId);
    wy::ErrorStatus _setProfile(wy3d::Sketch* pProfileSketch);

protected:
    wydb::ElementId _pathId;
    wydb::ElementId _profileId;
};

NS_WY3D_END

#endif // WY3D_SWEEP_H