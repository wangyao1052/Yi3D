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

#include "Test.h"
#include <wyVector2.h>
#include <wyVector3.h>
#include <wy3dSketch.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dExtrusion.h>
#include <wy3dHelix.h>
#include <wy3dImportedSolid.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

static void createExtrusion_CylibnderXOY(wydb::Database* pDb, wydb::Transaction* pTrans)
{
    wy3d::Sketch* pSketch(nullptr);
    wy3d::Plane sketchPlane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0));
    wy::ErrorStatus error = wy3d::Sketch::create(pTrans, sketchPlane, pSketch);
    assert(wy::ErrorStatus::Ok == error);
    assert(pSketch);

    wy3d::SketchCircle* pCircle(nullptr);
    error = wy3d::SketchCircle::create(pTrans, wy::Vector2(0.0, 0.0), 50.0, pCircle);
    assert(wy::ErrorStatus::Ok == error);
    assert(pCircle);
    pSketch->addEntity(pCircle);

    wy3d::Extrusion* pExtrusion(nullptr);
    error = wy3d::Extrusion::create(pTrans, pSketch, 100.0, pExtrusion);
    assert(wy::ErrorStatus::Ok == error);
    assert(pSketch);
}

static void createExtrusion_CylibnderYOZ(wydb::Database* pDb, wydb::Transaction* pTrans)
{
    wy3d::Sketch* pSketch(nullptr);
    wy3d::Plane sketchPlane(wy::Vector3(0.0, 0.0, 30.0), wy::Vector3(1.0, 0.0, 0.0), wy::Vector3(0.0, 1.0, 0.0));
    wy::ErrorStatus error = wy3d::Sketch::create(pTrans, sketchPlane, pSketch);
    assert(wy::ErrorStatus::Ok == error);
    assert(pSketch);

    wy3d::SketchCircle* pCircle(nullptr);
    error = wy3d::SketchCircle::create(pTrans, wy::Vector2(0.0, 0.0), 50.0, pCircle);
    assert(wy::ErrorStatus::Ok == error);
    assert(pCircle);
    pSketch->addEntity(pCircle);

    wy3d::Extrusion* pExtrusion(nullptr);
    error = wy3d::Extrusion::create(pTrans, pSketch, 100.0, pExtrusion);
    assert(wy::ErrorStatus::Ok == error);
    assert(pSketch);
}

static void createExtrusion_Box(wydb::Database* pDb, wydb::Transaction* pTrans)
{
    wy3d::Sketch* pSketch(nullptr);
    wy3d::Plane sketchPlane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0));
    wy::ErrorStatus error = wy3d::Sketch::create(pTrans, sketchPlane, pSketch);
    assert(wy::ErrorStatus::Ok == error);
    assert(pSketch);

    {
        wy3d::SketchLine* pLine(nullptr);
        error = wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 100.0), wy::Vector2(100.0, 0.0), pLine);
        assert(wy::ErrorStatus::Ok == error);
        assert(pLine);
        pSketch->addEntity(pLine);
    }
    {
        wy3d::SketchLine* pLine(nullptr);
        error = wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 100.0), wy::Vector2(0.0, 0.0), pLine);
        assert(wy::ErrorStatus::Ok == error);
        assert(pLine);
        pSketch->addEntity(pLine);
    }
    {
        wy3d::SketchLine* pLine(nullptr);
        error = wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 100.0), wy::Vector2(0.0, 100.0), pLine);
        assert(wy::ErrorStatus::Ok == error);
        assert(pLine);
        pSketch->addEntity(pLine);
    }
    {
        wy3d::SketchLine* pLine(nullptr);
        error = wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 0.0), pLine);
        assert(wy::ErrorStatus::Ok == error);
        assert(pLine);
        pSketch->addEntity(pLine);
    }

    wy3d::Extrusion* pExtrusion(nullptr);
    error = wy3d::Extrusion::create(pTrans, pSketch, 100.0, pExtrusion);
    assert(wy::ErrorStatus::Ok == error);
    assert(pSketch);
}

static void createExtrusion_Box_WithHole(wydb::Database* pDb, wydb::Transaction* pTrans)
{
    wy3d::Sketch* pSketch(nullptr);
    wy3d::Plane sketchPlane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0));
    wy::ErrorStatus error = wy3d::Sketch::create(pTrans, sketchPlane, pSketch);
    assert(wy::ErrorStatus::Ok == error);
    assert(pSketch);

    {
        wy3d::SketchLine* pLine(nullptr);
        error = wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 100.0), wy::Vector2(100.0, 0.0), pLine);
        assert(wy::ErrorStatus::Ok == error);
        assert(pLine);
        pSketch->addEntity(pLine);
    }
    {
        wy3d::SketchLine* pLine(nullptr);
        error = wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 100.0), wy::Vector2(0.0, 0.0), pLine);
        assert(wy::ErrorStatus::Ok == error);
        assert(pLine);
        pSketch->addEntity(pLine);
    }
    {
        wy3d::SketchLine* pLine(nullptr);
        error = wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 100.0), wy::Vector2(0.0, 100.0), pLine);
        assert(wy::ErrorStatus::Ok == error);
        assert(pLine);
        pSketch->addEntity(pLine);
    }
    {
        wy3d::SketchLine* pLine(nullptr);
        error = wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 0.0), pLine);
        assert(wy::ErrorStatus::Ok == error);
        assert(pLine);
        pSketch->addEntity(pLine);
    }

    {
        wy3d::SketchCircle* pCircle(nullptr);
        error = wy3d::SketchCircle::create(pTrans, wy::Vector2(30.0, 30.0), 10.0, pCircle);
        assert(wy::ErrorStatus::Ok == error);
        assert(pCircle);
        pSketch->addEntity(pCircle);
    }

    wy3d::Extrusion* pExtrusion(nullptr);
    error = wy3d::Extrusion::create(pTrans, pSketch, 100.0, pExtrusion);
    assert(wy::ErrorStatus::Ok == error);
    assert(pSketch);
}

static void createExtrusion_Other(wydb::Database* pDb, wydb::Transaction* pTrans)
{
    if (1)
    {
        wy3d::Sketch* pSketch(nullptr);
        wy3d::Plane sketchPlane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0));
        wy::ErrorStatus error = wy3d::Sketch::create(pTrans, sketchPlane, pSketch);
        assert(wy::ErrorStatus::Ok == error);
        assert(pSketch);

        {
            wy3d::SketchLine* pLine(nullptr);
            //error = wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 50.0), wy::Vector2(100.0, 100.0), pLine);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 100.0), wy::Vector2(100.0, 50.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }

        {
            wy3d::SketchLine* pLine(nullptr);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 50.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }



        {
            wy3d::SketchLine* pLine(nullptr);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 100.0), wy::Vector2(0.0, 100.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }

        {
            wy3d::SketchLine* pLine(nullptr);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 100.0), wy::Vector2(0.0, 0.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }

        if (0)
        {
            {
                wy3d::SketchLine* pLine(nullptr);
                error = wy3d::SketchLine::create(pTrans, wy::Vector2(80.0, 80.0), wy::Vector2(90.0, 80.0), pLine);
                assert(wy::ErrorStatus::Ok == error);
                assert(pLine);
                pSketch->addEntity(pLine);
            }
            {
                wy3d::SketchLine* pLine(nullptr);
                error = wy3d::SketchLine::create(pTrans, wy::Vector2(90.0, 80.0), wy::Vector2(90.0, 90.0), pLine);
                assert(wy::ErrorStatus::Ok == error);
                assert(pLine);
                pSketch->addEntity(pLine);
            }
            {
                wy3d::SketchLine* pLine(nullptr);
                error = wy3d::SketchLine::create(pTrans, wy::Vector2(90.0, 90.0), wy::Vector2(80.0, 80.0), pLine);
                assert(wy::ErrorStatus::Ok == error);
                assert(pLine);
                pSketch->addEntity(pLine);
            }
        }
        else
        {
            {
                wy3d::SketchLine* pLine(nullptr);
                error = wy3d::SketchLine::create(pTrans, wy::Vector2(90.0, 80.0), wy::Vector2(80.0, 80.0), pLine);
                assert(wy::ErrorStatus::Ok == error);
                assert(pLine);
                pSketch->addEntity(pLine);
            }

            {
                wy3d::SketchLine* pLine(nullptr);
                error = wy3d::SketchLine::create(pTrans, wy::Vector2(80.0, 80.0), wy::Vector2(90.0, 90.0), pLine);
                assert(wy::ErrorStatus::Ok == error);
                assert(pLine);
                pSketch->addEntity(pLine);
            }
            {
                wy3d::SketchLine* pLine(nullptr);
                error = wy3d::SketchLine::create(pTrans, wy::Vector2(90.0, 90.0), wy::Vector2(90.0, 80.0), pLine);
                assert(wy::ErrorStatus::Ok == error);
                assert(pLine);
                pSketch->addEntity(pLine);
            }
        }


        {
            wy3d::SketchCircle* pCircle(nullptr);
            error = wy3d::SketchCircle::create(pTrans, wy::Vector2(60.0, 60.0), 20.0, pCircle);
            assert(wy::ErrorStatus::Ok == error);
            assert(pCircle);
            pSketch->addEntity(pCircle);
        }

        wy3d::Extrusion* pExtrusion(nullptr);
        error = wy3d::Extrusion::create(pTrans, pSketch, 100.0, pExtrusion);
        assert(wy::ErrorStatus::Ok == error);
        assert(pSketch);
    }

    if (0)
    {
        wy3d::Sketch* pSketch(nullptr);
        wy3d::Plane sketchPlane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0));
        wy::ErrorStatus error = wy3d::Sketch::create(pTrans, sketchPlane, pSketch);
        assert(wy::ErrorStatus::Ok == error);
        assert(pSketch);

        {
            wy3d::SketchLine* pLine(nullptr);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(100.0, 50.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }

        if (1)
        {
            wy3d::SketchLine* pLine(nullptr);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 50.0), wy::Vector2(100.0, 100.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }

        if (1)
        {
            wy3d::SketchLine* pLine(nullptr);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 100.0), wy::Vector2(0.0, 100.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }

        if (1)
        {
            wy3d::SketchLine* pLine(nullptr);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 100.0), wy::Vector2(0.0, 0.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }

        if (0)
        {
            wy3d::SketchCircle* pCircle(nullptr);
            error = wy3d::SketchCircle::create(pTrans, wy::Vector2(30.0, 45.0), 10, pCircle);
            assert(wy::ErrorStatus::Ok == error);
            assert(pCircle);
            pSketch->addEntity(pCircle);
        }

        if (0)
        {
            wy3d::SketchArc* pArc(nullptr);
            error = wy3d::SketchArc::create(pTrans, wy::Vector2(0.0, 0.0), 10.0, 0.0, wy3d::PI / 2, pArc);
            assert(wy::ErrorStatus::Ok == error);
            assert(pArc);
            pSketch->addEntity(pArc);
        }

        if (0)
        {
            wy3d::SketchArc* pArc(nullptr);
            error = wy3d::SketchArc::create(pTrans, wy::Vector2(100.0, 100.0), 10.0, wy3d::PI / 2, wy3d::PI / 4, pArc);
            assert(wy::ErrorStatus::Ok == error);
            assert(pArc);
            pSketch->addEntity(pArc);
        }

        if (0)
        {
            wy3d::SketchArc* pArc(nullptr);
            error = wy3d::SketchArc::create(pTrans, wy::Vector2(105.0, 105.0), 10.0, wy3d::PI / 2, wy3d::PI / 2, pArc);
            assert(wy::ErrorStatus::Ok == error);
            assert(pArc);
            pSketch->addEntity(pArc);
        }

        wy3d::Extrusion* pExtrusion(nullptr);
        error = wy3d::Extrusion::create(pTrans, pSketch, 100.0, pExtrusion);
        assert(wy::ErrorStatus::Ok == error);
        assert(pSketch);
    }
    if (0)
    {
        wy3d::Sketch* pSketch(nullptr);
        wy3d::Plane sketchPlane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, -1.0), wy::Vector3(1.0, 0.0, 0.0));
        wy::ErrorStatus error = wy3d::Sketch::create(pTrans, sketchPlane, pSketch);
        assert(wy::ErrorStatus::Ok == error);
        assert(pSketch);

        {
            wy3d::SketchLine* pLine(nullptr);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 0.0), wy::Vector2(0.0, 100.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }

        if (1)
        {
            wy3d::SketchLine* pLine(nullptr);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(0.0, 100.0), wy::Vector2(100.0, 100.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }

        if (1)
        {
            wy3d::SketchLine* pLine(nullptr);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 100.0), wy::Vector2(100.0, 0.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }

        if (1)
        {
            wy3d::SketchLine* pLine(nullptr);
            error = wy3d::SketchLine::create(pTrans, wy::Vector2(100.0, 0.0), wy::Vector2(0.0, 0.0), pLine);
            assert(wy::ErrorStatus::Ok == error);
            assert(pLine);
            pSketch->addEntity(pLine);
        }


        wy3d::Extrusion* pExtrusion(nullptr);
        error = wy3d::Extrusion::create(pTrans, pSketch, 100.0, pExtrusion);
        assert(wy::ErrorStatus::Ok == error);
        assert(pSketch);
    }
}

static void createHelix(wydb::Database* pDb, wydb::Transaction* pTrans)
{
    wy3d::Sketch* pSketch(nullptr);
    wy3d::Plane sketchPlane(wy::Vector3(0.0, 0.0, 0.0), wy::Vector3(0.0, 0.0, 1.0), wy::Vector3(1.0, 0.0, 0.0));
    wy::ErrorStatus error = wy3d::Sketch::create(pTrans, sketchPlane, pSketch);
    assert(wy::ErrorStatus::Ok == error);
    assert(pSketch);
    {
        wy3d::SketchCircle* pCircle(nullptr);
        error = wy3d::SketchCircle::create(pTrans, wy::Vector2(30.0, 30.0), 100.0, pCircle);
        assert(wy::ErrorStatus::Ok == error);
        assert(pCircle);
        pSketch->addEntity(pCircle);
    }

    wy3d::Helix* pHelix(nullptr);
    error = wy3d::Helix::create(pTrans, pSketch, 15.0, 10.0, 0.0, pHelix);
    assert(wy::ErrorStatus::Ok == error);
    assert(pHelix);
}

static void createImportedSolid(wydb::Database* pDb, wydb::Transaction* pTrans)
{
    wy3d::ImportedSolid* pImportedSolid(nullptr);
    wy::ErrorStatus error = wy3d::ImportedSolid::create(pTrans, std::wstring(L"D:/github/123.brep"), pImportedSolid);
    assert(wy::ErrorStatus::Ok == error);
    assert(pImportedSolid);
}

void Test::test(wydb::Database* pDb, wydb::Transaction* pTrans)
{
    //createExtrusion_CylibnderXOY(pDb, pTrans);
    //createExtrusion_CylibnderYOZ(pDb, pTrans);
    //createExtrusion_Box(pDb, pTrans);
    //createExtrusion_Box_WithHole(pDb, pTrans);
    //createHelix(pDb, pTrans);
    //createImportedSolid(pDb, pTrans);
}

} // namespace wy3dApp
