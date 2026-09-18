///////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2025 WangYao. All rights reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef WY3D_TOPO_NAMING_H
#define WY3D_TOPO_NAMING_H

#include <cassert>
#include <memory>
#include <unordered_map>
#include <vector>

#include <TopoDS_Shape.hxx>
#include <wy3dDefs.h>
#include <wy3dTopoName.h>
#include <utils/wy3dTopoShapeMap.h>

NS_WY3D_BEG

class TopoShapeComparer;

class WY3D_EXPORT TopoNaming
{
public:
    using NameMap = std::unordered_map<TopoDS_Shape, TopoName, ShapeHasher, ShapeEqual>;

    bool isEmpty() const { return _nameMap.empty(); }
    bool contains(const TopoDS_Shape& shape) const;
    const NameMap& getNameMap() const { return _nameMap; }

    virtual void clear() { _nameMap.clear(); }
    bool erase(const TopoDS_Shape& shape);

    void setName(const TopoDS_Shape& shape, const TopoName& name);
    void setName(const TopoDS_Shape& shape, TopoName&& name);

    TopoName getTopoName(const TopoDS_Shape& shape) const;
    bool getName(const TopoDS_Shape& shape, TopoName& name) const;

    TopoDS_Shape smartFind(TopAbs_ShapeEnum shapeType, const TopoName& name) const;
    TopoDS_Shape find(TopAbs_ShapeEnum shapeType, const TopoName& name) const;

    void update(const TopoShapeComparer* pShapeComparer, unsigned int updateId);

    bool print(const std::string& fileFullPath, const TopoDS_Shape& topShape = TopoDS_Shape()) const;
    bool check(const TopoDS_Shape& shape, std::vector<std::string>& info) const;

    bool set(const TopoNaming& rhs, const TopoDS_Shape& shape, const TopoDS_Shape& originalShape);
    bool merge(const TopoNaming& rhs, const TopoDS_Shape& shape, const TopoDS_Shape& originalShape);

private:
    void add(const ShapeDelta& delta, unsigned int updateId, unsigned int recursionLevel = 0);
    void modify(const ShapeDelta& delta);
    void erase(const ShapeDelta& delta);
    void clearModify(const ShapeDelta& delta);

private:
    NameMap _nameMap;
};

typedef std::shared_ptr<TopoNaming> TopoNamingSPtr;

NS_WY3D_END

#endif // WY3D_TOPO_NAMING_H
