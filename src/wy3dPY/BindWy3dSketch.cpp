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

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <wyVector2.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSketchPlane.h>
#include <wy3dSketch.h>
#include <wy3dSketchEntity.h>
#include <wy3dSketchPoint.h>
#include <wy3dSketchCurve.h>
#include <wy3dSketchLine.h>
#include <wy3dSketchCenterLine.h>
#include <wy3dSketchCircle.h>
#include <wy3dSketchArc.h>
#include <wy3dSketchEllipse.h>
#include <wy3dSketchEllipseArc.h>
#include <wy3dSketchSpline.h>
#include <cstdint>

namespace py = pybind11;

class PySketchIterator
{
private:
    wy::Iterator<wydb::ElementId> _iter;

public:
    PySketchIterator(wy::Iterator<wydb::ElementId> iter)
        : _iter(std::move(iter)) {}

    PySketchIterator& __iter__()
    {
        return *this;
    }

    wydb::ElementId __next__()
    {
        if (!_iter.isDone())
        {
            wydb::ElementId id = _iter.current();
            _iter.moveNext();
            return id;
        }
        else
        {
            throw py::stop_iteration();
        }
    }
};

void bindWy3dSketch(py::module_& m)
{
    py::class_<wy3d::SketchPlane>(m, "SketchPlane")
        .def(py::init<>())
        .def(py::init<const wy::Vector3&, const wy::Vector3&, const wy::Vector3&>(),
            py::arg("origin"),
            py::arg("normal"),
            py::arg("xDir"))

        .def_readonly_static("kInvalid", &wy3d::SketchPlane::kInvalid)

        .def("getOrigin", &wy3d::SketchPlane::getOrigin)
        .def("setOrigin", &wy3d::SketchPlane::setOrigin)
        .def("getNormal", &wy3d::SketchPlane::getNormal)
        .def("getXDir", &wy3d::SketchPlane::getXDir)
        .def("getYDir", &wy3d::SketchPlane::getYDir)
        .def("isValid", &wy3d::SketchPlane::isValid)

        .def("value", py::overload_cast<double, double>(&wy3d::SketchPlane::value, py::const_),
            py::arg("u"), py::arg("v"))
        .def("value", py::overload_cast<const wy::Vector2&>(&wy3d::SketchPlane::value, py::const_),
            py::arg("uv"))

        .def("uv", py::overload_cast<const wy::Vector3&>(&wy3d::SketchPlane::uv, py::const_),
            py::arg("pnt"))
        .def("uv", py::overload_cast<double, double, double>(&wy3d::SketchPlane::uv, py::const_),
            py::arg("x"), py::arg("y"), py::arg("z"))

        .def("project", &wy3d::SketchPlane::project)
        .def("distanceTo", &wy3d::SketchPlane::distanceTo)

        .def_static("offset", &wy3d::SketchPlane::offset,
            py::arg("plane"), py::arg("distance"))

        .def("__eq__", [](const wy3d::SketchPlane& lhs, const wy3d::SketchPlane& rhs) {
        return lhs == rhs; })
        .def("__ne__", [](const wy3d::SketchPlane& lhs, const wy3d::SketchPlane& rhs) {
            return lhs != rhs; })

        .def("__repr__", [](const wy3d::SketchPlane& plane) {
            wy::Vector3 origin = plane.getOrigin();
            wy::Vector3 normal = plane.getNormal();
            wy::Vector3 xDir = plane.getXDir();
            // 纯字符串拼接，与Vector3的__repr__风格一致
            return "SketchPlane(origin=Vector3("
                + std::to_string(origin.x()) + ", "
                + std::to_string(origin.y()) + ", "
                + std::to_string(origin.z()) + "), normal=Vector3("
                + std::to_string(normal.x()) + ", "
                + std::to_string(normal.y()) + ", "
                + std::to_string(normal.z()) + "), xDir=Vector3("
                + std::to_string(xDir.x()) + ", "
                + std::to_string(xDir.y()) + ", "
                + std::to_string(xDir.z()) + "))";
        });

    py::class_<PySketchIterator>(m, "PySketchIterator")
        .def("__iter__", &PySketchIterator::__iter__)
        .def("__next__", &PySketchIterator::__next__);

    py::class_<wy3d::Sketch, wydb::Element, std::unique_ptr<wy3d::Sketch, py::nodelete>>(m, "Sketch")
        .def("getPlane", &wy3d::Sketch::getPlane)
        .def("setPlane", &wy3d::Sketch::setPlane)
        .def("addEntity", &wy3d::Sketch::addEntity, py::arg("entity"))
        // A sketch read back from the database comes back as a plain Element, which the
        // feature entry points cannot take; this recovers the Sketch view of it
        .def_static("cast",
            [](wydb::Element* pElement) -> wy3d::Sketch*
            {
                return wy3d::Sketch::cast(pElement);
            },
            py::arg("element"),
            py::return_value_policy::reference)

        // iterator
        .def("__iter__", [](const wy3d::Sketch& sketch) {
            return PySketchIterator(sketch.createIterator());
        })

        .def_static("create",
            [](wydb::Transaction* pTrans, const wy3d::SketchPlane& plane) -> wy3d::Sketch*
            {
                wy3d::Sketch* pOutSketch = nullptr;
                wy::ErrorStatus status = wy3d::Sketch::create(pTrans, plane, pOutSketch);
                return pOutSketch;
            },
            py::arg("transaction"),
            py::arg("plane"),
            py::return_value_policy::reference);

    py::class_<wy3d::SketchEntity, wydb::Element, std::unique_ptr<wy3d::SketchEntity, py::nodelete>>(m, "SketchEntity")
        .def("translate", &wy3d::SketchEntity::translate, py::arg("vector"));

    py::class_<wy3d::SketchPoint, wy3d::SketchEntity, std::unique_ptr<wy3d::SketchPoint, py::nodelete>>(m, "SketchPoint")
        .def("getPosition", &wy3d::SketchPoint::getPosition)
        .def("setPosition", &wy3d::SketchPoint::setPosition)

        .def_static("create",
            [](wydb::Transaction* pTrans, const wy::Vector2& position) -> wy3d::SketchPoint*
            {
                wy3d::SketchPoint* pOutSketchPoint = nullptr;
                wy::ErrorStatus status = wy3d::SketchPoint::create(pTrans, position, pOutSketchPoint);
                return pOutSketchPoint;
            },
            py::arg("transaction"),
            py::arg("position"),
            py::return_value_policy::reference);

    py::class_<wy3d::SketchCurve, wy3d::SketchEntity, std::unique_ptr<wy3d::SketchCurve, py::nodelete>>(m, "SketchCurve")
        .def("isConstruction", &wy3d::SketchCurve::isConstruction)
        .def("setConstruction", &wy3d::SketchCurve::setConstruction)
        .def("getStartPoint", &wy3d::SketchCurve::getStartPoint)
        .def("getEndPoint", &wy3d::SketchCurve::getEndPoint)
        .def("isClosed", &wy3d::SketchCurve::isClosed)
        .def("getLength", &wy3d::SketchCurve::getLength);

    py::class_<wy3d::SketchLine, wy3d::SketchCurve, std::unique_ptr<wy3d::SketchLine, py::nodelete>>(m, "SketchLine")
        .def("setStartPoint", &wy3d::SketchLine::setStartPoint)
        .def("setEndPoint", &wy3d::SketchLine::setEndPoint)

        .def_static("create",
            [](wydb::Transaction* pTrans, const wy::Vector2& startPnt, const wy::Vector2& endPnt) -> wy3d::SketchLine*
            {
                wy3d::SketchLine* pOutSketchLine = nullptr;
                wy::ErrorStatus status = wy3d::SketchLine::create(pTrans, startPnt, endPnt, pOutSketchLine);
                return pOutSketchLine;
            },
            py::arg("transaction"),
            py::arg("startPnt"),
            py::arg("endPnt"),
            py::return_value_policy::reference);

    py::class_<wy3d::SketchCenterLine, wy3d::SketchCurve, std::unique_ptr<wy3d::SketchCenterLine, py::nodelete>>(m, "SketchCenterLine")
        .def("setStartPoint", &wy3d::SketchCenterLine::setStartPoint)
        .def("setEndPoint", &wy3d::SketchCenterLine::setEndPoint)

        .def_static("create",
            [](wydb::Transaction* pTrans, const wy::Vector2& startPnt, const wy::Vector2& endPnt) -> wy3d::SketchCenterLine*
            {
                wy3d::SketchCenterLine* pOutSketchCenterLine = nullptr;
                wy::ErrorStatus status = wy3d::SketchCenterLine::create(pTrans, startPnt, endPnt, pOutSketchCenterLine);
                return pOutSketchCenterLine;
            },
            py::arg("transaction"),
            py::arg("startPnt"),
            py::arg("endPnt"),
            py::return_value_policy::reference);

    py::class_<wy3d::SketchCircle, wy3d::SketchCurve, std::unique_ptr<wy3d::SketchCircle, py::nodelete>>(m, "SketchCircle")
        .def("getCenter", &wy3d::SketchCircle::getCenter)
        .def("setCenter", &wy3d::SketchCircle::setCenter)
        .def("getRadius", &wy3d::SketchCircle::getRadius)
        .def("setRadius", &wy3d::SketchCircle::setRadius)

        .def_static("create",
            [](wydb::Transaction* pTrans, const wy::Vector2& center, double radius) -> wy3d::SketchCircle*
            {
                wy3d::SketchCircle* pOutSketchCircle = nullptr;
                wy::ErrorStatus status = wy3d::SketchCircle::create(pTrans, center, radius, pOutSketchCircle);
                return pOutSketchCircle;
            },
            py::arg("transaction"),
            py::arg("center"),
            py::arg("radius"),
            py::return_value_policy::reference);

    py::class_<wy3d::SketchArc, wy3d::SketchCurve, std::unique_ptr<wy3d::SketchArc, py::nodelete>>(m, "SketchArc")
        .def("getCenter", &wy3d::SketchArc::getCenter)
        .def("setCenter", &wy3d::SketchArc::setCenter)
        .def("getRadius", &wy3d::SketchArc::getRadius)
        .def("setRadius", &wy3d::SketchArc::setRadius)
        .def("getStartAngle", &wy3d::SketchArc::getStartAngle)
        .def("setStartAngle", &wy3d::SketchArc::setStartAngle)
        .def("getEndAngle", &wy3d::SketchArc::getEndAngle)
        .def("setEndAngle", &wy3d::SketchArc::setEndAngle)
        .def("getTotalAngle", &wy3d::SketchArc::getTotalAngle)
        .def("getMiddlePoint", &wy3d::SketchArc::getMiddlePoint)

        .def_static("create",
            [](wydb::Transaction* pTrans, const wy::Vector2& center, double radius, double startAngle, double endAngle) -> wy3d::SketchArc*
            {
                wy3d::SketchArc* pOutSketchArc = nullptr;
                wy::ErrorStatus status = wy3d::SketchArc::create(pTrans, center, radius, startAngle, endAngle, pOutSketchArc);
                return pOutSketchArc;
            },
            py::arg("transaction"),
            py::arg("center"),
            py::arg("radius"),
            py::arg("startAngle"),
            py::arg("endAngle"),
            py::return_value_policy::reference);

    py::class_<wy3d::SketchEllipse, wy3d::SketchCurve, std::unique_ptr<wy3d::SketchEllipse, py::nodelete>>(m, "SketchEllipse")
        .def("getCenter", &wy3d::SketchEllipse::getCenter)
        .def("setCenter", &wy3d::SketchEllipse::setCenter)
        .def("getMajorAxis", &wy3d::SketchEllipse::getMajorAxis)
        .def("setMajorAxis", &wy3d::SketchEllipse::setMajorAxis)
        .def("getMinorAxis", &wy3d::SketchEllipse::getMinorAxis)
        .def("getMajorRadius", &wy3d::SketchEllipse::getMajorRadius)
        .def("setMajorRadius", &wy3d::SketchEllipse::setMajorRadius)
        .def("getMinorRadius", &wy3d::SketchEllipse::getMinorRadius)
        .def("getRadiusRatio", &wy3d::SketchEllipse::getRadiusRatio)
        .def("setRadiusRatio", &wy3d::SketchEllipse::setRadiusRatio)

        .def_static("create",
            [](wydb::Transaction* pTrans, const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio) -> wy3d::SketchEllipse*
            {
                wy3d::SketchEllipse* pOutSketchEllipse = nullptr;
                wy::ErrorStatus status = wy3d::SketchEllipse::create(pTrans, center, majorAxis, radiusRatio, pOutSketchEllipse);
                return pOutSketchEllipse;
            },
            py::arg("transaction"),
            py::arg("center"),
            py::arg("majorAxis"),
            py::arg("radiusRatio"),
            py::return_value_policy::reference);

    py::class_<wy3d::SketchEllipseArc, wy3d::SketchCurve, std::unique_ptr<wy3d::SketchEllipseArc, py::nodelete>>(m, "SketchEllipseArc")
        .def("getCenter", &wy3d::SketchEllipseArc::getCenter)
        .def("setCenter", &wy3d::SketchEllipseArc::setCenter)
        .def("getMajorAxis", &wy3d::SketchEllipseArc::getMajorAxis)
        .def("setMajorAxis", &wy3d::SketchEllipseArc::setMajorAxis)
        .def("getMinorAxis", &wy3d::SketchEllipseArc::getMinorAxis)
        .def("getMajorRadius", &wy3d::SketchEllipseArc::getMajorRadius)
        .def("setMajorRadius", &wy3d::SketchEllipseArc::setMajorRadius)
        .def("getMinorRadius", &wy3d::SketchEllipseArc::getMinorRadius)
        .def("getRadiusRatio", &wy3d::SketchEllipseArc::getRadiusRatio)
        .def("setRadiusRatio", &wy3d::SketchEllipseArc::setRadiusRatio)
        .def("getStartAngle", &wy3d::SketchEllipseArc::getStartAngle)
        .def("setStartAngle", &wy3d::SketchEllipseArc::setStartAngle)
        .def("getEndAngle", &wy3d::SketchEllipseArc::getEndAngle)
        .def("setEndAngle", &wy3d::SketchEllipseArc::setEndAngle)
        .def("getTotalAngle", &wy3d::SketchEllipseArc::getTotalAngle)

        .def_static("create",
            [](wydb::Transaction* pTrans, const wy::Vector2& center, const wy::Vector2& majorAxis, double radiusRatio,
               double startAngle, double endAngle) -> wy3d::SketchEllipseArc*
            {
                wy3d::SketchEllipseArc* pOutSketchEllipseArc = nullptr;
                wy::ErrorStatus status = wy3d::SketchEllipseArc::create(pTrans, center, majorAxis, radiusRatio, startAngle, endAngle, pOutSketchEllipseArc);
                return pOutSketchEllipseArc;
            },
            py::arg("transaction"),
            py::arg("center"),
            py::arg("majorAxis"),
            py::arg("radiusRatio"),
            py::arg("startAngle"),
            py::arg("endAngle"),
            py::return_value_policy::reference);

    py::enum_<wy3d::SplineMode>(m, "SplineMode")
        .value("Undefined", wy3d::SplineMode::Undefined)
        .value("InterpolationPoints", wy3d::SplineMode::InterpolationPoints)
        .value("ControlPoints", wy3d::SplineMode::ControlPoints)
        .def("__repr__", [](wy3d::SplineMode mode) {
            switch (mode) {
            case wy3d::SplineMode::Undefined: return "wy3d.SplineMode.Undefined";
            case wy3d::SplineMode::InterpolationPoints: return "wy3d.SplineMode.InterpolationPoints";
            case wy3d::SplineMode::ControlPoints: return "wy3d.SplineMode.ControlPoints";
            default: return "wy3d.SplineMode.Unknown";
        }});

    py::class_<wy3d::SketchSpline, wy3d::SketchCurve, std::unique_ptr<wy3d::SketchSpline, py::nodelete>>(m, "SketchSpline")
        .def("getMode", &wy3d::SketchSpline::getMode)
        .def("getDegree", &wy3d::SketchSpline::getDegree)
        .def("setDegree", &wy3d::SketchSpline::setDegree)
        .def("getPoints", &wy3d::SketchSpline::getPoints)
        .def("setPoints", &wy3d::SketchSpline::setPoints)

        .def_static("createByFitPoints",
            [](wydb::Transaction* pTrans, const std::vector<wy::Vector2>& fitPoints) -> wy3d::SketchSpline*
            {
                wy3d::SketchSpline* pOutSketchSpline = nullptr;
                wy::ErrorStatus status = wy3d::SketchSpline::create(pTrans, fitPoints, pOutSketchSpline);
                return pOutSketchSpline;
            },
            py::arg("transaction"),
            py::arg("fitPoints"),
            py::return_value_policy::reference)

        .def_static("createByControlPoints",
            [](wydb::Transaction* pTrans, std::uint32_t degree, const std::vector<wy::Vector2>& controlPoints) -> wy3d::SketchSpline*
            {
                wy3d::SketchSpline* pOutSketchSpline = nullptr;
                wy::ErrorStatus status = wy3d::SketchSpline::create(pTrans, degree, controlPoints, pOutSketchSpline);
                return pOutSketchSpline;
            },
            py::arg("transaction"),
            py::arg("degree"),
            py::arg("controlPoints"),
            py::return_value_policy::reference);
}

