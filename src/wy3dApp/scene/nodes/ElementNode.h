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

#ifndef WY3DAPP_ELEMENT_NODE_H
#define WY3DAPP_ELEMENT_NODE_H

#include <vector>

#include <osg/Node>
#include <osg/Group>
#include <osg/Polytope>
#include <wyVector3.h>
#include <wydbElementId.h>
#include <wydbElement.h>
#include <wy3dFeature.h>
#include <wy3dSolid.h>
#include <wy3dSketch.h>
#include "ElementNodeType.h"

class Scene;

// 元素节点
// 生命周期由Scene类管理
class ElementNode
{
public:
    static osg::Matrix createMatrix(const wy::Vector3& pos, const wy::Vector3& rot);

protected:
    // 构造析构
    explicit ElementNode(const wydb::ElementId& id);
    virtual ~ElementNode() {}

public:
    virtual ElementNodeType getNodeType() const = 0;

    // 获取顶点数组
    osg::ref_ptr<osg::Vec3Array> getVertices() const
    {
        return _vertices;
    }

public:
    // 面/边信息: 实体节点与片体节点共用一套, 索引均为形体上的拓扑面/边序号
    enum class FaceInfoFlag
    {
        Highlight = 0x00000001,
    };
    struct FaceInfo
    {
        unsigned int numTriangles;
        std::vector<int> edgeIndices; // 序号从0开始
        unsigned int flags;

        inline void addFlag(FaceInfoFlag flag)
        {
            flags |= static_cast<unsigned int>(flag);
        }
        inline void removeFlag(FaceInfoFlag flag)
        {
            flags &= ~static_cast<unsigned int>(flag);
        }
        inline bool hasFlag(FaceInfoFlag flag) const
        {
            return flags & static_cast<unsigned int>(flag);
        }

        FaceInfo() : numTriangles(0), flags(0) {}
    };

    enum class EdgeInfoFlag
    {
        Highlight = 0x00000001,
    };
    struct EdgeInfo
    {
        unsigned int numLines;
        unsigned int flags;

        inline void addFlag(EdgeInfoFlag flag)
        {
            flags |= static_cast<unsigned int>(flag);
        }
        inline void removeFlag(EdgeInfoFlag flag)
        {
            flags &= ~static_cast<unsigned int>(flag);
        }
        inline bool hasFlag(EdgeInfoFlag flag) const
        {
            return flags & static_cast<unsigned int>(flag);
        }

        EdgeInfo() : numLines(0), flags(0) {}
    };

public:
    enum NodeMask
    {
        Visible   = 0x3FFFFFFF, // 可见:最高位为0,由高亮状态控制
        Highlight = 0x80000000, // 最高位为1
        Preview   = 0x40000000, // 次高位为1
    };

    // 状态标志
    enum class Status : unsigned int
    {
        Highlighted = 0x00000001, // 高亮
        Inactive    = 0x00000002, // 非活动状态,布尔体的成员为此状态
        Hidden      = 0x00000004, // 隐藏状态
    };
    // 获取当前状态
    unsigned int getStatus() const
    {
        return _status;
    }
    // 设置状态
    //void setStatus(unsigned int status);

    // 是否高亮
    inline bool isHighlighted() const
    {
        return _status & static_cast<unsigned int>(Status::Highlighted);
    }
    // 高亮
    void highlight(bool flag, bool forced = false);
    // 预览
    void preview(bool flag);

    // 是否激活
    inline bool isActive() const
    {
        return !(_status & static_cast<unsigned int>(Status::Inactive));
    }
    // 激活
    virtual void setActive(bool flag);

    // 是否隐藏
    inline bool isHidden() const
    {
        return _status & static_cast<unsigned int>(Status::Hidden);
    }
    // 隐藏
    virtual void hide(bool flag);

public:
    // 获取元素ID
    const wydb::ElementId& getElementId() const { return _id; }
    // 获取外包围盒
    const osg::BoundingBox& getBoundingBox() const
    {
        return _boundBox;
    }
    // 获取Osg节点
    osg::Group* getOsgNode() const { return _osgNode.get(); }
    // 重新计算NodeMask
    void recomputeNodeMask();

    // 默认框选(完全框住才选中)
    bool pickByNormalBox(osg::Polytope& polytope) const;
    // 交叉框选(任何部分有交集就选中)
    bool pickByCrossBox(osg::Polytope& polytope) const;

protected:
    void addStatus(Status status)
    {
        _status |= static_cast<unsigned int>(status);
    }

    void removeStatus(Status status)
    {
        _status &= ~static_cast<unsigned int>(status);
    }

protected:
    // 默认框选(完全框住才选中)
    virtual bool pickByNormalBoxImpl(osg::Polytope& polytope) const { return false; }

    // 移动
    virtual bool transform(wydb::Database* pDb) = 0;
    // 更新显示效果
    virtual bool updateApperance(wydb::Database* pDb);
     
    // 生成渲染对象
    bool generateRenderObject(Scene* pScene, wydb::Database* pDb, bool isInitial = true);
    virtual void generateRenderObjectImpl(Scene* pScene, const wydb::Element* pElem) {}
    virtual void generateRenderObjectFinished(const wydb::Element* pElem) {}
    // 重生渲染对象
    void reGenerateRenderObject(Scene* pScene, wydb::Database* pDb);

    // 生成渲染数据
    enum class GenRenderDataRet
    {
        Ok = 0,
        Ok_Empty = 1,
        Error = 2
    };
    GenRenderDataRet generateRenderData(Scene* pScene, const wydb::Element* pElement);
    virtual GenRenderDataRet generateRenderDataImpl(Scene* pScene, const wydb::Element* pElement);

protected:
    // 清空渲染对象
    virtual void clearRenderObjects()
    {
        unsigned int numChild = _osgNode->getNumChildren();
        _osgNode->removeChildren(0, numChild);
    }

    // 清空渲染数据
    virtual void clearRenderData()
    {
        if (_vertices) _vertices = nullptr;
    }

    // 初始化渲染数据
    virtual void initRenderData()
    {
        _vertices = new osg::Vec3Array();
    }

protected:
    // 重置包围盒
    void resetBoundingBox() { _boundBox.init(); }
    // 生成包围盒
    osg::BoundingBox computeBoundingBox(const osg::Vec3Array& vertices);
    // 变换包围盒
    void transformBoundingBox(osg::BoundingBox& bbox, const osg::Matrix& matrix);

    virtual void highlightImpl(bool flag) {}
    virtual void previewImpl(bool flag) {}
    virtual void setActiveImpl(bool flag) {}

    // 计算是否Active
    virtual bool computeWhetherActive(const wydb::Element* pCurElem) const { return true; }

private:
    inline void updateVisibleNodeMask()
    {
        // 激活并显示
        if (this->isActive() && !this->isHidden())
        {
            // 显示:控制第1-31位为1                           0111 1111 1111 1111
            _osgNode->setNodeMask(_osgNode->getNodeMask() | NodeMask::Visible);
        }
        else
        {
            // 隐藏:控制第1-31位为0                            1000 0000 0000 0000
            _osgNode->setNodeMask(_osgNode->getNodeMask() & (~NodeMask::Visible));
        }
    }

protected:
    wydb::ElementId _id;
    unsigned int _status;
    // OSG节点
    // 默认的NodeMask是:0111 1111 1111 1111 (0x7FFFFFFF) NodeMask::Default
    // 最高位(32位)代表高亮状态,高亮时为1,普通状态时为0;
    // 其余位:(1-31为)由Inactive和Hidden状态联合控制
    osg::ref_ptr<osg::Group> _osgNode;
    // 包围盒
    osg::BoundingBox _boundBox;
    friend class Scene;

    //---------------------------------
    // 渲染数据
    //---------------------------------
    // 顶点数据
    osg::ref_ptr<osg::Vec3Array> _vertices;
};

#endif // WY3DAPP_ELEMENT_NODE_H
