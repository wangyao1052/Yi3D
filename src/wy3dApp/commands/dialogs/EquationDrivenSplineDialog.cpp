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

#include "commands/dialogs/EquationDrivenSplineDialog.h"
#include <QKeyEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <wyVector2.h>
#include <wy3dMath.h>
#include "utils/MessageBoxUtil.h"
#include <wy3dApp/wy3dAppDefs.h>

namespace wy3dApp {

EquationDrivenSplineDialog::EquationDrivenSplineDialog(QWidget* parent)
    : QDialog(parent), _minNum(3), _maxNum(100)
{
    this->setWindowTitle(tr("Equation Driven Spline"));

    QVBoxLayout* layout = new QVBoxLayout(this);

    // 方程式
    QGroupBox* equationGroupBox = new QGroupBox(tr("Equation"), this);
    QGridLayout* equationGridLayout = new QGridLayout(equationGroupBox);
    {
        // x(t)
        QLabel* label = new QLabel(equationGroupBox);
        label->setText(tr("x(t)"));
        equationGridLayout->addWidget(label, 0, 0);
        //
        _xtEdit = new QLineEdit(equationGroupBox);
        _xtEdit->setText("");
        equationGridLayout->addWidget(_xtEdit, 0, 1);
    }
    {
        // y(t)
        QLabel* label = new QLabel(equationGroupBox);
        label->setText(tr("y(t)"));
        equationGridLayout->addWidget(label, 1, 0);
        //
        _ytEdit = new QLineEdit(equationGroupBox);
        _ytEdit->setText("");
        equationGridLayout->addWidget(_ytEdit, 1, 1);
    }
    layout->addWidget(equationGroupBox);

    // 参数
    QGroupBox* paramGroupBox = new QGroupBox(tr("Parameter"), this);
    QGridLayout* paramGridLayout = new QGridLayout(paramGroupBox);
    {
        // t1
        QLabel* label = new QLabel(paramGroupBox);
        label->setText(tr("t1"));
        paramGridLayout->addWidget(label, 0, 0);
        //
        _t1Edit = new QLineEdit(paramGroupBox);
        _t1Edit->setText("");
        paramGridLayout->addWidget(_t1Edit, 0, 1);
    }
    {
        // t2
        QLabel* label = new QLabel(paramGroupBox);
        label->setText(tr("t2"));
        paramGridLayout->addWidget(label, 1, 0);
        //
        _t2Edit = new QLineEdit(paramGroupBox);
        _t2Edit->setText("");
        paramGridLayout->addWidget(_t2Edit, 1, 1);
    }
    layout->addWidget(paramGroupBox);

    // 采样点
    QGroupBox* pointsGroupBox = new QGroupBox(tr("Sample Points"), this);
    QHBoxLayout* pointsLayout = new QHBoxLayout(pointsGroupBox);
    {
        // 插值点数量
        QLabel* label = new QLabel(pointsGroupBox);
        label->setText(tr("Count"));
        pointsLayout->addWidget(label);
        //
        _pointsNumSpinBox = new QSpinBox(pointsGroupBox);
        _pointsNumSpinBox->setRange(_minNum, _maxNum);
        _pointsNumSpinBox->setValue(20);
        _pointsNumSpinBox->setSingleStep(1);
        _pointsNumSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        pointsLayout->addWidget(_pointsNumSpinBox);
        //
        QLabel* labelRange = new QLabel(pointsGroupBox);
        std::stringstream ss;
        ss << "[" << _minNum << ", " << _maxNum << "]";
        labelRange->setText(ss.str().c_str());
        pointsLayout->addWidget(labelRange);
    }
    layout->addWidget(pointsGroupBox);

    // 创建OK和Cancel按钮
    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton(tr("Ok"), this);
    QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    // 信号槽
    this->connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
    this->connect(cancelBtn, SIGNAL(clicked()), this, SLOT(onCancelBtnClicked()));
}

void EquationDrivenSplineDialog::onOkBtnClicked()
{
    if (!this->evalute())
    {
        return;
    }

    this->accept();
}

void EquationDrivenSplineDialog::onCancelBtnClicked()
{
    this->reject();
}

void EquationDrivenSplineDialog::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    {
        return this->onOkBtnClicked(); // Ok
    }
    break;

    case Qt::Key_Escape:
    {
        return this->onCancelBtnClicked(); // Cancel
    }
    break;
    }

    QWidget::keyPressEvent(event);
}

void EquationDrivenSplineDialog::initParser(mu::Parser& parser)
{
    // 定义常量PI
    parser.DefineConst("PI", wy3d::PI);
    parser.DefineConst("pi", wy3d::PI);
    parser.DefineConst("Pi", wy3d::PI);
    parser.DefineConst("pI", wy3d::PI);

    // 添加自然常数e
    parser.DefineConst("e", wy3d::E);
    parser.DefineConst("E", wy3d::E);
}

bool EquationDrivenSplineDialog::evalute()
{
    // 基本校验:文本框内容是否为空
    auto checkEditTextEmpty = [](QLineEdit* pEdit) -> bool
    {
        assert(pEdit);
        if (pEdit->text().isEmpty())
        {
            pEdit->setFocus();
            MessageBoxUtil::showWarning(tr("Expression is empty."));
            return false;
        }
        else
        {
            return true;
        }
    };
    if (!checkEditTextEmpty(_t1Edit)) return false;
    if (!checkEditTextEmpty(_t2Edit)) return false;
    if (!checkEditTextEmpty(_xtEdit)) return false;
    if (!checkEditTextEmpty(_ytEdit)) return false;

    // 采样点数量
    unsigned int numPnts = _pointsNumSpinBox->value();
    if (numPnts < _minNum || numPnts > _maxNum)
    {
        assert(false);
        return false;
    }

    // t1
    double t1(0.0);
    try
    {
        std::string t1Text = _t1Edit->text().toStdString();
        mu::Parser t1Parser;
        this->initParser(t1Parser);
        t1Parser.SetExpr(t1Text);
        t1 = t1Parser.Eval();
    }
    catch (const mu::Parser::exception_type& e)
    {
        _t1Edit->setFocus();
        std::string strError = e.GetMsg();
        MessageBoxUtil::showError(strError.c_str());
        return false;
    }
    catch (...)
    {
        assert(false);
        return false;
    }

    // t2
    double t2(0.0);
    try
    {
        std::string t2Text = _t2Edit->text().toStdString();
        mu::Parser t2Parser;
        this->initParser(t2Parser);
        t2Parser.SetExpr(t2Text);
        t2 = t2Parser.Eval();
    }
    catch (const mu::Parser::exception_type& e)
    {
        _t2Edit->setFocus();
        std::string strError = e.GetMsg();
        MessageBoxUtil::showError(strError.c_str());
        return false;
    }
    catch (...)
    {
        assert(false);
        return false;
    }

    // 校验t1t2
    if (t1 == t2)
    {
        _t1Edit->setFocus();
        MessageBoxUtil::showError(tr("Parameter values cannot be equal."));
        return false;
    }
    if (t1 > t2) std::swap(t1, t2);
    assert(t1 < t2);

    // xt yt
    enum class ParseXY
    {
        Null = 0,
        X = 1,
        Y = 2,
    };
    ParseXY parseXY = ParseXY::Null;
    try
    {
        // xt
        std::string xtText = _xtEdit->text().toStdString();
        mu::Parser xtParser;
        this->initParser(xtParser);
        xtParser.SetExpr(xtText);

        // yt
        std::string ytText = _ytEdit->text().toStdString();
        mu::Parser ytParser;
        this->initParser(ytParser);
        ytParser.SetExpr(ytText);

        // 插值点数量
        _points.clear();
        _points.reserve(numPnts);

        // 生成插值点
        double delta = (t2 - t1) / (numPnts - 1);
        double t(0.0);
        xtParser.DefineVar("t", &t);
        ytParser.DefineVar("t", &t);
        for (unsigned int i = 0; i < numPnts; ++i)
        {
            t = t1 + i * delta;
            if (i == numPnts - 1)
            {
                t = t2;
            }
            parseXY = ParseXY::X;
            double x = xtParser.Eval();
            parseXY = ParseXY::Y;
            double y = ytParser.Eval();
            _points.emplace_back(wy::Vector2(x, y));
        }

        return true;
    }
    catch (const mu::Parser::exception_type& e)
    {
        if (ParseXY::X == parseXY) _xtEdit->setFocus();
        else if (ParseXY::Y == parseXY) _ytEdit->setFocus();
        std::string strError = e.GetMsg();
        MessageBoxUtil::showError(strError.c_str());
        return false;
    }
    catch (...)
    {
        assert(false);
        return false;
    }
}

} // namespace wy3dApp
