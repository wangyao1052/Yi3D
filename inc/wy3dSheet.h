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

#ifndef WY3D_SHEET_H
#define WY3D_SHEET_H

#include <TopoDS_Shape.hxx>
#include <vector>

#include <wy3dDefs.h>
#include <wy3dFeature.h>
#include <wy3dTopoNaming.h>
#include <wy3dColor.h>

NS_WY3D_BEG

class SolidModification;

class WY3D_EXPORT Sheet : public wy3d::Feature
{
    WYDB_DECLARE_ABSTRACT_MEMBERS(Sheet, wy3d::Sheet, wy3d::Feature)

public:
    virtual const TopoDS_Shape& getShape() const { return _shape; }

    virtual wydb::ElementId getParent() const override { return _parent; }
    wy::ErrorStatus setParent(const wydb::ElementId& parent);

    virtual std::vector<wydb::ElementId> getChildren() const override { return _modifications; }

    const TopoNaming* getTopoNaming() const { return _pTopoNaming.get(); }
    TopoNaming* getTopoNaming() { return _pTopoNaming.get(); }
    wy::ErrorStatus setTopoNaming(TopoNamingSPtr pTopoNaming);

    wy3d::Color getColor() const { return _color; }
    wy::ErrorStatus setColor(const wy3d::Color& color);

    wy::ErrorStatus addModification(wy3d::SolidModification* pModification);
    const std::vector<wydb::ElementId>& getModifications() const { return _modifications; }

public:
    virtual wydb::ParameterValueUPtr getParameterValue(
        const std::string& className,
        const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(
        const std::string& className,
        const std::string& paramName,
        const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;

    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;

    virtual void reportDependencies(
        std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(
        const std::set<wydb::ElementId>& erasedDependencies) override;

    virtual void onChainUpdater_Completion(
        const wydb::ElementDataPiece& dirtyDataPiece,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector);
    virtual TopoDS_Shape generateShape(
        TopoNaming* pTopoNaming,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector);

protected:
    wy::ErrorStatus setShapeImpl(const TopoDS_Shape& shape);

    std::pair<bool, TopoDS_Shape> modifyShape(
        const TopoDS_Shape& shape,
        TopoNaming* pTopoNaming,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector);

private:
    wy::ErrorStatus _setModifications(const std::vector<wydb::ElementId>& modifications);

protected:
    wydb::ElementId _parent;
    wy3d::Color _color;
    TopoDS_Shape _shape;
    TopoNamingSPtr _pTopoNaming;
    std::vector<wydb::ElementId> _modifications;
};

NS_WY3D_END

#endif // WY3D_SHEET_H
