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

#include "OutputWidget.h"

#include <sstream>
#include <string>
#include <cassert>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QAction>
#include <QFile>
#include <QTextStream>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

OutputWidget::OutputWidget(QWidget* parent, Qt::WindowFlags flags)
	: QWidget(parent, flags)
{
	//
	this->setWindowTitle(tr("Output"));
	this->resize(400, 20);
	//
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	this->setLayout(mainLayout);

    // 设置布局边距为0，消除内部边框
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

	//
	_textBrowser = new QTextBrowser(this);
	{
		_textBrowser->setContextMenuPolicy(Qt::ContextMenuPolicy::ActionsContextMenu);

        // 去除文本浏览器的边框
        _textBrowser->setFrameShape(QFrame::NoFrame);
        // 可选：设置滚动条策略，需要时才显示
        _textBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        _textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

		//
		QAction* actionClear = new QAction(this);
		actionClear->setText(tr("Clear"));
		_textBrowser->addAction(actionClear);
		this->connect(actionClear, SIGNAL(triggered()), this, SLOT(onClear()));
	}
	mainLayout->addWidget(_textBrowser);
}

void OutputWidget::info(const std::string& text)
{
	_textBrowser->append(text.c_str());
}

void OutputWidget::warn(const std::string& text)
{
	std::stringstream ss;
	ss << "<div style='color:#FF8000'>Warning: </div>" << text;
	_textBrowser->append(ss.str().c_str());
}

void OutputWidget::error(const std::string& text)
{
	std::stringstream ss;
	ss << "<div style='color:red'>Error: </div>" << text;
	_textBrowser->append(ss.str().c_str());
}

void OutputWidget::clear()
{
	_textBrowser->clear();
}

void OutputWidget::onClear()
{
	_textBrowser->clear();
}

void OutputWidget::refresh()
{
    const QString filePath = "python_output.txt";
    QFile outputFile(filePath);
    if (outputFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&outputFile);
        in.setCodec("UTF-8");
        const QString content = in.readAll();
        _textBrowser->append(content);
        outputFile.close();
    }
    else
    {
        assert(false);
    }
}

const QString& OutputWidget::text() const
{
	return _textBrowser->toPlainText();
}

}
