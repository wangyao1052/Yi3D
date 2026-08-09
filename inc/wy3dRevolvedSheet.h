///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024-2026 Wang Yao <wangyao1052@163.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_REVOLVED_SHEET_H
#define WY3D_REVOLVED_SHEET_H

#include <wy3dDefs.h>
#include <wy3dSheet.h>
#include <wy3dSketch.h>
#include <wy3dSketchCurve.h>

NS_WY3D_BEG

class WY3D_EXPORT RevolvedSheet : public wy3d::Sheet
{
    WYDB_DECLARE_MEMBERS(RevolvedSheet, wy3d::RevolvedSheet, wy3d::Sheet)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, const wy3d::SketchCurve* pAxis, double startAngle, double endAngle,
        RevolvedSheet*& pOutSheet);

    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children = __baseClass::getChildren();
        children.push_back(_sketchId);
        if (!_axisId.isNull()) children.push_back(_axisId);
        return children;
    }

    wydb::ElementId getSketch() const { return _sketchId; }

    wydb::ElementId getAxis() const { return _axisId; }
    wy::ErrorStatus setAxis(const wy3d::SketchCurve* pAxis);

    double getStartAngle() const { return _startAngle; }
    wy::ErrorStatus setStartAngle(double startAngle);

    double getEndAngle() const { return _endAngle; }
    wy::ErrorStatus setEndAngle(double endAngle);

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

    virtual TopoDS_Shape generateShape(
        TopoNaming* pTopoNaming,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    wy::ErrorStatus _setSketch(const wydb::ElementId& sketchId);
    wy::ErrorStatus _setSketch(wy3d::Sketch* pSketch);
    wy::ErrorStatus _setAxis(const wydb::ElementId& axisId);

protected:
    wydb::ElementId _sketchId;
    wydb::ElementId _axisId;
    double _startAngle;
    double _endAngle;
};

NS_WY3D_END

#endif // WY3D_REVOLVED_SHEET_H
