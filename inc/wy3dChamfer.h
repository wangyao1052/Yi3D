///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_CHAMFER_H
#define WY3D_CHAMFER_H

#include <vector>
#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dSolidModification.h>

NS_WY3D_BEG

enum class ChamferType : std::int32_t
{
    EqualDistance    = 0,
    DistanceDistance = 1,
    DistanceAngle    = 2,
};

class WY3D_EXPORT Chamfer : public wy3d::SolidModification
{
    WYDB_DECLARE_MEMBERS(Chamfer, wy3d::Chamfer, wy3d::SolidModification)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Solid* pSolid,
        const std::vector<std::uint32_t>& faceIndices,
        const std::vector<std::uint32_t>& edgeIndices,
        double distance,
        Chamfer*& pOutChamfer);

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Solid* pSolid,
        const std::vector<std::uint32_t>& faceIndices,
        const std::vector<std::uint32_t>& edgeIndices,
        ChamferType chamferType,
        double distance1,
        double distance2,
        double angle,
        bool isFlipped,
        Chamfer*& pOutChamfer);

    double getDistance1() const { return _distance1; }
    wy::ErrorStatus setDistance1(double distance1);

    double getDistance2() const { return _distance2; }
    wy::ErrorStatus setDistance2(double distance2);

    double getAngle() const { return _angle; }
    wy::ErrorStatus setAngle(double angle);

    ChamferType getChamferType() const { return _chamferType; }
    wy::ErrorStatus setChamferType(ChamferType chamferType);

    bool isFlipped() const { return _isFlipped; }
    wy::ErrorStatus setFlipped(bool isFlipped);

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
    ChamferType _chamferType;
    double _distance1;
    double _distance2;
    double _angle;
    bool _isFlipped;
};

NS_WY3D_END

#endif // WY3D_CHAMFER_H
