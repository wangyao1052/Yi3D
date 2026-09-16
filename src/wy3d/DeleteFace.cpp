///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2026 Wang Yao <wangyao1052@163.com>
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

#include <cassert>

#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeShape.hxx>

#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wydbFiler.h>
#include <wydbFieldRegistry.h>
#include <wy3dDeleteFace.h>
#include <wy3dSheet.h>
#include <wy3dErrorCode.h>
#include <wy3dDefaultChainUpdateFeedback.h>
#include "topo/TopoNamingUtil.h"
#include "topo/TopoShapeComparer.h"
#include "SolidModificationUtil.h"
#include "utils/FilerUtil.h"
#include "utils/Util.h"

NS_WY3D_BEG

WYDB_IMPLEMENT_MEMBERS(DeleteFace)

BEGIN_FIELD_REGISTRATION()
    REGISTER_FIELD(DeleteFace, _faceNames)
END_FIELD_REGISTRATION()

namespace
{

class wyBRepBuilderAPI_DeleteFace : public BRepBuilderAPI_MakeShape
{
public:
    wyBRepBuilderAPI_DeleteFace(const TopoDS_Shape& shape, const TopoShapeSet& facesToRemove)
    {
        TopoDS_Shape outShape;
        _isAnyRemoved = this->removeFaces(shape, facesToRemove, outShape);
        myShape = outShape;
        Done();
    }

    // Whether a face was actually taken off; none taken means the targets are not in this shape
    bool isAnyRemoved() const { return _isAnyRemoved; }

private:
    // Whether anything under this node was taken off. A null outShape means the node is all gone.
    bool removeFaces(const TopoDS_Shape& inShape, const TopoShapeSet& facesToRemove, TopoDS_Shape& outShape)
    {
        const TopAbs_ShapeEnum shapeType = inShape.ShapeType();
        if (TopAbs_ShapeEnum::TopAbs_FACE == shapeType)
        {
            if (facesToRemove.find(inShape) != facesToRemove.cend())
            {
                outShape = TopoDS_Shape();
                return true;
            }
            else
            {
                outShape = inShape;
                return false;
            }
        }

        BRep_Builder builder;
        TopoDS_Shape retShape;
        if (TopAbs_ShapeEnum::TopAbs_COMPOUND == shapeType)
        {
            TopoDS_Compound compound;
            builder.MakeCompound(compound);
            retShape = compound;
        }
        else if (TopAbs_ShapeEnum::TopAbs_SHELL == shapeType)
        {
            TopoDS_Shell shell;
            builder.MakeShell(shell);
            retShape = shell;
        }
        else
        {
            outShape = inShape;
            return false;
        }

        bool removed(false);
        bool keptAny(false);
        for (TopoDS_Iterator iter(inShape); iter.More(); iter.Next())
        {
            TopoDS_Shape newChild;
            if (this->removeFaces(iter.Value(), facesToRemove, newChild))
            {
                removed = true;
            }
            if (newChild.IsNull())
            {
                continue;
            }
            else
            {
                builder.Add(retShape, newChild);
                keptAny = true;
            }
        }
        if (keptAny) outShape = retShape;
        else outShape = TopoDS_Shape();

        return removed;
    }

    bool _isAnyRemoved;
};

} // namespace

DeleteFace::DeleteFace() : wy3d::SolidModification()
{
}

DeleteFace::~DeleteFace()
{
}

void DeleteFace::registerParameters(wydb::ParameterSchemaExtension* pParamSchema)
{
}

wy::ErrorStatus DeleteFace::create(
    wydb::Transaction* pTrans,
    wy3d::Sheet* pSheet,
    const std::vector<unsigned int>& faceIndices,
    DeleteFace*& pOutDeleteFace)
{
    pOutDeleteFace = nullptr;

    if (!pTrans) return wy::ErrorStatus::NullTransactionPointer;
    if (!pSheet) return wy::ErrorStatus::NullElementPointer;
    if (faceIndices.empty()) return wy::ErrorStatus::InvalidInput;

    const TopoNaming* pTopoNaming = pSheet->getTopoNaming();
    if (!pTopoNaming)
    {
        assert(false);
        return wy::ErrorStatus::InvalidInput;
    }
    TopoNameList faceNames;
    if (!TopoNamingUtil::assemblyTopoNames(*pTopoNaming, pSheet->getShape(),
        TopAbs_ShapeEnum::TopAbs_FACE, faceIndices, faceNames))
    { return wy::ErrorStatus::InvalidInput; }
    if (faceNames.empty())
    {
        assert(false);
        return wy::ErrorStatus::InvalidInput;
    }

    DeleteFace* pDeleteFace = new DeleteFace();
    wy::ErrorStatus error = pTrans->addNewlyCreatedElement(pDeleteFace);
    if (wy::ErrorStatus::Ok != error)
    {
        wydb::deleteElement(pDeleteFace);
        pDeleteFace = nullptr;
        return error;
    }
    error = pDeleteFace->setFacesImpl(faceNames);
    CHECK_ERROR_FOR_CREATE(error, pDeleteFace);
    error = pSheet->addModification(pDeleteFace);
    CHECK_ERROR_FOR_CREATE(error, pDeleteFace);

    pOutDeleteFace = pDeleteFace;
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus DeleteFace::setFacesImpl(const TopoNameList& faceNames)
{
    if (faceNames.empty()) return wy::ErrorStatus::InvalidInput;
    if (faceNames == _faceNames) return wy::ErrorStatus::Ok;
    wy::ErrorStatus error = this->prepareForFieldChange(kDeleteFace_faceNames);
    if (wy::ErrorStatus::Ok == error)
    {
        _faceNames = faceNames;
        return wy::ErrorStatus::Ok;
    }
    else
    {
        return error;
    }
}

bool DeleteFace::getFieldValue(wydb::FieldId fieldId, std::any& value)
{
    switch (fieldId.value())
    {
    case kDeleteFace_faceNames.value():
        value = _faceNames;
        return true;
    default:
        bool baseRet = __baseClass::getFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

bool DeleteFace::setFieldValue(wydb::FieldId fieldId, const std::any& value)
{
    switch (fieldId.value())
    {
    case kDeleteFace_faceNames.value():
        _faceNames = std::any_cast<const TopoNameList&>(value);
        return true;
    default:
        bool baseRet = __baseClass::setFieldValue(fieldId, value);
        assert(baseRet);
        return baseRet;
    }
}

wy::ErrorStatus DeleteFace::writeToFiler(wydb::OutFiler& filer) const
{
    __baseClass::writeToFiler(filer);
    FilerUtil::writeVector(filer, _faceNames);
    return wy::ErrorStatus::Ok;
}

wy::ErrorStatus DeleteFace::readFromFiler(wydb::InFiler& filer)
{
    __baseClass::readFromFiler(filer);
    FilerUtil::readTopoNameList(filer, _faceNames);
    return wy::ErrorStatus::Ok;
}

std::pair<bool, TopoDS_Shape> DeleteFace::modifyOwnerShape(const TopoDS_Shape& shape, TopoNaming* pTopoNaming, wydb::ChainUpdateFeedbackCollector& feedbackCollector)
{
    assert(pTopoNaming);

    TopTools_IndexedMapOfShape solidMap;
    TopExp::MapShapes(shape, TopAbs_ShapeEnum::TopAbs_SOLID, solidMap);
    if (solidMap.Extent() > 0)
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::TOPOSHAPE_GenerateShapeError));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    if (_faceNames.empty())
    {
        assert(false);
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
            static_cast<std::uint32_t>(ErrorCode::DELETEFACE_InvalidData));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    std::vector<TopoDS_Face> targetFaces;
    ErrorCode errorCode = SolidModificationUtil::getTopoFacesByTopoNamings<
        ErrorCode::DELETEFACE_InvalidData,
        ErrorCode::DELETEFACE_FaceNotExists>(*pTopoNaming, _faceNames, targetFaces);
    if (ErrorCode::NoError != errorCode)
    {
        wy3d::reportChainUpdateError(feedbackCollector, this->getId(), static_cast<unsigned int>(errorCode));
        return std::pair<bool, TopoDS_Shape>(false, shape);
    }

    try
    {
        TopoShapeSet facesToRemove;
        for (const TopoDS_Face& face : targetFaces)
        {
            facesToRemove.insert(face);
        }

        wyBRepBuilderAPI_DeleteFace deleteFaces(shape, facesToRemove);
        if (!deleteFaces.isAnyRemoved())
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::DELETEFACE_GenerateError));
            return std::pair<bool, TopoDS_Shape>(false, shape);
        }

        // A null shape is how the command says every face went away
        const TopoDS_Shape retShape = deleteFaces.Shape();
        if (retShape.IsNull())
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::DELETEFACE_NoFaceLeft));
            return std::pair<bool, TopoDS_Shape>(false, shape);
        }
        TopTools_IndexedMapOfShape leftFaceMap;
        TopExp::MapShapes(retShape, TopAbs_ShapeEnum::TopAbs_FACE, leftFaceMap);
        if (leftFaceMap.Extent() == 0)
        {
            wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
                static_cast<std::uint32_t>(ErrorCode::DELETEFACE_NoFaceLeft));
            return std::pair<bool, TopoDS_Shape>(false, shape);
        }

        TopoShapeComparer topoComparer(deleteFaces, shape);
        topoComparer.perform();
        pTopoNaming->update(&topoComparer, this->getId().value());

        return std::pair<bool, TopoDS_Shape>(true, retShape);
    }
    catch (const Standard_Failure&)
    {
        assert(false);
    }

    wy3d::reportChainUpdateError(feedbackCollector, this->getId(),
        static_cast<std::uint32_t>(ErrorCode::DELETEFACE_GenerateError));
    return std::pair<bool, TopoDS_Shape>(false, shape);
}

NS_WY3D_END
