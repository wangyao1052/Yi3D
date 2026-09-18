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

#include <string>
#include <map>
#include <set>
#include <QWidget>
#include <QTreeView>
#include <QStandardItemModel>
#include <QMouseEvent>

#include <wydbDatabase.h>
#include <wyapDocument.h>
#include <wyapDocManager.h>
#include <wyapSelManager.h>
#include <wy3dFeature.h>

#include "FeatureTreeView.h"
#include "FeatureTreeHoverDelegate.h"
#include "select/SelectPreview.h"

class QFeatureNameItem;
class QFeatureIdItem;
class QFeatureItemManager;
class FeatureTreeModel;

class FeatureTreeWidget : public QWidget,
    public wydb::DatabaseReactor,
    public wyap::DocManagerReactor,
    public wyap::SelManagerReactor 
{
    Q_OBJECT
public:
    const static int kColumn_Name;
    const static int kColumn_Id;
public:
    explicit FeatureTreeWidget(QWidget* parent = nullptr);
    ~FeatureTreeWidget();

    // 重载设置首选大小
    // 对布局特别有用
    virtual QSize sizeHint() const override
    {
        return QSize(300, 400);
    }

    // database reactor
    virtual void onDatabaseChanged(
        const wydb::Database* pDb,
        const wydb::Transaction* pTransaction,
        const wydb::DatabaseChangeInfo& changeInfo) override;

    // selection reactor
    virtual void onSelectionChanged(
        const wyap::SelectionSet&,
        const wyap::SelectionSet&,
        const wyap::SelectionSet&) override;

    // Hover preview switch: a command may turn it off while it owns per-face colors
    void setHoverPreviewEnabled(bool enabled);

    // 设置整体是否支持选择操作
    void setSelectable(bool selectable)
    {
        if (selectable)
        {
            _treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
            //_treeView->viewport()->setCursor(Qt::ArrowCursor);
        }
        else
        {
            _treeView->setSelectionMode(QAbstractItemView::NoSelection);
            //_treeView->viewport()->setCursor(Qt::BusyCursor);
        }
    }

    // 设置是否可选中
    //void setSelectable(const wydb::ElementId& id, bool selectable);
    //void setSelectable(const std::set<wydb::ElementId>& ids, bool selectable);
    // 设置选中状态
    //void setSelected(const wydb::ElementId& id, bool selected);
    //void setSelected(const std::set<wydb::ElementId>& ids, bool selected);

    // 事件过滤器
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    // 本身的选择集事件
    void onSelfSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    // 自定义上下文菜单
    void onCustomContextMenu(const QPoint& pos);
    // 上下文菜单:删除元素
    void onContextMenu_Erase();
    // 上下文菜单:显示或隐藏ID列
    void onContextMenu_ShowHideColumnID();
    // 上下文菜单:取消布尔
    void onContextMenu_CancelBoolean();
    // 上下文菜单:显示错误信息
    void onContextMenu_ShowErrorInfo();

    // 树节点折叠与展开
    void onCollapsed(const QModelIndex& index);
    void onExpanded(const QModelIndex& index);

    // 单击事件
    void onTreeViewClicked(const QModelIndex& index);

    // 文档激活事件
    virtual void onDocumentCreated(wyap::Document* pNewDoc) override;
    virtual void onDocumentToBeDestroyed(wyap::Document* pDocToDestroy) override;
    virtual void onDocumentDestroyed(const std::string& fileName) override;
    virtual void onDocumentToBeDeactivated(wyap::Document* pDocToDeactivate) override;
    virtual void onDocumentToBeActivated(wyap::Document* pDocToActivate) override;
    virtual void onDocumentActivated(wyap::Document* pActivatedDoc) override;

private:
    // 新增树控件
    void newTreeView();
    // 选择界面上的元素项
    void uiSelectItem(const wydb::ElementId& id, bool flag);
    void uiSelectItems(const std::map<wydb::ElementId, bool>& items);
    // 获取第row行的元素ID
    wydb::ElementId getElementIdByModelIndex(const QModelIndex& index);

    // 界面上新增元素
    void uiAddItems(const wydb::Database* pDb, const std::set<wydb::ElementId>& addedIds,
        std::map<wydb::ElementId, wydb::ElementId>& boolean2Target,
        const std::map<wydb::ElementId, unsigned int>& id2Errors);
    void uiAddItem(const wydb::ElementId& ownerId, const wy3d::Feature* pFeature,
        std::map<wydb::ElementId, wydb::ElementId>& boolean2Target,
        const std::map<wydb::ElementId, unsigned int>& id2Errors);
    void uiExpandItem(QFeatureNameItem* pNameItem);
    // 界面上删除元素
    void uiRemoveItems(const wydb::Database* pDb, const std::set<wydb::ElementId>& erasedIds);
    std::map<wydb::ElementId, unsigned int> uiRemoveItem(const wy3d::Feature* pFeature);
    // 界面上修改元素
    void uiModifyItems(const wydb::Database* pDb, const std::set<wydb::ElementId>& modifiedIds,
        std::map<wydb::ElementId, wydb::ElementId>& boolean2Target,
        const std::map<wydb::ElementId, unsigned int>& id2Errors);
    void uiModifyItem(const wy3d::Feature* pFeature, std::map<wydb::ElementId, wydb::ElementId>& boolean2Target,
        const std::map<wydb::ElementId, unsigned int>& id2Errors);
    // 清空界面上的元素
    void uiClearItems();

    // 重排序
    void reorderDirtyOwnerItems(const wydb::Database* pDb, const std::set<wydb::ElementId>& dirtyOwnerIds);

private:
    // ICON
    QIcon _iconError;
    QIcon _iconWarning;
    QIcon _iconEmpty;
    friend class FeatureRow;

    // 树控件
    FeatureTreeView* _treeView;
    FeatureTreeModel* _treeModel;

    // Hover
    FeatureTreeHoverDelegate* _hoverDelegate; // 自定义委托
    SelectPreviewSPtr _pHoverpreview;
    bool _isHoverPreviewEnabled;

    // 类名到显示名称
    std::map<std::string, QString> _className2DisplayName;
    // 默认基准面显示名称
    QString _xoyDatumPlaneDispName;
    QString _yozDatumPlaneDispName;
    QString _xozDatumPlaneDispName;

    // 类名称 <> 实例信息
    struct ClassInstsInfo
    {
        // 基础名称
        // 例如类名称为wy3d::Box的基础名称为Box
        QString baseName;
        // 当前所有实例序号
        std::set<unsigned int> indices;
    };
    std::map<std::string, ClassInstsInfo> _className2Info;
    // 删除过的元素ID <> 序号
    // 删除过的元素通过redo重新添加进来的时候直接使用之前的索引序号
    std::map<wydb::ElementId, unsigned int> _removedId2Index;
    // 元素ID <> 特征行
    enum class FeatureRowFlag : unsigned int
    {
        Hidden  = 0x00000001, // 隐藏
    };
    struct FeatureRow
    {
        QFeatureNameItem* pNameItem;
        QFeatureIdItem* pIdItem;
        unsigned int flags;
        unsigned int errorCode;

        inline void addFlag(FeatureRowFlag flag)
        {
            flags |= static_cast<unsigned int>(flag);
        }
        inline void removeFlag(FeatureRowFlag flag)
        {
            flags &= ~static_cast<unsigned int>(flag);
        }
        inline bool hasFlag(FeatureRowFlag flag) const
        {
            return flags & static_cast<unsigned int>(flag);
        }

        // 是否隐藏
        inline bool isHidden() const
        {
            return this->hasFlag(FeatureRowFlag::Hidden);
        }

        // 获取错误码
        inline unsigned int getErrorCode() const
        {
            return errorCode;
        }
        // 设置错误码
        void setErrorCode(unsigned int code, const FeatureTreeWidget* pFeatTreeWidget);

        FeatureRow() : pNameItem(nullptr), pIdItem(nullptr), flags(0), errorCode(0) {}
    };
    std::map<wydb::ElementId, FeatureRow> _id2FeatRows;
    // 元素ID <> 是否展开
    std::map<wydb::ElementId, bool> _id2IsExpanded;

    //新增
    FeatureRow* uiNewRow(const wydb::ElementId& id, const QString& qstrName, unsigned int instIndex);
    // 通过ID查找特征行
    inline FeatureRow* uiFindRow(const wydb::ElementId& id)
    {
        auto iter = _id2FeatRows.find(id);
        if (iter == _id2FeatRows.cend())
        {
            return nullptr;
        }
        else
        {
            return &(iter->second);
        }
    }
    // 递归获取子元素信息
    bool uiGetChildrenInfo(QFeatureNameItem* pNameItem, std::map<wydb::ElementId, unsigned int>& id2InstIndex);

    // 选择集反应类型
    // 同一时刻只能有一个选择集事件响应
    enum class OnSelChangeType
    {
        Idle = 0, // 空闲状态
        UI = 1,
        SelectionManager = 2,
    };
    OnSelChangeType _onSelChangeType;
    class AutoSwitchOnSelChange
    {
    public:
        explicit AutoSwitchOnSelChange(OnSelChangeType& onSelChangeType) : _onSelChangeType(onSelChangeType)
        {
        }
        ~AutoSwitchOnSelChange()
        {
            _onSelChangeType = OnSelChangeType::Idle;
        }

        void changeTo(OnSelChangeType type)
        {
            if (OnSelChangeType::Idle == _onSelChangeType)
            {
                _onSelChangeType = type;
            }
        }

    private:
        OnSelChangeType& _onSelChangeType;
    };
};
