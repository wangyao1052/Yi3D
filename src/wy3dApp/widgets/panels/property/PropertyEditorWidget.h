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

#ifndef WY3DAPP_PROPERTY_EDITOR_WIDGET_H
#define WY3DAPP_PROPERTY_EDITOR_WIDGET_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>

#include <wyVector3.h>
#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wydbParameter.h>
#include <wyapSelection.h>
#include <wyapSelManager.h>
#include <wyapGizmoManager.h>
#include <wy3dColor.h>
#include <wyapDocManager.h>

class PropertyEditorWidget : public QWidget,
    public wyap::SelManagerReactor,
    public wyap::DocManagerReactor,
    public wyap::GizmoManagerReactor,
    public wydb::DatabaseReactor
{
    Q_OBJECT
public:
    explicit PropertyEditorWidget(QWidget* parent);
    ~PropertyEditorWidget();

    // 重载设置首选大小
    // 对布局特别有用
    virtual QSize sizeHint() const override
    {
        return QSize(350, 400);
    }

    // 开始修改元素
    void startModifyElements() { _isModifyingElems = true; }
    // 结束修改元素
    void endModifyElements() { _isModifyingElems = false; }
    // 重新生成属性控件
    void regen();
    // 刷新
    void refresh();
    // 设置只读
    void setReadOnly(bool isReadOnly);
    // 是否只读
    bool isReadOnly() const { return _isReadOnly; }

private:
    // 显示Parameter
    struct ParamInfo
    {
        const wydb::ParameterDefinition* paramDef = nullptr;
        wydb::ParameterValueUPtr paramValue;
        bool hasSameValue = true;
    };
    void showParameterValueList(const std::vector<const wydb::Element*>& elems);

    // 收集选中元素的公共参数：找到共同基类后，沿 ClassInfo 层级链向上遍历收集参数定义
    bool collectCommonParams(const std::vector<const wydb::Element*>& elements,
        std::vector<const wydb::ParameterDefinition*>& commonParamDefs,
        std::string& commonClassName);
    // 显示Transform
    void showTransform(const std::vector<const wydb::Element*>& elems);
    // 显示样条的点选择器(仅单选一条样条时)
    void showSketchSplinePoints(const std::vector<const wydb::Element*>& elems);
    // 创建Transform位置布局
    QGridLayout* newTransformLayout_Position(const wy::Vector3& pos, bool isSameX, bool isSameY, bool isSameZ);
    // 创建Transform旋转布局
    QGridLayout* newTransformLayout_Rotation(const wy::Vector3& rot, bool isSameX, bool isSameY, bool isSameZ);
    // 清空Params布局
    void clearParamsGridLayout();
    void clearLayoutContents(QLayout* pLayout);

    // 选择集变更事件
    virtual void onSelectionChanged(
        const wyap::SelectionSet& addedSS,
        const wyap::SelectionSet& removedSS,
        const wyap::SelectionSet& curSS) override;

    // 文档激活事件
    virtual void onDocumentToBeDeactivated(wyap::Document* pDocToDeactivate) override;
    virtual void onDocumentActivated(wyap::Document* pActivatedDoc) override;

    // database reactor
    virtual void onDatabaseChanged(
        const wydb::Database* pDb,
        const wydb::Transaction* pTransaction,
        const wydb::DatabaseChangeInfo& changeInfo) override;

    // gizmo manager reactor
    virtual void onGizmoActivated(wyap::GizmoSPtr pGizmo) override;
    virtual void onGizmoToBeDeactivated(wyap::GizmoSPtr pGizmo) override;

    QLabel* newLabel(const QString& text, QWidget* parent);

    // 根据参数值类型创建对应的编辑控件
    QWidget* createEditorWidgetForParam(ParamInfo& info);

private:
    QVBoxLayout* _pMainLayout;
    QGridLayout* _pParamsGridLayout;
    bool _isModifyingElems;
    bool _isReadOnly;

    struct GizmoState
    {
        bool isActive = false;
        bool savedReadOnly = false;
        bool needsRefresh = false;
    } _gizmoState;


};

#endif // WY3DAPP_PROPERTY_EDITOR_WIDGET_H