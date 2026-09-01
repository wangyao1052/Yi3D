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

#ifndef WY3DAPP_MEASURE_CMD_PANEL_H
#define WY3DAPP_MEASURE_CMD_PANEL_H

#include <QList>
#include <QVector>

#include <wyapSelection.h>

#include "commands/dialogs/FloatingCmdPanel.h"

class QRadioButton;
class QLabel;
class QGridLayout;
class QTableWidget;
class QTableWidgetItem;

enum class MeasureMode
{
    PointToPoint = 0,
    Edge = 1,
    Face = 2,
    Body = 3,
};
constexpr int kMeasureModeCount = 4;

enum class MeasureValueKind
{
    Distance = 0,
    Length = 1,
    Area = 2,
    Perimeter = 3,
    Volume = 4,
    SurfaceArea = 5,
};

// 一条测量值:类别+数值(用于面板表格与累计)
struct MeasureValue
{
    MeasureValueKind kind;
    double value;
};

class MeasureCmdPanel : public FloatingCmdPanel
{
    Q_OBJECT
public:
    explicit MeasureCmdPanel(QWidget* parent = nullptr);
    ~MeasureCmdPanel();

    MeasureMode currentMode() const { return _mode; }
    void setMode(MeasureMode mode);

    virtual QSize sizeHint() const override
    {
        return QSize(360, 500);
    }

    // 追加一行结果,名称(边1/面2/…)由面板按模式自动编号
    void addResult(const wyap::Selection& sel, const QVector<MeasureValue>& values);
    // Removes the result row associated with the selection and updates the total.
    void removeResult(const wyap::Selection& sel);
    bool hasResult(const wyap::Selection& sel) const;
    void clearResults();
    // 视口悬停联动:高亮对应结果行(无匹配则清除;仅管理行底色,不发信号)
    void setHoveredRowBySel(const wyap::Selection& sel);

signals:
    void modeChanged(MeasureMode mode);
    void tabPressed();
    void escapePressed();
    void resultHovered(const wyap::Selection& sel);
    // 行号悬停(行驱动;-1=离开表格):P2P行sel为kNull,命令侧按行号映射到测量线
    void resultRowHovered(int row);
    // 右键菜单"清除":清空所有结果(命令侧同步清高亮)
    void clearRequested();

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onCellEntered(int row, int column);
    void onTableContextMenu(const QPoint& pos);

private:
    void configureTable();
    void updateTotalLabel();
    QString kindName(MeasureValueKind kind) const;
    QString modeName() const;
    void clearRowHover();

    QRadioButton* _pRbP2P;
    QRadioButton* _pRbEdge;
    QRadioButton* _pRbFace;
    QRadioButton* _pRbBody;
    QWidget* _pTotalWidget;
    QGridLayout* _pTotalGrid;
    QTableWidget* _pResultTable;

    MeasureMode _mode;
    QList<wyap::Selection> _rowSels;
    QList<QVector<MeasureValue>> _rowValues;
    unsigned int _rowCounter;
    int _hoverRow;
};

#endif // WY3DAPP_MEASURE_CMD_PANEL_H
