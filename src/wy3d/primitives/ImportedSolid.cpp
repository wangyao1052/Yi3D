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

#include <filesystem>
#include <TopoDS_Iterator.hxx>
#include <BRepTools.hxx>
#include <wydbDatabase.h>
#include <wydbFiler.h>
#include <wydbTransaction.h>
#include <wydbFieldRegistry.h>
#include <wy3dImportedSolid.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "topo/TopoNamingUtil.h"
#include "utils/ImportFileUtil.h"
#include "utils/StringUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG
WYDB_IMPLEMENT_MEMBERS(ImportedSolid)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(ImportedSolid, _filePath)
    REGISTER_FIELD(ImportedSolid, _initShape)
END_FIELD_REGISTRATION()

wy::ErrorStatus ImportedSolid::isValidFilePath(const std::wstring& filePath)
{
    try
    {
        std::filesystem::path fsPath(filePath);
        std::error_code ec;

        if (!std::filesystem::exists(fsPath, ec))
        {
            return ec ? wy::ErrorStatus::FileSystemError : wy::ErrorStatus::FileNotFound;
        }
        if (!std::filesystem::is_regular_file(fsPath, ec))
        {
            return ec ? wy::ErrorStatus::FileSystemError : wy::ErrorStatus::NotRegularFile;
        }

        std::string ext = fsPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });

        static std::vector<std::string> supExt;
        if (supExt.empty())
        {
            supExt.reserve(5);
            supExt.emplace_back(std::string(".step"));
            supExt.emplace_back(std::string(".stp"));
            supExt.emplace_back(std::string(".brep"));
        }
        if (std::find(supExt.cbegin(), supExt.cend(), ext) == supExt.cend())
        {
            return wy::ErrorStatus::UnsupportedFileFormat;
        }

        return wy::ErrorStatus::Ok;
    }
    catch (...)
    {
        return wy::ErrorStatus::InvalidFilePath;
    }
}

ImportedSolid::ImportedSolid() : wy3d::Solid(), _filePath(L""), _needGenInitShape(false)
{
}

ImportedSolid::~ImportedSolid()
{
}

wy::ErrorStatus ImportedSolid::create(wydb::Transaction* pTrans, const std::wstring& filePath, ImportedSolid*& pOut)
{
    if (!pTrans)
    {
        pOut = nullptr;
        return wy::ErrorStatus::NullDatabasePointer;
    }

    ImportedSolid* pImportedSolid = new ImportedSolid();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pImportedSolid);
    if (error != wy::ErrorStatus::Ok)
    {
        wydb::deleteElement(pImportedSolid);
        pImportedSolid = nullptr;
        return error;
    }

    error = pImportedSolid->setFilePath(filePath);
    CHECK_ERROR_FOR_CREATE(error, pImportedSolid)

    pOut = pImportedSolid;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus ImportedSolid::setFilePath(const std::wstring& filePath)
{
    wy::ErrorStatus validStatus = isValidFilePath(filePath);
    if (validStatus != wy::ErrorStatus::Ok)
    {
        return validStatus;
    }

    wy::ErrorStatus error = this->prepareForFieldChange(kImportedSolid_filePath);
    if (wy::ErrorStatus::Ok == error)
    {
        _filePath = filePath;
        _needGenInitShape = true;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

wy::ErrorStatus ImportedSolid::setInitShape(const TopoDS_Shape& shape)
{
    wy::ErrorStatus error = this->prepareForFieldChange(kImportedSolid_initShape, wydb::ElementDataPieceType::None);
    if (wy::ErrorStatus::Ok == error)
    {
        _initShape = shape;
        _needGenInitShape = false;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}


void ImportedSolid::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
    {
        wydb::ParameterDefinitionData def;
        def.name = ImportedSolid::PARAM_FILE_PATH;
        pParamSchema->addParameterDefinition(def);
    }
}
wydb::ParameterValueUPtr ImportedSolid::getParameterValue(const std::string& className, const std::string& n) const
{
    if (className == ImportedSolid::classInfo()->className())
    {
        if (ImportedSolid::PARAM_FILE_PATH == n)
        {
            return wydb::ParameterValue::createString(StringUtil::wstringToUtf8(_filePath));
        }
        else
        {
            return nullptr;
        }
    }
    return __baseClass::getParameterValue(className, n);
}

wy::ErrorStatus ImportedSolid::setParameterValue(const std::string& className, const std::string& n, const wydb::ParameterValue& v)
{
    if (className == ImportedSolid::classInfo()->className())
    {
        if (ImportedSolid::PARAM_FILE_PATH == n)
        {
            if (!v.isString())
            {
                return wy::ErrorStatus::InvalidInput;
            }
            return this->setFilePath(StringUtil::utf8ToWString(v.asString()));
        }
        else
        {
            return wy::ErrorStatus::ParameterNotFound;
        }
    }
    return __baseClass::setParameterValue(className, n, v);
}

bool ImportedSolid::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kImportedSolid_filePath.value():
        value = _filePath;
        return true;
    case kImportedSolid_initShape.value():
        value = _initShape;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool ImportedSolid::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kImportedSolid_filePath.value():
        _filePath = std::any_cast<const std::wstring&>(value);
        return true;
    case kImportedSolid_initShape.value():
        _initShape = std::any_cast<const TopoDS_Shape&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus ImportedSolid::writeToFiler(wydb::OutFiler& f) const
{
    __baseClass::writeToFiler(f);

    std::string b64FilePath;
    try
    {
        b64FilePath = StringUtil::wstringToBase64(_filePath);
    }
    catch (...)
    {
        assert(false);
        b64FilePath = "";
    }
    f << b64FilePath;

    std::string b64ShapeData;
    try
    {
        b64ShapeData = StringUtil::shapeToBase64(_initShape);
    }
    catch (...)
    {
        assert(false);
        b64ShapeData = "";
    }
    f << b64ShapeData;

    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus ImportedSolid::readFromFiler(wydb::InFiler& f)
{
    __baseClass::readFromFiler(f);

    std::string b64FilePath;
    f >> b64FilePath;
    try
    {
        _filePath = StringUtil::base64ToWString(b64FilePath);
    }
    catch (...)
    {
        _filePath = L"";
    }

    std::string b64ShapeData;
    f >> b64ShapeData;
    try
    {
        _initShape = StringUtil::base64ToShape(b64ShapeData);
    }
    catch (...)
    {
        _initShape = TopoDS_Shape();
    }

    _needGenInitShape = false;
    return wy::ErrorStatus::Ok;
}

static void _topoNaming(const TopoDS_Shape& shape, unsigned int idValue, TopoNaming* pTopoNaming)
{
    assert(idValue > 0);
    assert(pTopoNaming);
    TopoNamingUtil::primitiveNaming(shape, idValue, *pTopoNaming);
}

TopoDS_Shape ImportedSolid::generateShape(TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    if (this->isNeedGenInitShape())
    {
        wydb::Database* pDb = this->getDatabase();
        assert(pDb);
        wydb::Transaction* pTrans = pDb->getTransactionManager()->getActiveTransaction();
        assert(pTrans);
        assert(false == pTrans->isGroup());

        TopoDS_Shape shapeFromFile;
        ErrorCode errCode = this->readShapeFromFile(_filePath, shapeFromFile);
        if (ErrorCode::NoError != errCode)
        {
            assert(false);
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<unsigned int>(errCode));
        }
        this->setInitShape(shapeFromFile);
    }

    _topoNaming(_initShape, this->getId().value(), pTopoNaming);

    return _initShape;
}

ErrorCode ImportedSolid::readShapeFromFile(const std::wstring& filePath, TopoDS_Shape& resultShape) const
{
    resultShape = TopoDS_Shape();

    ImportFileUtil::ReadResult readRet = ImportFileUtil::readFile(_filePath);
    if (!readRet.flag)
    {
        return ErrorCode::FILE_ReadError;
    }

    TopoDS_Compound solidCompound = readRet.solids;
    if (solidCompound.IsNull())
    {
        return ErrorCode::warnTOPOSHAPE_NullShape;
    }

    int countOfSolids(0);
    TopoDS_Iterator iter;
    for (iter.Initialize(solidCompound); iter.More(); iter.Next())
    {
        ++countOfSolids;
    }
    if (0 == countOfSolids)
    {
        return ErrorCode::warnTOPOSHAPE_NullShape;
    }

    resultShape = solidCompound;
    return ErrorCode::NoError;
}

NS_WY3D_END
