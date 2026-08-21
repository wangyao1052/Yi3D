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

#ifndef WY3D_SOLIDIFY_H
#define WY3D_SOLIDIFY_H

#include <wy3dDefs.h>
#include <wy3dSolid.h>

NS_WY3D_BEG

class Sheet;

class WY3D_EXPORT Solidify : public wy3d::Solid
{
    WYDB_DECLARE_MEMBERS(Solidify, wy3d::Solidify, wy3d::Solid)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sheet* pSource,
        Solidify*& pOut);

    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children = __baseClass::getChildren();
        if (!_sourceId.isNull()) children.push_back(_sourceId);
        return children;
    }

    const wydb::ElementId& getSource() const { return _sourceId; }

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
    wy::ErrorStatus setSourceImpl(const wydb::ElementId& sourceId);
    wy::ErrorStatus setSourceImpl(wy3d::Sheet* pSource);

private:
    wydb::ElementId _sourceId;
};

NS_WY3D_END

#endif // WY3D_SOLIDIFY_H
