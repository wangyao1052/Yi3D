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

#ifndef WY3DAPP_FEATURE_TREE_MODEL_H
#define WY3DAPP_FEATURE_TREE_MODEL_H

#include <set>
#include <QStandardItemModel>
#include <wydbElementId.h>
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

class FeatureTreeModel : public QStandardItemModel
{
    Q_OBJECT
public:
    FeatureTreeModel(QObject* parent = nullptr);
};

class QFeatureItem : public QStandardItem
{
public:
    QFeatureItem(const wydb::ElementId& id) : _id(id) {}

    const wydb::ElementId& getElementId() const
    {
        return _id;
    }

protected:
    wydb::ElementId _id;
};

class QFeatureNameItem : public QFeatureItem
{
public:
    QFeatureNameItem(const wydb::ElementId& id, const QString& text, unsigned int instIndex)
        : QFeatureItem(id), _instIndex(instIndex)
    {
        this->setText(text);
    }

    unsigned int getInstIndex() const
    {
        return _instIndex;
    }

private:
    unsigned int _instIndex;
};

class QFeatureIdItem : public QFeatureItem
{
public:
    QFeatureIdItem(const wydb::ElementId& id) : QFeatureItem(id)
    {
        this->setText(QString::number(_id.value()));
    }
};

} // namespace wy3dApp

#endif // WY3DAPP_FEATURE_TREE_MODEL_H
