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

#ifndef WY3DAPP_EXPORTER_H
#define WY3DAPP_EXPORTER_H

#include <string>
#include <map>
#include <memory>
#include <QObject>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Compound.hxx>
#include <wydbDatabase.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>

class Exporter;

// 导出管理器
class ExporterManager : public QObject
{
    Q_OBJECT
public:
    // 单例
    static ExporterManager& instance();

    // 获取所有的文件导出器
    const std::map<QString, std::shared_ptr<Exporter>>& getAllExporters() const
    {
        return _filter2Exporter;
    }

private:
    ExporterManager();
    virtual ~ExporterManager();

private:
    // filter <> exporter
    std::map<QString, std::shared_ptr<Exporter>> _filter2Exporter;
};

// 文件导出类
class Exporter
{
public:
    virtual ~Exporter() {}

    virtual bool perform(const wydb::Database* pDb, const std::wstring& fileFullPath);
    bool perform(const TopoDS_Shape& shape, const std::wstring& fileFullPath);

protected:
    // 具体执行函数,需要子类继承
    virtual bool performImpl(const TopoDS_Shape& shape, const std::wstring& fileFullPath) = 0;

    // 获取数据库的形体
    bool computeDatabaseCompound(const wydb::Database* pDb, TopoDS_Compound& compound) const;
};

// BREP文件导出类
// 文件后缀:*.brep
class BrepExporter : public Exporter
{
protected:
    virtual bool performImpl(const TopoDS_Shape& shape, const std::wstring& fileFullPath) override;
};

// STEP文件导出类
// 文件后缀:*.step, *.stp
class StepExporter : public Exporter
{
protected:
    virtual bool performImpl(const TopoDS_Shape& shape, const std::wstring& fileFullPath) override;
};

// IGES文件导出类
// 文件后缀:*.iges, *.igs
class IgesExporter : public Exporter
{
protected:
    virtual bool performImpl(const TopoDS_Shape& shape, const std::wstring& fileFullPath) override;
};

// STL文件导出类
// 文件后缀:*.stl
class StlExporter : public Exporter
{
protected:
    virtual bool performImpl(const TopoDS_Shape& shape, const std::wstring& fileFullPath) override;
};

#endif // WY3DAPP_EXPORTER_H