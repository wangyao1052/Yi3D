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

#ifndef WY3D_SKETCH_CURVE3D_H
#define WY3D_SKETCH_CURVE3D_H

#include <wyVector3.h>
#include <wy3dDefs.h>
#include <wy3dSketchEntity3D.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchCurve3D : public wy3d::SketchEntity3D
{
    WYDB_DECLARE_ABSTRACT_MEMBERS(SketchCurve3D, wy3d::SketchCurve3D, wy3d::SketchEntity3D)

public:
    virtual wy::Vector3 getStartPoint() const { return wy::Vector3::kZero; }
    virtual wy::Vector3 getEndPoint() const { return wy::Vector3::kZero; }

    virtual wy::Vector3 getPointAt(double t, bool clamp = true) const { return wy::Vector3::kZero; }
    virtual wy::Vector3 getDirectionAt(double t, bool clamp = true) const { return wy::Vector3::kZero; }
    virtual bool isClosed() const { return false; }
    virtual bool isDegenerate(double tol) const { return false; }
    virtual double getLength() const { return 0.0; }

    virtual wydb::ParameterValueUPtr getParameterValue(
        const std::string& className,
        const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(
        const std::string& className,
        const std::string& paramName,
        const wydb::ParameterValue& paramValue) override;
};

NS_WY3D_END

#endif // WY3D_SKETCH_CURVE3D_H