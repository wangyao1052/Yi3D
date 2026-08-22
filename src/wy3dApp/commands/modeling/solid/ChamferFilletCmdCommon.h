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

#ifndef WY3DAPP_CHAMFER_FILLET_CMD_COMMON_H
#define WY3DAPP_CHAMFER_FILLET_CMD_COMMON_H

#include <vector>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wyapSelection.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include <wy3dSolid.h>
#include <wy3dChamfer.h>
#include <wy3dFillet.h>

class ChamferFilletCmdCommon
{
public:
    template<typename T, wy3d::ErrorCode CreateErrorCode>
    static bool createChamferOrFillet(const wyap::SelectionSet& sels, double value, unsigned int& errorCode)
    {
        errorCode = 0;

        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return false;
        if (sels.isEmpty()) return false;

        // 执行倒角圆角的实体
        const wy3d::Solid* pConstSolid(nullptr);
        for (auto iter = sels.createIterator(); !iter.isDone(); iter.moveNext())
        {
            pConstSolid = wy3d::Solid::cast(pDb->getElement(iter.current().getElementId()));
            break;
        }
        if (!pConstSolid) return false;
        wydb::ElementId solidId = pConstSolid->getId();

        // 根据选择集提取边和面
        std::vector<unsigned int> edgeIndices, faceIndices;
        edgeIndices.reserve(10);
        faceIndices.reserve(10);
        for (auto iter = sels.createIterator(); !iter.isDone(); iter.moveNext())
        {
            const wyap::Selection& sel = iter.current();
            if (sel.getElementId() != solidId) // 在选择过滤器中已经确保了只能选择单一主体的面或边
            {
                assert(false);
                return false;
            }

            switch (static_cast<wy3d::SelectionType>(sel.getSelectionType()))
            {
            case wy3d::SelectionType::SolidEdge:
            {
                const std::string& subPath = sel.getSubPath();
                if (subPath.empty())
                {
                    assert(false);
                    return false;
                }
                unsigned int edgeIndex = std::stoul(subPath);
                edgeIndices.emplace_back(edgeIndex);
            }
            break;

            case wy3d::SelectionType::SolidFace:
            {
                const std::string& subPath = sel.getSubPath();
                if (subPath.empty())
                {
                    assert(false);
                    return false;
                }
                unsigned int faceIndex = std::stoul(subPath);
                faceIndices.emplace_back(faceIndex);
            }
            break;

            default:
            {
                assert(false);
                return false;
            }
            break;
            }
        }
        assert(!edgeIndices.empty() || !faceIndices.empty());

        // 开启事务创建倒角圆角
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        if (!pTrans) return false;
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(solidId));
        if (!pSolid)
        {
            assert(false);
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }
        T* pChamferOrFillet(nullptr);
        if (wy::ErrorStatus::Ok != T::create(pTrans, pSolid, faceIndices, edgeIndices, value, pChamferOrFillet))
        {
            errorCode = static_cast<unsigned int>(CreateErrorCode);
            assert(false);
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }
        pDb->getTransactionManager()->endTransaction();

        // 圆角已经创建成功但还需要查看有无错误码
        errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
            pDb->getTransactionManager()->getChainUpdateFeedback(pChamferOrFillet->getId()).get());
        if (errorCode != 0)
        {
            return false;
        }

        return true;
    }

    // 创建倒角 (多模式; angle 为弧度, 核心口径)
    static bool createChamfer(const wyap::SelectionSet& sels,
        wy3d::ChamferType chamferType,
        double distance1, double distance2, double angle,
        bool isFlipped,
        unsigned int& errorCode)
    {
        errorCode = 0;

        wydb::Database* pDb = Application::instance().getActiveDatabase();
        if (!pDb) return false;
        if (sels.isEmpty()) return false;

        // 执行倒角的实体
        const wy3d::Solid* pConstSolid(nullptr);
        for (auto iter = sels.createIterator(); !iter.isDone(); iter.moveNext())
        {
            pConstSolid = wy3d::Solid::cast(pDb->getElement(iter.current().getElementId()));
            break;
        }
        if (!pConstSolid) return false;
        wydb::ElementId solidId = pConstSolid->getId();

        // 根据选择集提取边和面
        std::vector<unsigned int> edgeIndices, faceIndices;
        edgeIndices.reserve(10);
        faceIndices.reserve(10);
        for (auto iter = sels.createIterator(); !iter.isDone(); iter.moveNext())
        {
            const wyap::Selection& sel = iter.current();
            if (sel.getElementId() != solidId) // 在选择过滤器中已经确保了只能选择单一主体的面或边
            {
                assert(false);
                return false;
            }

            switch (static_cast<wy3d::SelectionType>(sel.getSelectionType()))
            {
            case wy3d::SelectionType::SolidEdge:
            {
                const std::string& subPath = sel.getSubPath();
                if (subPath.empty())
                {
                    assert(false);
                    return false;
                }
                unsigned int edgeIndex = std::stoul(subPath);
                edgeIndices.emplace_back(edgeIndex);
            }
            break;

            case wy3d::SelectionType::SolidFace:
            {
                const std::string& subPath = sel.getSubPath();
                if (subPath.empty())
                {
                    assert(false);
                    return false;
                }
                unsigned int faceIndex = std::stoul(subPath);
                faceIndices.emplace_back(faceIndex);
            }
            break;

            default:
            {
                assert(false);
                return false;
            }
            break;
            }
        }
        assert(!edgeIndices.empty() || !faceIndices.empty());

        // 开启事务创建倒角
        wydb::Transaction* pTrans = pDb->getTransactionManager()->startTransaction();
        if (!pTrans) return false;
        wy3d::Solid* pSolid = wy3d::Solid::cast(pTrans->getElementForWrite(solidId));
        if (!pSolid)
        {
            assert(false);
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }
        wy3d::Chamfer* pChamfer(nullptr);
        if (wy::ErrorStatus::Ok != wy3d::Chamfer::create(pTrans, pSolid, faceIndices,
                edgeIndices, chamferType, distance1, distance2, angle, isFlipped, pChamfer))
        {
            errorCode = static_cast<unsigned int>(wy3d::ErrorCode::CHAMFER_CreateChamferError);
            assert(false);
            pDb->getTransactionManager()->abortTransaction();
            return false;
        }
        pDb->getTransactionManager()->endTransaction();

        // 倒角已经创建成功但还需要查看有无错误码
        errorCode = wy3d::getErrorCodeFromChainUpdateFeedback(
            pDb->getTransactionManager()->getChainUpdateFeedback(pChamfer->getId()).get());
        if (errorCode != 0)
        {
            return false;
        }

        return true;
    }
};

#endif // WY3DAPP_CHAMFER_FILLET_CMD_COMMON_H
