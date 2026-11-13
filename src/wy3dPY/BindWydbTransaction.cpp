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
#include <wydbTransaction.h>
#include <wydbElementId.h>
#include <wydbElement.h>

namespace py = pybind11;

void bindTransactionStatus(py::module_& m)
{
    py::enum_<wydb::TransactionStatus>(m, "TransactionStatus")
        .value("Initialized", wydb::TransactionStatus::Initialized)
        .value("Starting", wydb::TransactionStatus::Starting)
        .value("Started", wydb::TransactionStatus::Started)
        .value("Committing", wydb::TransactionStatus::Committing)
        .value("Committed", wydb::TransactionStatus::Committed)
        .value("Aborting", wydb::TransactionStatus::Aborting)
        .value("Aborted", wydb::TransactionStatus::Aborted)
        .value("Error", wydb::TransactionStatus::Error);
}

void bindTransaction(py::module_& m)
{
    py::class_<wydb::Transaction, std::unique_ptr<wydb::Transaction, py::nodelete>>(m, "Transaction")
        .def("isGroup", &wydb::Transaction::isGroup)
        .def("getStatus", &wydb::Transaction::getStatus)
        .def("getElementForRead", py::overload_cast<const wydb::ElementId&>(&wydb::Transaction::getElementForRead, py::const_), py::arg("id"))
        .def("getElementForWrite", py::overload_cast<const wydb::ElementId&>(&wydb::Transaction::getElementForWrite), py::arg("id"))
        .def("addNewlyCreatedElement", &wydb::Transaction::addNewlyCreatedElement, py::arg("pElement"));
}

void bindChainUpdateScope(py::module_& m)
{
    py::enum_<wydb::ChainUpdateScope>(m, "ChainUpdateScope")
        .value("Local", wydb::ChainUpdateScope::Local)
        .value("Cascade", wydb::ChainUpdateScope::Cascade)
        .value("UpTo", wydb::ChainUpdateScope::UpTo);
}

void bindTransactionOption(py::module_& m)
{
    py::class_<wydb::TransactionOption>(m, "TransactionOption")
        .def(py::init<>())
        .def_readwrite("chainUpdateScope", &wydb::TransactionOption::chainUpdateScope)
        .def_readwrite("targetId", &wydb::TransactionOption::targetId);
}

void bindTransactionManager(py::module_& m)
{
    py::class_<wydb::TransactionManager, std::unique_ptr<wydb::TransactionManager, py::nodelete>>(m, "TransactionManager")
        .def("startTransaction", static_cast<wydb::Transaction* (wydb::TransactionManager::*)(const std::string&, const wydb::TransactionOption&)>(&wydb::TransactionManager::startTransaction), py::arg("name") = "", py::arg("option") = wydb::TransactionOption())
        .def("startTransactionGroup", static_cast<wydb::Transaction* (wydb::TransactionManager::*)(const std::string&)>(&wydb::TransactionManager::startTransactionGroup), py::arg("name") = "")
        .def("endTransaction", &wydb::TransactionManager::endTransaction)
        .def("abortTransaction", &wydb::TransactionManager::abortTransaction)
        .def("getActiveTransaction", &wydb::TransactionManager::getActiveTransaction);
}

void bindWydbTransaction(py::module_& m)
{
    bindTransactionStatus(m);
    bindChainUpdateScope(m);
    bindTransactionOption(m);
    bindTransaction(m);
    bindTransactionManager(m);
}
