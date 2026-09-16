///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_SPLIT_FACE_H
#define WY3D_SPLIT_FACE_H

#include <vector>
#include <cstdint>
#include <wydbElementId.h>
#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dBodyModification.h>

NS_WY3D_BEG

class Sketch3D;

class WY3D_EXPORT SplitFace : public wy3d::BodyModification
{
    WYDB_DECLARE_MEMBERS(SplitFace, wy3d::SplitFace, wy3d::BodyModification)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Solid* pSolid,
        const std::vector<unsigned int>& faceIndices,
        wy3d::Sketch3D* pSketch3D,
        SplitFace*& pOutSplitFace);

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sheet* pSheet,
        const std::vector<unsigned int>& faceIndices,
        wy3d::Sketch3D* pSketch3D,
        SplitFace*& pOutSplitFace);

    const TopoNameList& getFaces() const { return _faceNames; }

    wydb::ElementId getSketch() const { return _sketchId; }

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies) override;
    virtual std::pair<bool, TopoDS_Shape> modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    static wy::ErrorStatus createImpl(
        wydb::Transaction* pTrans,
        const TopoDS_Shape& shape,
        TopoNaming* pTopoNaming,
        const std::vector<unsigned int>& faceIndices,
        wy3d::Sketch3D* pSketch3D,
        SplitFace*& pOutSplitFace);

    wy::ErrorStatus setFacesImpl(const TopoNameList& faceNames);
    wy::ErrorStatus setSketchImpl(wy3d::Sketch3D* pSketch3D);
    wy::ErrorStatus setSketchIdImpl(const wydb::ElementId& sketchId);

private:
    TopoNameList _faceNames;
    wydb::ElementId _sketchId;
};

NS_WY3D_END

#endif // WY3D_SPLIT_FACE_H
