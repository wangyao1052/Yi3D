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

#ifndef WY3DAPP_MAKE_NON_PARAMETRIC_SHEET_H
#define WY3DAPP_MAKE_NON_PARAMETRIC_SHEET_H

#include <cassert>
#include <TopoDS_Shape.hxx>
#include <wy3dNonParametricSheet.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "commands/GuiCmdMakeElement.h"

class MakeNonParametricSheet : public GuiCmdMakeElement
{
public:
    MakeNonParametricSheet(GuiCommand* pGuiCmd)
        : GuiCmdMakeElement(pGuiCmd), _pNonParametricSheet(nullptr) {}
    ~MakeNonParametricSheet() {}

    virtual void collectElements(std::set<wydb::ElementId>& idSet) const override
    {
        if (_pNonParametricSheet) idSet.insert(_pNonParametricSheet->getId());
    }

    bool init(const TopoDS_Shape& shape, unsigned int& errorCode)
    {
        errorCode = 0;
        if (!_pDb || !_pTopTrans || _pNonParametricSheet || _isFinished)
            return false;
        if (shape.IsNull())
            return false;

        wy3d::NonParametricSheet* pSheet = nullptr;
        wydb::Transaction* pTrans = _pDb->getTransactionManager()->startTransaction();
        if (!pTrans) return false;

        if (wy::ErrorStatus::Ok != wy3d::NonParametricSheet::create(pTrans, shape, pSheet) || !pSheet)
        {
            assert(false);
            goto ABORT_TRANS;
        }
        _pDb->getTransactionManager()->endTransaction();
        _pNonParametricSheet = pSheet;
        errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
            _pDb->getTransactionManager()->getChainUpdateFeedback(pSheet->getId()).get());
        if (errorCode != 0) return false;
        return true;

    ABORT_TRANS:
        assert(false);
        _pDb->getTransactionManager()->abortTransaction();
        _pNonParametricSheet = nullptr;
        return false;
    }

private:
    wy3d::NonParametricSheet* _pNonParametricSheet;
};

#endif // WY3DAPP_MAKE_NON_PARAMETRIC_SHEET_H
