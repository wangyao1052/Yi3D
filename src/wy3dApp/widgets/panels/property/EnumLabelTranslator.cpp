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

#include "EnumLabelTranslator.h"

#include <QCoreApplication>
#include <map>

QString EnumLabelTranslator::translate(const std::string& englishLabel)
{
    static const char* kContext = "EnumLabelTranslator";

    // 翻译表 — tr() 字面量由 lupdate 扫描发现
    static const std::map<std::string, QString> _map = []() {
        std::map<std::string, QString> m;

        // ShellJoinType
        m["Arc"]          = QCoreApplication::translate(kContext, "Arc");
        m["Intersection"] = QCoreApplication::translate(kContext, "Intersection");

        // ShellOffsetMode
        m["Skin"]         = QCoreApplication::translate(kContext, "Skin");
        m["Pipe"]         = QCoreApplication::translate(kContext, "Pipe");
        m["RectoVerso"]   = QCoreApplication::translate(kContext, "RectoVerso");

        // ThickenDirection
        m["One Side"]     = QCoreApplication::translate(kContext, "One Side");
        m["Symmetric"]    = QCoreApplication::translate(kContext, "Symmetric");

        return m;
    }();

    auto it = _map.find(englishLabel);
    return it != _map.end() ? it->second : QString::fromStdString(englishLabel);
}

// lupdate 扫描用 —— 确保翻译字面量被发现（部分 lupdate 版本扫不到 lambda 内的调用）
void _enumLabelTrDummy()
{
    QCoreApplication::translate("EnumLabelTranslator", "Arc");
    QCoreApplication::translate("EnumLabelTranslator", "Intersection");
    QCoreApplication::translate("EnumLabelTranslator", "Skin");
    QCoreApplication::translate("EnumLabelTranslator", "Pipe");
    QCoreApplication::translate("EnumLabelTranslator", "RectoVerso");
    QCoreApplication::translate("EnumLabelTranslator", "One Side");
    QCoreApplication::translate("EnumLabelTranslator", "Symmetric");
}
