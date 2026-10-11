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

#ifndef WY3DAPP_GUI_CMD_CONTROLS_H
#define WY3DAPP_GUI_CMD_CONTROLS_H

#include <memory>
#include <vector>

#include <QObject>
#include <QLineEdit>

#include "wy3dAppDefs.h"

class GuiCmdLabel;

class WY3DAPP_EXPORT GuiCmdControls : public QObject
{
public:
	GuiCmdControls();
	~GuiCmdControls();

	GuiCmdLabel* newLabel();

protected:
	virtual void timerEvent(QTimerEvent* event) override;

private:
	std::vector<QWidget*> _controls;
};

typedef std::shared_ptr<GuiCmdControls> GuiCmdControlsSPtr;

class GuiCmdLabel : public QLineEdit
{
	Q_OBJECT
public:
	explicit GuiCmdLabel(QWidget* parent = nullptr);
	~GuiCmdLabel();

	// 在文本变化时更新尺寸
	void setText(const QString& text);

private:
	// 计算适合文本的尺寸
	QSize calculateSizeForText(const QString& text) const;
	// 更新尺寸
	void updateSize();

private:
	int _minWidth;
};

#endif // WY3DAPP_GUI_CMD_CONTROLS_H