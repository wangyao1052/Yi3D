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

#ifndef WY3DAPP_FILE_COMMANDS_H
#define WY3DAPP_FILE_COMMANDS_H

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapCmdExecution.h>
#include <wyapDocument.h>

class FileCmdsUtil
{
public:
    enum class UiAskRet : unsigned int
    {
        Continue = 0, // 继续
        Cancel = 1,   // 取消
    };

    // 是否保存文件
    static UiAskRet uiAskWhetherSaveDocument(wyap::Document* pDoc);

    // 保存文件
    // isSaveAs       --- [in]是否是另存为
    // isUserCanceled --- [out]用户是否取消
    static bool uiSaveFile(wyap::Document* pDoc, bool isSaveAs, bool& isUserCanceled);
};

class NewFileCommand : public wyap::ImmediateCmdExecution
{
    WYRX_DECLARE_MEMBERS(NewFileCommand, NewFileCommand, wyap::ImmediateCmdExecution)
public:
    NewFileCommand() : wyap::ImmediateCmdExecution() {};
    ~NewFileCommand() {}

    virtual int run() override;

private:
    void createDefaultElements(wydb::Database* pDb, wydb::Transaction* pTrans);
};

class OpenFileCommand : public wyap::ImmediateCmdExecution
{
    WYRX_DECLARE_MEMBERS(OpenFileCommand, OpenFileCommand, wyap::ImmediateCmdExecution)
public:
    OpenFileCommand() : wyap::ImmediateCmdExecution() {};
    ~OpenFileCommand() {}

    virtual int run() override;

    // 打开文件
    static int openFile(const std::string& u8FileFullPath);
};

class SaveFileCommand : public wyap::ImmediateCmdExecution
{
    WYRX_DECLARE_MEMBERS(SaveFileCommand, SaveFileCommand, wyap::ImmediateCmdExecution)
public:
    SaveFileCommand() : wyap::ImmediateCmdExecution() {}
    ~SaveFileCommand() {}

    virtual int run() override;
};

class SaveAsFileCommand : public wyap::ImmediateCmdExecution
{
    WYRX_DECLARE_MEMBERS(SaveAsFileCommand, SaveAsFileCommand, wyap::ImmediateCmdExecution)
public:
    SaveAsFileCommand() : wyap::ImmediateCmdExecution() {}
    ~SaveAsFileCommand() {}

    virtual int run() override;
};

class ExportFileCommand : public wyap::ImmediateCmdExecution
{
    WYRX_DECLARE_MEMBERS(ExportFileCommand, ExportFileCommand, wyap::ImmediateCmdExecution)
public:
    ExportFileCommand() : wyap::ImmediateCmdExecution() {}
    ~ExportFileCommand() {}

    virtual int run() override;
};

class ExportSelectedCommand : public wyap::ImmediateCmdExecution
{
    WYRX_DECLARE_MEMBERS(ExportSelectedCommand, ExportSelectedCommand, wyap::ImmediateCmdExecution)
public:
    virtual int run() override;
};

class ImportFileCommand : public wyap::ImmediateCmdExecution
{
    WYRX_DECLARE_MEMBERS(ImportFileCommand, ImportFileCommand, wyap::ImmediateCmdExecution)
public:
    virtual int run() override;
};

class RunScriptCommand : public wyap::ImmediateCmdExecution
{
    WYRX_DECLARE_MEMBERS(RunScriptCommand, RunScriptCommand, wyap::ImmediateCmdExecution)
public:
    virtual int run() override;

private:
    void abortActiveTransaction();
};

#endif // WY3DAPP_FILE_COMMANDS_H
