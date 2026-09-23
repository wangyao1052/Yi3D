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

#ifndef WY3DAPP_SKETCH_SPLINE_POINTS_EDITOR_H
#define WY3DAPP_SKETCH_SPLINE_POINTS_EDITOR_H

#include <cstddef>
#include <vector>
#include <QWidget>
#include <wydbElement.h>

class QLabel;
class QSpinBox;
class QGridLayout;
class QMouseEvent;
class QToolButton;
class PropertyEditorWidget;
class SketchSplinePointLineEdit;
class SketchSplineTangentCheckBox;

namespace wy3d { class SketchSpline; }

// Collapsible "Points" section of the property panel, SolidWorks style.
// This widget is the header bar itself and spans the panel's label + editor columns; the
// content rows (Index / X / Y / tangency) are appended to that same panel grid, so their
// labels stay in the panel's label column and line up with the parameters above. Clicking
// anywhere on the header expands or collapses.
//
// The row range is getPoints().size(): a closed spline's duplicated last point keeps its own
// index. No add/remove.
//
// A control point spline has no tangency to constrain, so the driving check box and the two
// rows under it are not part of its section at all.
//
// The edits derive from ParamLineEdit and ParamCheckBox, so PropertyEditorWidget's recursive
// findChildren read-only handling reaches them without any wiring here; the check boxes are
// not on the refresh path though, which is why refresh() exists.
class SketchSplinePointsEditor : public QWidget
{
    Q_OBJECT
public:
    SketchSplinePointsEditor(const wydb::ElementId& id, PropertyEditorWidget* pPropertyPanel);

    // Appends this header bar and its content rows to the bottom of pParamsGridLayout. The
    // panel owns the grid, so placing the rows is its call, not a side effect of construction.
    void addToGrid(QGridLayout* pParamsGridLayout);

    // Current point index, 0 based
    std::size_t getCurrPointIndex() const;
    // The spline being edited, nullptr on failure
    const wy3d::SketchSpline* getSplineFromDb() const;

    // Re-reads the current point and re-evaluates what the tangency rows allow
    void refresh();
    // Shows the tangency rows only where the spline has a tangency to constrain, and makes the
    // ones it shows read only where they have no say
    void updateTangentRows();

protected:
    virtual void mousePressEvent(QMouseEvent* pEvent) override;

private:
    // One collapsible row: label in the panel's label column, editor in its editor column
    struct ContentRow
    {
        QWidget* pLabel = nullptr;
        QWidget* pEditor = nullptr;
        // The tangency rows belong to an interpolated spline only
        bool isTangentRow = false;
    };

    void initUi(PropertyEditorWidget* pPropertyPanel);
    void setExpanded(bool isExpanded);
    void applyContentRowsVisible();
    void updateIndexRange();

private slots:
    void onIndexChanged();

private:
    wydb::ElementId _id;
    PropertyEditorWidget* _pPropertyPanel;
    QToolButton* _pArrowButton;
    QSpinBox* _pIndexSpinBox;
    SketchSplinePointLineEdit* _pLineEditX;
    SketchSplinePointLineEdit* _pLineEditY;
    SketchSplinePointLineEdit* _pLineEditTangentAngle;
    SketchSplinePointLineEdit* _pLineEditTangentWeight;
    SketchSplineTangentCheckBox* _pTangentDrivingCheckBox;
    std::vector<ContentRow> _contentRows;
    bool _isExpanded;
    bool _isTangentRowsVisible;
};

#endif // WY3DAPP_SKETCH_SPLINE_POINTS_EDITOR_H
