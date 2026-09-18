///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2024 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_DEFS_H
#define WY3D_DEFS_H

#include <cstdint>

#define NS_WY3D_BEG namespace wy3d {
#define NS_WY3D_END }

#define NS_WY3D_GEOM_BEG namespace wy3d { namespace geom {
#define NS_WY3D_GEOM_END } }

#if defined(_WIN32)
    #if defined(WY3D_LIBRARY)
    #define WY3D_EXPORT __declspec(dllexport)
    #else
    #define WY3D_EXPORT __declspec(dllimport)
    #endif

#else
    #define WY3D_EXPORT __attribute__((visibility("default")))
#endif

#endif // WY3D_DEFS_H
