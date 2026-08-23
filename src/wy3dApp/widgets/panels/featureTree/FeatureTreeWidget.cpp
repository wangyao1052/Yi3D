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

#include "FeatureTreeWidget.h"
#include "application/Application.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableView>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QCheckBox>
#include <QPushButton>
#include <QAction>
#include <QMessageBox>
#include <QMenu>

#include <sstream>

#include <wydbElementId.h>
#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocument.h>
#include <wyapDocManager.h>
#include <wyapCmdManager.h>
#include <wy3dErrorCode.h>
#include <wy3dFeature.h>
#include <wy3dDatumPlane.h>
#include <wy3dBox.h>
#include <wy3dCylinder.h>
#include <wy3dSphere.h>
#include <wy3dCone.h>
#include <wy3dTorus.h>
#include <wy3dTube.h>
#include <wy3dBoolean.h>
#include <wy3dUnion.h>
#include <wy3dDifference.h>
#include <wy3dIntersection.h>
#include <wy3dSketch.h>
#include <wy3dExtrusion.h>
#include <wy3dRevolution.h>
#include <wy3dSweep.h>
#include <wy3dLoft.h>
#include <wy3dImportedSolid.h>
#include <wy3dImportedSheet.h>
#include <wy3dChamfer.h>
#include <wy3dFillet.h>
#include <wy3dShell.h>
#include <wy3dDraft.h>
#include <wy3dMove.h>
#include <wy3dRotate.h>
#include <wy3dMirror.h>
#include <wy3dLinearPattern.h>
#include <wy3dCircularPattern.h>
#include <wy3dSolid.h>
#include <wy3dSheet.h>
#include <wy3dHelix.h>
#include <wy3dExtrudedSheet.h>
#include <wy3dRevolvedSheet.h>
#include <wy3dSweptSheet.h>
#include <wy3dLoftedSheet.h>
#include <wy3dThicken.h>
#include <wy3dOffsetSheet.h>
#include <wy3dNonParametricSheet.h>
#include <wy3dNonParametricSolid.h>
#include <wy3dPlanarSheet.h>
#include <wy3dSewnSheet.h>
#include <wy3dSolidify.h>
#include <wy3dDefaultChainUpdateFeedback.h>

#include "FeatureTreeModel.h"
#include "commands/CommandNames.h"
#include "commands/GuiCommand.h"
#include "commands/CommandAction.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"
#include "utils/TransactionUtil.h"
#include "utils/MessageBoxUtil.h"
#include "utils/CopyPasteUtil.h"

const int FeatureTreeWidget::kColumn_Name = 0;
const int FeatureTreeWidget::kColumn_Id = 1;

static std::map<wydb::ElementId, unsigned int> collectErrorCodesFromChainUpdateFeedbacks(
    wydb::TransactionManager* pTransMgr,
    const std::set<wydb::ElementId>& ids)
{
    std::map<wydb::ElementId, unsigned int> id2Errors;
    if (!pTransMgr)
    {
        assert(false);
        return id2Errors;
    }

    for (const wydb::ElementId& id : ids)
    {
        id2Errors[id] = wy3d::getErrorCodeFromChainUpdateFeedback(
            pTransMgr->getChainUpdateFeedback(id).get());
    }
    return id2Errors;
}

FeatureTreeWidget::FeatureTreeWidget(QWidget* parent)
    : QWidget(parent), _treeView(nullptr), _treeModel(nullptr), _hoverDelegate(nullptr),
    _onSelChangeType(OnSelChangeType::Idle)
{
    this->setWindowTitle(tr("FeatureTree"));
    this->setMinimumWidth(200);
    this->setMinimumHeight(200);
    this->resize(200, 200);

    // 初始化图标
    _iconError.addFile(":/images/MsgBox_Error.svg", QSize(16, 16));
    _iconWarning.addFile(":/images/MsgBox_Warning.svg", QSize(16, 16));
    QPixmap pixmap(0, 0);
    pixmap.fill(Qt::transparent);
    _iconEmpty.addPixmap(pixmap);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    this->setLayout(mainLayout);

    // 树控件
    this->newTreeView();
    mainLayout->addWidget(_treeView);

    // 文档管理器反应器
    wyap::DocManager* pDocMgr = Application::instance().getDocManager();
    assert(pDocMgr);
    pDocMgr->addReactor(this);
    // 选择集管理器反应器
    wyap::SelManager* pSelMgr = Application::instance().getSelManager();
    assert(pSelMgr);
    pSelMgr->addReactor(this);

    // 类名到显示名称
    _className2DisplayName[wy3d::Box::className()] = tr("Box");
    _className2DisplayName[wy3d::Cylinder::className()] = tr("Cylinder");
    _className2DisplayName[wy3d::Sphere::className()] = tr("Sphere");
    _className2DisplayName[wy3d::Cone::className()] = tr("Cone");
    _className2DisplayName[wy3d::Torus::className()] = tr("Torus");
    _className2DisplayName[wy3d::Tube::className()] = tr("Tube");
    _className2DisplayName[wy3d::Union::className()] = tr("Union");
    _className2DisplayName[wy3d::Difference::className()] = tr("Difference");
    _className2DisplayName[wy3d::Intersection::className()] = tr("Intersection");
    _className2DisplayName[wy3d::DatumPlane::className()] = tr("DatumPlane");
    _className2DisplayName[wy3d::Sketch::className()] = tr("Sketch");
    _className2DisplayName[wy3d::Extrusion::className()] = tr("Extrusion");
    _className2DisplayName[wy3d::Revolution::className()] = tr("Revolution");
    _className2DisplayName[wy3d::Sweep::className()] = tr("Sweep");
    _className2DisplayName[wy3d::Loft::className()] = tr("Loft");
    _className2DisplayName[wy3d::Chamfer::className()] = tr("Chamfer");
    _className2DisplayName[wy3d::Fillet::className()] = tr("Fillet");
    _className2DisplayName[wy3d::Shell::className()] = tr("Shell");
    _className2DisplayName[wy3d::Draft::className()] = tr("Draft");
    _className2DisplayName[wy3d::Move::className()] = tr("Move");
    _className2DisplayName[wy3d::Rotate::className()] = tr("Rotate");
    _className2DisplayName[wy3d::Mirror::className()] = tr("Mirror");
    _className2DisplayName[wy3d::LinearPattern::className()] = tr("Linear Pattern");
    _className2DisplayName[wy3d::CircularPattern::className()] = tr("Circular Pattern");
    _className2DisplayName[wy3d::Helix::className()] = tr("Helix");
    _className2DisplayName[wy3d::ImportedSolid::className()] = tr("Imported Solid");
    _className2DisplayName[wy3d::ImportedSheet::className()] = tr("Imported Sheet");
    _className2DisplayName[wy3d::ExtrudedSheet::className()]  = tr("Extruded Sheet");
    _className2DisplayName[wy3d::RevolvedSheet::className()]  = tr("Revolved Sheet");
    _className2DisplayName[wy3d::SweptSheet::className()]     = tr("Swept Sheet");
    _className2DisplayName[wy3d::LoftedSheet::className()]    = tr("Lofted Sheet");
    _className2DisplayName[wy3d::Thicken::className()]        = tr("Thicken");
    _className2DisplayName[wy3d::NonParametricSheet::className()] = tr("NonParametric Sheet");
    _className2DisplayName[wy3d::NonParametricSolid::className()] = tr("NonParametric Solid");
    _className2DisplayName[wy3d::PlanarSheet::className()]    = tr("Planar Sheet");
    _className2DisplayName[wy3d::SewnSheet::className()]      = tr("Sewn Sheet");
    _className2DisplayName[wy3d::Solidify::className()]       = tr("Solidify");
    _className2DisplayName[wy3d::OffsetSheet::className()]   = tr("Offset Sheet");
    // 默认基准面显示名称
    _xoyDatumPlaneDispName = tr("XOY");
    _yozDatumPlaneDispName = tr("YOZ");
    _xozDatumPlaneDispName = tr("XOZ");
}

void FeatureTreeWidget::newTreeView()
{
    // TreeView
    _treeView = new FeatureTreeView(this);
    // 选中整行
    _treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    // 扩展选中:多行选中+Shift连续选中
    _treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // 不可编辑
    _treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // Model
    _treeModel = new FeatureTreeModel(_treeView);
    _treeView->setModel(_treeModel);
    {
        _treeModel->setColumnCount(2);
        _treeModel->setHeaderData(kColumn_Name, Qt::Orientation::Horizontal, tr("Name"));
        _treeModel->setHeaderData(kColumn_Id, Qt::Orientation::Horizontal, tr("Id"));
    }
    // 第一列初始列宽
    _treeView->setColumnWidth(kColumn_Name, 200);
    _treeView->setColumnWidth(kColumn_Id, 50);

    // 本身的选择集事件
    this->connect(_treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
        this, SLOT(onSelfSelectionChanged(const QItemSelection&, const QItemSelection&)));

    // 上下文菜单
    _treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    this->connect(_treeView, &QTreeView::customContextMenuRequested,
        this, &FeatureTreeWidget::onCustomContextMenu);

    // 树节点折叠与展开
    this->connect(_treeView, SIGNAL(collapsed(const QModelIndex&)), this, SLOT(onCollapsed(const QModelIndex&)));
    this->connect(_treeView, SIGNAL(expanded(const QModelIndex&)), this, SLOT(onExpanded(const QModelIndex&)));

    // 键盘事件
    this->connect(_treeView, SIGNAL(eraseCurrentSelections()), this, SLOT(onContextMenu_Erase()));

    // 自定义
    _treeView->viewport()->installEventFilter(this);
    _hoverDelegate = new FeatureTreeHoverDelegate(_treeView, this);
    _treeView->setItemDelegate(_hoverDelegate);

    // added by wangyao 2025.07.03 {
    // 单击特征树上的元素项时通知具体的命令
    // 比如在新建草图命令中,单击特征树上的基准面则表示选择此基准面为草图平面
    this->connect(_treeView, &QTreeView::clicked, this, &FeatureTreeWidget::onTreeViewClicked);
    // }
}

FeatureTreeWidget::~FeatureTreeWidget()
{
}

bool FeatureTreeWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _treeView->viewport())
    {
        switch (event->type())
        {
        case QEvent::HoverEnter:
        case QEvent::HoverMove:
        {
            QHoverEvent* hoverEvent = static_cast<QHoverEvent*>(event);
            QModelIndex index = _treeView->indexAt(hoverEvent->pos());
            if (_hoverDelegate)
            {
                _hoverDelegate->setHoveredIndex(index);
            }

            wydb::ElementId id(wydb::ElementId::kNull);
            if (index.isValid())
            {
                id = this->getElementIdByModelIndex(index);
            }
            if (id.isNull())
            {
                _pHoverpreview = nullptr;
            }
            else
            {
                wyap::Selection sel(id);
                if (_pHoverpreview)
                {
                    if (!_pHoverpreview->isEqual(sel))
                    {
                        _pHoverpreview = std::make_shared<SelectPreview>(sel);
                    }
                }
                else
                {
                    _pHoverpreview = std::make_shared<SelectPreview>(sel);
                }
            }
        }
        break;

        case QEvent::HoverLeave:
        {
            if (_hoverDelegate)
            {
                _hoverDelegate->setHoveredIndex(QModelIndex());
            }
            _pHoverpreview = nullptr;
        }
        break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

// TreeView的选择集变更响应
void FeatureTreeWidget::onSelfSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    if (OnSelChangeType::Idle != _onSelChangeType)
    {
        return;
    }
    AutoSwitchOnSelChange autoSwitch(_onSelChangeType);
    autoSwitch.changeTo(OnSelChangeType::UI);

    // 新选择的
    std::set<wydb::ElementId> idsToAddSel;
    QModelIndexList selectedIndices = selected.indexes();
    for (const QModelIndex& index : selectedIndices)
    {
        if (index.column() != kColumn_Id) continue;
        wydb::ElementId id = this->getElementIdByModelIndex(index);
        if (!id.isNull()) idsToAddSel.insert(id);
    }

    // 取消选择的
    std::set<wydb::ElementId> idsToRemoveSel;
    QModelIndexList deselectedIndices = deselected.indexes();
    for (const QModelIndex& index : deselectedIndices)
    {
        if (index.column() != kColumn_Id) continue;
        wydb::ElementId id = this->getElementIdByModelIndex(index);
        if (!id.isNull()) idsToRemoveSel.insert(id);
    }

    // 变更选择集
    wyap::SelManager* pSelMgr = Application::instance().getSelManager();
    assert(pSelMgr);
    pSelMgr->beginChange();
    for (const wydb::ElementId& id : idsToAddSel)
    {
        pSelMgr->addSelection(wyap::Selection(id));
    }
    for (const wydb::ElementId& id : idsToRemoveSel)
    {
        pSelMgr->removeSelection(wyap::Selection(id));
    }
    pSelMgr->endChange();
}

// 选择集的变更响应
void FeatureTreeWidget::onSelectionChanged(
    const wyap::SelectionSet& addedSS,
    const wyap::SelectionSet& removedSS,
    const wyap::SelectionSet& curSS)
{
    if (OnSelChangeType::Idle != _onSelChangeType)
    {
        return;
    }
    AutoSwitchOnSelChange autoSwitch(_onSelChangeType);
    autoSwitch.changeTo(OnSelChangeType::SelectionManager);

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }
    wydb::ElementId id = wydb::ElementId::kNull;
    const wy3d::Feature* pFeature(nullptr);

    // 移除的选择集
    for (auto iterToRmv = removedSS.createIterator(); !iterToRmv.isDone(); iterToRmv.moveNext())
    {
        // 过滤出特征
        // 特征树上只会显示特征,但在草图模式下可以选中非特征的草图图元
        id = iterToRmv.current().getElementId();
        pFeature = wy3d::Feature::cast(pDb->getElement(id));
        if (pFeature)
        {
            this->uiSelectItem(id, false);
        }
    }

    // 添加的选择集
    for (auto iterToAdd = addedSS.createIterator(); !iterToAdd.isDone(); iterToAdd.moveNext())
    {
        // 过滤出特征
        // 特征树上只会显示特征,但在草图模式下可以选中非特征的草图图元
        id = iterToAdd.current().getElementId();
        pFeature = wy3d::Feature::cast(pDb->getElement(id));
        if (pFeature)
        {
            this->uiSelectItem(id, true);
        }
    }
}

wydb::ElementId FeatureTreeWidget::getElementIdByModelIndex(const QModelIndex& index)
{
    QFeatureItem* pFeatItem = dynamic_cast<QFeatureItem*>(_treeModel->itemFromIndex(index));
    if (!pFeatItem) return wydb::ElementId::kNull;
    return pFeatItem->getElementId();
}

void FeatureTreeWidget::uiSelectItems(const std::map<wydb::ElementId, bool>& items)
{
    // TODO
    // 对于一次性修改比较多的选择集,调用此方法批量修改
}

void FeatureTreeWidget::uiSelectItem(const wydb::ElementId& id, bool flag)
{
    FeatureRow* pFeatRow = this->uiFindRow(id);
    if (!pFeatRow || !pFeatRow->pNameItem || !pFeatRow->pIdItem)
    {
        assert(false);
        return;
    }

    _treeView->selectionModel()->select(pFeatRow->pNameItem->index(),
        flag ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
    _treeView->selectionModel()->select(pFeatRow->pIdItem->index(),
        flag ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
}

void FeatureTreeWidget::onDocumentCreated(wyap::Document* pNewDoc)
{
}

void FeatureTreeWidget::onDocumentToBeDestroyed(wyap::Document* pDocToDestroy)
{
}

void FeatureTreeWidget::onDocumentDestroyed(const std::string& fileName)
{
}

void FeatureTreeWidget::onDocumentToBeDeactivated(wyap::Document* pDocToDeactivate)
{
    assert(pDocToDeactivate);

    // remove database reactor
    wydb::Database* pDb = pDocToDeactivate->getDatabase();
    assert(pDb);
    pDb->removeReactor(this);

    // 清空窗口
    this->uiClearItems();
}

void FeatureTreeWidget::onDocumentToBeActivated(wyap::Document* pDocToActivate)
{
    assert(pDocToActivate);
}

void FeatureTreeWidget::onDocumentActivated(wyap::Document* pActivatedDoc)
{
    assert(pActivatedDoc);

    // 清空窗口
    this->uiClearItems();

    // 添加元素
    wydb::Database* pDb = pActivatedDoc->getDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }
    std::set<wydb::ElementId> addedIds;
    for (auto iter = pDb->createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current();
        if (id.isNull())
        {
            assert(false);
            continue;
        }
        addedIds.insert(id);
    }
    if (!addedIds.empty())
    {
        std::map<wydb::ElementId, wydb::ElementId> boolean2Target;

        // 获取元素错误
        wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
        assert(pTransMgr);
        std::map<wydb::ElementId, unsigned int> id2Errors =
            collectErrorCodesFromChainUpdateFeedbacks(pTransMgr, addedIds);

        this->uiAddItems(pDb, addedIds, boolean2Target, id2Errors);
    }

    // 重排序所有owner节点
    std::set<wydb::ElementId> dirtyOwnerIds;
    for (const wydb::ElementId& id : addedIds)
    {
        FeatureRow* pFeatRow = this->uiFindRow(id);
        if (!pFeatRow) continue; // 有可能是草图图元
        if (pFeatRow->pNameItem->rowCount() <= 1)
        {
            continue;
        }
        dirtyOwnerIds.insert(id);
    }
    this->reorderDirtyOwnerItems(pDb, dirtyOwnerIds);

    // add database reactor
    pDb->addReactor(this);
}

void FeatureTreeWidget::onDatabaseChanged(
    const wydb::Database* pDb,
    const wydb::Transaction* pTransaction,
    const wydb::DatabaseChangeInfo& changeInfo)
{
    assert(pDb);
    std::map<wydb::ElementId, wydb::ElementId> boolean2Target;

    // 获取元素错误
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    assert(pTransMgr);
    std::map<wydb::ElementId, unsigned int> id2Errors;
    std::set<wydb::ElementId> changedIds = changeInfo.addedIds;
    changedIds.insert(changeInfo.modifiedIds.cbegin(), changeInfo.modifiedIds.cend());
    id2Errors = collectErrorCodesFromChainUpdateFeedbacks(pTransMgr, changedIds);

    //---------------------------------
    // 新增的元素
    //---------------------------------
    if (!changeInfo.addedIds.empty())
    {
        this->uiAddItems(pDb, changeInfo.addedIds, boolean2Target, id2Errors);
    }
    
    //---------------------------------
    // 修改的元素
    //---------------------------------
    if (!changeInfo.modifiedIds.empty())
    {
        this->uiModifyItems(pDb, changeInfo.modifiedIds, boolean2Target, id2Errors);
    }

    //---------------------------------
    // 删除的元素
    //---------------------------------
    if (!changeInfo.erasedIds.empty())
    {
        this->uiRemoveItems(pDb, changeInfo.erasedIds);
    }

    //---------------------------------
    // 父节点有新增子项则需要重新排序子节点.(比如布尔体先倒了圆角,再添加新的工具体,之前的圆角要调整到最后)
    // 父节点的子节点层级有发生改变(改变到父节点)则需要重新排序子节点.
    //---------------------------------
    std::set<wydb::ElementId> dirtyOwnerIds;
    for (const wydb::ElementId& id : changeInfo.addedIds)
    {
        FeatureRow* pFeatRow = this->uiFindRow(id);
        if (!pFeatRow) continue; // 有可能是草图图元
        QFeatureNameItem* pParentNameItem = dynamic_cast<QFeatureNameItem*>(pFeatRow->pNameItem->parent());
        if (!pParentNameItem) continue; // 首层的特征节点

        wydb::ElementId ownerId = pParentNameItem->getElementId();
        if (!ownerId.isNull())
        {
            dirtyOwnerIds.insert(ownerId);
        }
    }
    for (const wydb::ElementId& id : changeInfo.modifiedIds)
    {
        FeatureRow* pFeatRow = this->uiFindRow(id);
        if (!pFeatRow) continue; // 有可能是草图图元
        QFeatureNameItem* pParentNameItem = dynamic_cast<QFeatureNameItem*>(pFeatRow->pNameItem->parent());
        if (!pParentNameItem) continue; // 首层的特征节点

        if (!changeInfo.details.isDataPieceDirty(id, wydb::ElementDataPieceType::Hierarchy)) continue; // 子节点的层级没有发生改变

        wydb::ElementId ownerId = pParentNameItem->getElementId();
        if (!ownerId.isNull())
        {
            dirtyOwnerIds.insert(ownerId);
        }
    }
    this->reorderDirtyOwnerItems(pDb, dirtyOwnerIds);
}

void FeatureTreeWidget::reorderDirtyOwnerItems(const wydb::Database* pDb, const std::set<wydb::ElementId>& dirtyOwnerIds)
{
    assert(pDb);

    // 重新排序子项
    for (const wydb::ElementId& id : dirtyOwnerIds)
    {
        FeatureRow* pFeatRow = this->uiFindRow(id);
        if (!pFeatRow)
        {
            assert(false);
            continue;
        }
        if (pFeatRow->pNameItem->rowCount() <= 1)
        {
            continue;
        }

        // 获取实体的所有子元素
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(pFeatRow->pNameItem->getElementId()));
        if (!pSolid) continue;
        std::vector<wydb::ElementId> children = pSolid->getChildren();

        // take所有子节点
        std::map<wydb::ElementId, QList<QStandardItem*>> id2ChildItems;
        while (pFeatRow->pNameItem->rowCount() > 0)
        {
            QList<QStandardItem*> children = pFeatRow->pNameItem->takeRow(0);
            if (children.isEmpty())
            {
                assert(false);
                continue;
            }
            QFeatureNameItem* pChildNameItem = dynamic_cast<QFeatureNameItem*>(children[0]);
            if (!pChildNameItem)
            {
                assert(false);
                continue;
            }
            id2ChildItems[pChildNameItem->getElementId()] = std::move(children);
        }

        // 重新按照顺序添加子节点
        assert(children.size() == id2ChildItems.size());
        for (const wydb::ElementId& childId : children)
        {
            auto iter = id2ChildItems.find(childId);
            if (iter == id2ChildItems.cend())
            {
                assert(false);
                continue;
            }
            pFeatRow->pNameItem->appendRow(iter->second);
        }
    }
}

// 提取出特征的通俗名称
static std::string extractFeatureNormalName(const std::string& className)
{
    std::string name;

    // 去除命名空间
    size_t nPos = className.find_first_of("::");
    if (nPos != size_t(-1))
    {
        name = className.substr(nPos + strlen("::"));
    }

    if (name.empty())
    {
        name = className;
    }
    return name;
}

class AddedIdsSortAlgo
{
public:
    struct Node
    {
        wydb::ElementId id;
        std::list<Node*> children;
    };

public:
    AddedIdsSortAlgo(const wydb::Database* pDb, const std::set<wydb::ElementId>& addedIds)
        : _pDb(pDb), _addedIds(addedIds)
    {
        assert(_pDb);
    }

    std::list<wydb::ElementId> sort()
    {
        // 构建网状节点关系
        std::map<wydb::ElementId, Node*> id2Node;
        std::list<Node*> topNodes;
        for (const wydb::ElementId& id : _addedIds)
        {
            const wydb::Element* pElem = _pDb->getElement(id);
            if (!pElem)
            {
                assert(false);
                continue;
            }
            const wy3d::Feature* pFeat = wy3d::Feature::cast(pElem);
            if (!pFeat) continue;
            wydb::ElementId ownerId = pFeat->getParent();

            // 创建节点
            Node* pNode(nullptr);
            auto iterNode = id2Node.find(id);
            if (iterNode == id2Node.cend())
            {
                pNode = this->makeNode(id);
                id2Node[id] = pNode;
            }
            else // 之前已经创建
            {
                pNode = iterNode->second;
            }
            assert(pNode);

            // 确定节点是否是top节点
            if (!ownerId.isNull() && _addedIds.find(ownerId) != _addedIds.cend()) // 子节点
            {
                Node* pOwnerNode(nullptr);
                auto iterOwnerNode = id2Node.find(ownerId);
                if (iterOwnerNode == id2Node.cend())
                {
                    pOwnerNode = this->makeNode(ownerId);
                    id2Node[ownerId] = pOwnerNode;
                }
                else
                {
                    pOwnerNode = iterOwnerNode->second;
                }
                assert(pOwnerNode);
                pOwnerNode->children.emplace_back(pNode);
            }
            else // 独立成节点
            {
                topNodes.emplace_back(pNode);
            }
        }

        // 确定顺序
        std::list<wydb::ElementId> retSortedIds;
        std::map<wydb::ElementId, Node*> id2TopNode;
        for (Node* pTopNode : topNodes)
        {
            assert(pTopNode);
            id2TopNode[pTopNode->id] = pTopNode;
        }
        for (const auto& kvp : id2TopNode)
        {
            Node* pTopNode = kvp.second;
            assert(pTopNode);
            retSortedIds.emplace_back(pTopNode->id);
            this->visitChildren(pTopNode, retSortedIds);
        }

        return retSortedIds;
    }

private:
    Node* makeNode(const wydb::ElementId& id)
    {
        std::shared_ptr<Node> pNode = std::make_shared<Node>();
        pNode->id = id;
        _nodes.emplace_back(pNode);
        return pNode.get();
    }

    void visitChildren(Node* pNode, std::list<wydb::ElementId>& retSortedIds)
    {
        assert(pNode);
        for (Node* pChildNode : pNode->children)
        {
            retSortedIds.emplace_back(pChildNode->id);
            visitChildren(pChildNode, retSortedIds);
        }
    }

private:
    const wydb::Database* _pDb;
    const std::set<wydb::ElementId>& _addedIds;
    std::list<std::shared_ptr<Node>> _nodes;
};

void FeatureTreeWidget::uiAddItems(const wydb::Database* pDb, const std::set<wydb::ElementId>& addedIds,
    std::map<wydb::ElementId, wydb::ElementId>& boolean2Target,
    const std::map<wydb::ElementId, unsigned int>& id2Errors)
{
    assert(pDb);

    // 1.排序
    AddedIdsSortAlgo sortAlgo(pDb, addedIds);
    std::list<wydb::ElementId> sortedIds = sortAlgo.sort();
    
    // 2.按照顺序添加
    for (const wydb::ElementId& id : sortedIds)
    {
        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem)
        {
            assert(false);
            continue;
        }
        const wy3d::Feature* pFeat = wy3d::Feature::cast(pElem);
        if (!pFeat) continue;
    
        this->uiAddItem(pFeat->getParent(), pFeat, boolean2Target, id2Errors);
    }
}

void FeatureTreeWidget::FeatureRow::setErrorCode(unsigned int code, const FeatureTreeWidget* pFeatTreeWidget)
{
    assert(pFeatTreeWidget);
    if (errorCode == code) return;
    errorCode = code;
    if (0 == errorCode)
    {
        pNameItem->setIcon(pFeatTreeWidget->_iconEmpty);
    }
    else if (wy3d::isError(code))
    {
        pNameItem->setIcon(pFeatTreeWidget->_iconError);
    }
    else
    {
        pNameItem->setIcon(pFeatTreeWidget->_iconWarning);
    }
}

void FeatureTreeWidget::uiAddItem(
    const wydb::ElementId& ownerId,
    const wy3d::Feature* pFeature,
    std::map<wydb::ElementId, wydb::ElementId>& boolean2Target,
    const std::map<wydb::ElementId, unsigned int>& id2Errors)
{
    if (!pFeature)
    {
        assert(false);
        return;
    }
    wydb::ElementId featId = pFeature->getId();

    // 生成特征标识
    const std::string& className = pFeature->getClassInfo()->className();
    unsigned int currIndex(0);
    auto iter = _className2Info.find(className);
    if (_className2Info.cend() == iter) // 创建的第一个实例
    {
        ClassInstsInfo info;
        if (_className2DisplayName.find(className) != _className2DisplayName.cend())
        {
            info.baseName = _className2DisplayName[className];
        }
        else
        {
            std::string baseName = extractFeatureNormalName(className);
            info.baseName = baseName.c_str();
        }
        assert(info.baseName.count() > 0);
        _className2Info[className] = info;

        currIndex = 1;
        iter = _className2Info.find(className);
        assert(iter != _className2Info.cend());
    }
    else
    {
        auto iterRemoved = _removedId2Index.find(featId);
        if (iterRemoved != _removedId2Index.cend()) // 被删除过现在redo重新添加,使用原来的序号
        {
            currIndex = iterRemoved->second;
            assert(iter->second.indices.find(currIndex) == iter->second.indices.cend());
        }
        else
        {
            const std::set<unsigned int>& indices = iter->second.indices;
            if (indices.empty())
            {
                currIndex = 1;
            }
            else
            {
                currIndex = (*indices.rbegin()) + 1; // 已经存在的最大的序号+1
            }
        }
    }
    assert(currIndex >= 1);
    iter->second.indices.insert(currIndex);
    QString qstrText = iter->second.baseName + QString::number(currIndex);
    // added by wangyao 2025.05.21 {
    // 判断是否是切除特征
    if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pFeature))
    {
        
        if (pSolid->isCut())
        {
            qstrText = iter->second.baseName + tr("-Cut") + QString::number(currIndex);
        }
    }
    // }
    // added by wangyao 2025.01.23 {
    // 对于参照面特殊处理:显示参照面的名称
    if (pFeature->getClassInfo() == wy3d::DatumPlane::classInfo())
    {
        const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pFeature);
        const std::string& datumPlaneName = pDatumPlane->getName();
        if (datumPlaneName == "TOP")
            qstrText = _xoyDatumPlaneDispName;
        else if (datumPlaneName == "RIGHT")
            qstrText = _yozDatumPlaneDispName;
        else if (datumPlaneName == "FRONT")
            qstrText = _xozDatumPlaneDispName;
        else if (!datumPlaneName.empty())
            qstrText = datumPlaneName.c_str();
    }
    // }

    // 添加特征项
    if (ownerId.isNull())
    {
        FeatureRow* pFeatRow = this->uiNewRow(featId, qstrText, currIndex);
        if (!pFeatRow || !pFeatRow->pNameItem || !pFeatRow->pIdItem)
        {
            assert(false);
            return;
        }
        // added by wangyao 2025.02.25 {
        if (pFeature->isHidden())
        {
            pFeatRow->pNameItem->setForeground(QColor(Qt::lightGray));
            pFeatRow->pIdItem->setForeground(QColor(Qt::lightGray));
            pFeatRow->addFlag(FeatureRowFlag::Hidden);
        }
        // }
        // added by wangyao 2025.04.16
        auto iter = id2Errors.find(featId);
        if (iter != id2Errors.cend())
        {
            pFeatRow->setErrorCode(iter->second, this);
        }
        // }
        int row = _treeModel->rowCount();
        _treeModel->setItem(row, kColumn_Name, pFeatRow->pNameItem);
        _treeModel->setItem(row, kColumn_Id, pFeatRow->pIdItem);

        // 展开or折叠
        this->uiExpandItem(pFeatRow->pNameItem);
    }
    else
    {
        FeatureRow* pOwnerFeatRow = this->uiFindRow(ownerId);
        if (!pOwnerFeatRow || !pOwnerFeatRow->pNameItem || !pOwnerFeatRow->pIdItem)
        {
            assert(false);
            return;
        }
        FeatureRow* pFeatRow = this->uiNewRow(featId, qstrText, currIndex);
        if (!pFeatRow || !pFeatRow->pNameItem || !pFeatRow->pIdItem)
        {
            assert(false);
            return;
        }
        // added by wangyao 2025.02.25 {
        if (pFeature->isHidden())
        {
            pFeatRow->pNameItem->setForeground(QColor(Qt::lightGray));
            pFeatRow->pIdItem->setForeground(QColor(Qt::lightGray));
            pFeatRow->addFlag(FeatureRowFlag::Hidden);
        }
        //}
        // added by wangyao 2025.04.16
        auto iter = id2Errors.find(featId);
        if (iter != id2Errors.cend())
        {
            pFeatRow->setErrorCode(iter->second, this);
        }
        // }
        QFeatureNameItem* pOwnerNameItem = pOwnerFeatRow->pNameItem;

        auto iterTarget = boolean2Target.find(ownerId);
        if (iterTarget == boolean2Target.cend())
        {
            const wy3d::Boolean* pBoolean = wy3d::Boolean::cast(pFeature->getDatabase()->getElement(ownerId));
            if (pBoolean)
            {
                boolean2Target[ownerId] = pBoolean->getTarget();
            }
            else
            {
                boolean2Target[ownerId] = wydb::ElementId::kNull;
            }
            iterTarget = boolean2Target.find(ownerId);
        }

        if (iterTarget->second == featId) // 如果当前特征是目标体
        {
            pOwnerNameItem->insertRow(0, pFeatRow->pNameItem);
            pOwnerNameItem->setChild(0, kColumn_Id, pFeatRow->pIdItem);
        }
        else
        {
            int row = pOwnerNameItem->rowCount();
            pOwnerNameItem->setChild(row, kColumn_Name, pFeatRow->pNameItem);
            pOwnerNameItem->setChild(row, kColumn_Id, pFeatRow->pIdItem);
        }

        // 展开or折叠
        this->uiExpandItem(pFeatRow->pNameItem);
    }
}

void FeatureTreeWidget::uiExpandItem(QFeatureNameItem* pNameItem)
{
    if (!pNameItem) return;
    bool isExpanded = true;
    auto iterExpanded = _id2IsExpanded.find(pNameItem->getElementId());
    if (iterExpanded != _id2IsExpanded.cend())
    {
        isExpanded = iterExpanded->second;
    }
    else
    {
        assert(false);
    }
    QModelIndex indexName = _treeModel->indexFromItem(pNameItem);
    _treeView->setExpanded(indexName, isExpanded);
}

bool FeatureTreeWidget::uiGetChildrenInfo(QFeatureNameItem* pNameItem, std::map<wydb::ElementId, unsigned int>& id2InstIndex)
{
    int nRowCnt = pNameItem->rowCount();
    for (int row = 0; row < nRowCnt; ++row)
    {
        QFeatureNameItem* pChildNameItem = dynamic_cast<QFeatureNameItem*>(pNameItem->child(row, kColumn_Name));
        if (!pChildNameItem)
        {
            assert(false);
            return false;
        }
        id2InstIndex[pChildNameItem->getElementId()] = pChildNameItem->getInstIndex();
        if (!uiGetChildrenInfo(pChildNameItem, id2InstIndex))
        {
            return false;
        }
    }

    return true;
}

void FeatureTreeWidget::uiRemoveItems(const wydb::Database* pDb, const std::set<wydb::ElementId>& erasedIds)
{
    std::map<wydb::ElementId, unsigned int> totalErasedChildren;
    for (const wydb::ElementId& id : erasedIds)
    {
        if (totalErasedChildren.find(id) != totalErasedChildren.cend())
        {
            continue;
        }
        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem)
        {
            assert(false);
            continue;
        }
        const wy3d::Feature* pFeat = wy3d::Feature::cast(pElem);
        if (!pFeat) continue;

        std::map<wydb::ElementId, unsigned int> erasedChildren = this->uiRemoveItem(pFeat);
        for (const auto& kvp : erasedChildren)
        {
            totalErasedChildren.insert(kvp);
        }
    }
}

std::map<wydb::ElementId, unsigned int> FeatureTreeWidget::uiRemoveItem(const wy3d::Feature* pFeature)
{
    if (!pFeature)
    {
        assert(false);
        return std::map<wydb::ElementId, unsigned int>();
    }
    
    // 类型实例信息
    wydb::ElementId id = pFeature->getId();
    const std::string& className = pFeature->getClassInfo()->className();
    auto iterClassInstsInfo = _className2Info.find(className);
    if (iterClassInstsInfo == _className2Info.cend())
    {
        assert(false);
        return std::map<wydb::ElementId, unsigned int>();
    }
    ClassInstsInfo& classInstsInfo = iterClassInstsInfo->second;

    // 根据ID查找Row
    FeatureRow* pFeatRow = this->uiFindRow(id);
    if (!pFeatRow || !pFeatRow->pNameItem || !pFeatRow->pIdItem)
    {
        assert(false);
        return std::map<wydb::ElementId, unsigned int>();
    }
    QFeatureNameItem* pNameItem = pFeatRow->pNameItem;
    unsigned int instIndex = pNameItem->getInstIndex();

    // 如果还有子节点
    std::map<wydb::ElementId, unsigned int> childId2InstIndex;
    if (pNameItem->rowCount() > 0)
    {
        if (!this->uiGetChildrenInfo(pNameItem, childId2InstIndex))
        {
            assert(false);
            return std::map<wydb::ElementId, unsigned int>();
        }
    }

    // 移除实例序号
    auto iterIndex = classInstsInfo.indices.find(instIndex);
    if (iterIndex == classInstsInfo.indices.cend())
    {
        assert(false);
        return std::map<wydb::ElementId, unsigned int>();
    }
    classInstsInfo.indices.erase(iterIndex);
    _removedId2Index[id] = instIndex; // 记录删除特征的实例序号,REDO的时候会使用该序号,可以存在不同的ID对应相同的序号,这是合理的.

    // 删除UI行
    if (pNameItem->parent())
    {
        pNameItem->parent()->removeRow(pNameItem->row());
    }
    else
    {
        _treeModel->removeRow(pNameItem->row());
    }
    _id2FeatRows.erase(pFeature->getId());

    // 移除子节点的信息
    {
        wydb::Database* pDb = pFeature->getDatabase();
        assert(pDb);
        for (const auto& kvp : childId2InstIndex)
        {
            const wy3d::Feature* pChildFeat = wy3d::Feature::cast(pDb->getElement(kvp.first));
            if (!pChildFeat)
            {
                assert(false);
                continue;
            }
            const std::string& className = pChildFeat->getClassInfo()->className();
            auto iterClassInstsInfo = _className2Info.find(className);
            if (iterClassInstsInfo == _className2Info.cend())
            {
                assert(false);
                continue;
            }
            ClassInstsInfo& classInstsInfo = iterClassInstsInfo->second;

            auto iterIndex = classInstsInfo.indices.find(kvp.second);
            if (iterIndex == classInstsInfo.indices.cend())
            {
                assert(false);
                continue;
            }
            classInstsInfo.indices.erase(iterIndex);
            _removedId2Index[kvp.first] = kvp.second;
            _id2FeatRows.erase(kvp.first);
        }
    }
    
    return childId2InstIndex;
}

void FeatureTreeWidget::uiModifyItems(const wydb::Database* pDb, const std::set<wydb::ElementId>& modifiedIds,
    std::map<wydb::ElementId, wydb::ElementId>& boolean2Target,
    const std::map<wydb::ElementId, unsigned int>& id2Errors)
{
    if (!pDb)
    {
        assert(false);
        return;
    }

    for (const wydb::ElementId& id : modifiedIds)
    {
        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem)
        {
            assert(false);
            continue;
        }
        const wy3d::Feature* pFeat = wy3d::Feature::cast(pElem);
        if (!pFeat) continue;
        this->uiModifyItem(pFeat, boolean2Target, id2Errors);
    }
}

void FeatureTreeWidget::uiModifyItem(
    const wy3d::Feature* pFeature,
    std::map<wydb::ElementId, wydb::ElementId>& boolean2Target,
    const std::map<wydb::ElementId, unsigned int>& id2Errors)
{
    if (!pFeature)
    {
        assert(false);
        return;
    }

    wydb::ElementId id = pFeature->getId();
    wydb::ElementId ownerId = pFeature->getParent();
    FeatureRow* pFeatRow = this->uiFindRow(id);
    if (!pFeatRow || !pFeatRow->pNameItem || !pFeatRow->pIdItem)
    {
        assert(false);
        return;
    }

    // added by wangyao 2025.02.25 {
    // 特征的显示与隐藏状态更改时,需要相应更改树节点的显示效果
    if (pFeature->isHidden() != pFeatRow->isHidden())
    {
        if (pFeature->isHidden())
        {
            pFeatRow->pNameItem->setForeground(QColor(Qt::lightGray));
            pFeatRow->pIdItem->setForeground(QColor(Qt::lightGray));
            pFeatRow->addFlag(FeatureRowFlag::Hidden);
        }
        else
        {
            pFeatRow->pNameItem->setForeground(QColor(Qt::black));
            pFeatRow->pIdItem->setForeground(QColor(Qt::black));
            pFeatRow->removeFlag(FeatureRowFlag::Hidden);
        }
    }
    // }

    // added by wangyao 2025.04.16
    auto iter = id2Errors.find(id);
    if (iter != id2Errors.cend())
    {
        pFeatRow->setErrorCode(iter->second, this);
    }
    else
    {
        pFeatRow->setErrorCode(0, this);
    }
    // }

    QFeatureNameItem* pParentNameItem = dynamic_cast<QFeatureNameItem*>(pFeatRow->pNameItem->parent());
    wydb::ElementId oldOwnerId = pParentNameItem ? pParentNameItem->getElementId() : wydb::ElementId::kNull;
    if (ownerId == oldOwnerId) return;

    // take row
    QList<QStandardItem*> rowItems;
    if (pParentNameItem)
        rowItems = pParentNameItem->takeRow(pFeatRow->pNameItem->row());
    else
        rowItems = _treeModel->takeRow(pFeatRow->pNameItem->row());
    // append row
    if (ownerId.isNull())
    {
        _treeModel->appendRow(rowItems);
    }
    else
    {
        FeatureRow* pOwnerFeatRow = this->uiFindRow(ownerId);
        if (!pOwnerFeatRow || !pOwnerFeatRow->pNameItem || !pOwnerFeatRow->pIdItem)
        {
            assert(false);
            return;
        }
        auto iterTarget = boolean2Target.find(ownerId);
        if (iterTarget == boolean2Target.cend())
        {
            const wy3d::Boolean* pBoolean = wy3d::Boolean::cast(pFeature->getDatabase()->getElement(ownerId));
            if (pBoolean)
            {
                boolean2Target[ownerId] = pBoolean->getTarget();
            }
            else
            {
                boolean2Target[ownerId] = wydb::ElementId::kNull;
            }
            iterTarget = boolean2Target.find(ownerId);
        }
        
        if (iterTarget->second == id) // 如果当前特征是目标体
            pOwnerFeatRow->pNameItem->insertRow(0, rowItems);
        else
            pOwnerFeatRow->pNameItem->appendRow(rowItems);
    }
}

void FeatureTreeWidget::uiClearItems()
{
    assert(_treeModel);
    _treeModel->removeRows(0, _treeModel->rowCount());
    _className2Info.clear();
    _removedId2Index.clear();
    _id2FeatRows.clear();
    _id2IsExpanded.clear();
}

FeatureTreeWidget::FeatureRow* FeatureTreeWidget::uiNewRow(const wydb::ElementId& id, const QString& qstrName, unsigned int instIndex)
{
    if (_id2FeatRows.find(id) != _id2FeatRows.cend())
    {
        return nullptr;
    }

    FeatureRow row;
    row.pIdItem = new QFeatureIdItem(id);
    row.pNameItem = new QFeatureNameItem(id, qstrName, instIndex);
    auto iter = _id2FeatRows.insert(std::pair<wydb::ElementId, FeatureRow>(id, row));

    auto iterExpanded = _id2IsExpanded.find(id);
    if (iterExpanded == _id2IsExpanded.cend())
    {
        _id2IsExpanded[id] = true;
    }
    
    return &(iter.first->second);
}

void FeatureTreeWidget::onContextMenu_Erase()
{
    // 提取选中的元素ID
    std::set<wydb::ElementId> ids;
    QModelIndexList modelIndices = _treeView->selectionModel()->selectedRows();
    for (const QModelIndex& index : modelIndices)
    {
        wydb::ElementId id = this->getElementIdByModelIndex(index);
        if (!id.isNull()) ids.insert(id);
    }
    if (ids.empty()) return;

    // added by wangyao 2025.04.19 {
    // 删除前清空选择集应该是一个不错的选择,避免未考虑到的一些逻辑问题.
    // 清空选择集
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();
    // }

    // 删除元素
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    assert(pTransMgr);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans)
    {
        assert(false);
        return;
    }
    for (const wydb::ElementId& id : ids)
    {
        wydb::Element* pElem = pTrans->getElementForWrite(id);
        if (!pElem)
        {
            assert(false);
            continue;
        }
        pElem->erase();
    }
    pTransMgr->endTransaction();
}

void FeatureTreeWidget::onContextMenu_CancelBoolean()
{
    // 提取选中的元素ID
    std::set<wydb::ElementId> ids;
    QModelIndexList modelIndices = _treeView->selectionModel()->selectedRows();
    for (const QModelIndex& index : modelIndices)
    {
        wydb::ElementId id = this->getElementIdByModelIndex(index);
        if (!id.isNull()) ids.insert(id);
    }
    if (ids.size() != 1) return;
    wydb::ElementId id = *ids.cbegin();

    // 开启事务
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }
    wydb::TransactionManager* pTransMgr = pDb->getTransactionManager();
    assert(pTransMgr);
    wydb::Transaction* pTrans = pTransMgr->startTransaction();
    if (!pTrans)
    {
        assert(false);
        return;
    }

    // 取消布尔
    {
        wy3d::Boolean* pBoolean = wy3d::Boolean::cast(pTrans->getElementForWrite(id));
        if (!pBoolean)
        {
            assert(false);
            pTransMgr->abortTransaction();
            return;
        }
        pBoolean->cancelBoolean();
    }

    // 提交事务
    pTransMgr->endTransaction();
}

void FeatureTreeWidget::onContextMenu_ShowHideColumnID()
{
    assert(_treeView);
    bool isIdColHidden = _treeView->isColumnHidden(kColumn_Id);
    if (isIdColHidden)
    {
        _treeView->showColumn(kColumn_Id);
    }
    else
    {
        _treeView->hideColumn(kColumn_Id);
    }
}

void FeatureTreeWidget::onContextMenu_ShowErrorInfo()
{
    FeatureRow* pFeatRow(nullptr);
    QModelIndexList selectedIndices = _treeView->selectionModel()->selectedIndexes();
    for (const QModelIndex& index : selectedIndices)
    {
        wydb::ElementId id = this->getElementIdByModelIndex(index);
        pFeatRow = this->uiFindRow(id);
        if (pFeatRow) break;
    }
    if (pFeatRow && pFeatRow->errorCode > 0)
    {
        MessageBoxUtil::showError(static_cast<unsigned int>(pFeatRow->errorCode));
    }
    else
    {
        assert(false);
    }
}

void FeatureTreeWidget::onCustomContextMenu(const QPoint& pos)
{
    if (TransactionUtil::hasActiveTransaction())
    {
        return;
    }
    assert(_treeView);

    QMenu menu;
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.getCount() > 0) // 有选中项
    {
        // 提取选中的元素
        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb)
        {
            assert(false);
            return;
        }
        std::set<wydb::ElementId> selectedIds;
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            selectedIds.insert(iter.current().getElementId());
        }
        std::list<const wydb::Element*> selectedElems;
        for (const wydb::ElementId& id : selectedIds)
        {
            const wydb::Element* pElem = pDb->getElement(id);
            if (!pElem)
            {
                assert(false);
                continue;
            }
            selectedElems.emplace_back(pElem);
        }

        bool addExportAction(!selectedElems.empty());
        for (const wydb::Element* pElem : selectedElems)
        {
            if (!wy3d::Solid::cast(pElem) && !wy3d::Sheet::cast(pElem))
            {
                addExportAction = false;
                break;
            }
        }

        if (ss.getCount() == 1)
        {
            const wydb::Element* pElem = selectedElems.front();
            if (const wy3d::Boolean* pBoolean = wy3d::Boolean::cast(pElem))
            {
                // 取消布尔
                QAction* actionCancelBoolean = menu.addAction(tr("Cancel Boolean"));
                this->connect(actionCancelBoolean, SIGNAL(triggered()), this, SLOT(onContextMenu_CancelBoolean()));
            }
            else if (const wy3d::Sketch* pSketch = wy3d::Sketch::cast(pElem))
            {
                // 编辑草图
                CommandAction* pActionEditSketch = new CommandAction(CommandNames::EditSketch, &menu);
                pActionEditSketch->setText(QCoreApplication::translate("MainWindow","Edit Sketch"));
                pActionEditSketch->setIcon(QIcon(":/images/Edit_Sketch.svg"));
                menu.addAction(pActionEditSketch);

                // 导出草图
                CommandAction* pActionExportSketch = new CommandAction(CommandNames::ExportSketch, &menu);
                pActionExportSketch->setText(QCoreApplication::translate("MainWindow", "Export Sketch"));
                pActionExportSketch->setIcon(QIcon(":/images/Document_Export.svg"));
                menu.addAction(pActionExportSketch);

                // 正视于
                CommandAction* pActionViewNormalTo = new CommandAction(CommandNames::ViewNormalTo, &menu);
                pActionViewNormalTo->setText(QCoreApplication::translate("MainWindow", "View Normal To"));
                pActionViewNormalTo->setIcon(QIcon(":/images/View_Normal.svg"));
                menu.addAction(pActionViewNormalTo);

            }
            else if (const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pElem))
            {
                // 正视于
                CommandAction* pActionViewNormalTo = new CommandAction(CommandNames::ViewNormalTo, &menu);
                pActionViewNormalTo->setText(QCoreApplication::translate("MainWindow", "View Normal To"));
                pActionViewNormalTo->setIcon(QIcon(":/images/View_Normal.svg"));
                menu.addAction(pActionViewNormalTo);
            }

        }

        // 遍历选择集合中的元素
        bool addShowAction(false);
        bool addHideAction(false);
        for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
        {
            const wydb::Element* pElem = pDb->getElement(iter.current().getElementId());
            if (!pElem) continue;
            if (pElem->isHidden()) addShowAction = true;
            else addHideAction = true;
        }

        // 显示
        if (addShowAction)
        {
            CommandAction* pActionShow = new CommandAction(CommandNames::Show, &menu);
            pActionShow->setText(QCoreApplication::translate("MainWindow", "Show"));
            pActionShow->setIcon(QIcon(":/images/Edit_Show.svg"));
            menu.addAction(pActionShow);
        }

        // 隐藏
        if (addHideAction)
        {
            CommandAction* pActionHide = new CommandAction(CommandNames::Hide, &menu);
            pActionHide->setText(QCoreApplication::translate("MainWindow", "Hide"));
            pActionHide->setIcon(QIcon(":/images/Edit_Hide.svg"));
            menu.addAction(pActionHide);
        }

        // 复制
        if (CopyPasteUtil::canCopy(selectedElems))
        {
            CommandAction* pActionCopyClip = new CommandAction(CommandNames::CopyClip, &menu);
            pActionCopyClip->setText(QCoreApplication::translate("MainWindow", "Copy"));
            pActionCopyClip->setIcon(QIcon(":/images/Edit_Copy.svg"));
            menu.addAction(pActionCopyClip);
        }

        // 删除
        QAction* actionErase = menu.addAction(tr("Erase"));
        actionErase->setIcon(QIcon(":/images/Edit_Delete.svg"));
        this->connect(actionErase, SIGNAL(triggered()), this, SLOT(onContextMenu_Erase()));

        // 导出实体
        if (addExportAction)
        {
            QAction* pActionExport = new CommandAction(CommandNames::ExportSelected, &menu);
            pActionExport->setText(QCoreApplication::translate("MainWindow", "Export"));
            pActionExport->setIcon(QIcon(":/images/Document_Export.svg"));
            menu.addAction(pActionExport);
        }

        // 选中单项:显示错误信息
        if (ss.getCount() == 1)
        {
            FeatureRow* pFeatRow(nullptr);
            QModelIndexList selectedIndices = _treeView->selectionModel()->selectedIndexes();
            for (const QModelIndex& index : selectedIndices)
            {
                wydb::ElementId id = this->getElementIdByModelIndex(index);
                pFeatRow = this->uiFindRow(id);
                if (pFeatRow) break;
            }
            if (pFeatRow && pFeatRow->errorCode > 0)
            {
                QAction* actionErrorInfo = menu.addAction(tr("Error Information"));
                this->connect(actionErrorInfo, SIGNAL(triggered()), this, SLOT(onContextMenu_ShowErrorInfo()));
            }
        }
    }
    else // 没有选中项(点击空白处)
    {
        // 粘贴
        if (CopyPasteUtil::canPaste())
        {
            CommandAction* pActionPasteClip = new CommandAction(CommandNames::PasteClip, &menu);
            pActionPasteClip->setText(QCoreApplication::translate("MainWindow", "Paste"));
            pActionPasteClip->setIcon(QIcon(":/images/Edit_PasteClip.svg"));
            menu.addAction(pActionPasteClip);
        }

        // 显示ID列
        bool isIdColHidden = _treeView->isColumnHidden(kColumn_Id);
        QAction* actionShowIdColumn = menu.addAction(isIdColHidden ? tr("Show ID column") : tr("Hide ID column"));
        this->connect(actionShowIdColumn, SIGNAL(triggered()), this, SLOT(onContextMenu_ShowHideColumnID()));
    }
    menu.exec(_treeView->viewport()->mapToGlobal(pos));
}

void FeatureTreeWidget::onCollapsed(const QModelIndex& index)
{
    wydb::ElementId id = this->getElementIdByModelIndex(index);
    if (id.isNull()) return;
    auto iterExpanded = _id2IsExpanded.find(id);
    if (iterExpanded == _id2IsExpanded.cend())
    {
        assert(false);
        return;
    }
    iterExpanded->second = false;
}

void FeatureTreeWidget::onExpanded(const QModelIndex& index)
{
    wydb::ElementId id = this->getElementIdByModelIndex(index);
    if (id.isNull()) return;
    auto iterExpanded = _id2IsExpanded.find(id);
    if (iterExpanded == _id2IsExpanded.cend())
    {
        assert(false);
        return;
    }
    iterExpanded->second = true;
}

void FeatureTreeWidget::onTreeViewClicked(const QModelIndex& index)
{
    if (!index.isValid())
    {
        return;
    }

    wydb::ElementId id = this->getElementIdByModelIndex(index);
    if (id.isNull()) return;

    wyap::CmdManager* pCmdMgr = Application::instance().getCmdManager();
    if (!pCmdMgr) return;
    wyap::CmdExecution* pCmdExecutor = pCmdMgr->getCurrentModalCmdExecution();
    if (!pCmdExecutor) return;
    GuiCommand* pGuiCommand = dynamic_cast<GuiCommand*>(pCmdExecutor);
    if (!pGuiCommand) return; // 当前模态命令不是GuiCommand时忽略
    pGuiCommand->onFeatureTreeItemClicked(id);
}
