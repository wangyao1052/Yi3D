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

#include "CircularPatternGuiCmd.h"
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
#include <wy3dCircularPattern.h>
#include <wy3dRotate.h>
#include <wy3dDatumPlane.h>

#include "application/Application.h"
#include "scene/Scene.h"
#include "scene/nodes/ElementNodeType.h"
#include "scene/nodes/SolidElementNode.h"
#include "utils/SketchUtil.h"
#include "utils/MessageBoxUtil.h"
#include "utils/GuiCommandUtil.h"
#include "select/filters/CommonSelFilters.h"
#include "utils/TopoShapeUtil.h"
#include "commands/dialogs/LinearPatternDialog.h"
#include "commands/modeling/solid/ChamferFilletCmdCommon.h"
#include "snap/SnapObject.h"
#include "snap/SketchSnapSystem.h"
#include "commands/datumPlane/MakeDatumPlane.h"
#include "select/filters/SolidFaceSelFilter.h"
#include "commands/sketch/dialogs/SketchPolarArrayDialog.h"
#include "scene/Colors.h"


class CircularPatternGuiCmdPreSelFilter : public SelectPreFilterFunctor
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

CircularPatternGuiCmd::CircularPatternGuiCmd() : OsgGuiCommand(),
    _step(Step::Undefined), _sourceId(wydb::ElementId::kNull), _cylindricalSurfaceSel(wyap::Selection(wydb::ElementId::kNull)),
    _centerPoint(wy::Vector3::kZero), _axisDirection(wy::Vector3::kZAxis)
{
    _options.pointSelect = false;
    _options.boxSelect = false;
}

CircularPatternGuiCmd::~CircularPatternGuiCmd()
{
}

wyap::CmdExecution::StartResult CircularPatternGuiCmd::onStart()
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
void CircularPatternGuiCmd::onEnd()
{
    // 基类
    __baseClass::onEnd();

}
void CircularPatternGuiCmd::onAbort(wyap::CmdExecution::AbortCause cause)
{
    // 基类
    __baseClass::onAbort(cause);

}

bool CircularPatternGuiCmd::finishStep(Step step)
{
    switch (step)
    {
    case Step::SelectSource:
    {
        // 阵列源对象
        if (_sourceId.isNull())
        {
            assert(false);
            return false;
        }
        Application::instance().getSelManager()->beginChange();
        Application::instance().getSelManager()->clearSelections();
        Application::instance().getSelManager()->addSelection(wyap::Selection(_sourceId));
        Application::instance().getSelManager()->endChange();

        // 下一步
        this->gotoStep(Step::SpecifyCylindricalSurface);
        return true;
    }
    break;

    case Step::SpecifyCylindricalSurface:
    {
        // 获取实体圆柱面信息
        if (_cylindricalSurfaceSel.getElementId().isNull())
        {
            assert(false);
            return false;
        }
        wy3d::SketchPlane plane;
        double radius(0.0);
        if (!MakeDatumPlane::getSolidCylindricalFaceCenterPlane(
            _cylindricalSurfaceSel, plane, radius))
        {
            assert(false);
            this->requestAbort(AbortCause::ErrorTerminate);
            return false;
        }
        _centerPoint = plane.getOrigin();
        _axisDirection = plane.getNormal();

        // 高亮实体圆柱面
        _pCylindricalSurfaceHighlightor = nullptr;
        _pCylindricalSurfaceHighlightor = std::make_shared<SelectionSetHighlightor>(
            wyap::SelectionSet(), Colors::kPink);
        _pCylindricalSurfaceHighlightor->addSelection(_cylindricalSurfaceSel);

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

void CircularPatternGuiCmd::gotoStep(Step step)
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
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("CircularPatternGuiCmd",
            "Select the solid feature to pattern."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);
        
        // 点选选项
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
        _pointPickOption.selType = wy3d::SelectionType::Element;
        _pointPickOption.pSelPreFilter = std::make_shared<CircularPatternGuiCmdPreSelFilter>();

        // 预览
        _pSourcePreview = nullptr;
    }
    break;

    case Step::SpecifyCylindricalSurface:
    {
        // 禁用文本输入
        // 提示
        Application::instance().getStatusBar()->setTips(QCoreApplication::translate("CircularPatternGuiCmd",
            "Select solid cylindrical face."));

        // 鼠标样式
        Application::instance().setCursor(CursorType::SelectElements);

        // 点选选项:实体圆柱面
        _pointPickOption.pickMask = static_cast<unsigned int>(ElementNodeType::Solid);
        _pointPickOption.selType = wy3d::SelectionType::Face;
        _pointPickOption.acceptElement = false;
        auto pSelFilter = std::make_shared<SolidFaceSelFilterFunctor<Geom_CylindricalSurface>>();
        pSelFilter->addExcludeId(_sourceId);
        _pointPickOption.pSelFilter = pSelFilter;

        // 预览
        _pCylindricalSurfacePreview = nullptr;
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

        // 圆周阵列对话框
        SketchPolarArrayDialog::Options options;
        options.minTotalAngle = 0.0;
        options.maxTotalAngle = 360.0;
        options.minCount = 1;
        options.maxCount = wy3d::kMaxCircularPatternCount;
        SketchPolarArrayDialog dialog(
            QCoreApplication::translate("CircularPatternGuiCmd", "Circular Pattern"),
            360.0, 6, true, options);
        if (QDialog::Accepted != dialog.exec())
        {
            this->requestAbort(AbortCause::UserCancel);  // 退出
            return;
        }

        // 圆周阵列数据
        double totalAngle = dialog.getTotalAngle();
        unsigned int instanceCount = dialog.getCount();
        bool isCCW = dialog.isCCW();

        // 创建圆周阵列
        unsigned int errorCode(0);
        if (!this->createCircularPattern(totalAngle, instanceCount, isCCW, errorCode))
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

void CircularPatternGuiCmd::onMouseMove(const MouseEvent& event)
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

    case Step::SpecifyCylindricalSurface:
    {
        // 点选预览:圆柱实体面
        this->mouseMovePointPickPreview(event.x, event.y, _pointPickOption, _pCylindricalSurfacePreview);
        return;
    }
    break;
    }
    
    return;
}

void CircularPatternGuiCmd::onLeftMouseUp(const MouseEvent& event)
{
    switch (_step)
    {
    case Step::SelectSource:
    {
        if (_pSourcePreview)
        {
            _sourceId = _pSourcePreview->getSelection().getElementId();
            _pSourcePreview = nullptr;
            this->finishStep(_step);
            return;
        }
    }
    break;

    case Step::SpecifyCylindricalSurface:
    {
        if (_pCylindricalSurfacePreview)
        {
            _cylindricalSurfaceSel = _pCylindricalSurfacePreview->getSelection();
            _pCylindricalSurfacePreview = nullptr;
            this->finishStep(_step);
            return;
        }
    }
    break;
    }

    return;
}

void CircularPatternGuiCmd::onFeatureTreeItemClicked(const wydb::ElementId& id)
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
}

bool CircularPatternGuiCmd::createCircularPattern(
    double totalAngle, unsigned int instanceCount, bool isCCW,
    unsigned int& errorCode)
{
    errorCode = 0;

    if (totalAngle <= wy3d::TOL)
    {
        assert(false);
        return false;
    }

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
        // 创建圆周阵列
        wy3d::CircularPattern* pCircularPattern(nullptr);
        wy::ErrorStatus error = wy3d::CircularPattern::create(pTrans, pSolidOwner, pSourceSolid,
            _centerPoint, _axisDirection, totalAngle, instanceCount, !isCCW, pCircularPattern);
        if (wy::ErrorStatus::Ok != error || !pCircularPattern)
        {
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }
        pDb->getTransactionManager()->endTransaction();
    
        // 圆周阵列已经创建成功但还需要查看有无错误码
        errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
            pDb->getTransactionManager()->getChainUpdateFeedback(pCircularPattern->getId()).get());
        if (errorCode != 0)
        {
            return false;
        }
        return true;
    }
    else // 源对象是顶层的实体对象
    {
        if (instanceCount <= 1)
        {
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }

        std::vector<wydb::ElementId> elemIds;
        elemIds.push_back(pSourceSolid->getId());
        std::shared_ptr<wyap::ElementsClipData> pClipData = wyap::Clipboard::newElementsClipData(pDb, elemIds);
        if (!pClipData)
        {
            assert(false);
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }

        // 计算单个实例的旋转角度
        double angleDelta(0.0);
        if (totalAngle >= wy3d::TWO_PI - wy3d::TOL) // 一整圈
        {
            angleDelta = wy3d::TWO_PI / instanceCount;
        }
        else
        {
            angleDelta = totalAngle / (instanceCount - 1);
        }

        // 是否是顺时针
        if (!isCCW)
        {
            angleDelta = -angleDelta;
        }

        for (unsigned int i = 2; i <= instanceCount; ++i)
        {
            std::vector<wydb::Element*> copyedElems;
            if (wy::ErrorStatus::Ok != wyap::Clipboard::createElements(pTrans, *pClipData, copyedElems))
            {
                assert(false);
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

                wy3d::Rotate* pRotate(nullptr);
                wy::ErrorStatus error = wy3d::Rotate::create(pTrans, pCopyedSolid,
                    _centerPoint, _axisDirection, (i - 1) * angleDelta, pRotate);
                assert(wy::ErrorStatus::Ok == error);
                if (pRotate)
                {
                }
            }
        }
    
        pDb->getTransactionManager()->endTransaction();
        return true;
    }
}
