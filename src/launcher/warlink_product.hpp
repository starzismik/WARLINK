#pragma once
#include <rapidjson/document.h>
namespace cef {class cef_ui;}
namespace warlink_product { inline constexpr const char* version="1.0.3"; rapidjson::Document update_status(); void register_commands(cef::cef_ui& ui); }
