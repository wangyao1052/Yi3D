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

#include "PropertyEditorWidget.h"
#include "adapters/ParamEditorAdapter.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTextBrowser>
#include <QComboBox>
#include <QCheckBox>

#include <wyVector3.h>
#include <wy3dMath.h>
#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dFeature.h>
#include <wy3dSolid.h>
#include <wy3dPrimitive.h>
#include <wy3dBoolean.h>
#include <wy3dParamNames.h>
#include <wyapDocument.h>
#include <wyapDocManager.h>
#include <wyapSelection.h>
#include <wyapSelManager.h>
#include <QHBoxLayout>
#include <QGridLayout>
#include <set>

#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchCurve.h>

#include "application/Application.h"
#include "ParamLineEdit.h"
#include "TransformLineEdit.h"
#include "translation/ParamNamesTranslation.h"
#include "environments/sketch/SketchEnvironment.h"
#include "utils/MessageBoxUtil.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNode.h"

PropertyEditorWidget::PropertyEditorWidget(QWidget* parent)
    : QWidget(parent), _pMainLayout(nullptr), _pParamsGridLayout(nullptr), _isModifyingElems(false), _isReadOnly(false)
{
    this->setWindowTitle("Property");
    this->setMinimumWidth(180);
    this->resize(200, 400);

    _pMainLayout = new QVBoxLayout(this);
    this->setLayout(_pMainLayout);
    {
        _pParamsGridLayout = new QGridLayout();
        _pMainLayout->addLayout(_pParamsGridLayout);
        _pMainLayout->addStretch();
    }

    // 文档管理器反应器
    wyap::DocManager* pDocMgr = Application::instance().getDocManager();
    assert(pDocMgr);
    pDocMgr->addReactor(this);

    // 选择集反应器
    wyap::SelManager* pSelMgr = Application::instance().getSelManager();
    assert(pSelMgr);
    pSelMgr->addReactor(this);

    // Gizmo 管理器反应器
    wyap::GizmoManager* pGizmoMgr = Application::instance().getGizmoManager();
    assert(pGizmoMgr);
    pGizmoMgr->addReactor(this);
}

PropertyEditorWidget::~PropertyEditorWidget()
{
    wyap::GizmoManager* pGizmoMgr = Application::instance().getGizmoManager();
    if (pGizmoMgr)
        pGizmoMgr->removeReactor(this);
    wyap::DocManager* pDocMgr = Application::instance().getDocManager();
    if (pDocMgr)
        pDocMgr->removeReactor(this);
    wyap::SelManager* pSelMgr = Application::instance().getSelManager();
    if (pSelMgr)
        pSelMgr->removeReactor(this);
}

void PropertyEditorWidget::clearLayoutContents(QLayout* pLayout)
{
    if (!pLayout) return;
    int count = pLayout->count();
    if (count == 0) return;

    int lastIndex = count - 1;
    while (lastIndex >= 0)
    {
        QLayoutItem* pLayoutItem = pLayout->itemAt(lastIndex);
        if (pLayoutItem)
        {
            QWidget* pWidget = pLayoutItem->widget();
            QLayout* pChildLayout = pLayoutItem->layout();
            QSpacerItem* pSpacerItem = pLayoutItem->spacerItem();

            if (pWidget)
            {
                pLayout->removeWidget(pWidget);
                delete pWidget;
            }
            else if (pChildLayout)
            {
                this->clearLayoutContents(pChildLayout);
                pLayout->removeItem(pChildLayout);
                delete pChildLayout;
            }
            else if (pSpacerItem)
            {
                pLayout->removeItem(pSpacerItem);
                delete pSpacerItem;
            }
        }
        else
        {
            assert(false);
        }
        --lastIndex;
    }
}

void PropertyEditorWidget::clearParamsGridLayout()
{
    assert(_pParamsGridLayout);
    this->clearLayoutContents(_pParamsGridLayout);
}

static wyrx::ClassInfo* findLowestCommonBaseClass(
    const std::set<wyrx::ClassInfo*>& classInfos)
{
    if (classInfos.empty()) return nullptr;
    for (wyrx::ClassInfo* pClassInfo : classInfos)
    {
        if (!pClassInfo)
        {
            assert(false);
            return nullptr;
        }
    }

    wyrx::ClassInfo* pFirstClassInfo = *classInfos.cbegin();
    if (classInfos.size() == 1)
    {
        return pFirstClassInfo;
    }

    std::vector<wyrx::ClassInfo*> chain;
    for (wyrx::ClassInfo* pClassInfo = pFirstClassInfo;
        pClassInfo != nullptr;
        pClassInfo = pClassInfo->parent())
    {
        chain.push_back(pClassInfo);
    }

    wyrx::ClassInfo* pCommonClassInfo = nullptr;
    for (wyrx::ClassInfo* pAncestor : chain)
    {
        bool allDerived = true;
        for (wyrx::ClassInfo* pClassInfo : classInfos)
        {
            if (!pClassInfo->isDerivedFrom(pAncestor))
            {
                allDerived = false;
                break;
            }
        }
        if (allDerived)
        {
            pCommonClassInfo = pAncestor;
            break;
        }
    }
    assert(pCommonClassInfo);
    return pCommonClassInfo;
}

bool PropertyEditorWidget::collectCommonParams(
    const std::vector<const wydb::Element*>& elements,
    std::vector<const wydb::ParameterDefinition*>& commonParamDefs,
    std::string& commonClassName)
{
    commonParamDefs.clear();
    commonClassName.clear();
    if (elements.empty())
    {
        assert(false);
        return false;
    }

    // Collect unique class infos.
    std::set<wyrx::ClassInfo*> classInfos;
    for (const wydb::Element* pElem : elements)
    {
        if (!pElem)
        {
            assert(false);
            return false;
        }
        classInfos.insert(pElem->getClassInfo());
    }

    // Find common base class.
    wyrx::ClassInfo* pCommonClassInfo = findLowestCommonBaseClass(classInfos);
    if (!pCommonClassInfo)
    {
        assert(false);
        return false;
    }
    commonClassName = pCommonClassInfo->className();

    // Collect parameter definitions by walking up the ClassInfo hierarchy,
    // from the common base class up to the root.
    // <1> Build the ancestor chain: walk from the common base class up to the root.
    std::vector<const wyrx::ClassInfo*> classHierarchy;
    classHierarchy.reserve(10);
    for (const wyrx::ClassInfo* pClassInfo = pCommonClassInfo; pClassInfo != nullptr; pClassInfo = pClassInfo->parent())
    {
        classHierarchy.push_back(pClassInfo);
    }
    // <2>Iterate from root down so base-class params come before derived-class params.
    commonParamDefs.clear();
    commonParamDefs.reserve(20);
    for (auto iter = classHierarchy.crbegin(); iter != classHierarchy.crend(); ++iter)
    {
        const wyrx::ClassExtension* pClassExt = (*iter)->getExtension(wydb::ParameterSchemaExtension::classInfo());
        if (!pClassExt)
        {
            continue;
        }
        const wydb::ParameterSchemaExtension* pParamSchemaExt = wydb::ParameterSchemaExtension::cast(pClassExt);
        if (!pParamSchemaExt)
        {
            assert(false);
            continue;
        }
        std::vector<const wydb::ParameterDefinition*> paramDefs = pParamSchemaExt->getParameterDefinitions();
        commonParamDefs.insert(commonParamDefs.end(), paramDefs.begin(), paramDefs.end());
    }
    return true;
}

void PropertyEditorWidget::showParameterValueList(const std::vector<const wydb::Element*>& elements)
{
    std::vector<const wydb::ParameterDefinition*> commonParamDefs;
    std::string commonClassName;
    if (!this->collectCommonParams(elements, commonParamDefs, commonClassName))
    {
        return;
    }
    assert(!commonClassName.empty());
    if (commonParamDefs.empty() || commonClassName.empty())
    {
        return;
    }

    std::vector<ParamInfo> paramValueInfos;
    paramValueInfos.reserve(commonParamDefs.size());
    for (size_t elementIndex = 0; elementIndex < elements.size(); ++elementIndex)
    {
        const wydb::Element* pElem = elements[elementIndex];
        if (!pElem)
        {
            assert(false);
            return;
        }
        for (size_t i = 0; i < commonParamDefs.size(); ++i)
        {
            const std::string& paramName = commonParamDefs[i]->getName();
            const std::string& paramClassName = commonParamDefs[i]->getClassName();
            wydb::ParameterValueUPtr paramValue = pElem->getParameterValue(paramClassName, paramName);
            if (!paramValue)
            {
                assert(false);
                return;
            }
            if (0 == elementIndex) // first element
            {
                ParamInfo paramValueInfo;
                paramValueInfo.paramDef = commonParamDefs[i];
                paramValueInfo.paramValue = std::move(paramValue);
                paramValueInfos.emplace_back(std::move(paramValueInfo));
            }
            else
            {
                ParamInfo& paramInfo = paramValueInfos[i];
                assert(paramInfo.paramValue->getType() == paramValue->getType());
                if (paramInfo.hasSameValue && !paramInfo.paramValue->equals(*paramValue))
                {
                    paramInfo.hasSameValue = false;
                }
            }
        }
    }

    // 显示参数属性
    for (size_t i = 0; i < commonParamDefs.size(); ++i)
    {
        ParamInfo& info = paramValueInfos[i];
        assert(info.paramValue);

        QLabel* pLabel = this->newLabel("", this);
        QString qstrParamDispName = ParamNamesTranslation::instance().getParamDisplayName(info.paramDef->getClassName(), info.paramDef->getName());
        pLabel->setText(qstrParamDispName);

        QWidget* pEditControl = this->createEditorWidgetForParam(info);

        int row = _pParamsGridLayout->rowCount();
        _pParamsGridLayout->addWidget(pLabel, row, 1);
        _pParamsGridLayout->addWidget(pEditControl, row, 2);
    }
}

QWidget* PropertyEditorWidget::createEditorWidgetForParam(ParamInfo& info)
{
    const wydb::ParameterValue& paramValue = *info.paramValue;
    const ParamEditorAdapter* adapter = ParamEditorRegistry::instance().find(paramValue);
    assert(adapter);
    return adapter->create(info.paramDef->getClassName(), info.paramDef->getName(), paramValue, info.hasSameValue, info.paramDef->isReadonly(), this);
}

void PropertyEditorWidget::showTransform(const std::vector<const wydb::Element*>& elements)
{
    if (elements.empty()) return;
    Scene* pScene = Application::instance().getActiveScene();
    if (!pScene)
    {
        assert(false);
        return;
    }

    wy::Vector3 pos, rot;
    int index(0);
    bool isSame_PosX(true), isSame_PosY(true), isSame_PosZ(true);
    bool isSame_RotX(true), isSame_RotY(true), isSame_RotZ(true);
    for (const wydb::Element* pElem : elements)
    {
        if (!pElem)
        {
            assert(false);
            return;
        }
        const wy3d::Primitive* pPrimitive = wy3d::Primitive::cast(pElem);
        if (!pPrimitive) return;

        // 基础形体有实体修改不显示Transform
        if (!pPrimitive->getModifications().empty())
        {
            return;
        }
        // 基础形体的渲染结点为实体修改结点不显示Transform
        ElementNode* pElemNode = pScene->getElementNode(pPrimitive->getId());
        if (pElemNode && pElemNode->getNodeType() == ElementNodeType::SolidModification)
        {
            return;
        }

        ++index;
        wy::Vector3 curPos = pPrimitive->getPosition();
        wy::Vector3 curRot = pPrimitive->getRotation();
        if (1 == index) // 第一个元素
        {
            pos = curPos;
            rot = curRot;
        }
        else // 第2...n个元素
        {
            // 对比PosX
            if (isSame_PosX)
            {
                if (pos.x() != curPos.x()) isSame_PosX = false;
            }
            // 对比PosY
            if (isSame_PosY)
            {
                if (pos.y() != curPos.y()) isSame_PosY = false;
            }
            // 对比PosZ
            if (isSame_PosZ)
            {
                if (pos.z() != curPos.z()) isSame_PosZ = false;
            }
            // 对比RotX
            if (isSame_RotX)
            {
                if (rot.x() != curRot.x()) isSame_RotX = false;
            }
            // 对比RotY
            if (isSame_RotY)
            {
                if (rot.y() != curRot.y()) isSame_RotY = false;
            }
            // 对比RotZ
            if (isSame_RotZ)
            {
                if (rot.z() != curRot.z()) isSame_RotZ = false;
            }
        }
    }
    if (0 == index) return;

    // 显示位移控件
    {
        int row = _pParamsGridLayout->rowCount();
        QLabel* pLabelPos = this->newLabel(tr("Position "), this);
        _pParamsGridLayout->addWidget(pLabelPos, row, 1);
        QGridLayout* pPosLayout = newTransformLayout_Position(pos, isSame_PosX, isSame_PosY, isSame_PosZ);
        _pParamsGridLayout->addLayout(pPosLayout, row, 2);
    }

    // 显示旋转控件
    {
        int row = _pParamsGridLayout->rowCount();
        QLabel* pLabelPos = this->newLabel(tr("Rotation "), this);
        _pParamsGridLayout->addWidget(pLabelPos, row, 1);
        QGridLayout* pPosLayout = newTransformLayout_Rotation(rot, isSame_RotX, isSame_RotY, isSame_RotZ);
        _pParamsGridLayout->addLayout(pPosLayout, row, 2);
    }
}

QLabel* PropertyEditorWidget::newLabel(const QString& text, QWidget* parent)
{
    QLabel* pLabel = new QLabel(text, parent);
    QFont font = pLabel->font();
    font.setPointSize(12);
    pLabel->setFont(font);
    return pLabel;
}

void PropertyEditorWidget::regen()
{
    // 清空窗口内容
    this->clearParamsGridLayout();

    // 获取选择集中所有的元素
    const wyap::SelectionSet& ss = Application::instance().getSelManager()->getSelections();
    if (ss.isEmpty()) return;
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }
    std::vector<const wydb::Element*> elements;
    elements.reserve(ss.getCount());
    for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::ElementId& id = iter.current().getElementId();
        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem)
        {
            assert(false);
            return;
        }
        elements.emplace_back(pElem);
    }
    if (elements.empty()) return;

    this->showParameterValueList(elements);

    wyap::Environment* pCurEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    // 草图环境下不显示 Transform
    if (!dynamic_cast<SketchEnvironment*>(pCurEnv))
    {
        this->showTransform(elements);
    }
}

void PropertyEditorWidget::refresh()
{
    // 遍历所有的参数文本框子控件,刷新文本显示.
    QList<ParamLineEdit*> paramLineEdits = this->findChildren<ParamLineEdit*>();
    for (ParamLineEdit* pParamLineEdit : paramLineEdits)
    {
        assert(pParamLineEdit);
        pParamLineEdit->refresh();
    }
}

void PropertyEditorWidget::setReadOnly(bool isReadOnly)
{
    if (_isReadOnly == isReadOnly)
    {
        return;
    }

    _isReadOnly = isReadOnly;
    QList<QWidget*> children = this->findChildren<QWidget*>();
    for (QWidget* child : children)
    {
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(child))
        {
            lineEdit->setReadOnly(isReadOnly);
        }
        else if (QTextEdit* textEdit = qobject_cast<QTextEdit*>(child))
        {
            textEdit->setReadOnly(isReadOnly);
        }
        else if (QSpinBox* spinBox = qobject_cast<QSpinBox*>(child))
        {
            spinBox->setReadOnly(isReadOnly);
        }
        else if (QDoubleSpinBox* doubleSpinBox = qobject_cast<QDoubleSpinBox*>(child))
        {
            doubleSpinBox->setReadOnly(isReadOnly);
        }
        else if (QTextBrowser* textBrowser = qobject_cast<QTextBrowser*>(child))
        {
            textBrowser->setReadOnly(isReadOnly);
        }
        else if (QComboBox* comboBox = qobject_cast<QComboBox*>(child))
        {
            comboBox->setEditable(!isReadOnly);
        }
        else if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(child))
        {
            checkBox->setCheckable(!isReadOnly);
        }
    }
}

QGridLayout* PropertyEditorWidget::newTransformLayout_Position(const wy::Vector3& pos,
    bool isSameX, bool isSameY, bool isSameZ)
{
    QGridLayout* pLayout = new QGridLayout();

    // position x
    QLabel* pLabelX = this->newLabel("X", this);
    wydb::ParameterValueUPtr pParamValX = wydb::ParameterValue::createDouble(pos.x());
    TransformLineEdit* pLineEditX = new TransformLineEdit(TransformUnit::PosX, std::move(pParamValX), isSameX, this);
    pLayout->addWidget(pLabelX, 1, 1);
    pLayout->addWidget(pLineEditX, 1, 2);

    // position y
    QLabel* pLabelY = this->newLabel("Y", this);
    wydb::ParameterValueUPtr pParamValY = wydb::ParameterValue::createDouble(pos.y());
    TransformLineEdit* pLineEditY = new TransformLineEdit(TransformUnit::PosY, std::move(pParamValY), isSameY, this);
    pLayout->addWidget(pLabelY, 1, 3);
    pLayout->addWidget(pLineEditY, 1, 4);

    // position z
    QLabel* pLabelZ = this->newLabel("Z", this);
    wydb::ParameterValueUPtr pParamValZ = wydb::ParameterValue::createDouble(pos.z());
    TransformLineEdit* pLineEditZ = new TransformLineEdit(TransformUnit::PosZ, std::move(pParamValZ), isSameZ, this);
    pLayout->addWidget(pLabelZ, 1, 5);
    pLayout->addWidget(pLineEditZ, 1, 6);

    return pLayout;
}

QGridLayout* PropertyEditorWidget::newTransformLayout_Rotation(const wy::Vector3& rot,
    bool isSameX, bool isSameY, bool isSameZ)
{
    QGridLayout* pLayout = new QGridLayout();

    // rotation x
    QLabel* pLabelX = this->newLabel("X", this);
    wydb::ParameterValueUPtr pParamValX = wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(rot.x()));
    TransformLineEdit* pLineEditX = new TransformLineEdit(TransformUnit::RotX, std::move(pParamValX), isSameX, this);
    pLayout->addWidget(pLabelX, 1, 1);
    pLayout->addWidget(pLineEditX, 1, 2);

    // rotation y
    QLabel* pLabelY = this->newLabel("Y", this);
    wydb::ParameterValueUPtr pParamValY = wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(rot.y()));
    TransformLineEdit* pLineEditY = new TransformLineEdit(TransformUnit::RotY, std::move(pParamValY), isSameY, this);
    pLayout->addWidget(pLabelY, 1, 3);
    pLayout->addWidget(pLineEditY, 1, 4);

    // rotation z
    QLabel* pLabelZ = this->newLabel("Z", this);
    wydb::ParameterValueUPtr pParamValZ = wydb::ParameterValue::createDouble(wy3d::radiansToDegrees(rot.z()));
    TransformLineEdit* pLineEditZ = new TransformLineEdit(TransformUnit::RotZ, std::move(pParamValZ), isSameZ, this);
    pLayout->addWidget(pLabelZ, 1, 5);
    pLayout->addWidget(pLineEditZ, 1, 6);

    return pLayout;
}

void PropertyEditorWidget::onSelectionChanged(
    const wyap::SelectionSet& addedSS,
    const wyap::SelectionSet& removedSS,
    const wyap::SelectionSet& curSS)
{
    this->regen();
}

void PropertyEditorWidget::onDocumentToBeDeactivated(wyap::Document* pDocToDeactivate)
{
    if (!pDocToDeactivate)
    {
        assert(false);
        return;
    }

    // Remove database reactor.
    wydb::Database* pDb = pDocToDeactivate->getDatabase();
    if (pDb) pDb->removeReactor(this);
    else assert(false);
}

void PropertyEditorWidget::onDocumentActivated(wyap::Document* pActivatedDoc)
{
    if (!pActivatedDoc)
    {
        assert(false);
        return;
    }

    // Add database reactor.
    wydb::Database* pDb = pActivatedDoc->getDatabase();
    if (pDb) pDb->addReactor(this);
    else assert(false);
    
}

void PropertyEditorWidget::onDatabaseChanged(
    const wydb::Database* pDb,
    const wydb::Transaction* pTransaction,
    const wydb::DatabaseChangeInfo& changeInfo)
{
    if (_isModifyingElems) return;

    // Gizmo 活跃时仅标记 dirty，不刷新
    if (_gizmoState.isActive)
    {
        _gizmoState.needsRefresh = true;
        return;
    }

    const wyap::SelectionSet& sels = Application::instance().getSelManager()->getSelections();
    if (sels.isEmpty()) return;
    for (auto iter = sels.createIterator();
        !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current().getElementId();
        if (changeInfo.modifiedIds.find(id) != changeInfo.modifiedIds.cend())
        {
            this->regen();
        }
    }
}

void PropertyEditorWidget::onGizmoActivated(wyap::GizmoSPtr pGizmo)
{
    _gizmoState.isActive = true;
    _gizmoState.needsRefresh = false;
    _gizmoState.savedReadOnly = _isReadOnly;
    if (!_isReadOnly)
        this->setReadOnly(true);
}

void PropertyEditorWidget::onGizmoToBeDeactivated(wyap::GizmoSPtr pGizmo)
{
    _gizmoState.isActive = false;
    if (!_gizmoState.savedReadOnly)
        this->setReadOnly(false);
    if (_gizmoState.needsRefresh)
    {
        this->refresh();
        _gizmoState.needsRefresh = false;
    }
}
