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

#ifndef WY3DAPP_SKETCH_TEXT_DIALOG_H
#define WY3DAPP_SKETCH_TEXT_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <string>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <wy3dMakeSketchText.h>
#include "commands/sketch/SketchTextCommand.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchTextDialog : public QDialog
{
    Q_OBJECT
public:
    SketchTextDialog(QWidget* parent, const SketchTextHistory* pTextHistory);

    // 获取文本
    const std::wstring& getText() const
    {
        return _text;
    }

    // 获取字体
    QFont getFont() const
    {
        return _curFont;
    }

    // 获取字体文件路径
    const std::string& getFontFilePath() const
    {
        return _fontFilePath;
    }

    // 获取字体面序号
    FT_Long getFontFaceIndex() const
    {
        return _fontFaceIndex;
    }

    // 获取字体高度
    double getFontHeight() const
    {
        return _height;
    }

    // 获取水平间距
    double getHorzSpacing() const
    {
        return _horzSpacing;
    }

protected:
    virtual void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onOkBtnClicked();
    void onCancelBtnClicked();
    void onFontBtnClicked();

private:
    // 初始化默认字体
    bool initDefaultFont(QFont& font);
    // 获取字体显示名
    QString getFontDisplayName(const QFont& font);
    // 获取字体的文件路径和面序号
    bool getFontFilePathAndFaceIndex(const QFont& font,
        std::string& fontFilePath, FT_Long& fontFaceIndex);

private:
    QLineEdit* _textEdit;
    QLineEdit* _heightEdit;
    QLineEdit* _horzSpacingEdit;
    QPushButton* _fontButton;
    QLabel* _selectedFontLabel;

    // 文本
    std::wstring _text;
    // 字体高度
    double _height;
    // 字体水平间距
    double _horzSpacing;

    // 当前字体
    QFont _curFont;
    // 字体文件路径
    std::string _fontFilePath;
    // 字体面序号
    FT_Long _fontFaceIndex;
};

}

#endif // WY3DAPP_SKETCH_TEXT_DIALOG_H
