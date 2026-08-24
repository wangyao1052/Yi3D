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

#include "Exporter.h"

#include <filesystem>

#include <TopoDS.hxx>
#include <BRep_Builder.hxx>
#include <BRepTools.hxx>
#include <TopoDS_Compound.hxx>
#include <STEPControl_Writer.hxx>
#include <Interface_Static.hxx>
#include <XSControl_WorkSession.hxx>
#include <APIHeaderSection_MakeHeader.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Writer.hxx>
#include <IGESData_GlobalSection.hxx>
#include <IGESData_IGESModel.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopExp_Explorer.hxx>

#include <wydbElement.h>
#include <wydbDatabase.h>
#include <wyapDocument.h>
#include <wy3dSolid.h>
#include <wy3dSolid.h>

ExporterManager& ExporterManager::instance()
{
    static ExporterManager instance;
    return instance;
}

ExporterManager::ExporterManager()
{
    _filter2Exporter[tr("BREP format (*.brep)")] = std::make_shared<BrepExporter>();
    _filter2Exporter[tr("STEP format (*.step *.stp)")] = std::make_shared<StepExporter>();
    _filter2Exporter[tr("IGES format (*.iges *.igs)")] = std::make_shared<IgesExporter>();
    _filter2Exporter[tr("STL Mesh (*.stl)")] = std::make_shared<StlExporter>();
}

ExporterManager::~ExporterManager()
{
}

bool Exporter::perform(const wydb::Database* pDb, const std::wstring& fileFullPath)
{
    try
    {
        if (!pDb) return false;
        TopoDS_Compound compound;
        if (!this->computeDatabaseCompound(pDb, compound))
        {
            return false;
        }
        return this->performImpl(compound, fileFullPath);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return false;
    }
    catch (...)
    {
        assert(false);
        return false;
    }
}

bool Exporter::perform(const TopoDS_Shape& shape, const std::wstring& fileFullPath)
{
    try
    {
        if (shape.IsNull()) return false;
        return this->performImpl(shape, fileFullPath);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
        return false;
    }
    catch (...)
    {
        assert(false);
        return false;
    }
}

bool Exporter::computeDatabaseCompound(const wydb::Database* pDb, TopoDS_Compound& compound) const
{
    if (!pDb) return false;

    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (auto iter = pDb->createIterator(); !iter.isDone(); iter.moveNext())
    {
        wydb::ElementId id = iter.current();
        const wydb::Element* pElem = pDb->getElement(id);
        if (!pElem) continue;
        if (pElem->getParent() != wydb::ElementId::kNull) continue;
        TopoDS_Shape featRetShape;
        if (const wy3d::Solid* pSolid = wy3d::Solid::cast(pElem))
            featRetShape = pSolid->getShape();
        else if (const wy3d::Sheet* pSheet = wy3d::Sheet::cast(pElem))
            featRetShape = pSheet->getShape();
        else continue;
        if (featRetShape.IsNull()) continue;
        builder.Add(compound, featRetShape);
    }

    return true;
}

bool BrepExporter::performImpl(const TopoDS_Shape& shape, const std::wstring& fileFullPath)
{
    std::filesystem::path filePath(fileFullPath);
    std::string utf8_path = filePath.u8string();
    if (Standard_True != BRepTools::Write(shape, utf8_path.c_str()))
    {
        assert(false);
        return false;
    }

    return true;
}

bool StepExporter::performImpl(const TopoDS_Shape& shape, const std::wstring& fileFullPath)
{
    // Do not write out any assembly information when using the simplified STEP export
    Interface_Static::SetIVal("write.step.assembly", 0);

    // write step file
    STEPControl_Writer aWriter;
    const Handle(XSControl_TransferWriter)& hTransferWriter = aWriter.WS()->TransferWriter();
    Handle(Transfer_FinderProcess) hFinder = hTransferWriter->FinderProcess();
#pragma warning(disable : 26812)
    if (aWriter.Transfer(shape, STEPControl_AsIs) != IFSelect_ReturnStatus::IFSelect_RetDone)
#pragma warning(default : 26812) 
    {
        assert(false);
        return false;
    }

    APIHeaderSection_MakeHeader makeHeader(aWriter.Model());
    // Don't set name because STEP doesn't support UTF-8
    makeHeader.SetAuthorValue(1, new TCollection_HAsciiString("Yi3D"));
    makeHeader.SetOrganizationValue(1, new TCollection_HAsciiString("Yi3D"));
    makeHeader.SetOriginatingSystem(new TCollection_HAsciiString("Yi3D"));
    makeHeader.SetDescriptionValue(1, new TCollection_HAsciiString("Yi3D Model"));
    std::filesystem::path filePath(fileFullPath);
    std::string utf8_path = filePath.u8string();
    if (aWriter.Write(utf8_path.c_str()) != IFSelect_RetDone)
    {
        assert(false);
        return false;
    }

    return true;
}

bool IgesExporter::performImpl(const TopoDS_Shape& shape, const std::wstring& fileFullPath)
{
    // write iges file
    IGESControl_Controller::Init();
    IGESControl_Writer aWriter;
    IGESData_GlobalSection header = aWriter.Model()->GlobalSection();
    header.SetAuthorName(new TCollection_HAsciiString(Interface_Static::CVal("write.iges.header.author")));
    header.SetCompanyName(new TCollection_HAsciiString(Interface_Static::CVal("write.iges.header.company")));
    header.SetSendName(new TCollection_HAsciiString(Interface_Static::CVal("write.iges.header.product")));
    aWriter.Model()->SetGlobalSection(header);
    aWriter.AddShape(shape);
    aWriter.ComputeModel();
    std::filesystem::path filePath(fileFullPath);
    std::string utf8_path = filePath.u8string();
    if (aWriter.Write(utf8_path.c_str()) != IFSelect_RetDone)
    {
        assert(false);
        return false;
    }

    return true;
}

static bool writeStlBinary(
    const Handle(Poly_Triangulation)& theMesh,
    FILE* theFile);

bool StlExporter::performImpl(const TopoDS_Shape& shape, const std::wstring& fileFullPath)
{
    BRepMesh_IncrementalMesh incrementalMesh(
        shape,
        0.01, // 相对比例
        Standard_True, // isRelative
        0.1, // 参照FreeCAD设置为0.1(OCC默认为0.5)
        Standard_True // isInParallel
    );
    incrementalMesh.Perform();
    
    // commented by wangyao 2025.07.11 {
    // 不使用StlAPI_Writer因为会依赖到OpenVR_API.dll,这是不合理的,感觉有可能是我自己编译OCC时候的环境有问题,
    // 但是不想深究这个问题了,直接参考OCC读写STL的源码来实现.
    // StlAPI_Writer writer;
    //if (Standard_True != writer.Write(compound, fileFullPath.c_str()))
    //{
    //    assert(false);
    //    return false;
    //}
    // }

    // calculate total number of the nodes and triangles
    Standard_Integer aNbNodes = 0;
    Standard_Integer aNbTriangles = 0;
    for (TopExp_Explorer anExpSF(shape, TopAbs_FACE); anExpSF.More(); anExpSF.Next())
    {
        TopLoc_Location aLoc;
        Handle(Poly_Triangulation) aTriangulation = BRep_Tool::Triangulation(TopoDS::Face(anExpSF.Current()), aLoc);
        if (!aTriangulation.IsNull())
        {
            aNbNodes += aTriangulation->NbNodes();
            aNbTriangles += aTriangulation->NbTriangles();
        }
    }
    if (aNbNodes == 0 || aNbTriangles == 0)
    {
        return false;
    }

    // temporary triangulation
    Handle(Poly_Triangulation) aMesh = new Poly_Triangulation(aNbNodes, aNbTriangles, Standard_False);
    Standard_Integer aNodeOffset = 0;
    Standard_Integer aTriangleOffet = 0;
    for (TopExp_Explorer anExpSF(shape, TopAbs_FACE); anExpSF.More(); anExpSF.Next())
    {
        const TopoDS_Shape& aFace = anExpSF.Current();
        TopLoc_Location aLoc;
        Handle(Poly_Triangulation) aTriangulation = BRep_Tool::Triangulation(TopoDS::Face(aFace), aLoc);
        if (aTriangulation.IsNull())
        {
            continue;
        }

        // copy nodes
        gp_Trsf aTrsf = aLoc.Transformation();
        for (Standard_Integer aNodeIter = 1; aNodeIter <= aTriangulation->NbNodes(); ++aNodeIter)
        {
            gp_Pnt aPnt = aTriangulation->Node(aNodeIter);
            aPnt.Transform(aTrsf);
            aMesh->SetNode(aNodeIter + aNodeOffset, aPnt);
        }

        // copy triangles
        const TopAbs_Orientation anOrientation = anExpSF.Current().Orientation();
        for (Standard_Integer aTriIter = 1; aTriIter <= aTriangulation->NbTriangles(); ++aTriIter)
        {
            Poly_Triangle aTri = aTriangulation->Triangle(aTriIter);
            Standard_Integer anId[3];
            aTri.Get(anId[0], anId[1], anId[2]);
            if (anOrientation == TopAbs_REVERSED)
            {
                // Swap 1, 2.
                Standard_Integer aTmpIdx = anId[1];
                anId[1] = anId[2];
                anId[2] = aTmpIdx;
            }

            // Update nodes according to the offset.
            anId[0] += aNodeOffset;
            anId[1] += aNodeOffset;
            anId[2] += aNodeOffset;
            aTri.Set(anId[0], anId[1], anId[2]);
            aMesh->SetTriangle(aTriIter + aTriangleOffet, aTri);
        }

        // offset
        aNodeOffset += aTriangulation->NbNodes();
        aTriangleOffet += aTriangulation->NbTriangles();
    }

    // 写文件
    std::filesystem::path filePath(fileFullPath);
#ifdef _WIN32
    // Windows平台使用宽字符API
    FILE* fp = _wfopen(filePath.c_str(), L"wb");
#else
    // 其他平台使用UTF-8路径
    std::string utf8_path = filePath.u8string();
    FILE* fp = fopen(utf8_path.c_str(), "wb");
#endif
    if (!fp)
    {
        return false;
    }
    bool isOK = writeStlBinary(aMesh, fp);
    fclose(fp);
    return isOK;
}

//! Writing a Little Endian 32 bits integer
inline static void convertInteger(const Standard_Integer theValue,
    Standard_Character* theResult)
{
    union
    {
        Standard_Integer i;
        Standard_Character c[4];
    } anUnion;
    anUnion.i = theValue;

    theResult[0] = anUnion.c[0];
    theResult[1] = anUnion.c[1];
    theResult[2] = anUnion.c[2];
    theResult[3] = anUnion.c[3];
}

//! Writing a Little Endian 32 bits float
inline static void convertDouble(const Standard_Real theValue,
    Standard_Character* theResult)
{
    union
    {
        Standard_ShortReal i;
        Standard_Character c[4];
    } anUnion;
    anUnion.i = (Standard_ShortReal)theValue;

    theResult[0] = anUnion.c[0];
    theResult[1] = anUnion.c[1];
    theResult[2] = anUnion.c[2];
    theResult[3] = anUnion.c[3];
}

static bool writeStlBinary(
    const Handle(Poly_Triangulation)& theMesh,
    FILE* theFile)
{
    char aHeader[80] = "STL Exported by YI3D[www.wangyaosoft.com].";
    if (fwrite(aHeader, 1, 80, theFile) != 80)
    {
        return false;
    }

    const Standard_Integer aNBTriangles = theMesh->NbTriangles();
    const Standard_Size aNbChunkTriangles = 4096;
    static const Standard_Integer THE_STL_SIZEOF_FACET = 50;
    const Standard_Size aChunkSize = aNbChunkTriangles * THE_STL_SIZEOF_FACET;
    NCollection_Array1<Standard_Character> aData(1, aChunkSize);
    Standard_Character* aDataChunk = &aData.ChangeFirst();

    Standard_Character aConv[4];
    convertInteger(aNBTriangles, aConv);
    if (fwrite(aConv, 1, 4, theFile) != 4)
    {
        return false;
    }

    Standard_Size aByteCount = 0;
    for (Standard_Integer aTriIter = 1; aTriIter <= aNBTriangles; ++aTriIter)
    {
        Standard_Integer id[3];
        const Poly_Triangle aTriangle = theMesh->Triangle(aTriIter);
        aTriangle.Get(id[0], id[1], id[2]);

        const gp_Pnt aP1 = theMesh->Node(id[0]);
        const gp_Pnt aP2 = theMesh->Node(id[1]);
        const gp_Pnt aP3 = theMesh->Node(id[2]);

        gp_Vec aVec1(aP1, aP2);
        gp_Vec aVec2(aP1, aP3);
        gp_Vec aVNorm = aVec1.Crossed(aVec2);
        if (aVNorm.SquareMagnitude() > gp::Resolution())
        {
            aVNorm.Normalize();
        }
        else
        {
            aVNorm.SetCoord(0.0, 0.0, 0.0);
        }

        convertDouble(aVNorm.X(), &aDataChunk[aByteCount]); aByteCount += 4;
        convertDouble(aVNorm.Y(), &aDataChunk[aByteCount]); aByteCount += 4;
        convertDouble(aVNorm.Z(), &aDataChunk[aByteCount]); aByteCount += 4;

        convertDouble(aP1.X(), &aDataChunk[aByteCount]); aByteCount += 4;
        convertDouble(aP1.Y(), &aDataChunk[aByteCount]); aByteCount += 4;
        convertDouble(aP1.Z(), &aDataChunk[aByteCount]); aByteCount += 4;

        convertDouble(aP2.X(), &aDataChunk[aByteCount]); aByteCount += 4;
        convertDouble(aP2.Y(), &aDataChunk[aByteCount]); aByteCount += 4;
        convertDouble(aP2.Z(), &aDataChunk[aByteCount]); aByteCount += 4;

        convertDouble(aP3.X(), &aDataChunk[aByteCount]); aByteCount += 4;
        convertDouble(aP3.Y(), &aDataChunk[aByteCount]); aByteCount += 4;
        convertDouble(aP3.Z(), &aDataChunk[aByteCount]); aByteCount += 4;

        aDataChunk[aByteCount] = 0; aByteCount += 1;
        aDataChunk[aByteCount] = 0; aByteCount += 1;

        // Chunk is filled. Dump it to the file.
        if (aByteCount == aChunkSize)
        {
            if (fwrite(aDataChunk, 1, aChunkSize, theFile) != aChunkSize)
            {
                return false;
            }

            aByteCount = 0;
        }
    }

    // Write last part if necessary.
    if (aByteCount != aChunkSize)
    {
        if (fwrite(aDataChunk, 1, aByteCount, theFile) != aByteCount)
        {
            return false;
        }
    }

    return true;
}