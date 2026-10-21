///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_IMPORTED_SOLID_H
#define WY3D_IMPORTED_SOLID_H

#include <string>
#include <wy3dDefs.h>
#include <wy3dSolid.h>
#include <wy3dErrorCode.h>

NS_WY3D_BEG

class WY3D_EXPORT ImportedSolid : public wy3d::Solid
{
    WYDB_DECLARE_MEMBERS(ImportedSolid, wy3d::ImportedSolid, wy3d::Solid)

public:
    // Parameter names
    static constexpr const char* PARAM_FILE_PATH = "FilePath";

    static wy::ErrorStatus create(wydb::Transaction* pTrans, const std::wstring& filePath, ImportedSolid*& pOut);
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

#endif // WY3D_IMPORTED_SOLID_H