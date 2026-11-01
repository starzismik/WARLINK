#include "std_include.hpp"
#include "warlink_product.hpp"
#include "cef/cef_ui.hpp"
#include <utils/http.hpp>
#include <utils/io.hpp>
#include <utils/properties.hpp>
#include <utils/string.hpp>
#include <regex>
#include <utils/flags.hpp>
#include <array>
namespace warlink_product {
namespace {
std::array<int,3> numbers(const std::string& s){std::smatch m;if(!std::regex_match(s,m,std::regex("([0-9]{1,6})[.]([0-9]{1,6})[.]([0-9]{1,6})")))throw std::runtime_error("Version invalide");return {std::stoi(m[1]),std::stoi(m[2]),std::stoi(m[3])};}
std::string str(const rapidjson::Value& d,const char* k){return d.IsObject()&&d.HasMember(k)&&d[k].IsString()?d[k].GetString():"";}
void text(rapidjson::Document& d,const char* k,const std::string& v){d.AddMember(rapidjson::Value(k,d.GetAllocator()),rapidjson::Value(v.c_str(),d.GetAllocator()),d.GetAllocator());}
bool valid(const rapidjson::Document& d){try{numbers(str(d,"version"));const auto url=str(d,"downloadUrl");return url.empty()||url.starts_with("https://github.com/starzismik/WARLINK/releases/download/");}catch(...){return false;}}
}
rapidjson::Document update_status(){
 if(utils::flags::has_flag("preview-required-update")){rapidjson::Document out;out.SetObject();out.AddMember("verified",true,out.GetAllocator());out.AddMember("required",true,out.GetAllocator());out.AddMember("preview",true,out.GetAllocator());text(out,"current",version);text(out,"latest","1.0.1");text(out,"downloadUrl","");text(out,"notes","Améliorations des serveurs WARLINK, des bots et de l’interface.");return out;}

 const auto cache=utils::properties::get_appdata_path()/"user"/"warlink-required-update.json";
 rapidjson::Document manifest;manifest.SetObject();std::string saved;bool verified=false;
 if(utils::io::read_file(cache,&saved)){if(manifest.Parse(saved).HasParseError()||!valid(manifest))manifest.SetObject();}
 const auto response=utils::http::get_data("https://warlink-community-production.up.railway.app/v1/updates",{},{},{},10,0);
 if(response&&response->response_code==200){rapidjson::Document fresh;if(!fresh.Parse(response->buffer).HasParseError()&&valid(fresh)){
  verified=true;
  if(!valid(manifest)||numbers(str(fresh,"version"))>=numbers(str(manifest,"version")))manifest.CopyFrom(fresh,manifest.GetAllocator());
  if(numbers(str(manifest,"version"))>numbers(version)) {rapidjson::StringBuffer b;rapidjson::Writer<rapidjson::StringBuffer> w(b);manifest.Accept(w);utils::io::write_file(cache,b.GetString());}
 }}
 const auto latest=valid(manifest)?str(manifest,"version"):version;
 rapidjson::Document out;out.SetObject();out.AddMember("verified",verified,out.GetAllocator());out.AddMember("required",numbers(latest)>numbers(version),out.GetAllocator());text(out,"current",version);text(out,"latest",latest);text(out,"downloadUrl",str(manifest,"downloadUrl"));text(out,"notes",str(manifest,"notes"));return out;
}
void register_commands(cef::cef_ui& ui){ui.add_command("warlink-update-status",[](const auto&,rapidjson::Document& out){out=update_status();});ui.add_command("warlink-discord-info",[](const auto&,rapidjson::Document& out){
 const auto r=utils::http::get_data("https://discord.com/api/v10/invites/3hrvcWNDPa?with_counts=true",{},{},{},8,0);out.SetObject();if(!r||r->response_code!=200){out.AddMember("ok",false,out.GetAllocator());return;}rapidjson::Document j;if(j.Parse(r->buffer).HasParseError()||!j.IsObject()){out.AddMember("ok",false,out.GetAllocator());return;}auto& a=out.GetAllocator();out.AddMember("ok",true,a);for(const auto* k:{"approximate_member_count","approximate_presence_count"})if(j.HasMember(k)&&j[k].IsNumber())out.AddMember(rapidjson::Value(k,a),rapidjson::Value(j[k],a),a);
});}
}
