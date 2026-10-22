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

#include "DemoParametersCmd.h"

#include <cassert>

#include <QCoreApplication>
#include <QMessageBox>
#include <QStringList>
#include <QTimer>

#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbElement.h>
#include <wydbParameter.h>
#include <wy3dDefs.h>
#include <wy3dSketchEntity.h>

#include <wy3dApp/Application.h>
#include <wy3dApp/SketchEnvironment.h>
#include "scene/nodes/ElementNodeType.h"
#include <wy3dApp/CommonSelFilters.h>
#include <wy3dApp/ParamDisplayNameRegistry.h>
#include "widgets/frame/MainWindow.h"

namespace {

QString paramValueToText(const wydb::ParameterValue& paramValue)
{
    switch (paramValue.getType())
    {
    case wydb::ParameterValue::Type::Integer:
        return QString::number(paramValue.asInteger());
    case wydb::ParameterValue::Type::Double:
        return QString::number(paramValue.asDouble());
    case wydb::ParameterValue::Type::Boolean:
        return paramValue.asBoolean() ? QStringLiteral("1") : QStringLiteral("0");
    case wydb::ParameterValue::Type::String:
        return QString::fromUtf8(paramValue.asString().c_str());
    case wydb::ParameterValue::Type::ElementId:
        return QString::number(paramValue.asElementId().value());
    case wydb::ParameterValue::Type::Vector2:
    {
        const wy::Vector2 v = paramValue.asVector2();
        return QStringLiteral("(%1, %2)").arg(v.x()).arg(v.y());
    }
    case wydb::ParameterValue::Type::Vector3:
    {
        const wy::Vector3 v = paramValue.asVector3();
        return QStringLiteral("(%1, %2, %3)").arg(v.x()).arg(v.y()).arg(v.z());
    }
    default:
        return QStringLiteral("-");
    }
}

// Collects the parameter definitions registered along the whole class hierarchy
// (root first, so base-class parameters come before derived-class ones).
std::vector<const wydb::ParameterDefinition*> collectParamDefinitions(
    const wyrx::ClassInfo* pClassInfo)
{
    std::vector<const wyrx::ClassInfo*> classHierarchy;
    for (const wyrx::ClassInfo* pIter = pClassInfo; pIter != nullptr; pIter = pIter->parent())
    {
        classHierarchy.push_back(pIter);
    }

    std::vector<const wydb::ParameterDefinition*> paramDefs;
    for (auto iter = classHierarchy.crbegin(); iter != classHierarchy.crend(); ++iter)
    {
        const wyrx::ClassExtension* pClassExt =
            (*iter)->getExtension(wydb::ParameterSchemaExtension::classInfo());
        if (!pClassExt)
        {
            continue;
        }
        const wydb::ParameterSchemaExtension* pParamSchemaExt =
            wydb::ParameterSchemaExtension::cast(pClassExt);
        if (!pParamSchemaExt)
        {
            assert(false);
            continue;
        }
        const std::vector<const wydb::ParameterDefinition*> defs = pParamSchemaExt->getParameterDefinitions();
        paramDefs.insert(paramDefs.cend(), defs.cbegin(), defs.cend());
    }
    return paramDefs;
}

} // namespace

DemoParametersCmd::DemoParametersCmd()
    : wy3dApp::OsgGuiCommand()
    , _pointPickOption()
{
}

DemoParametersCmd::~DemoParametersCmd()
{
}

wyap::CmdExecution::StartResult DemoParametersCmd::onStart()
{
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    wyap::Environment* pEnv = wy3dApp::getApplication()->getEnvManager()->getActiveEnvironment();
    if (!dynamic_cast<wy3dApp::SketchEnvironment*>(pEnv))
    {
        return wyap::CmdExecution::StartResult::Rejected;
    }

    _pointPickOption.allowedElemTypes = wy3dApp::ElementNodeType::SketchEntity;
    _pointPickOption.selType = wy3d::SelectionType::Element;
    _pointPickOption.postFilter = wy3dApp::SingleClassSelPostFilter::create<wy3d::SketchEntity>();

    wy3dApp::getApplication()->getStatusBar()->setTips(QCoreApplication::translate("DemoParametersCmd",
        "Click a sketch element to list its parameters."));

    wy3dApp::getApplication()->setCursor(wy3dApp::CursorType::SelectElements);

    return wyap::CmdExecution::StartResult::Succeeded;
}

void DemoParametersCmd::cleanUp()
{
    _hoverPreview.clear();
}

void DemoParametersCmd::onMouseMove(const wy3dApp::MouseEvent& event)
{
    wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);
    _hoverPreview.update(sel);
}

void DemoParametersCmd::onLeftMouseUp(const wy3dApp::MouseEvent& event)
{
    if (_hoverPreview.isNull()) return;
    wydb::ElementId id = _hoverPreview.getSelection().getElementId();
    if (id.isNull()) return;

    QTimer::singleShot(0, wy3dApp::getApplication(), [this, id]()
    {
        this->showParameters(id);
        this->requestEnd();
    });
}

void DemoParametersCmd::showParameters(const wydb::ElementId& elementId)
{
    const wydb::Database* pDb = wy3dApp::getApplication()->getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return;
    }
    const wydb::Element* pElem = pDb->getElement(elementId);
    if (!pElem)
    {
        assert(false);
        return;
    }

    QStringList lines;
    lines << QCoreApplication::translate("DemoParametersCmd", "Type: %1")
        .arg(QString::fromUtf8(pElem->getClassInfo()->className().c_str()));

    const std::vector<const wydb::ParameterDefinition*> paramDefs =
        collectParamDefinitions(pElem->getClassInfo());
    for (const wydb::ParameterDefinition* pParamDef : paramDefs)
    {
        if (!pParamDef)
        {
            assert(false);
            continue;
        }

        const std::string& className = pParamDef->getClassName();
        const std::string& paramName = pParamDef->getName();
        wydb::ParameterValueUPtr pParamValue = pElem->getParameterValue(className, paramName);
        if (!pParamValue)
        {
            assert(false);
            continue;
        }

        const QString displayName = wy3dApp::ParamDisplayNameRegistry::instance().lookup(className, paramName);
        lines << QStringLiteral("%1: %2").arg(displayName, paramValueToText(*pParamValue));
    }

    QMessageBox::information(wy3dApp::getApplication()->getMainWindow(),
        QCoreApplication::translate("DemoParametersCmd", "Sketch Element Parameters"),
        lines.join(QStringLiteral("\n")));
}
