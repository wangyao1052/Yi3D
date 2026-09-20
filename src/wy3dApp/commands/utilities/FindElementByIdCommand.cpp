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

#include "commands/utilities/FindElementByIdCommand.h"

#include <set>
#include <string>

#include <QCoreApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QRegExp>
#include <QStringList>

#include <wy3dSketchEntity.h>
#include <wy3dSketchEntity3D.h>
#include <wydbDatabase.h>
#include <wydbElementId.h>
#include <wyapEnvironment.h>
#include <wyapSelManager.h>
#include <wyapSelection.h>

#include "application/Application.h"
#include "environments/modeling/ModelingEnvironment.h"
#include "environments/sketch/SketchEnvironment.h"
#include "environments/sketch3d/Sketch3DEnvironment.h"
#include "utils/MessageBoxUtil.h"
#include "widgets/frame/MainWindow.h"

int FindElementByIdCommand::run()
{
    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb)
    {
        assert(false);
        return -1;
    }

    enum class EnvType
    {
        Undefined = 0,
        Modeling  = 1,
        Sketch    = 2,
        Sketch3D  = 3,
    };
    struct EnvInfo
    {
        EnvType type = EnvType::Undefined;
        wydb::ElementId id = wydb::ElementId::kNull;
    } envInfo;

    wyap::Environment* pActiveEnv = Application::instance().getEnvManager()->getActiveEnvironment();
    if (!pActiveEnv)
    {
        assert(false);
        return -1;
    }
    ModelingEnvironment* pModelingEnv = dynamic_cast<ModelingEnvironment*>(pActiveEnv);
    SketchEnvironment* pSketchEnv = dynamic_cast<SketchEnvironment*>(pActiveEnv);
    Sketch3DEnvironment* pSketch3DEnv = dynamic_cast<Sketch3DEnvironment*>(pActiveEnv);
    if (pModelingEnv)
    {
        envInfo.type = EnvType::Modeling;
        envInfo.id = wydb::ElementId::kNull;
    }
    else if (pSketchEnv)
    {
        envInfo.type = EnvType::Sketch;
        envInfo.id = pSketchEnv->getSketchId();
    }
    else if (pSketch3DEnv)
    {
        envInfo.type = EnvType::Sketch3D;
        envInfo.id = pSketch3DEnv->getSketch3dId();
    }
    else
    {
        assert(false);
        return -1;
    }

    QInputDialog dialog(Application::instance().getMainWindow());
    dialog.setWindowTitle(QCoreApplication::translate("FindElementByIdCommand", "Find Element By ID"));
    dialog.setLabelText(QCoreApplication::translate("FindElementByIdCommand",
        "Element IDs (separated by ; , or space):"));
    dialog.setTextValue("");
    dialog.setInputMode(QInputDialog::TextInput);
    if (dialog.exec() != QDialog::Accepted)
    {
        return 0;
    }
    const QString text = dialog.textValue().trimmed();
    if (text.isEmpty())
    {
        return 0;
    }

    std::set<wydb::ElementId> inputIds;
    const QStringList tokens = text.split(QRegExp("[;,\\s]+"), QString::SkipEmptyParts);
    for (const QString& token : tokens)
    {
        bool ok = false;
        const unsigned long long value = token.toULongLong(&ok);
        if (!ok)
        {
            continue;
        }
        wydb::ElementId id = wydb::ElementId(value);
        if (id.isNull()) continue;
        inputIds.insert(id);
    }
    if (inputIds.empty())
    {
        MessageBoxUtil::showError(QCoreApplication::translate("FindElementByIdCommand",
            "No valid element IDs were entered."));
        return 0;
    }

    wyap::SelectionSet ss;
    QStringList notFoundIds;
    QStringList sketchEnvRejectedIds;
    QStringList sketch3DEnvRejectedIds;
    for (const wydb::ElementId& id : inputIds)
    {
        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem)
        {
            notFoundIds << QString::number(id.value());
            continue;
        }

        switch (envInfo.type)
        {
        case EnvType::Modeling:
        {
            if (const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem))
            {
                const wydb::ElementId parentId = pSketchEntity->getParent();
                if (!parentId.isNull())
                {
                    ss.add(wyap::Selection(parentId));
                }
                else
                {
                    assert(false);
                }
            }
            else if (const wy3d::SketchEntity3D* pSketchEntity3D = wy3d::SketchEntity3D::cast(pElem))
            {
                const wydb::ElementId parentId = pSketchEntity3D->getParent();
                if (!parentId.isNull())
                {
                    ss.add(wyap::Selection(parentId));
                }
                else
                {
                    assert(false);
                }
            }
            else
            {
                ss.add(wyap::Selection(id));
            }
        }
        break;

        case EnvType::Sketch:
        {
            const wy3d::SketchEntity* pSketchEntity = wy3d::SketchEntity::cast(pElem);
            if (pSketchEntity)
            {
                if (pSketchEntity->getParent() != envInfo.id)
                {
                    sketchEnvRejectedIds << QString::number(id.value());
                }
                else
                {
                    ss.add(wyap::Selection(id));
                }
            }
            else
            {
                sketchEnvRejectedIds << QString::number(id.value());
            }
        }
        break;

        case EnvType::Sketch3D:
        {
            const wy3d::SketchEntity3D* pSketchEntity3D = wy3d::SketchEntity3D::cast(pElem);
            if (pSketchEntity3D)
            {
                if (pSketchEntity3D->getParent() != envInfo.id)
                {
                    sketch3DEnvRejectedIds << QString::number(id.value());
                }
                else
                {
                    ss.add(wyap::Selection(id));
                }
            }
            else
            {
                sketch3DEnvRejectedIds << QString::number(id.value());
            }
        }
        break;

        default:
        {
            assert(false);
            return -1;
        }
        break;
        }
    }

    QStringList invalidMsgs;
    if (!notFoundIds.isEmpty())
    {
        invalidMsgs << QCoreApplication::translate("FindElementByIdCommand", "IDs not found: %1")
            .arg(notFoundIds.join(", "));
    }
    if (!sketchEnvRejectedIds.isEmpty())
    {
        invalidMsgs << QCoreApplication::translate("FindElementByIdCommand",
            "In the sketch environment, only entities of the current sketch can be found: %1")
            .arg(sketchEnvRejectedIds.join(", "));
    }
    if (!sketch3DEnvRejectedIds.isEmpty())
    {
        invalidMsgs << QCoreApplication::translate("FindElementByIdCommand",
            "In the 3D sketch environment, only entities of the current 3D sketch can be found: %1")
            .arg(sketch3DEnvRejectedIds.join(", "));
    }

    if (ss.isEmpty())
    {
        MessageBoxUtil::showError(invalidMsgs.join("\n"));
        return 0;
    }

    wyap::SelManager* pSelMgr = Application::instance().getSelManager();
    assert(pSelMgr);
    pSelMgr->beginChange();
    pSelMgr->setSelections(ss);
    pSelMgr->endChange();

    QString tips = QCoreApplication::translate("FindElementByIdCommand", "Selected %1 element(s).")
        .arg(ss.getCount());
    if (!invalidMsgs.isEmpty())
    {
        tips += QCoreApplication::translate("FindElementByIdCommand", " %1 ID(s) invalid.")
            .arg(notFoundIds.size() + sketchEnvRejectedIds.size() + sketch3DEnvRejectedIds.size());
        MessageBoxUtil::showError(invalidMsgs.join("\n"));
    }
    Application::instance().getStatusBar()->setTips(tips);

    return 0;
}
