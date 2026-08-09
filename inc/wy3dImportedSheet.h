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

#ifndef WY3D_IMPORTED_SHEET_H
#define WY3D_IMPORTED_SHEET_H

#include <string>
#include <wy3dDefs.h>
#include <wy3dSheet.h>
#include <wy3dErrorCode.h>

NS_WY3D_BEG

class WY3D_EXPORT ImportedSheet : public wy3d::Sheet
{
    WYDB_DECLARE_MEMBERS(ImportedSheet, wy3d::ImportedSheet, wy3d::Sheet)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, const std::wstring& filePath, ImportedSheet*& pOut);
    static wy::ErrorStatus isValidFilePath(const std::wstring& filePath);

public:
    const std::wstring& getFilePath() const { return _filePath; }
    wy::ErrorStatus setFilePath(const std::wstring& filePath);

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;
    virtual TopoDS_Shape generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    inline bool isNeedGenInitShape() const { return _needGenInitShape; }
    wy::ErrorStatus setInitShape(const TopoDS_Shape& shape);
    ErrorCode readShapeFromFile(const std::wstring& filePath, TopoDS_Shape& resultShape) const;

private:
    std::wstring _filePath;
    bool _needGenInitShape;
    TopoDS_Shape _initShape;
};

NS_WY3D_END

#endif // WY3D_IMPORTED_SHEET_H
