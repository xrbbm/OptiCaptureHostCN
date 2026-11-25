#pragma once

namespace oc::ui
{
#define OC_UI(id, text) inline constexpr wchar_t id[] = text;
#ifdef OC_UI_ZH_CN
#include <ui_zh_CN.inc>
#else
#include "ui_strings.inc"
#endif
#undef OC_UI
} // namespace oc::ui
