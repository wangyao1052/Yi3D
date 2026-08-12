///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_BOOLEAN_H
#define WY3D_BOOLEAN_H

#include <vector>
#include <wydbElementId.h>
#include <wy3dDefs.h>
#include <wy3dSolid.h>

NS_WY3D_BEG

enum class BooleanType : std::uint16_t { Undefined=0, Union=1, Difference=2, Intersection=3 };

class Union;
class Intersection;
class Difference;

class WY3D_EXPORT Boolean : public wy3d::Solid
{
    WYDB_DECLARE_ABSTRACT_MEMBERS(Boolean, wy3d::Boolean, wy3d::Solid)

public:
    Boolean(BooleanType boolType);

    virtual std::vector<wydb::ElementId> getChildren() const override;


    BooleanType getBooleanType() const { return _boolType; }
    wydb::ElementId getTarget() const { return _target; }
    const std::vector<wydb::ElementId>& getTools() const { return _tools; }

    wy::ErrorStatus addTool(wy3d::Solid* pTool);
    wy::ErrorStatus cancelBoolean();

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies) override;
    void eraseOnResponse(const std::set<wydb::ElementId>& erasedDependencies);
    virtual TopoDS_Shape generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    wy::ErrorStatus _setTarget(wy3d::Solid* pTarget);
    wy::ErrorStatus _setTarget(const wydb::ElementId& targetId);
    wy::ErrorStatus _setTools(std::vector<wydb::ElementId>&& tools);

private:
    BooleanType _boolType;
    wydb::ElementId _target;
    std::vector<wydb::ElementId> _tools;

    friend class Union;
    friend class Intersection;
    friend class Difference;
};

NS_WY3D_END

#endif // WY3D_BOOLEAN_H