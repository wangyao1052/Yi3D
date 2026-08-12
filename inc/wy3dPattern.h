///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_PATTERN_H
#define WY3D_PATTERN_H

#include <wy3dDefs.h>
#include <wy3dSolidModification.h>

NS_WY3D_BEG

class WY3D_EXPORT Pattern : public wy3d::SolidModification
{
    WYDB_DECLARE_ABSTRACT_MEMBERS(Pattern, wy3d::Pattern, wy3d::SolidModification)

public:
    wydb::ElementId getSource() const { return _source; }
    static bool isValidSource(const wy3d::Solid* pSolid);

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
    virtual std::pair<bool, TopoDS_Shape> modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;
    virtual std::pair<bool, TopoDS_Shape> modifyOwnerShapeImpl(
        const TopoDS_Shape& shape, TopoNaming* pTopoNaming,
        const TopoDS_Shape& sourceShape, const wy3d::TopoNaming* pSourceNaming,
        bool isCut,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector);

protected:
    wy::ErrorStatus _setSource(const wy3d::Solid* pSource);
    wy::ErrorStatus setSourceId(const wydb::ElementId& source);

private:
    wydb::ElementId _source;
};

NS_WY3D_END

#endif // WY3D_PATTERN_H