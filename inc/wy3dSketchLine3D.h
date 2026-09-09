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

#ifndef WY3D_SKETCH_LINE3D_H
#define WY3D_SKETCH_LINE3D_H

#include <wyVector3.h>
#include <wy3dDefs.h>
#include <wy3dSketchCurve3D.h>

NS_WY3D_BEG

class WY3D_EXPORT SketchLine3D : public wy3d::SketchCurve3D
{
    WYDB_DECLARE_MEMBERS(SketchLine3D, wy3d::SketchLine3D, wy3d::SketchCurve3D)

public:
    static wy::ErrorStatus create(wydb::Transaction* pTrans, const wy::Vector3& startPnt, const wy::Vector3& endPnt, SketchLine3D*& pOutSketchLine3D);

    virtual wy::Vector3 getStartPoint() const override { return _startPnt; }
    wy::ErrorStatus setStartPoint(const wy::Vector3& startPnt);

    virtual wy::Vector3 getEndPoint() const override { return _endPnt; }
    wy::ErrorStatus setEndPoint(const wy::Vector3& endPnt);

    virtual wy::Vector3 getPointAt(double t, bool clamp = true) const override;
    virtual wy::Vector3 getDirectionAt(double t, bool clamp = true) const override;
    virtual bool isClosed() const override { return false; }
    virtual bool isDegenerate(double tol) const override;
    virtual double getLength() const override { return (_endPnt - _startPnt).length(); }

public:
    virtual wydb::ParameterValueUPtr getParameterValue(const std::string& className, const std::string& paramName) const override;
    virtual wy::ErrorStatus setParameterValue(const std::string& className, const std::string& paramName, const wydb::ParameterValue& paramValue) override;

protected:
    virtual bool getFieldValue(wydb::FieldId fieldId, std::any& value) override;
    virtual bool setFieldValue(wydb::FieldId fieldId, const std::any& value) override;
    virtual wy::ErrorStatus writeToFiler(wydb::OutFiler& filer) const override;
    virtual wy::ErrorStatus readFromFiler(wydb::InFiler& filer) override;

private:
    wy::Vector3 _startPnt;
    wy::Vector3 _endPnt;
};

NS_WY3D_END

#endif // WY3D_SKETCH_LINE3D_H
