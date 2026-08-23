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

#ifndef WY3DAPP_SKETCH_DXF_IMPORTER_H
#define WY3DAPP_SKETCH_DXF_IMPORTER_H

#include "SketchImporter.h"

// DXF sketch importer backed by the embedded Python runtime and the ezdxf library
class SketchDxfImporter : public SketchImporter
{
public:
    SketchDxfImporter() {}
    virtual ~SketchDxfImporter() {}

protected:
    virtual bool performImpl(wydb::Database* pDb, const std::wstring& fileFullPath) override;
};

#endif // WY3DAPP_SKETCH_DXF_IMPORTER_H
