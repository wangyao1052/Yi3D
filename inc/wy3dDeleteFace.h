///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_DELETE_FACE_H
#define WY3D_DELETE_FACE_H

#include <vector>
#include <cstdint>
#include <wy3dDefs.h>
#include <wy3dSheet.h>
#include <wy3dSolidModification.h>

NS_WY3D_BEG

class WY3D_EXPORT DeleteFace : public wy3d::SolidModification
{
    WYDB_DECLARE_MEMBERS(DeleteFace, wy3d::DeleteFace, wy3d::SolidModification)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sheet* pSheet,
        const std::vector<unsigned int>& faceIndices,
        DeleteFace*& pOutDeleteFace);

    const TopoNameList& getFaces() const { return _faceNames; }

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual std::pair<bool, TopoDS_Shape> modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    wy::ErrorStatus setFacesImpl(const TopoNameList& faceNames);

private:
    TopoNameList _faceNames;
};

NS_WY3D_END

#endif // WY3D_DELETE_FACE_H
