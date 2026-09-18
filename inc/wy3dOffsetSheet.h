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

#ifndef WY3D_OFFSET_SHEET_H
#define WY3D_OFFSET_SHEET_H

#include <cstdint>
#include <vector>
#include <wy3dDefs.h>
#include <wy3dErrorCode.h>
#include <wy3dSheet.h>
#include <wy3dBodyModification.h>

NS_WY3D_BEG

class WY3D_EXPORT OffsetSheet : public wy3d::BodyModification
{
    WYDB_DECLARE_MEMBERS(OffsetSheet, wy3d::OffsetSheet, wy3d::BodyModification)

public:
    enum class Target
    {
        WholeSheet    = 0,
        SelectedFaces = 1,
    };

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sheet* pSheet,
        double offset,
        OffsetSheet*& pOut);

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sheet* pSheet,
        const std::vector<unsigned int>& faceIndices,
        double offset,
        OffsetSheet*& pOut);

    Target getTarget() const { return _target; }
    const TopoNameList& getFaceNames() const { return _faceNames; }

    double getOffset() const { return _offset; }
    wy::ErrorStatus setOffset(double offset);

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

    virtual std::pair<bool, TopoDS_Shape> modifyOwnerShape(
        const TopoDS_Shape& shape,
        TopoNaming* pTopoNaming,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    static wy::ErrorStatus createImpl(
        wydb::Transaction* pTrans,
        wy3d::Sheet* pSheet,
        Target target,
        const std::vector<unsigned int>& faceIndices,
        double offset,
        OffsetSheet*& pOut);

    wy::ErrorStatus setTargetImpl(Target target);
    wy::ErrorStatus setFaceNamesImpl(const TopoNameList& faceNames);

protected:
    Target _target;
    TopoNameList _faceNames;
    double _offset;
};

NS_WY3D_END

#endif // WY3D_OFFSET_SHEET_H
