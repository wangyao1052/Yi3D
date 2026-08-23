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

#ifndef WY3D_EXTRUDED_SHEET_H
#define WY3D_EXTRUDED_SHEET_H

#include <wy3dDefs.h>
#include <wy3dExtrusion.h>
#include <wy3dSheet.h>

NS_WY3D_BEG

class Sketch;

class WY3D_EXPORT ExtrudedSheet : public wy3d::Sheet
{
    WYDB_DECLARE_MEMBERS(ExtrudedSheet, wy3d::ExtrudedSheet, wy3d::Sheet)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch, double depth,
        ExtrudedSheet*& pOutSheet);

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch,
        ExtrusionDirection direction,
        double depth,
        ExtrudedSheet*& pOutSheet);

    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children = __baseClass::getChildren();
        children.push_back(_sketchId);
        return children;
    }

    const wydb::ElementId& getSketch() const { return _sketchId; }

    double getDepth() const { return _depth; }
    wy::ErrorStatus setDepth(double depth);

    double getStartOffset() const { return _startOffset; }
    wy::ErrorStatus setStartOffset(double startOffset);

    ExtrusionDirection getDirection() const { return _direction; }
    wy::ErrorStatus setDirection(ExtrusionDirection direction);

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
    wy::ErrorStatus _setSketch(wy3d::Sketch* pSketch); // only called by create

protected:
    wydb::ElementId _sketchId;
    ExtrusionDirection _direction;
    double _depth;
    double _startOffset;
};

NS_WY3D_END

#endif // WY3D_EXTRUDED_SHEET_H
