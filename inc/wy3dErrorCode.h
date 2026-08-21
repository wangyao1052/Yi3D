///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_ERROR_CODE_H
#define WY3D_ERROR_CODE_H

#include <wy3dDefs.h>

NS_WY3D_BEG

// 错误码
enum class ErrorCode : std::uint32_t
{
    NoError                                        = 0,

    // 1 - 1000 为警告, >1000 为错误.
    warnTOPOSHAPE_NullShape                        = 101,  // 形体为空

    // Element
    ELEMENT_InvalidData                            = 1001, // 无效数据

    // File
    FILE_ReadError                                 = 1080, // 读取文件失败
            
    // Shape                                       
    TOPOSHAPE_GenerateShapeError                   = 1100, // 生成形体失败
    TOPOSHAPE_NullShapeError                       = 1101, // 形体为空

    // Sketch
    SKETCH_MoreThanTwoCurvesAtOneEndPoint          = 1151, // 端点处有超过2条曲线

    // Path                                        
    PATH_InvalidPath                               = 1161, // 无效路径(未知具体原因)
    PATH_NoCurves                                  = 1162, // 路径为空
    PATH_MoreThanOneLoopIsNotAllowed               = 1163, // 不允许有一个以上的环

    // Profile                                     
    PROFILE_InvalidProfile                         = 1201, // 无效轮廓(未知具体原因)
    PROFILE_NoClosedLoop                           = 1202, // 没有闭合环
    PROFILE_ClosedCurveIntersectWithOtherCurves    = 1203, // 闭合曲线与其它曲线相交
    PROFILE_ExistCurveNotInClosedLoop              = 1204, // 线必须在闭合的环内

    // Revolution
    REVOLUTION_UnspecifiedAxisLine                 = 1301, // 未指定旋转体轴线
    REVOLUTION_NoRevolutionAxisLine                = 1302, // 旋转草图没有轴线
    REVOLUTION_MoreThanOneRevolutionAxisLine       = 1303, // 旋转草图有超过一条轴线
    REVOLUTION_InvalidRevolutionAxisLine           = 1304, // 无效的旋转轴

    // Sweep
    SWEEP_PathPlaneAndProfilePlaneAreNotOrthogonal = 1310, // 扫描体的路径平面与轮廓平面不正交

    // Boolean
    BOOLEAN_InvalidTargetId                        = 1401, // 无效的目标体ID
    BOOLEAN_InvalidToolId                          = 1402, // 无效的参与体ID

    // Chamfer
    CHAMFER_InvalidData                            = 1501, // 无效的倒角数据
    CHAMFER_CreateChamferError                     = 1502, // 创建倒角特征失败
    CHAMFER_EdgeNotExists                          = 1503, // 倒角边不存在
    CHAMFER_FaceNotExists                          = 1504, // 倒角面不存在
    CHAMFER_GenerateChamferError                   = 1505, // 倒角生成失败

    // Fillet
    FILLET_InvalidData                             = 1551, // 无效的圆角数据
    FILLET_CreateFilletError                       = 1552, // 创建圆角特征失败
    FILLET_EdgeNotExists                           = 1553, // 圆角边不存在
    FILLET_FaceNotExists                           = 1554, // 圆角面不存在
    FILLET_GenerateFilletError                     = 1555, // 圆角生成失败

    // Shell
    SHELL_InvalidData                              = 1601, // 无效的抽壳数据
    SHELL_CreateShellError                         = 1602, // 创建抽壳特征失败
    SHELL_FaceNotExists                            = 1603, // 抽壳面不存在
    SHELL_GenerateShellError                       = 1604, // 抽壳生成失败

    // Draft
    DRAFT_InvalidData                              = 1651, // 无效的拔模数据
    DRAFT_CreateDraftError                         = 1652, // 创建拔模特征失败
    DRAFT_FaceNotExists                            = 1653, // 拔模面不存在
    DRAFT_GenerateDraftError                       = 1654, // 拔模生成失败

    // Helix
    HELIX_InvalidData                               = 1701, // 无效的螺旋线数据
    HELIX_InvalidSketch                             = 1702, // 无效的螺旋线草图:用于生成螺旋线的草图必须为一个圆形

    // Thicken
    THICKEN_InvalidData                             = 1751, // 无效的加厚数据
    THICKEN_GenerateError                           = 1752, // 加厚生成失败

    // OffsetSheet
    OFFSETSHEET_InvalidData                         = 1801, // 无效的偏置曲面数据
    OFFSETSHEET_GenerateError                       = 1802, // 偏置曲面生成失败

    // PlanarSheet
    PLANARSHEET_InvalidData                         = 1851, // 无效的平面片体数据
    PLANARSHEET_EdgesNotClosed                      = 1852, // 边未构成单一闭合环
    PLANARSHEET_EdgesNotCoplanar                    = 1853, // 边不共面

    // SewnSheet
    SEWNSHEET_InvalidData                           = 1901, // 无效的缝合片体数据
    SEWNSHEET_GenerateError                         = 1902, // 缝合片体生成失败

    // Solidify
    SOLIDIFY_InvalidData                            = 1951, // 无效的实体化数据
    SOLIDIFY_GenerateError                          = 1952, // 实体化生成失败

    // SweptSheet
    SWEPTSHEET_InvalidData                          = 2001, // 无效的扫掠曲面数据
    SWEPTSHEET_GenerateError                        = 2002, // 扫掠曲面生成失败

    // LoftedSheet
    LOFTSHEET_InvalidData                           = 2051, // 无效的放样曲面数据
    LOFTSHEET_GenerateError                         = 2052, // 放样曲面生成失败
};

inline bool isError(unsigned int code)
{
    return code > 1000;
}

inline bool isWarning(unsigned int code)
{
    return code > 0 && code <= 1000;
}

NS_WY3D_END

#endif // WY3D_ERROR_CODE_H