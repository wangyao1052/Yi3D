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

#ifndef WY3DAPP_IMPORTER_H
#define WY3DAPP_IMPORTER_H

#include <string>
#include <map>
#include <memory>
#include <QObject>
#include <QString>
#include <wydbDatabase.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class Importer;

// 导入管理器
class ImporterManager : public QObject
{
    Q_OBJECT
public:
    // 单例
    static ImporterManager& instance();

    // 获取所有的文件导入器
    const std::map<QString, std::shared_ptr<Importer>>& getAllImporters() const
    {
        return _filter2Importer;
    }

private:
    ImporterManager();
    virtual ~ImporterManager();

private:
    // filter <> importer
    std::map<QString, std::shared_ptr<Importer>> _filter2Importer;
};

// 文件导入类
class Importer
{
public:
    virtual ~Importer() {}

    // 导入文件
    virtual bool perform(wydb::Database* pDb, const std::wstring& fileFullPath);
};

// BREP文件导入类
class BrepImporter : public Importer
{
};

// Step文件导入类
class StepImporter : public Importer
{
};

//// Iges文件导入类
//class IgesImporter : public Importer
//{
//};

} // namespace wy3dApp

#endif // WY3DAPP_IMPORTER_H
