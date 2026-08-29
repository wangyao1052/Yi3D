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

#include "commands/edit/MirrorGuiCmd.h"
#include <wydbTransaction.h>
#include <wyapSelection.h>
#include <wyapSelManager.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dSolid.h>
#include <wy3dMirror.h>
#include <wy3dMirror.h>
#include "application/Application.h"
#include "scene/nodes/ElementNodeType.h"
#include "utils/MessageBoxUtil.h"
#include "select/SketchPlaneSelFilter.h"
#include "utils/GuiCommandUtil.h"
#include "select/filters/CommonSelFilters.h"


class MirrorGuiCmdPreSelFilter : public SelectPreFilterFunctor
{
public:
    virtual SelectFilterStatus operator()(
        const wydb::Database* pDb,
        const wydb::ElementId& id,
        SelectAction selectAction) const override
    {
        if (id.isNull()) return SelectFilterStatus::Continue;

        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        if (!pSolid)
        {
            return SelectFilterStatus::Continue;
        }
        if (pSolid->getParent().isNull()) // 在场景中点选只能选择顶层实体特征
        {
            return SelectFilterStatus::Ok;
        }
        else
        {
            return SelectFilterStatus::Continue;
        }
    }
};

MirrorGuiCmd::MirrorGuiCmd() : OsgGuiCommand(), _step(Step::Undefined), _sourceId(wydb::ElementId::kNull), _mirrorPlane()
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

wyap::CmdExecution::StartResult MirrorGuiCmd::onStart()
{
    // 基类
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 过滤出源对象
    wydb::ElementId sourceId = GuiCommandUtil::filterMirrorSourceFrom(
        Application::instance().getSelManager()->getSelections());
    Application::instance().getSelManager()->beginChange();
    Application::instance().getSelManager()->clearSelections();
    Application::instance().getSelManager()->endChange();

    // 初始化
    if (sourceId.isNull())
    {
        this->gotoStep(Step::SelectSource);
    }
    else
    {
        _sourceId = sourceId;
        this->finishStep(Step::SelectSource);
    }

    return wyap::CmdExecution::StartResult::Succeeded;
}

bool MirrorGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSource:
    {
        // 源对象
        if (_sourceId.isNull())
        {
            return false;
        }
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->addSelection(wyap::Selection(_sourceId));
        Application::instance().getSelManager()->endChange();

        // 下一步
        this->gotoStep(Step::SelectMirrorPlane);
        return true;
    }
    break;

    case Step::SelectMirrorPlane:
    {
        // 镜像面
        if (!_pMirrorPlanePreview)
        {
            assert(false);
            return false;
        }
        if (!GuiCommandUtil::getWorkingPlane(_pMirrorPlanePreview->getSelection(), _mirrorPlane))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _pMirrorPlanePreview = nullptr;

        // 执行镜像
        unsigned int errorCode(0);
        if (!this->createMirror(_sourceId, _mirrorPlane, errorCode))
        {
            if (0 != errorCode)
            {
                MessageBoxUtil::showError(errorCode);
            }
        }

        // 退出命令
        this->requestEnd();
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }

    return false;
}

void MirrorGuiCmd::gotoStep(Step step)
{
    _step = step;

    switch (step)
    {
    case Step::SelectSource:
    {
        // 清空选择集
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->endChange();

        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MirrorGuiCmd",
            "Select the solids to mirror. Press Enter or Spacebar to confirm. Press Esc to cancel."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
        _pointPickOption.selType = wy3d::SelectionType::Element;
        _pointPickOption.pSelPreFilter = std::make_shared<MirrorGuiCmdPreSelFilter>();

        // 预览
        _pSourcePreview = nullptr;
    }
    break;

    case Step::SelectMirrorPlane:
    {
        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("MirrorGuiCmd",
            "Select datum plane as mirror plane."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 预览
        _pMirrorPlanePreview = nullptr;

        // 点选选项:基准面
        // 不允许选择实体面,因为若Hover到要镜像的实体特征的表面,再移开,该面会恢复到原始的颜色,但是该实体特征当前是选中态,这就冲突了.
        assert(!_sourceId.isNull());
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::DatumPlane);
        _pointPickOption.selType = wy3d::SelectionType::Element;
        _pointPickOption.pSelPreFilter = nullptr;
    }
    break;

    default:
    {
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
}

void MirrorGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectSource:
    {
        // 点选预览:要阵列的对象
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pSourcePreview);
        return;
    }
    break;

    case Step::SelectMirrorPlane:
    {
        // 点选预览:基准面
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pMirrorPlanePreview);
        return;
    }
    break;
    }

    return;
}

void MirrorGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectSource:
    {
        if (_pSourcePreview)
        {
            _sourceId = _pSourcePreview->getSelection().getElementId();
            this->finishStep(_step);
            return;
        }
    }
    break;

    case Step::SelectMirrorPlane:
    {
        if (_pMirrorPlanePreview)
        {
            this->finishStep(_step);
            return;
        }
    }
    break;

    default:
    {
        assert(false);
    }
    break;
    }

    return;
}

void MirrorGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (Step::SelectSource == _step)
    {
        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return;
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        if (!pSolid) return;
        _sourceId = id;
        this->finishStep(_step);
    }
    else if (Step::SelectMirrorPlane == _step)
    {
        if (_pMirrorPlanePreview)
        {
            assert(false);
            return;
        }
        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return;
        const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(id));
        if (!pDatumPlane) return;
        _pMirrorPlanePreview = std::make_shared<SelectPreview>(wyap::Selection(id));
        this->finishStep(_step);
    }
}

bool MirrorGuiCmd::createMirror(
    const wydb::ElementId& sourceId,
    const wy3d::SketchPlane& mirrorPlane,
    unsigned int& errorCode)
{
    errorCode = 0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Solid* pSourceSolid = wy3d::Solid::cast(pDb->getElement(sourceId));
    if (!pSourceSolid)
    {
        assert(false);
        return false;
    }

    wydb::ElementId ownerId = pSourceSolid->getParent();
    if (ownerId.isNull())
    {
        ownerId = sourceId;
    }
    else
    {
        if (pSourceSolid->isCut())
        {
            const wy3d::Solid* pConstOwnerSolid = wy3d::Solid::cast(pDb->getElement(ownerId));
            if (!pConstOwnerSolid)
            {
                assert(false);
                return false;
            }
        }
        else
        {
            ownerId = sourceId;
        }
    }

    // 开启事务
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;

    // 镜像对象的主体对象
    wy3d::Solid* pOwnerSolid = wy3d::Solid::cast(pTrans->getElementForWrite(ownerId));
    if (!pOwnerSolid)
    {
        assert(false);
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }

    // 创建镜像
    wy3d::Mirror* pMirror(nullptr);
    wy::ErrorStatus error = wy3d::Mirror::create(pTrans, pOwnerSolid, pSourceSolid, mirrorPlane, pMirror);
    if (wy::ErrorStatus::Ok != error || !pMirror)
    {
        pDb->getTransactionManager()->abortTransaction();
        return false;
    }
    pDb->getTransactionManager()->endTransaction();

    // 已经创建成功但还需要查看有无错误码
    errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
        pDb->getTransactionManager()->getChainUpdateFeedback(pMirror->getId()).get());
    if (errorCode != 0)
    {
        return false;
    }

    return true;
}
