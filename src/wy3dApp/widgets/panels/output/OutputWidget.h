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

#ifndef WY3DAPP_OUTPUT_WIDGET_H
#define WY3DAPP_OUTPUT_WIDGET_H

#include <string>

#include <QWidget>
#include <QTextBrowser>
#include "wy3dAppDefs.h"

namespace wy3dApp {

class OutputWidget : public QWidget
{
	Q_OBJECT

public:
	explicit OutputWidget(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

	// 输出:基本信息
	void info(const std::string& text);
	// 输出:警告信息
	void warn(const std::string& text);
	// 输出:错误信息
	void error(const std::string& text);

	// 清空信息
	void clear();

    // 刷新
    void refresh();

	// 获取信息
	const QString& text() const;

private slots:
	void onClear();

private:
	QTextBrowser* _textBrowser;
};

}

#endif // WY3DAPP_OUTPUT_WIDGET_H
