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

class WY3D_EXPORT Sheet : public wy3d::Feature
{
    WYDB_DECLARE_ABSTRACT_MEMBERS(Sheet, wy3d::Sheet, wy3d::Feature)

public:
    virtual const TopoDS_Shape& getShape() const { return _shape; }

    wy3d::Color getColor() const { return _color; }
    wy::ErrorStatus setColor(const wy3d::Color& color);

    const TopoNaming* getTopoNaming() const
    {
        return _pTopoNaming.get();
    }
    TopoNaming* getTopoNaming()
    {
        return _pTopoNaming.get();
    }
    wy::ErrorStatus setTopoNaming(TopoNamingSPtr pTopoNaming);

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

    virtual void onChainUpdater_Completion(
        const wydb::ElementDataPiece& dirtyDataPiece,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector);
    virtual TopoDS_Shape generateShape(
        TopoNaming* pTopoNaming,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector);

protected:
    // can only be called by onChainUpdater_Completion
    wy::ErrorStatus setShape(const TopoDS_Shape& shape);

protected:
    wy3d::Color _color;
    TopoNamingSPtr _pTopoNaming;
    TopoDS_Shape _shape;
};

NS_WY3D_END

#endif // WY3D_SHEET_H
