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

#ifndef WY3D_FILLED_SHEET_H
#define WY3D_FILLED_SHEET_H

#include <utility>
#include <vector>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>

#include <wy3dDefs.h>
#include <wy3dSheet.h>
#include <wy3dErrorCode.h>

NS_WY3D_BEG

class Sketch;
class Sketch3D;

class WY3D_EXPORT FilledSheet : public wy3d::Sheet
{
    WYDB_DECLARE_MEMBERS(FilledSheet, wy3d::FilledSheet, wy3d::Sheet)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sketch* pSketch,
        FilledSheet*& pOutSheet);

    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        wy3d::Sketch3D* pSketch3D,
        FilledSheet*& pOutSheet);

    virtual std::vector<wydb::ElementId> getChildren() const override
    {
        std::vector<wydb::ElementId> children;
        std::vector<wydb::ElementId> baseChildren = __baseClass::getChildren();
        children.reserve(_constraintSketchIds.size() + 1 + baseChildren.size());
        if (!_boundarySketchId.isNull()) children.emplace_back(_boundarySketchId);
        children.insert(children.cend(), _constraintSketchIds.cbegin(), _constraintSketchIds.cend());
        children.insert(children.cend(), baseChildren.cbegin(), baseChildren.cend());
        return children;
    }

    wydb::ElementId getBoundarySketch() const { return _boundarySketchId; }

    const std::vector<wydb::ElementId>& getConstraintSketches() const
    {
        return _constraintSketchIds;
    }

    wy::ErrorStatus addConstraintSketch(wy3d::Sketch* pConstraintSketch);
    wy::ErrorStatus addConstraintSketch(wy3d::Sketch3D* pConstraintSketch);
    wy::ErrorStatus removeConstraintSketch(wy3d::Sketch* pConstraintSketch);
    wy::ErrorStatus removeConstraintSketch(wy3d::Sketch3D* pConstraintSketch);

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;

    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;

    virtual void reportDependencies(std::set<wydb::ElementId>& dependencies) const override;
    virtual bool onDependenciesErased(const std::set<wydb::ElementId>& erasedDependencies) override;

    virtual TopoDS_Shape generateShape(
        TopoNaming* pTopoNaming,
        wydb::ChainUpdateFeedbackCollector& feedbackCollector) override;

private:
    wy::ErrorStatus setBoundarySketchImpl(const wydb::ElementId& sketchId);
    wy::ErrorStatus setBoundarySketchImpl(wy3d::Sketch* pSketch);
    wy::ErrorStatus setBoundarySketchImpl(wy3d::Sketch3D* pSketch3D);

    wy::ErrorStatus setConstraintSketchImpl(const std::vector<wydb::ElementId>& constraintSketchIds);
    wy::ErrorStatus addConstraintSketchImpl(const wydb::ElementId& constraintSketchId);
    wy::ErrorStatus removeConstraintSketchImpl(const wydb::ElementId& constraintSketchId);

    ErrorCode collectConstraints(std::vector<TopoDS_Edge>& constraintEdges,
        std::vector<TopoDS_Vertex>& constraintVertices) const;

    struct GenerateShapeResult
    {
        ErrorCode errorCode = ErrorCode::ELEMENT_InvalidData;
        TopoDS_Shape shape;
        TopoNaming topoNaming;
    };
    GenerateShapeResult generateShape(const wy3d::Sketch* pSketch);
    GenerateShapeResult generateShape(const wy3d::Sketch3D* pSketch3D);

protected:
    wydb::ElementId _boundarySketchId;
    std::vector<wydb::ElementId> _constraintSketchIds;
};

NS_WY3D_END

#endif // WY3D_FILLED_SHEET_H
