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

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <wyVector3.h>
#include <wydbDatabase.h>
#include <wydbTransaction.h>
#include <wy3dSketch3D.h>
#include <wy3dSketchEntity3D.h>
#include <wy3dSketchCurve3D.h>
#include <wy3dSketchLine3D.h>
#include <wy3dSketchCircle3D.h>
#include <cstdint>

namespace py = pybind11;

class PySketch3DIterator
{
private:
    wy::Iterator<wydb::ElementId> _iter;

public:
    PySketch3DIterator(wy::Iterator<wydb::ElementId> iter)
        : _iter(std::move(iter)) {}

    PySketch3DIterator& __iter__()
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

void bindWy3dSketch3D(py::module_& m)
{
    py::class_<PySketch3DIterator>(m, "PySketch3DIterator")
        .def("__iter__", &PySketch3DIterator::__iter__)
        .def("__next__", &PySketch3DIterator::__next__);

    py::class_<wy3d::Sketch3D, wy3d::Feature, std::unique_ptr<wy3d::Sketch3D, py::nodelete>>(m, "Sketch3D")
        .def("getChildren", &wy3d::Sketch3D::getChildren)
        .def("addEntity", &wy3d::Sketch3D::addEntity, py::arg("entity"))

        // iterator
        .def("__iter__", [](const wy3d::Sketch3D& sketch3d) {
            return PySketch3DIterator(sketch3d.createIterator());
        })

        .def_static("create",
            [](wydb::Transaction* pTrans) -> wy3d::Sketch3D*
            {
                wy3d::Sketch3D* pOutSketch3D = nullptr;
                wy::ErrorStatus status = wy3d::Sketch3D::create(pTrans, pOutSketch3D);
                return pOutSketch3D;
            },
            py::arg("transaction"),
            py::return_value_policy::reference);

    py::class_<wy3d::SketchEntity3D, wydb::Element, std::unique_ptr<wy3d::SketchEntity3D, py::nodelete>>(m, "SketchEntity3D");

    py::class_<wy3d::SketchCurve3D, wy3d::SketchEntity3D, std::unique_ptr<wy3d::SketchCurve3D, py::nodelete>>(m, "SketchCurve3D")
        .def("getStartPoint", &wy3d::SketchCurve3D::getStartPoint)
        .def("getEndPoint", &wy3d::SketchCurve3D::getEndPoint)
        .def("getPointAt", &wy3d::SketchCurve3D::getPointAt,
            py::arg("t"), py::arg("clamp") = true)
        .def("getDirectionAt", &wy3d::SketchCurve3D::getDirectionAt,
            py::arg("t"), py::arg("clamp") = true)
        .def("isClosed", &wy3d::SketchCurve3D::isClosed)
        .def("isDegenerate", &wy3d::SketchCurve3D::isDegenerate, py::arg("tol"))
        .def("getLength", &wy3d::SketchCurve3D::getLength);

    py::class_<wy3d::SketchLine3D, wy3d::SketchCurve3D, std::unique_ptr<wy3d::SketchLine3D, py::nodelete>>(m, "SketchLine3D")
        .def("getStartPoint", &wy3d::SketchLine3D::getStartPoint)
        .def("setStartPoint", &wy3d::SketchLine3D::setStartPoint)
        .def("getEndPoint", &wy3d::SketchLine3D::getEndPoint)
        .def("setEndPoint", &wy3d::SketchLine3D::setEndPoint)
        .def("getLength", &wy3d::SketchLine3D::getLength)

        .def_static("create",
            [](wydb::Transaction* pTrans, const wy::Vector3& startPnt, const wy::Vector3& endPnt) -> wy3d::SketchLine3D*
            {
                wy3d::SketchLine3D* pOutSketchLine3D = nullptr;
                wy::ErrorStatus status = wy3d::SketchLine3D::create(pTrans, startPnt, endPnt, pOutSketchLine3D);
                return pOutSketchLine3D;
            },
            py::arg("transaction"),
            py::arg("startPnt"),
            py::arg("endPnt"),
            py::return_value_policy::reference);

    py::class_<wy3d::SketchCircle3D, wy3d::SketchCurve3D, std::unique_ptr<wy3d::SketchCircle3D, py::nodelete>>(m, "SketchCircle3D")
        .def("getCenter", &wy3d::SketchCircle3D::getCenter)
        .def("setCenter", &wy3d::SketchCircle3D::setCenter)
        .def("getNormal", &wy3d::SketchCircle3D::getNormal)
        .def("getXDir", &wy3d::SketchCircle3D::getXDir)
        .def("getRadius", &wy3d::SketchCircle3D::getRadius)
        .def("setRadius", &wy3d::SketchCircle3D::setRadius)

        .def_static("create",
            [](wydb::Transaction* pTrans, const wy::Vector3& center, const wy::Vector3& normal, const wy::Vector3& xDir, double radius) -> wy3d::SketchCircle3D*
            {
                wy3d::SketchCircle3D* pOutSketchCircle3D = nullptr;
                wy::ErrorStatus status = wy3d::SketchCircle3D::create(pTrans, center, normal, xDir, radius, pOutSketchCircle3D);
                return pOutSketchCircle3D;
            },
            py::arg("transaction"),
            py::arg("center"),
            py::arg("normal"),
            py::arg("xDir"),
            py::arg("radius"),
            py::return_value_policy::reference);
}
