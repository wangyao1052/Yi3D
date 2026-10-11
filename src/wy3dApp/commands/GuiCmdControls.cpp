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

#include <wy3dApp/GuiCmdControls.h>
#include <wy3dApp/Application.h>
#include "widgets/frame/MainWindow.h"

GuiCmdControls::GuiCmdControls()
{
    _controls.reserve(10);
    this->startTimer(10);
}

GuiCmdControls::~GuiCmdControls()
{
    for (QWidget* pControl : _controls)
    {
        if (!pControl)
        {
            assert(false);
            continue;
        }
        delete pControl;
    }
    _controls.clear();
}

void GuiCmdControls::timerEvent(QTimerEvent* event)
{
}

GuiCmdLabel* GuiCmdControls::newLabel()
{
    QWidget* pMainWindow = Application::instance().getMainWindow();
    assert(pMainWindow);
    GuiCmdLabel* pLabel = new GuiCmdLabel(pMainWindow);
    pLabel->hide();
    _controls.emplace_back(pLabel);
    return pLabel;
}

GuiCmdLabel::GuiCmdLabel(QWidget* parent) : QLineEdit(parent), _minWidth(10)
{
    // 样式
    this->setStyleSheet(
        "QLineEdit {"
        "   border: 1px solid #C0C0C0;" // 去除3D显示效果
        "   background: white;"
        "}"
        "QLineEdit:read-only {" // 只读时文本灰色
        "   color: gray;"
        "}"
        "QLineEdit:focus {"
        "   border: 1px solid #4A90E2;"  // 获得焦点时蓝色边框
        "}"
    );

    // 只读
    this->setReadOnly(true);

    // 不接受焦点
    this->setFocusPolicy(Qt::NoFocus);

    // 设置字体大小
    QFont font = this->font();
    font.setPointSize(10);
    this->setFont(font);

    // 计算最小宽度
    QSize size = this->calculateSizeForText("0.0000");
    _minWidth = size.width();

    // 更新尺寸
    this->updateSize();
}

GuiCmdLabel::~GuiCmdLabel()
{
}

// 计算适合文本的尺寸
QSize GuiCmdLabel::calculateSizeForText(const QString& text) const
{
    // 计算文本宽度和高度
    QFontMetrics fm(this->font());
    int textWidth = fm.horizontalAdvance(text);
    int textHeight = fm.height();

    // 添加水平和垂直边距
    static const int horizontalPadding = 10; // 左右各5px边距
    static const int verticalPadding = 6;    // 上下各3px边距
    static const int borderWidth = 2;        // 边框宽度(上下各1px)

    // 计算最终尺寸
    int finalWidth = textWidth + horizontalPadding;
    int finalHeight = textHeight + verticalPadding + borderWidth;
    return QSize(finalWidth, finalHeight);
}

void GuiCmdLabel::setText(const QString& text)
{
    QLineEdit::setText(text);
    this->updateSize();
}

void GuiCmdLabel::updateSize()
{
    QSize size = this->calculateSizeForText(this->text());
    if (size.width() < _minWidth)
    {
        size.setWidth(_minWidth);
    }
    this->setFixedSize(size);
}