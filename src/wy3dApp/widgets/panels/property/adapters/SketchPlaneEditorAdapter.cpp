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

#include "ParamEditorAdapter.h"

#include "../PropertyEditorWidget.h"
#include "../SketchPlaneEditor.h"

#include <wy3dPlane.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelManager.h>

#include <wy3dApp/Application.h>
#include "utils/MessageBoxUtil.h"

#include <typeindex>
#include <cassert>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class SketchPlaneEditorAdapter : public ParamEditorAdapter
{
public:
    static const ParamEditorAdapter* instance()
    {
        static SketchPlaneEditorAdapter inst;
        return &inst;
    }

    QWidget* create(const std::string& className,
                    const std::string& paramName,
                    const wydb::ParameterValue& paramValue,
                    bool isTheSameValue,
                    bool /*readOnly*/,
                    PropertyEditorWidget* parent) const override
    {
        const auto* pAnyVal = dynamic_cast<const wydb::AnyParameterValue*>(&paramValue);
        assert(pAnyVal);
        const wy3d::Plane* pPlane = pAnyVal->tryGet<wy3d::Plane>();
        assert(pPlane);

        auto* pEditor = new SketchPlaneEditor(*pPlane, isTheSameValue, parent);
        QObject::connect(pEditor, &SketchPlaneEditor::applyRequested, pEditor,
            [pEditor, className, paramName]() {
                bool ok = false;
                wy3d::Plane plane = pEditor->getPlane(ok);
                if (!ok) return;

                auto& app = Application::instance();
                wydb::Database* pDb = app.getActiveDatabase();
                if (!pDb) { assert(false); return; }

                const wyap::SelectionSet& ss = app.getSelManager()->getSelections();
                if (ss.isEmpty()) { assert(false); return; }

                wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
                if (!pTrans) { assert(false); return; }

                for (auto iter = ss.createIterator(); !iter.isDone(); iter.moveNext())
                {
                    wydb::Element* pElem = pTrans->getElementForWrite(iter.current().getElementId());
                    if (!pElem) continue;
                    if (wy::ErrorStatus::Ok != pElem->setParameterValue(className, paramName,
                        *wydb::ParameterValue::createAny(plane)))
                    {
                        pDb->getTransactionManager()->abortTransaction();
                        MessageBoxUtil::showError(QObject::tr("Modify failed!"));
                        return;
                    }
                }
                pDb->getTransactionManager()->endTransaction();
                MessageBoxUtil::showInformation(QObject::tr("Modify successfully."));
            });
        return pEditor;
    }
};

// 自注册到 Any<Plane>
static struct {
    struct Reg {
        Reg() {
            ParamEditorRegistry::instance().registerForAny(
                std::type_index(typeid(wy3d::Plane)),
                SketchPlaneEditorAdapter::instance());
        }
    } reg;
} _regSketchPlaneEditor;

} // namespace wy3dApp
