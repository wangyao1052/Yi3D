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

#ifndef WY3DAPP_KEY_CODE_H
#define WY3DAPP_KEY_CODE_H

#include <osgGA/GUIEventAdapter>

#include "wy3dAppDefs.h"

namespace wy3dApp {

enum KeyCode
{
    Enter        = osgGA::GUIEventAdapter::KEY_Return,
    Escape       = osgGA::GUIEventAdapter::KEY_Escape,
    Space        = osgGA::GUIEventAdapter::KEY_Space,
    Tab          = osgGA::GUIEventAdapter::KEY_Tab,
    Delete       = osgGA::GUIEventAdapter::KEY_Delete,
    Backspace    = osgGA::GUIEventAdapter::KEY_BackSpace,
    Insert       = osgGA::GUIEventAdapter::KEY_Insert,
    Home         = osgGA::GUIEventAdapter::KEY_Home,
    End          = osgGA::GUIEventAdapter::KEY_End,
    PageUp       = osgGA::GUIEventAdapter::KEY_Page_Up,
    PageDown     = osgGA::GUIEventAdapter::KEY_Page_Down,
    Left         = osgGA::GUIEventAdapter::KEY_Left,
    Right        = osgGA::GUIEventAdapter::KEY_Right,
    Up           = osgGA::GUIEventAdapter::KEY_Up,
    Down         = osgGA::GUIEventAdapter::KEY_Down,
    Plus         = osgGA::GUIEventAdapter::KEY_Plus,
    Minus        = osgGA::GUIEventAdapter::KEY_Minus,
    Asterisk     = osgGA::GUIEventAdapter::KEY_Asterisk,
    Slash        = osgGA::GUIEventAdapter::KEY_Slash,
    Comma        = osgGA::GUIEventAdapter::KEY_Comma,
    Period       = osgGA::GUIEventAdapter::KEY_Period,
    Semicolon    = osgGA::GUIEventAdapter::KEY_Semicolon,
    Colon        = osgGA::GUIEventAdapter::KEY_Colon,
    Question     = osgGA::GUIEventAdapter::KEY_Question,
    Exclaim      = osgGA::GUIEventAdapter::KEY_Exclaim,
    Quote        = osgGA::GUIEventAdapter::KEY_Quote,
    Quotedbl     = osgGA::GUIEventAdapter::KEY_Quotedbl,
    Backquote    = osgGA::GUIEventAdapter::KEY_Backquote,
    Backslash    = osgGA::GUIEventAdapter::KEY_Backslash,
    Underscore   = osgGA::GUIEventAdapter::KEY_Underscore,
    Equal        = osgGA::GUIEventAdapter::KEY_Equals,
    At           = osgGA::GUIEventAdapter::KEY_At,
    Hash         = osgGA::GUIEventAdapter::KEY_Hash,
    Dollar       = osgGA::GUIEventAdapter::KEY_Dollar,
    Caret        = osgGA::GUIEventAdapter::KEY_Caret,
    Ampersand    = osgGA::GUIEventAdapter::KEY_Ampersand,
    Less         = osgGA::GUIEventAdapter::KEY_Less,
    Greater      = osgGA::GUIEventAdapter::KEY_Greater,
    ParenLeft    = osgGA::GUIEventAdapter::KEY_Leftparen,
    ParenRight   = osgGA::GUIEventAdapter::KEY_Rightparen,
    BracketLeft  = osgGA::GUIEventAdapter::KEY_Leftbracket,
    BracketRight = osgGA::GUIEventAdapter::KEY_Rightbracket,
    PrintScreen  = osgGA::GUIEventAdapter::KEY_Print,
    ScrollLock   = osgGA::GUIEventAdapter::KEY_Scroll_Lock,

    F1  = osgGA::GUIEventAdapter::KEY_F1,
    F2  = osgGA::GUIEventAdapter::KEY_F2,
    F3  = osgGA::GUIEventAdapter::KEY_F3,
    F4  = osgGA::GUIEventAdapter::KEY_F4,
    F5  = osgGA::GUIEventAdapter::KEY_F5,
    F6  = osgGA::GUIEventAdapter::KEY_F6,
    F7  = osgGA::GUIEventAdapter::KEY_F7,
    F8  = osgGA::GUIEventAdapter::KEY_F8,
    F9  = osgGA::GUIEventAdapter::KEY_F9,
    F10 = osgGA::GUIEventAdapter::KEY_F10,
    F11 = osgGA::GUIEventAdapter::KEY_F11,
    F12 = osgGA::GUIEventAdapter::KEY_F12,

    A = osgGA::GUIEventAdapter::KEY_A,
    B = osgGA::GUIEventAdapter::KEY_B,
    C = osgGA::GUIEventAdapter::KEY_C,
    D = osgGA::GUIEventAdapter::KEY_D,
    E = osgGA::GUIEventAdapter::KEY_E,
    F = osgGA::GUIEventAdapter::KEY_F,
    G = osgGA::GUIEventAdapter::KEY_G,
    H = osgGA::GUIEventAdapter::KEY_H,
    I = osgGA::GUIEventAdapter::KEY_I,
    J = osgGA::GUIEventAdapter::KEY_J,
    K = osgGA::GUIEventAdapter::KEY_K,
    L = osgGA::GUIEventAdapter::KEY_L,
    M = osgGA::GUIEventAdapter::KEY_M,
    N = osgGA::GUIEventAdapter::KEY_N,
    O = osgGA::GUIEventAdapter::KEY_O,
    P = osgGA::GUIEventAdapter::KEY_P,
    Q = osgGA::GUIEventAdapter::KEY_Q,
    R = osgGA::GUIEventAdapter::KEY_R,
    S = osgGA::GUIEventAdapter::KEY_S,
    T = osgGA::GUIEventAdapter::KEY_T,
    U = osgGA::GUIEventAdapter::KEY_U,
    V = osgGA::GUIEventAdapter::KEY_V,
    W = osgGA::GUIEventAdapter::KEY_W,
    X = osgGA::GUIEventAdapter::KEY_X,
    Y = osgGA::GUIEventAdapter::KEY_Y,
    Z = osgGA::GUIEventAdapter::KEY_Z,

    Num0 = osgGA::GUIEventAdapter::KEY_0,
    Num1 = osgGA::GUIEventAdapter::KEY_1,
    Num2 = osgGA::GUIEventAdapter::KEY_2,
    Num3 = osgGA::GUIEventAdapter::KEY_3,
    Num4 = osgGA::GUIEventAdapter::KEY_4,
    Num5 = osgGA::GUIEventAdapter::KEY_5,
    Num6 = osgGA::GUIEventAdapter::KEY_6,
    Num7 = osgGA::GUIEventAdapter::KEY_7,
    Num8 = osgGA::GUIEventAdapter::KEY_8,
    Num9 = osgGA::GUIEventAdapter::KEY_9,

    KP_Enter     = osgGA::GUIEventAdapter::KEY_KP_Enter,
    KP_Delete    = osgGA::GUIEventAdapter::KEY_KP_Delete,
    KP_Insert    = osgGA::GUIEventAdapter::KEY_KP_Insert,
    KP_Home      = osgGA::GUIEventAdapter::KEY_KP_Home,
    KP_End       = osgGA::GUIEventAdapter::KEY_KP_End,
    KP_Page_Up   = osgGA::GUIEventAdapter::KEY_KP_Page_Up,
    KP_Page_Down = osgGA::GUIEventAdapter::KEY_KP_Page_Down,
    KP_Left      = osgGA::GUIEventAdapter::KEY_KP_Left,
    KP_Right     = osgGA::GUIEventAdapter::KEY_KP_Right,
    KP_Up        = osgGA::GUIEventAdapter::KEY_KP_Up,
    KP_Down      = osgGA::GUIEventAdapter::KEY_KP_Down,
    KP_Add       = osgGA::GUIEventAdapter::KEY_KP_Add,
    KP_Subtract  = osgGA::GUIEventAdapter::KEY_KP_Subtract,
    KP_Multiply  = osgGA::GUIEventAdapter::KEY_KP_Multiply,
    KP_Divide    = osgGA::GUIEventAdapter::KEY_KP_Divide,
    KP_Equal     = osgGA::GUIEventAdapter::KEY_KP_Equal,
    KP_Decimal   = osgGA::GUIEventAdapter::KEY_KP_Decimal,

    KP_0 = osgGA::GUIEventAdapter::KEY_KP_0,
    KP_1 = osgGA::GUIEventAdapter::KEY_KP_1,
    KP_2 = osgGA::GUIEventAdapter::KEY_KP_2,
    KP_3 = osgGA::GUIEventAdapter::KEY_KP_3,
    KP_4 = osgGA::GUIEventAdapter::KEY_KP_4,
    KP_5 = osgGA::GUIEventAdapter::KEY_KP_5,
    KP_6 = osgGA::GUIEventAdapter::KEY_KP_6,
    KP_7 = osgGA::GUIEventAdapter::KEY_KP_7,
    KP_8 = osgGA::GUIEventAdapter::KEY_KP_8,
    KP_9 = osgGA::GUIEventAdapter::KEY_KP_9,

    CtrlA = 1,
    CtrlB = 2,
    CtrlC = 3,
    CtrlD = 4,
    CtrlE = 5,
    CtrlF = 6,
    CtrlG = 7,
    CtrlH = 8,
    CtrlI = 9,
    CtrlJ = 10,
    CtrlK = 11,
    CtrlL = 12,
    CtrlM = 13,
    CtrlN = 14,
    CtrlO = 15,
    CtrlP = 16,
    CtrlQ = 17,
    CtrlR = 18,
    CtrlS = 19,
    CtrlT = 20,
    CtrlU = 21,
    CtrlV = 22,
    CtrlW = 23,
    CtrlX = 24,
    CtrlY = 25,
    CtrlZ = 26,
};

enum KeyMod
{
    None  = 0,
    Ctrl  = osgGA::GUIEventAdapter::MODKEY_CTRL,
    Shift = osgGA::GUIEventAdapter::MODKEY_SHIFT,
    Alt   = osgGA::GUIEventAdapter::MODKEY_ALT,
    Num   = osgGA::GUIEventAdapter::MODKEY_NUM_LOCK,
    Caps  = osgGA::GUIEventAdapter::MODKEY_CAPS_LOCK,
    Super = osgGA::GUIEventAdapter::MODKEY_SUPER
};

}

#endif // WY3DAPP_KEY_CODE_H
