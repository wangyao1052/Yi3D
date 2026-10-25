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

#include <vector>

#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace osgViewer
{
class View;
}

class QTabWidget;

namespace wy3dApp {

class ViewWidget;

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

    void applyViewSettings();

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

    // 弹出标签页右键菜单
    void showTabContextMenu(int tabIndex, const QPoint& globalPos);

    // 按当前Tab顺序快照所有文档(必须在任何关闭动作之前调用)
    std::vector<wyap::Document*> getDocumentsInTabOrder() const;

    // 关闭单个文档(必要时先询问是否保存).
    // 返回false表示用户取消/保存失败/关闭失败,文档保持打开.
    bool closeDocumentWithPrompt(wyap::Document* pDoc);

    // 批量关闭文档:非活动文档按给定顺序先关,活动文档最后关;
    // 遇到取消或失败立即停止,剩余文档保持打开.
    bool closeDocuments(const std::vector<wyap::Document*>& documents);

    // 中止当前模态命令.返回false表示出现意外状态.
    bool abortCurrentModalCommand();

private:
    QTabWidget* _pTabWidget;
    // Tab index armed by a middle-button press on the tab bar; -1 when disarmed.
    int _middlePressTabIndex;
    std::map<ViewWidget*, wyap::Document*> _page2Doc;
    std::map<wyap::Document*, ViewWidget*> _doc2Page;
};

} // namespace wy3dApp
