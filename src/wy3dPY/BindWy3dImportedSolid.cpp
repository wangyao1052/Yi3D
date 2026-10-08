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

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dImportedSolid.h>

namespace py = pybind11;

void bindWy3dImportedSolid(py::module_& m)
{
    py::class_<wy3d::ImportedSolid, wy3d::Solid, std::unique_ptr<wy3d::ImportedSolid, py::nodelete>>(
        m, "ImportedSolid")
        .def("getFilePath", [](const wy3d::ImportedSolid& self) -> std::wstring {
            return self.getFilePath();
        })
        .def("setFilePath", [](wy3d::ImportedSolid& self, const std::wstring& path) {
            return self.setFilePath(path);
        })
        .def_static("isValidFilePath", [](const std::wstring& path) -> bool {
            return wy3d::ImportedSolid::isValidFilePath(path) == wy::ErrorStatus::Ok;
        })
        .def_static("create",
            [](wydb::Transaction* pTrans, const std::wstring& filePath) -> wy3d::ImportedSolid*
            {
                wy3d::ImportedSolid* pOut = nullptr;
                wy::ErrorStatus status = wy3d::ImportedSolid::create(pTrans, filePath, pOut);
                return pOut;
            },
            py::arg("transaction"),
            py::arg("filePath"),
            py::return_value_policy::reference);
}
