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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QFont>
#include <QScrollArea>

#include "AboutDialog.h"
#include <wy3dAppVersion.h>

AboutDialog::AboutDialog(QWidget *parent)
	: QDialog(parent)
{
	this->setWindowTitle(tr("About Yi3D"));

	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	this->setLayout(mainLayout);
	{
		//
		QLabel* labelTitle = new QLabel(this);
		labelTitle->setAlignment(Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
		labelTitle->setFont(QFont("Microsoft YaHei", 15, QFont::Bold));
		labelTitle->setText(tr("Yi3D"));
		mainLayout->addWidget(labelTitle);

		// 永久免费
		QLabel* labelFree = new QLabel(this);
		labelFree->setAlignment(Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
		labelFree->setFont(QFont("Microsoft YaHei", 11, QFont::Normal));
		labelFree->setStyleSheet("color:#2e8b57;");
		labelFree->setText(tr("Permanently Free"));
		mainLayout->addWidget(labelFree);

        // 官网
        QLabel* labelWebsite = new QLabel(this);
        labelWebsite->setAlignment(Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
        labelWebsite->setFont(QFont("Microsoft YaHei", 12, QFont::Normal));
        labelWebsite->setText(QString("<a href=\"%1\" style=\"color:#0066cc;\">%1</a>").arg("https://www.wangyaosoft.com/"));
        labelWebsite->setTextFormat(Qt::RichText);
        labelWebsite->setOpenExternalLinks(true); // 允许点击链接
        mainLayout->addWidget(labelWebsite);

		//
		QLabel* labelVersion = new QLabel(this);
		labelVersion->setAlignment(Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
		labelVersion->setFont(QFont("Microsoft YaHei", 12, QFont::Normal));
		QString qstrVersion = tr("Version") + QString(" ") + QString(YI3D_APP_VERSION_STRING);
		labelVersion->setText(qstrVersion);
		mainLayout->addWidget(labelVersion);

		//
		QLabel* labelCopyRight = new QLabel(this);
		labelCopyRight->setAlignment(Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
		labelCopyRight->setFont(QFont("Microsoft YaHei", 12, QFont::Normal));
		QString qstrCopyRight = tr("© 2024-2026 WangYao <wangyao1052@163.com>");
		labelCopyRight->setText(qstrCopyRight);
		mainLayout->addWidget(labelCopyRight);

		//
		QLabel* labelAllRightsReserved = new QLabel(this);
		labelAllRightsReserved->setAlignment(Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
		labelAllRightsReserved->setFont(QFont("Microsoft YaHei", 12, QFont::Normal));
		QString qstrAllRightsReserved = tr("Source code is licensed under Apache 2.0");
		labelAllRightsReserved->setText(qstrAllRightsReserved);
		mainLayout->addWidget(labelAllRightsReserved);

        // 第三方库标题
        QLabel* labelThirdPartyTitle = new QLabel(this);
        labelThirdPartyTitle->setAlignment(Qt::AlignmentFlag::AlignHCenter | Qt::AlignmentFlag::AlignVCenter);
        labelThirdPartyTitle->setFont(QFont("Microsoft YaHei", 12));
        labelThirdPartyTitle->setText(tr("Third-Party Libraries"));
        mainLayout->addWidget(labelThirdPartyTitle);

        // 第三方库列表容器(放入滚动区域,便于后续继续添加库)
        QScrollArea* libScrollArea = new QScrollArea(this);
        QWidget* thirdPartyLibContainer = new QWidget(libScrollArea);
        QVBoxLayout* libLayout = new QVBoxLayout(thirdPartyLibContainer);
        libLayout->setSpacing(5); // 设置库项之间的间距
        {
            // OpenCASCADE
            addThirdPartyLibraryInfo(libLayout, "OpenCASCADE", "7.7.0", "LGPL License", "https://www.opencascade.com/");

            // OSG
            addThirdPartyLibraryInfo(libLayout, "OpenSceneGraph", "3.6.5", "OSGPL License", "https://www.openscenegraph.org/");

            // Qt
            addThirdPartyLibraryInfo(libLayout, "Qt", "5.15.2", "LGPL License", "https://www.qt.io/");

            // WYAF
            addThirdPartyLibraryInfo(libLayout, "WY Application Framework (WYAF)", "",
                "WYAF License", "https://github.com/wangyao1052/WYAF");

            // muParser
            addThirdPartyLibraryInfo(libLayout, "muparser", "2.3.5", "BSD 3 License", "https://github.com/beltoforion/muparser/");

            // RTree
            addThirdPartyLibraryInfo(libLayout, "RTree", "", "Public Domain", "https://github.com/nushoin/RTree/");

            // base64
            addThirdPartyLibraryInfo(libLayout, "base64", "2.rc.08", "Zlib License", "https://github.com/ReneNyffenegger/cpp-base64/");

            // utfcpp
            addThirdPartyLibraryInfo(libLayout, "utfcpp", "", "Boost Software License 1.0", "https://github.com/nemtrif/utfcpp/");
        }
        libLayout->addStretch(); // 条目少时内容居上
        libScrollArea->setWidget(thirdPartyLibContainer);
        libScrollArea->setWidgetResizable(true); // 内容宽度跟随滚动区
        libScrollArea->setFrameShape(QFrame::NoFrame);
        libScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 仅纵向滚动(条目已自动换行)
        libScrollArea->setMinimumHeight(220); // 默认高度;对话框拉高时随之延展
        mainLayout->addWidget(libScrollArea, 1); // 拉伸因子1:多余的纵向空间分给滚动区
	}

    mainLayout->setContentsMargins(50, 20, 50, 20);
    mainLayout->addStretch(); // 添加伸缩项,使内容居上
}

AboutDialog::~AboutDialog()
{
}

void AboutDialog::addThirdPartyLibraryInfo(
    QLayout* layout,
    const QString& name,
    const QString& version,
    const QString& license,
    const QString& url)
{
    assert(layout);

    QLabel* label = new QLabel(layout->parentWidget());
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignTop);
    label->setFont(QFont("Microsoft YaHei", 10, QFont::Normal));
    {
        QString text = QString("%1 %2<br>").arg(name, version);
        if (!url.isEmpty())
        {
            text += QString("<a href=\"%1\" style=\"color:#0066cc;\">%1</a><br>").arg(url);
        }
        text += QString("<span style=\"color:#666666;\">%1</span>").arg(license);

        label->setText(text);
        label->setTextFormat(Qt::RichText);
        label->setOpenExternalLinks(true); // 允许点击链接
    }
    layout->addWidget(label);

    // 添加分隔线
    QFrame* line = new QFrame(layout->parentWidget());
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);
}
