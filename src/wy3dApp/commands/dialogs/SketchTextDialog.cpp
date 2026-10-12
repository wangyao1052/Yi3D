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

#include "commands/dialogs/SketchTextDialog.h"

#include <QKeyEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QRegExpValidator>
#include <QFontDialog>
#include <QFontInfo>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QDir>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H

#include <wy3dMath.h>
#include <wy3dImpl.h>
#include "utils/MessageBoxUtil.h"
#include "utils/FontSystem.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

SketchTextDialog::SketchTextDialog(
    QWidget* parent, const SketchTextHistory* pTextHistory)
    : QDialog(parent), _text(L""), _height(10.0), _horzSpacing(1.0),
      _fontFilePath(""), _fontFaceIndex(0)
{
    if (pTextHistory)
    {
        _text = pTextHistory->data.text;
        _height = pTextHistory->data.height;
        _horzSpacing = pTextHistory->data.horizontalSpacing;
    }

    this->setWindowTitle(tr("Sketch Text"));

    QVBoxLayout* layout = new QVBoxLayout(this);

    QGridLayout* mainGridLayout = new QGridLayout();
    int row(-1);
    mainGridLayout->setColumnStretch(2, 1);  // 列2拉伸(占满剩余空间)
    mainGridLayout->setColumnStretch(0, 0);  // 列0、1、3、4不拉伸
    mainGridLayout->setColumnStretch(1, 0);
    mainGridLayout->setColumnStretch(3, 0);
    mainGridLayout->setColumnStretch(4, 0);
    layout->addLayout(mainGridLayout);

    ++row;
    // 文本标签
    QLabel* textLabel = new QLabel(this);
    textLabel->setText(tr("Text"));
    mainGridLayout->addWidget(textLabel, row, 0, 1, 1);
    // 文本内容输入框
    _textEdit = new QLineEdit(this);
    _textEdit->setText(QString::fromStdWString(_text));
    _textEdit->setMinimumWidth(300);
    mainGridLayout->addWidget(_textEdit, row, 1, 1, 4);

    ++row;
    // 文字高度标签
    QLabel* heightLabel = new QLabel(this);
    heightLabel->setText(tr("Height"));
    mainGridLayout->addWidget(heightLabel, row, 0, 1, 1);
    // 文字高度编辑框
    _heightEdit = new QLineEdit(this);
    _heightEdit->setText(QString::number(_height));
    _heightEdit->setMaximumWidth(60);
    {
        // 不允许输入负数
        QString pattern = "^(0|[1-9]\\d*)(\\.\\d*)?$";
        QRegExp regExp(pattern);
        QRegExpValidator* validator = new QRegExpValidator(regExp, _heightEdit);
        _heightEdit->setValidator(validator);
    }
    mainGridLayout->addWidget(_heightEdit, row, 1, 1, 1);

    ++row;
    // 水平间距标签
    QLabel* horzSpacingtLabel = new QLabel(this);
    horzSpacingtLabel->setText(tr("Horz Spacing"));
    mainGridLayout->addWidget(horzSpacingtLabel, row, 0, 1, 1);
    // 水平间距编辑框
    _horzSpacingEdit = new QLineEdit(this);
    _horzSpacingEdit->setText(QString::number(_horzSpacing));
    _horzSpacingEdit->setMaximumWidth(60);
    {
        // 不允许输入负数
        QString pattern = "^(0|[1-9]\\d*)(\\.\\d*)?$";
        QRegExp regExp(pattern);
        QRegExpValidator* validator = new QRegExpValidator(regExp, _horzSpacingEdit);
        _horzSpacingEdit->setValidator(validator);
    }
    mainGridLayout->addWidget(_horzSpacingEdit, row, 1, 1, 1);
    
    ++row;
    // 字体
    _fontButton = new QPushButton(this);
    _fontButton->setText(tr("Font..."));
    mainGridLayout->addWidget(_fontButton, row, 0, 1, 1);
    // 选中的字体
    if (pTextHistory)
    {
        _curFont = pTextHistory->font;
    }
    else
    {
        this->initDefaultFont(_curFont);
    }
    QString fontDisplayName = this->getFontDisplayName(_curFont);
    _selectedFontLabel = new QLabel(this);
    _selectedFontLabel->setText(fontDisplayName);
    mainGridLayout->addWidget(_selectedFontLabel, row, 1, 1, 1);

    // OK和Cancel按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton(tr("Ok"), this);
    QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    // 信号槽
    this->connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
    this->connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancelBtnClicked()));
    this->connect(_fontButton, &QPushButton::clicked, this, &SketchTextDialog::onFontBtnClicked);

    // 固定大小
    this->adjustSize();
    this->setFixedSize(this->size());
}

void SketchTextDialog::onOkBtnClicked()
{
    // 文本内容
    QString qstrText = _textEdit->text();
    if (qstrText.isEmpty())
    {
        MessageBoxUtil::showWarning(tr("Text is empty!"));
        _textEdit->setFocus();
        return;
    }
    _text = qstrText.toStdWString();

    // 字体高度
    QString qstrHeight = _heightEdit->text();
    bool isOk(false);
    _height = qstrHeight.toDouble(&isOk);
    if (!isOk)
    {
        MessageBoxUtil::showWarning_PleaseInputNumber();
        _heightEdit->setFocus();
        return;
    }
    _height = std::fabs(_height);
    double heightMin(0.01), heightMax(10000);
    if (_height < heightMin || _height > heightMax)
    {
        MessageBoxUtil::showWarning_InvalidValue(heightMin, heightMax);
        _heightEdit->setFocus();
        return;
    }

    // 间距
    QString qstrHorzSpacing = _horzSpacingEdit->text();
    _horzSpacing = qstrHorzSpacing.toDouble(&isOk);
    if (!isOk)
    {
        MessageBoxUtil::showWarning_PleaseInputNumber();
        _horzSpacingEdit->setFocus();
        return;
    }
    _horzSpacing = std::fabs(_horzSpacing);
    double horzSpacingMin(0.0), horzSpacingMax(10000);
    if (_horzSpacing < horzSpacingMin || _horzSpacing > horzSpacingMax)
    {
        MessageBoxUtil::showWarning_InvalidValue(horzSpacingMin, horzSpacingMax);
        _horzSpacingEdit->setFocus();
        return;
    }

    // 字体
    if (!this->getFontFilePathAndFaceIndex(_curFont, _fontFilePath, _fontFaceIndex)
        || _fontFilePath.empty())
    {
        MessageBoxUtil::showError(tr("Font file not found!"));
        return;
    }

    this->accept();
}

void SketchTextDialog::onCancelBtnClicked()
{
    this->reject();
}

void SketchTextDialog::keyPressEvent(QKeyEvent* event)
{
    //switch (event->key())
    //{
    //case Qt::Key_Enter:
    //case Qt::Key_Return:
    //{
    //    return this->onOkBtnClicked(); // Ok
    //}
    //break;

    //case Qt::Key_Escape:
    //{
    //    return this->onCancelBtnClicked(); // Cancel
    //}
    //break;
    //}

    QWidget::keyPressEvent(event);
}

void SketchTextDialog::onFontBtnClicked()
{
    bool ok;
    QFont selectedFont = QFontDialog::getFont(
        &ok,
        _curFont,
        nullptr,
        tr("Select Font"));
    if (!ok)
    {
        return;
    }
    _curFont = selectedFont;
    QString fontDisplayName = this->getFontDisplayName(_curFont);
    _selectedFontLabel->setText(fontDisplayName);
}

bool SketchTextDialog::initDefaultFont(QFont& font)
{
    QFontDatabase fontDb;

    std::vector<QString> fontNames;
    fontNames.emplace_back(tr("Microsoft YaHei")); // 微软雅黑
    fontNames.emplace_back(tr("Noto Sans SC"));    // 思源黑体
    fontNames.emplace_back(tr("SimHei"));          // 黑体
    fontNames.emplace_back(tr("SimSun"));          // 宋体

    for (const QString& fontName : fontNames)
    {
        if (fontDb.hasFamily(fontName))
        {
            font.setFamily(fontName);
            return true;
        }
    }

    return false;
}

QString SketchTextDialog::getFontDisplayName(const QFont& font)
{
    QString name = font.family();
    if (name.isEmpty())
    {
        return name;
    }

    if (font.bold())
    {
        name += " " + tr("Bold");
    }
    if (font.italic())
    {
        name += " " + tr("Italic");
    }
    return name;
}

bool SketchTextDialog::getFontFilePathAndFaceIndex(const QFont& font,
    std::string& fontFilePath, FT_Long& fontFaceIndex)
{
    fontFilePath = "";
    fontFaceIndex = 0;

    // 字体名称
    std::vector<QString> targets;
    const QString family = font.family();
    if (font.bold() && font.italic())
    {
        targets.emplace_back(family + " Bold" + " Italic");
        targets.emplace_back(family + " Italic" + " Bold");
    }
    else if (font.bold())
    {
        targets.emplace_back(family + " Bold");
    }
    else if (font.italic())
    {
        targets.emplace_back(family + " Italic");
    }
    targets.emplace_back(family);

    // 通过字体名称查找字体路径
    for (const QString& target : targets)
    {
        std::pair<std::string, FT_Long> ret = FontSystem::instance().getFontFilePath(target.toStdWString());
        if (!ret.first.empty())
        {
            fontFilePath = ret.first;
            fontFaceIndex = ret.second;
            return true;
        }
    }
    return false;
}

}
