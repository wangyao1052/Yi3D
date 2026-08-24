///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_HELIX_H
#define WY3D_HELIX_H

#include <wy3dDefs.h>
#include <wy3dCurve.h>

NS_WY3D_BEG

class Sketch;

enum class HelixFlag : std::uint32_t { ClockWise = 1 << 0, Reversed = 1 << 1 };

class WY3D_EXPORT Helix : public wy3d::Curve
{
    WYDB_DECLARE_MEMBERS(Helix, wy3d::Helix, wy3d::Curve)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, wy3d::Sketch* pSketch, double pitch, double turns, double startAngle, Helix*& pOutHelix);

    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children;
        std::vector<wydb::ElementId> baseChildren = __baseClass::getChildren();
        children.reserve(1 + baseChildren.size());
        if (!_sketchId.isNull()) children.emplace_back(_sketchId);
        children.insert(children.cend(), baseChildren.cbegin(), baseChildren.cend());
        return children;
    }

    wydb::ElementId getSketch() const { return _sketchId; }

    double getPitch() const { return _pitch; }
    wy::ErrorStatus setPitch(double pitch);

    double getTurns() const { return _turns; }
    wy::ErrorStatus setTurns(double turns);

    double getStartAngle() const { return _startAngle; }
    wy::ErrorStatus setStartAngle(double startAngle);

    bool isClockWise() const { return hasHelixFlag(HelixFlag::ClockWise); }
    wy::ErrorStatus setClockWise(bool clockwise);

    bool isReversed() const { return hasHelixFlag(HelixFlag::Reversed); }
    wy::ErrorStatus setReversed(bool reversed);

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies);
    virtual TopoDS_Edge generateShape(
        wydb::ChainUpdateFeedbackCollector& feedbackCollector) const override;

private:
    wy::ErrorStatus _setSketch(const wydb::ElementId& sketchId);
    wy::ErrorStatus _setSketch(wy3d::Sketch* pSketch);
    bool hasHelixFlag(HelixFlag flag) const;
    wy::ErrorStatus enableHelixFlag(HelixFlag flag);
    wy::ErrorStatus disableHelixFlag(HelixFlag flag);

private:
    wydb::ElementId _sketchId;
    double _pitch;
    double _turns;
    double _startAngle;
    std::uint32_t _helixFlags;
};

NS_WY3D_END

#endif // WY3D_HELIX_H