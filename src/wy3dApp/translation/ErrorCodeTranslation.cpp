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

#include "ErrorCodeTranslation.h"

#define TOUINT(code) static_cast<std::uint32_t>(code)

ErrorCodeTranslation& ErrorCodeTranslation::instance()
{
    static ErrorCodeTranslation instance(nullptr);
    return instance;
}

ErrorCodeTranslation::ErrorCodeTranslation(QObject* parent) : QObject(parent)
{
    // 警告
    _code2Desc[TOUINT(wy3d::ErrorCode::warnTOPOSHAPE_NullShape)] = tr(
        "Null shape!");

    //-----------------------------------------------------
    // 错误
    //-----------------------------------------------------
    // Element
    _code2Desc[TOUINT(wy3d::ErrorCode::ELEMENT_InvalidData)] = tr(
        "Invalid data!");

    // File
    _code2Desc[TOUINT(wy3d::ErrorCode::FILE_ReadError)] = tr(
        "Read file error!");

    // Shape
    _code2Desc[TOUINT(wy3d::ErrorCode::TOPOSHAPE_GenerateShapeError)] = tr(
        "Generate shape failed!");
    _code2Desc[TOUINT(wy3d::ErrorCode::TOPOSHAPE_NullShapeError)] = tr(
        "Null shape!");

    // Sketch
    _code2Desc[TOUINT(wy3d::ErrorCode::SKETCH_MoreThanTwoCurvesAtOneEndPoint)] = tr(
        "More than two curves at one end point!");

    // Path                                        
    _code2Desc[TOUINT(wy3d::ErrorCode::PATH_InvalidPath)] = tr(
        "Invalid path!");
    _code2Desc[TOUINT(wy3d::ErrorCode::PATH_NoCurves)] = tr(
        "Path sketch has no curves!");
    _code2Desc[TOUINT(wy3d::ErrorCode::PATH_MoreThanOneLoopIsNotAllowed)] = tr(
        "More than one loop is not allowed in path sketch!");

    // Profile
    _code2Desc[TOUINT(wy3d::ErrorCode::PROFILE_InvalidProfile)] = tr(
        "Invalid profile!");
    _code2Desc[TOUINT(wy3d::ErrorCode::PROFILE_NoClosedLoop)] = tr(
        "No closed loop!");
    _code2Desc[TOUINT(wy3d::ErrorCode::PROFILE_ClosedCurveIntersectWithOtherCurves)] = tr(
        "Closed curve intersect with other curves!");
    _code2Desc[TOUINT(wy3d::ErrorCode::PROFILE_ExistCurveNotInClosedLoop)] = tr(
        "Exists curve not in closed loop!");

    // Revolution
    _code2Desc[TOUINT(wy3d::ErrorCode::REVOLUTION_UnspecifiedAxisLine)] = tr(
        "Revolution axis line is unspecified!");
    _code2Desc[TOUINT(wy3d::ErrorCode::REVOLUTION_NoRevolutionAxisLine)] = tr(
        "No revolution axis center line!");
    _code2Desc[TOUINT(wy3d::ErrorCode::REVOLUTION_MoreThanOneRevolutionAxisLine)] = tr(
        "More than one revolution axis center lines!");
    _code2Desc[TOUINT(wy3d::ErrorCode::REVOLUTION_InvalidRevolutionAxisLine)] = tr(
        "Invalid revolution axis line!");

    // Sweep
    _code2Desc[TOUINT(wy3d::ErrorCode::SWEEP_PathPlaneAndProfilePlaneAreNotOrthogonal)] = tr(
        "Path plane and profile plane are not orthogonal!");

    // Boolean
    _code2Desc[TOUINT(wy3d::ErrorCode::BOOLEAN_InvalidTargetId)] = tr(
        "Invalid target id!");
    _code2Desc[TOUINT(wy3d::ErrorCode::BOOLEAN_InvalidToolId)] = tr(
        "Invalid tool id!");

    // Chamfer
    _code2Desc[TOUINT(wy3d::ErrorCode::CHAMFER_InvalidData)] = tr(
        "Invalid chamfer data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::CHAMFER_CreateChamferError)] = tr(
        "Create chamfer failed!");
    _code2Desc[TOUINT(wy3d::ErrorCode::CHAMFER_EdgeNotExists)] = tr(
        "Chamfer edge does not exists!");
    _code2Desc[TOUINT(wy3d::ErrorCode::CHAMFER_FaceNotExists)] = tr(
        "Chamfer face does not exists!");
    _code2Desc[TOUINT(wy3d::ErrorCode::CHAMFER_GenerateChamferError)] = tr(
        "Generate chamfer failed!");

    // Fillet
    _code2Desc[TOUINT(wy3d::ErrorCode::FILLET_InvalidData)] = tr(
        "Invalid fillet data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::FILLET_CreateFilletError)] = tr(
        "Create fillet failed!");
    _code2Desc[TOUINT(wy3d::ErrorCode::FILLET_EdgeNotExists)] = tr(
        "Fillet edge does not exists!");
    _code2Desc[TOUINT(wy3d::ErrorCode::FILLET_FaceNotExists)] = tr(
        "Fillet face does not exists!");
    _code2Desc[TOUINT(wy3d::ErrorCode::FILLET_GenerateFilletError)] = tr(
        "Generate fillet failed!");

    // Shell
    _code2Desc[TOUINT(wy3d::ErrorCode::SHELL_InvalidData)] = tr(
        "Invalid shell data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::SHELL_CreateShellError)] = tr(
        "Create shell failed!");
    _code2Desc[TOUINT(wy3d::ErrorCode::SHELL_FaceNotExists)] = tr(
        "Shell face does not exists!");
    _code2Desc[TOUINT(wy3d::ErrorCode::SHELL_GenerateShellError)] = tr(
        "Generate shell failed!");

    // Draft
    _code2Desc[TOUINT(wy3d::ErrorCode::DRAFT_InvalidData)] = tr(
        "Invalid draft data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::DRAFT_CreateDraftError)] = tr(
        "Create draft failed!");
    _code2Desc[TOUINT(wy3d::ErrorCode::DRAFT_FaceNotExists)] = tr(
        "Draft face does not exists!");
    _code2Desc[TOUINT(wy3d::ErrorCode::DRAFT_GenerateDraftError)] = tr(
        "Generate draft failed!");

    // Thicken
    _code2Desc[TOUINT(wy3d::ErrorCode::THICKEN_InvalidData)] = tr(
        "Invalid thicken data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::THICKEN_GenerateError)] = tr(
        "Generate thicken failed!");

    // OffsetSheet
    _code2Desc[TOUINT(wy3d::ErrorCode::OFFSETSHEET_InvalidData)] = tr(
        "Invalid offset sheet data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::OFFSETSHEET_GenerateError)] = tr(
        "Generate offset sheet failed!");

    // PlanarSheet
    _code2Desc[TOUINT(wy3d::ErrorCode::PLANARSHEET_InvalidData)] = tr(
        "Invalid planar sheet data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::PLANARSHEET_EdgesNotClosed)] = tr(
        "The selected edges must form a single closed loop!");
    _code2Desc[TOUINT(wy3d::ErrorCode::PLANARSHEET_EdgesNotCoplanar)] = tr(
        "The selected edges must be coplanar!");

    // SewnSheet
    _code2Desc[TOUINT(wy3d::ErrorCode::SEWNSHEET_InvalidData)] = tr(
        "Invalid sewn sheet data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::SEWNSHEET_GenerateError)] = tr(
        "Generate sewn sheet failed!");

    // Solidify
    _code2Desc[TOUINT(wy3d::ErrorCode::SOLIDIFY_InvalidData)] = tr(
        "Invalid solidify data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::SOLIDIFY_GenerateError)] = tr(
        "Generate solidify failed!");

    // SweptSheet
    _code2Desc[TOUINT(wy3d::ErrorCode::SWEPTSHEET_InvalidData)] = tr(
        "Invalid swept sheet data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::SWEPTSHEET_GenerateError)] = tr(
        "Generate swept sheet failed!");

    // LoftedSheet
    _code2Desc[TOUINT(wy3d::ErrorCode::LOFTSHEET_InvalidData)] = tr(
        "Invalid lofted sheet data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::LOFTSHEET_GenerateError)] = tr(
        "Generate lofted sheet failed!");

    // Helix
    _code2Desc[TOUINT(wy3d::ErrorCode::HELIX_InvalidData)] = tr(
        "Invalid helix data!");
    _code2Desc[TOUINT(wy3d::ErrorCode::HELIX_InvalidSketch)] = tr(
        "The sketch used to generate the helix must be a circle.");
}

ErrorCodeTranslation::~ErrorCodeTranslation()
{
}

QString ErrorCodeTranslation::getErrorCodeDescription(std::uint32_t code) const
{
    auto iter = _code2Desc.find(code);
    if (iter != _code2Desc.cend())
    {
        return iter->second;
    }
    else
    {
        return tr("ErrorCode: ") + QString::number(code) + tr("!");
    }
}