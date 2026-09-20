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

#include "ParamEditorAdapter.h"

#include "../PropertyEditorWidget.h"
#include "../ColorLabel.h"

#include <wy3dColor.h>

#include <typeindex>
#include <cassert>

class ColorLabelAdapter : public ParamEditorAdapter
{
public:
    static const ParamEditorAdapter* instance()
    {
        static ColorLabelAdapter inst;
        return &inst;
    }

    QWidget* create(const std::string& /*className*/,
                    const std::string& /*paramName*/,
                    const wydb::ParameterValue& paramValue,
                    bool isTheSameValue,
                    bool /*readOnly*/,
                    PropertyEditorWidget* parent) const override
    {
        const auto* pAnyVal = dynamic_cast<const wydb::AnyParameterValue*>(&paramValue);
        assert(pAnyVal);
        const wy3d::Color* pColor = pAnyVal->tryGet<wy3d::Color>();
        assert(pColor);

        return new ColorLabel(*pColor, isTheSameValue, parent);
    }
};

// 自注册到 Any<Color>
static struct {
    struct Reg {
        Reg() {
            ParamEditorRegistry::instance().registerForAny(
                std::type_index(typeid(wy3d::Color)),
                ColorLabelAdapter::instance());
        }
    } reg;
} _regColorLabel;
