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

#pragma once

#include <QWidget>
#include <QString>

#include <wyapDocManager.h>
#include <wyapDocument.h>

namespace osgViewer
{
class View;
}

class ViewWidget;
class QTabWidget;

class ViewWidgetContainer :
    public QWidget,
    public wyap::DocManagerReactor
{
    Q_OBJECT

public:
    explicit ViewWidgetContainer(QWidget* parent = nullptr);
    ~ViewWidgetContainer();

    // 添加页面
    // 返回值:页面索引;>=0创建成功;-1失败.
    int addPage(wyap::Document* pDoc, ViewWidget* pViewWidget);
    // 关闭页面
    bool closePage(wyap::Document* pDoc);

    // 查找文档对应的页面
    ViewWidget* findPage(wyap::Document* pDoc) const;

    // 获取文档对应的页面
    ViewWidget* getViewWidget(wyap::Document* pDoc) const;

    // 文档反应器回调函数
    virtual void onDocumentCreated(wyap::Document* pNewDoc) override;
    virtual void onDocumentToBeDestroyed(wyap::Document* pDocToDestroy) override;
    virtual void onDocumentDestroyed(const std::string& fileName) override;
    virtual void onDocumentToBeDeactivated(wyap::Document* pDocToDeactivate) override;
    virtual void onDocumentToBeActivated(wyap::Document* pDocToActivate) override;
    virtual void onDocumentActivated(wyap::Document* pActivatedDoc) override;
    virtual void onDocumentStatusChanged(wyap::Document* pDoc, unsigned int oldStatus) override;
    virtual void onDocumentTitleUpdated(wyap::Document* pDoc) override;

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onCurrentTabChanged(int index);
    void onTabCloseRequested(int index);

private:
    // 查找页面对应的文档
    wyap::Document* findDocument(ViewWidget* pViewWidget) const;

    // 通过索引获取页面窗口
    ViewWidget* getViewWidgetAt(int index) const;
    // 通过索引获取文档
    wyap::Document* getDocumentAt(int index) const;

    // 构建文档Tab页标题
    QString buildTabTitle(wyap::Document* pDoc) const;

private:
    QTabWidget* _pTabWidget;
    // Tab index armed by a middle-button press on the tab bar; -1 when disarmed.
    int _middlePressTabIndex;
    std::map<ViewWidget*, wyap::Document*> _page2Doc;
    std::map<wyap::Document*, ViewWidget*> _doc2Page;
};
