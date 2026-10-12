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

#include "LinearPatternGuiCmd.h"
#include <QCoreApplication>
#include <QToolTip>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapDocManager.h>
#include <wyapDocument.h>
#include <wyapClipboard.h>
#include <wy3dSketch.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dImpl.h>
#include <wy3dErrorCode.h>
#include <wy3dPattern.h>
#include <wy3dLinearPattern.h>
#include <wy3dMove.h>
#include <wy3dDatumPlane.h>

#include <wy3dApp/Application.h>
#include <wy3dApp/Scene.h>
#include "scene/nodes/ElementNodeType.h"
#include "scene/nodes/SolidElementNode.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include <wy3dApp/GuiCommandUtil.h>
#include <wy3dApp/CommonSelFilters.h>
#include "utils/TopoShapeUtil.h"
#include "commands/dialogs/LinearPatternDialog.h"
#include "commands/modeling/solid/ChamferFilletCmdCommon.h"
#include "snap/SnapObject.h"
#include "snap/SketchSnapSystem.h"
#include "wy3dAppDefs.h"

namespace wy3dApp {

class LinearPatternGuiCmdPreSelFilter : public SelectPreFilterFunctor
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

LinearPatternGuiCmd::LinearPatternGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _sourceId(wydb::ElementId::kNull)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

LinearPatternGuiCmd::~LinearPatternGuiCmd()
{
}

wyap::CmdExecution::StartResult LinearPatternGuiCmd::onStart()
{
    // 基类
    wyap::CmdExecution::StartResult ret = __baseClass::onStart();
    assert(wyap::CmdExecution::StartResult::Succeeded == ret);

    // 过滤出阵列源对象
    wydb::ElementId sourceId = GuiCommandUtil::filterPatternSourceFrom(
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
void LinearPatternGuiCmd::onEnd()
{
    // 基类
    __baseClass::onEnd();

    // 隐藏工作平面坐标系
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->hideSketchCSYS();
    }
    else
    {
        assert(false);
    }

}
void LinearPatternGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 基类
    __baseClass::onAbort(cause);

    // 隐藏工作平面坐标系
    if (Scene* pScene = Application::instance().getActiveScene())
    {
        pScene->hideSketchCSYS();
    }
    else
    {
        assert(false);
    }

}

bool LinearPatternGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSource:
    {
        // 阵列源对象
        if (_sourceId.isNull())
        {
            return false;
        }
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->addSelection(wyap::Selection(_sourceId));
        Application::instance().getSelManager()->endChange();

        // 下一步
        this->gotoStep(Step::SpecifyWorkingPlane);
        return true;
    }
    break;

    case Step::SpecifyWorkingPlane:
    {
        if (_workPlnPreview.isNull())
        {
            return false;
        }
        if (!GuiCommandUtil::getWorkingPlane(_workPlnPreview.getSelection(), _workPln))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _workPlnPreview.clear();

        // 显示工作平面坐标系
        if (Scene* pScene = Application::instance().getActiveScene())
        {
            pScene->showSketchCSYS(_workPln);
        }
        else
        {
            assert(false);
        }

        // 下一步
        this->gotoStep(Step::InputPatternData);
        return true;
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

void LinearPatternGuiCmd::gotoStep(Step step)
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
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("LinearPatternGuiCmd",
            "Select the solid feature to pattern."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
        
        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
        _pointPickOption.selType = wy3d::SelectionType::Element;
        _pointPickOption.pSelPreFilter = std::make_shared<LinearPatternGuiCmdPreSelFilter>();

        // 预览
        _sourcePreview.clear();
    }
    break;

    case Step::SpecifyWorkingPlane:
    {
        // 禁用文本输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("LinearPatternGuiCmd",
            "Select datum plane as working plane to determine the direction of the linear pattern."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 点选选项:工作平面
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::DatumPlane);
        _pointPickOption.selType = wy3d::SelectionType::Element;
        _pointPickOption.pSelPreFilter = nullptr;
    }
    break;

    case Step::InputPatternData:
    {
        // 禁用输入
        // 提示信息
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("LinearPatternGuiCmd",
            "Input pattern data."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::Select);

        // 倒角对话框
        LinearPatternDialog dialog(3, 10.0, 2, 20.0);
        if (QDialog::Accepted != dialog.exec())
        {
            this->requestAbort(AbortCause::UserCancel);  // 退出
            return;
        }

        // 线性阵列数据
        unsigned int count1st = dialog.getCount1st();
        unsigned int count2nd = dialog.getCount2nd();
        double spacing1st = dialog.getSpacing1st();
        double spacing2nd = dialog.getSpacing2nd();

        // 创建线性阵列
        unsigned int errorCode(0);
        if (!this->createLinearPattern(count1st, spacing1st, count2nd, spacing2nd, errorCode))
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
        // 清空提示
        Application::instance().getStatusBar()->setTips("");
        Application::instance().setCursor(CursorType::Select);
        assert(false);
    }
    break;
    }
}

void LinearPatternGuiCmd::onMouseMove(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectSource:
    {
        // 点选预览:要阵列的对象
        wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);
        _sourcePreview.update(sel);
        return;
    }
    break;

    case Step::SpecifyWorkingPlane:
    {
        // 点选预览:工作平面
        wyap::Selection sel = this->pointPick(event.x, event.y, _pointPickOption);
        _workPlnPreview.update(sel);
        return;
    }
    break;
    }
    
    return;
}

void LinearPatternGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectSource:
    {
        if (!_sourcePreview.isNull())
        {
            _sourceId = _sourcePreview.getSelection().getElementId();
            this->finishStep(_step);
            return;
        }
    }
    break;

    case Step::SpecifyWorkingPlane:
    {
        if (!_workPlnPreview.isNull())
        {
            this->finishStep(_step);
            return;
        }
    }
    break;
    }

    return;
}

void LinearPatternGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
{
    if (Step::SelectSource == _step)
    {
        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return;
        const wy3d::Solid* pSolid = wy3d::Solid::cast(pDb->getElement(id));
        if (!pSolid) return;
        if (pSolid->getParent().isNull() || // 顶层实体特征
            wy3d::Pattern::isValidSource(pSolid)) 
        {
            _sourceId = id;
            this->finishStep(_step);
        }
    }
    else if (Step::SpecifyWorkingPlane == _step)
    {
        if (!_workPlnPreview.isNull())
        {
            assert(false);
            return;
        }
        const wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return;
        const wy3d::DatumPlane* pDatumPlane = wy3d::DatumPlane::cast(pDb->getElement(id));
        if (!pDatumPlane) return;
        _workPlnPreview.update(wyap::Selection(id));
        this->finishStep(_step);
    }
}

bool LinearPatternGuiCmd::createLinearPattern(
    unsigned int count1st, double spacing1st,
    unsigned int count2nd, double spacing2nd,
    unsigned int& errorCode)
{
    errorCode = 0;

    wydb::Database* pDb = Application::instance().getActiveDatabase();
    if (!pDb) return false;
    const wy3d::Solid* pSourceSolid = wy3d::Solid::cast(pDb->getElement(_sourceId));
    if (!pSourceSolid)
    {
        assert(false);
        return false;
    }

    // 开启事务
    wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
    if (!pTrans) return false;

    // 阵列对象的主体
    wy3d::Solid* pSolidOwner = wy3d::Solid::cast(pTrans->getElementForWrite(pSourceSolid->getParent()));
    if (pSolidOwner)
    {
        // 创建线性阵列
        wy::Vector3 dir1st = _workPln.getXDir();
        wy::Vector3 dir2nd = _workPln.getYDir();
        wy3d::LinearPattern* pLinearPattern(nullptr);
        wy::ErrorStatus error = wy3d::LinearPattern::create(pTrans, pSolidOwner, pSourceSolid,
            dir1st, count1st, spacing1st, dir2nd, count2nd, spacing2nd, pLinearPattern);
        if (wy::ErrorStatus::Ok != error || !pLinearPattern)
        {
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }

        pDb->getTransactionManager()->endTransaction();

        // 线性阵列已经创建成功但还需要查看有无错误码
        errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
            pDb->getTransactionManager()->getChainUpdateFeedback(pLinearPattern->getId()).get());
        if (errorCode != 0)
        {
            return false;
        }
        return true;
    }
    else // 源对象是顶层的实体对象
    {
        std::vector<wydb::ElementId> elemIds;
        elemIds.push_back(pSourceSolid->getId());
        std::shared_ptr<wyap::ElementsClipData> pClipData = wyap::Clipboard::newElementsClipData(pDb, elemIds);
        if (!pClipData)
        {
            assert(false);
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }

        for (unsigned int row = 1; row <= count2nd; ++row)
        {
            for (unsigned int col = 1; col <= count1st; ++col)
            {
                if (1 == row && 1 == col) continue;

                wy::Vector3 vec = (col - 1) * _workPln.getXDir() * spacing1st 
                    + (row - 1) * _workPln.getYDir() * spacing2nd;
                std::vector<wydb::Element*> copyedElems;
                if (wy::ErrorStatus::Ok != wyap::Clipboard::createElements(pTrans, *pClipData, copyedElems))
                {
                    assert(copyedElems.empty());
                    pDb->getTransactionManager()->abortTransaction();
                    return false;
                }
                for (wydb::Element* pCopyedElem : copyedElems)
                {
                    if (!pCopyedElem)
                    {
                        assert(false);
                        continue;
                    }
                    pTrans->addNewlyCreatedElement(pCopyedElem);
                    wy3d::Solid* pCopyedSolid = wy3d::Solid::cast(pCopyedElem);
                    if (!pCopyedSolid) continue;
                    if (!pCopyedSolid->getParent().isNull()) continue;

                    wy3d::Move* pMove(nullptr);
                    wy::ErrorStatus error = wy3d::Move::create(pTrans, pCopyedSolid, vec, pMove);
                    assert(wy::ErrorStatus::Ok == error);
                    if (pMove)
                    {
                    }
                }
            }
        }

        pDb->getTransactionManager()->endTransaction();
        return true;
    }
}

}
