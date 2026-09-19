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

#include "OptionsDialog.h"

#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "application/Application.h"
#include "application/AutoSave.h"
#include "application/Config.h"
#include "widgets/frame/MainWindow.h"
#include "widgets/frame/ViewWidgetContainer.h"

OptionsDialog::OptionsDialog(QWidget *parent)
	: QDialog(parent)
	, _pRotationSpeedSlider(nullptr)
	, _pRotationSpeedSpin(nullptr)
	, _pInvertWheelCheck(nullptr)
	, _pLanguageCombo(nullptr)
	, _pAutoSaveIntervalSpin(nullptr)
{
	this->setWindowTitle(tr("Options"));
	this->initUi();
}

OptionsDialog::~OptionsDialog()
{
}

void OptionsDialog::initUi()
{
	QVBoxLayout* pMainLayout = new QVBoxLayout(this);

	pMainLayout->addWidget(this->createSystemGroup());
	pMainLayout->addWidget(this->createViewGroup());
	pMainLayout->addWidget(this->createAutoSaveGroup());

	QDialogButtonBox* pButtonBox = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	// 标准按钮文字取自Qt自带qtbase翻译(Qt5.15无简体版),改为走应用自身的翻译
	pButtonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));
	pButtonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
	this->connect(pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
	this->connect(pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
	pMainLayout->addWidget(pButtonBox);

	// 从Config读取当前值
	const Config* pConfig = Application::instance().getConfig();
	if (pConfig)
	{
		_pRotationSpeedSlider->setValue(pConfig->view.mouseRotationSpeed);
		_pRotationSpeedSpin->setValue(pConfig->view.mouseRotationSpeed);
		_pInvertWheelCheck->setChecked(pConfig->view.invertMouseWheelZoom);
		const int languageIndex = _pLanguageCombo->findData(pConfig->system.language);
		if (languageIndex >= 0)
		{
			_pLanguageCombo->setCurrentIndex(languageIndex);
		}
		_pAutoSaveIntervalSpin->setValue(pConfig->autoSave.intervalMinutes);
	}
}

QWidget* OptionsDialog::createViewGroup()
{
	QGroupBox* pGroup = new QGroupBox(tr("View"), this);

	QLabel* pSpeedLabel = new QLabel(tr("Mouse Rotation Speed"), pGroup);
	_pRotationSpeedSlider = new QSlider(Qt::Horizontal, pGroup);
	_pRotationSpeedSlider->setRange(1, 100);
	_pRotationSpeedSpin = new QSpinBox(pGroup);
	_pRotationSpeedSpin->setRange(1, 100);
	this->connect(_pRotationSpeedSlider, SIGNAL(valueChanged(int)), _pRotationSpeedSpin, SLOT(setValue(int)));
	this->connect(_pRotationSpeedSpin, SIGNAL(valueChanged(int)), _pRotationSpeedSlider, SLOT(setValue(int)));

	QHBoxLayout* pSpeedLayout = new QHBoxLayout();
	pSpeedLayout->addWidget(pSpeedLabel);
	pSpeedLayout->addWidget(_pRotationSpeedSlider, 1);
	pSpeedLayout->addWidget(_pRotationSpeedSpin);

	_pInvertWheelCheck = new QCheckBox(tr("Invert Mouse Wheel Zoom Direction"), pGroup);

	QVBoxLayout* pGroupLayout = new QVBoxLayout(pGroup);
	pGroupLayout->addLayout(pSpeedLayout);
	pGroupLayout->addWidget(_pInvertWheelCheck);

	return pGroup;
}

QWidget* OptionsDialog::createSystemGroup()
{
	QGroupBox* pGroup = new QGroupBox(tr("System"), this);

	QLabel* pLanguageLabel = new QLabel(tr("Language"), pGroup);
	_pLanguageCombo = new QComboBox(pGroup);
	_pLanguageCombo->addItem(tr("Chinese"), "zh-CN");
	_pLanguageCombo->addItem(tr("English"), "en-US");

	QLabel* pHintLabel = new QLabel(tr("Takes effect after restart"), pGroup);

	QHBoxLayout* pLanguageLayout = new QHBoxLayout();
	pLanguageLayout->addWidget(pLanguageLabel);
	pLanguageLayout->addWidget(_pLanguageCombo);
	pLanguageLayout->addStretch(1);
	pLanguageLayout->addWidget(pHintLabel);

	QVBoxLayout* pGroupLayout = new QVBoxLayout(pGroup);
	pGroupLayout->addLayout(pLanguageLayout);

	return pGroup;
}

QWidget* OptionsDialog::createAutoSaveGroup()
{
	QGroupBox* pGroup = new QGroupBox(tr("Auto Save"), this);

	QLabel* pIntervalLabel = new QLabel(tr("Auto Save Interval (minutes)"), pGroup);
	_pAutoSaveIntervalSpin = new QSpinBox(pGroup);
	_pAutoSaveIntervalSpin->setRange(0, 60);
	_pAutoSaveIntervalSpin->setSingleStep(5);

	QLabel* pDisableHintLabel = new QLabel(tr("0 disables auto save"), pGroup);

	QHBoxLayout* pIntervalLayout = new QHBoxLayout();
	pIntervalLayout->addWidget(pIntervalLabel);
	pIntervalLayout->addWidget(_pAutoSaveIntervalSpin);
	pIntervalLayout->addStretch(1);
	pIntervalLayout->addWidget(pDisableHintLabel);

	QVBoxLayout* pGroupLayout = new QVBoxLayout(pGroup);
	pGroupLayout->addLayout(pIntervalLayout);

	return pGroup;
}

void OptionsDialog::accept()
{
	// Config成员为const,写入走其setter(内部const_cast)
	Config* pConfig = const_cast<Config*>(Application::instance().getConfig());
	if (pConfig)
	{
		// 写入配置并持久化到config.ini
		pConfig->setMouseRotationSpeed(_pRotationSpeedSpin->value());
		pConfig->setInvertMouseWheelZoom(_pInvertWheelCheck->isChecked());
		pConfig->setLanguage(_pLanguageCombo->currentData().toString());
		pConfig->setAutoSaveIntervalMinutes(_pAutoSaveIntervalSpin->value());
		pConfig->saveConfig();

		// 视图设置实时应用到所有已打开视图
		MainWindow* pMainWindow = Application::instance().getMainWindow();
		ViewWidgetContainer* pContainer = pMainWindow ? pMainWindow->getViewWidgetContainer() : nullptr;
		if (pContainer)
		{
			pContainer->applyViewSettings();
		}

		// 自动保存间隔即时生效
		AutoSave* pAutoSave = Application::instance().getAutoSave();
		if (pAutoSave)
		{
			pAutoSave->setIntervalMinutes(_pAutoSaveIntervalSpin->value());
		}
	}

	QDialog::accept();
}