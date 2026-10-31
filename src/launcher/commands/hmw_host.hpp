#pragma once
#include <string>
#include <rapidjson/document.h>
namespace cef { class cef_ui; }
namespace hmw_host
{
    rapidjson::Document execute(const rapidjson::Value& request);
    void register_commands(cef::cef_ui& ui);
    int run_tool(const std::string& request_file);
}
