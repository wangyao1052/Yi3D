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

#ifndef WY3DAPP_SKETCH_EXPORTER_H
#define WY3DAPP_SKETCH_EXPORTER_H

#include <string>
#include <sstream>
#include <fstream>
#include <memory>

#include <QObject>

#include <wy3dSketch.h>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class SketchExporter;

// 草图导出管理器
class SketchExporterManager : public QObject
{
    Q_OBJECT
public:
    // 单例
    static SketchExporterManager& instance();

    // 获取所有的文件导出器
    const std::map<QString, std::shared_ptr<SketchExporter>>& getAllExporters() const
    {
        return _filter2Exporter;
    }

private:
    SketchExporterManager();
    virtual ~SketchExporterManager();

private:
    // filter <> exporter
    std::map<QString, std::shared_ptr<SketchExporter>> _filter2Exporter;
};

// 草图导出类
class SketchExporter
{
public:
    virtual ~SketchExporter() {}

    // 导出草图
    virtual bool perform(const wy3d::Sketch* pSketch, const std::wstring& fileFullPath);

protected:
    // 具体导出草图执行函数,需要子类继承
    virtual bool performImpl(const wy3d::Sketch* pSketch, const std::wstring& fileFullPath) = 0;
};

}

#endif // WY3DAPP_SKETCH_EXPORTER_H
