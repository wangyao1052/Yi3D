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

#ifndef WY3DAPP_OPTIONS_DIALOG_H
#define WY3DAPP_OPTIONS_DIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QSlider;
class QSpinBox;

// 选项对话框(工具菜单):视图/系统/自动保存设置
class OptionsDialog : public QDialog
{
	Q_OBJECT

public:
	OptionsDialog(QWidget *parent = Q_NULLPTR);
	~OptionsDialog();

protected:
	virtual void accept() override;

private:
	// 初始化界面
	void initUi();
	// 创建视图设置组
	QWidget* createViewGroup();
	// 创建系统设置组
	QWidget* createSystemGroup();
	// 创建自动保存设置组
	QWidget* createAutoSaveGroup();

	QSlider* _pRotationSpeedSlider;
	QSpinBox* _pRotationSpeedSpin;
	QCheckBox* _pInvertWheelCheck;
	QComboBox* _pLanguageCombo;
	QSpinBox* _pAutoSaveIntervalSpin;
};

#endif // WY3DAPP_OPTIONS_DIALOG_H