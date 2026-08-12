///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_FILLET_H
#define WY3D_FILLET_H

#include <vector>
#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dSolidModification.h>

NS_WY3D_BEG

class WY3D_EXPORT Fillet : public wy3d::SolidModification
{
    WYDB_DECLARE_MEMBERS(Fillet, wy3d::Fillet, wy3d::SolidModification)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Solid* pSolid,
        const std::vector<std::uint32_t>& faceIndices,
        const std::vector<std::uint32_t>& edgeIndices,
        double radius,
        Fillet*& pOutFillet);

    double getRadius() const { return _radius; }
    wy::ErrorStatus setRadius(double radius);

    const TopoNameList& getEdges() const { return _edgeNames; }
    wy::ErrorStatus setEdges(const TopoNameList& edges);

    const TopoNameList& getFaces() const { return _faceNames; }
    wy::ErrorStatus setFaces(const TopoNameList& faces);

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
    TopoNameList _edgeNames;
    TopoNameList _faceNames;
    double _radius;
};

NS_WY3D_END

#endif // WY3D_FILLET_H
