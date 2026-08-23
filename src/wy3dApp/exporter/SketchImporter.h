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

#ifndef WY3DAPP_SKETCH_IMPORTER_H
#define WY3DAPP_SKETCH_IMPORTER_H

#include <string>
#include <sstream>
#include <fstream>
#include <memory>
#include <QObject>

#include <wydbDatabase.h>
#include <wy3dImpl.h>

class SketchImporter;

// 草图导入管理器
class SketchImporterManager : public QObject
{
    Q_OBJECT
public:
    // 单例
    static SketchImporterManager& instance();

    // 获取所有的文件导入器
    const std::map<QString, std::shared_ptr<SketchImporter>>& getAllImporters() const
    {
        return _filter2Importer;
    }

private:
    SketchImporterManager();
    virtual ~SketchImporterManager();

private:
    // filter <> importer
    std::map<QString, std::shared_ptr<SketchImporter>> _filter2Importer;
};

// 草图导入器
// 导入器自建草图并管理事务生命周期,调用方只提供数据库与文件路径
class SketchImporter
{
public:
    virtual ~SketchImporter() {}

    // 执行函数
    virtual bool perform(wydb::Database* pDb, const std::wstring& fileFullPath);

protected:
    // 具体执行函数,需要子类继承
    virtual bool performImpl(wydb::Database* pDb, const std::wstring& fileFullPath) = 0;
};

#endif // WY3DAPP_SKETCH_IMPORTER_H