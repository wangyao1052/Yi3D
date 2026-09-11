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

#ifndef WY3D_SKETCH_ELLIPSE3D_H
#define WY3D_SKETCH_ELLIPSE3D_H

#include <wyVector3.h>
#include <wy3dDefs.h>
#include <wy3dSketchCurve3D.h>

NS_WY3D_BEG

// 3D草图椭圆:平面由 center/normal/xDir 定义(xDir 为长轴方向),
// 短轴方向 = normal × xDir,短半轴 = majorRadius * radiusRatio
class WY3D_EXPORT SketchEllipse3D : public wy3d::SketchCurve3D
{
    WYDB_DECLARE_MEMBERS(SketchEllipse3D, wy3d::SketchEllipse3D, wy3d::SketchCurve3D)

public:
    static wy::ErrorStatus create(
        wydb::Transaction* pTrans,
        const wy::Vector3& center,
        const wy::Vector3& normal,
        const wy::Vector3& xDir,
        double majorRadius,
        double radiusRatio,
        SketchEllipse3D*& pOutSketchEllipse3D);

    const wy::Vector3& getCenter() const { return _centerPnt; }
    wy::ErrorStatus setCenter(const wy::Vector3& centerPnt);

    const wy::Vector3& getNormal() const { return _normal; }
    const wy::Vector3& getXDir() const { return _xDir; }
    wy::ErrorStatus setPlane(const wy::Vector3& normal, const wy::Vector3& xDir);

    double getMajorRadius() const { return _majorRadius; }
    wy::ErrorStatus setMajorRadius(double majorRadius);

    double getRadiusRatio() const { return _radiusRatio; }
    wy::ErrorStatus setRadiusRatio(double radiusRatio);
    double getMinorRadius() const { return _majorRadius * _radiusRatio; }
    wy::ErrorStatus setMinorRadius(double minorRadius);

    virtual wy::Vector3 getStartPoint() const override;
    virtual wy::Vector3 getEndPoint() const override;
    virtual wy::Vector3 getPointAt(double t, bool clamp = true) const override;
    virtual wy::Vector3 getDirectionAt(double t, bool clamp = true) const override;
    virtual bool isClosed() const override { return true; }
    virtual bool isDegenerate(double tol) const override;
    virtual double getLength() const override;

public:
    virtual wydb::ParameterValueUPtr getParameterValue(
        const std::string& className,
        const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(
        const std::string& className,
        const std::string& paramName,
        const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;

private:
    wy::ErrorStatus setNormalImpl(const wy::Vector3& normal);
    wy::ErrorStatus setXDirImpl(const wy::Vector3& xDir);
    // 极角 -> 参数角(偏近点角)
    double polarToParametricAngle(double polarAngle) const;

private:
    wy::Vector3 _centerPnt;
    wy::Vector3 _normal;
    wy::Vector3 _xDir;
    double _majorRadius;
    double _radiusRatio;
};

NS_WY3D_END

#endif // WY3D_SKETCH_ELLIPSE3D_H
