///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_EXTRUSION_H
#define WY3D_EXTRUSION_H

#include <wy3dDefs.h>
#include <wy3dSolid.h>

NS_WY3D_BEG

class Sketch;

enum class ExtrusionDirection : std::int32_t
{
    OneSide   = 0,
    Symmetric = 1,
};

class WY3D_EXPORT Extrusion : public wy3d::Solid
{
    WYDB_DECLARE_MEMBERS(Extrusion, wy3d::Extrusion, wy3d::Solid)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch,
        double depth,
        Extrusion*& pOutExtrusion);

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch,
        ExtrusionDirection direction,
        double depth,
        Extrusion*& pOutExtrusion);

    static wy::ErrorStatus createCut(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch,
        double depth,
        wy3d::Solid* pSolidToCut,
        Extrusion*& pOutExtrusion);

    static wy::ErrorStatus createCut(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch,
        ExtrusionDirection direction,
        double depth,
        wy3d::Solid* pSolidToCut,
        Extrusion*& pOutExtrusion);

    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children;
        std::vector<wydb::ElementId> baseChildren = __baseClass::getChildren();
        children.reserve(1 + baseChildren.size());
        if (!_sketchId.isNull()) children.emplace_back(_sketchId);
        children.insert(children.cend(), baseChildren.cbegin(), baseChildren.cend());
        return children;
    }

    const wydb::ElementId& getSketch() const { return _sketchId; }

    ExtrusionDirection getDirection() const { return _direction; }
    wy::ErrorStatus setDirection(ExtrusionDirection direction);

    double getDepth() const { return _depth; }
    wy::ErrorStatus setDepth(double depth);

    double getStartOffset() const { return _startOffset; }
    wy::ErrorStatus setStartOffset(double startOffset);

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

    virtual TopoDS_Shape generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    // 设置草图
    wy::ErrorStatus _setSketch(const wydb::ElementId& sketchId);
    // 设置草图
    // only called by Extrusion static create function.
    wy::ErrorStatus _setSketch(wy3d::Sketch* pSketch);

protected:
    wydb::ElementId _sketchId;
    ExtrusionDirection _direction;
    double _depth;
    double _startOffset;
};

NS_WY3D_END

#endif // WY3D_EXTRUSION_H