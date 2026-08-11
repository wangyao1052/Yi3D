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

#ifndef WY3D_THICKEN_H
#define WY3D_THICKEN_H

#include <wy3dDefs.h>
#include <wy3dSolid.h>

NS_WY3D_BEG

class Sheet;

enum class ThickenDirection : std::int32_t
{
    OneSide  = 0,  // 单侧
    Symmetric = 1, // 对称
};

class WY3D_EXPORT Thicken : public wy3d::Solid
{
    WYDB_DECLARE_MEMBERS(Thicken, wy3d::Thicken, wy3d::Solid)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sheet* pSheet,
        double thickness,
        ThickenDirection direction,
        Thicken*& pOut);

    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children = __baseClass::getChildren();
        children.push_back(_source);
        return children;
    }

    const wydb::ElementId& getSourceSheet() const { return _source; }

    double getThickness() const { return _thickness; }
    wy::ErrorStatus setThickness(double thickness);

    ThickenDirection getDirection() const { return _direction; }
    wy::ErrorStatus setDirection(ThickenDirection direction);

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

    virtual TopoDS_Shape generateShape(
        TopoNaming* pTopoNaming,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    wy::ErrorStatus setSourceSheet(const wydb::ElementId& sheetId);
    wy::ErrorStatus setSourceSheet(wy3d::Sheet* pSheet); // only called by create

protected:
    wydb::ElementId _source;
    double _thickness;
    ThickenDirection _direction;
};

NS_WY3D_END

#endif // WY3D_THICKEN_H
