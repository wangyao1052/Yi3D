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

#ifndef WY3D_PLANAR_SHEET_H
#define WY3D_PLANAR_SHEET_H

#include <wy3dDefs.h>
#include <wy3dSheet.h>

NS_WY3D_BEG

class Sketch;

class WY3D_EXPORT PlanarSheet : public wy3d::Sheet
{
    WYDB_DECLARE_MEMBERS(PlanarSheet, wy3d::PlanarSheet, wy3d::Sheet)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch,
        PlanarSheet*& pOutSheet);

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
    wy::ErrorStatus setSketchImpl(const wydb::ElementId& sketchId);
    wy::ErrorStatus setSketchImpl(wy3d::Sketch* pSketch);

protected:
    wydb::ElementId _sketchId;
};

NS_WY3D_END

#endif // WY3D_PLANAR_SHEET_H
