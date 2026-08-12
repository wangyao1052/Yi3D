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

#include <wy3dSketchProfileForSheet.h>

#include <cassert>
#include <wy3dSketchCurve.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchEntity.h>
#include <wydbDatabase.h>
#include <wydbElement.h>

NS_WY3D_BEG

SketchProfileForSheet::SketchProfileForSheet(const Sketch* pSketch, double tol)
    : _pSketch(pSketch), _tol(tol)
{
}

bool SketchProfileForSheet::check()
{
    _loops.clear();
    _pError = nullptr;

    if (!_pSketch) return false;

    const wydb::Database* pDb = _pSketch->getDatabase();
    if (!pDb) return false;

    // 收集曲线（同 SketchProfile::init）
    std::vector<const SketchCurve*> curves;
    curves.reserve(50);
    for (auto iter = _pSketch->createIterator(); !iter.isDone(); iter.moveNext())
    {
        const wydb::Element* pElem = pDb->getElement(iter.current());
        const SketchCurve* pCurve = SketchCurve::cast(pElem);
        if (!pCurve) continue;
        if (pCurve->isConstruction()) continue;
        if (pCurve->isKindOf(SketchCenterLine::classInfo())) continue;
        curves.push_back(pCurve);
    }

    if (curves.empty()) return false;

    // 构建图并查找所有链
    SketchCurveGraph_Profile curveGraph(curves, _tol);
    if (!curveGraph.isValid())
    {
        _pError = curveGraph.getError();
        return false;
    }
    if (!curveGraph.findLoops())
    {
        _pError = curveGraph.getError();
        return false;
    }

    const auto& allChains = curveGraph.getLoops();
    if (allChains.empty()) return false;

    // 转换为 BiCurve Loop
    _loops.reserve(allChains.size());
    for (const auto& pChain : allChains)
    {
        auto pLoop = std::make_shared<SketchProfile::Loop>();
        for (const auto& entry : pChain->curves())
        {
            assert(entry.index < curves.size());
            bool orient = (entry.orient == SketchCurveGraph::Orientation::Normal);
            pLoop->curves.push_back(BiCurve(curves[entry.index], orient));
        }
        _loops.push_back(std::move(pLoop));
    }

    return true;
}

NS_WY3D_END
