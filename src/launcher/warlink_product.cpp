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
#include <shlobj.h>
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
 rapidjson::Document out;out.SetObject();out.AddMember("verified",verified,out.GetAllocator());out.AddMember("required",numbers(latest)>numbers(version),out.GetAllocator());text(out,"current",version);text(out,"latest",latest);text(out,"downloadUrl",str(manifest,"downloadUrl"));text(out,"notes",str(manifest,"notes"));text(out,"sha256",str(manifest,"sha256"));return out;
}
void register_commands(cef::cef_ui& ui){
 static std::filesystem::path update_run;
 static HANDLE update_process=nullptr;
 ui.add_command("warlink-update-progress",[](const auto&,rapidjson::Document& out){out.SetObject();std::string content;if(!update_run.empty()&&utils::io::read_file(update_run/"status.json",&content)){if(!out.Parse(content).HasParseError()&&out.IsObject())return;}out.SetObject();const bool failed=update_process&&WaitForSingleObject(update_process,0)==WAIT_OBJECT_0;text(out,"phase",failed?"error":"starting");text(out,"message",failed?"L’installateur n’a pas pu démarrer. Réessaie ou vérifie les permissions du dossier.":"Préparation du téléchargement…");});
 ui.add_command("warlink-update-install",[](const auto&,rapidjson::Document& out){out.SetObject();try{
  auto manifest=update_status();if(!manifest["verified"].GetBool()||!manifest["required"].GetBool())throw std::runtime_error("Aucune nouvelle version vérifiée disponible");
  const auto url=str(manifest,"downloadUrl"),hash=str(manifest,"sha256");
  if(!std::regex_match(url,std::regex("https://github[.]com/starzismik/WARLINK/releases/download/[A-Za-z0-9._-]+/WARLINK-[A-Za-z0-9._-]+[.]zip"))||!std::regex_match(hash,std::regex("[a-fA-F0-9]{64}")))throw std::runtime_error("Paquet de mise à jour non valide");
  const auto source=utils::properties::get_appdata_path()/"data"/"update-assets"/"install-update.ps1";
  if(!std::filesystem::exists(source))throw std::runtime_error("Installateur de mise à jour manquant");
  update_run=std::filesystem::temp_directory_path()/("WARLINK-update-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));std::filesystem::create_directories(update_run);
  std::filesystem::copy_file(source,update_run/"install-update.ps1");if(update_process){CloseHandle(update_process);update_process=nullptr;}
  rapidjson::Document request;request.SetObject();text(request,"installDir",utils::string::path_to_utf8(utils::nt::library{}.get_folder()));text(request,"url",url);text(request,"sha256",hash);text(request,"version",str(manifest,"latest"));request.AddMember("processId",static_cast<unsigned>(GetCurrentProcessId()),request.GetAllocator());
  rapidjson::StringBuffer buffer;rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);request.Accept(writer);utils::io::write_file(update_run/"request.json",buffer.GetString());
  wchar_t system_folder[MAX_PATH]{};if(!GetSystemDirectoryW(system_folder,MAX_PATH))throw std::runtime_error("Dossier système inaccessible");const auto shell=std::filesystem::path(system_folder)/"WindowsPowerShell"/"v1.0"/"powershell.exe";
  auto command=L"\""+shell.wstring()+L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""+(update_run/"install-update.ps1").wstring()+L"\" -RequestPath \""+(update_run/"request.json").wstring()+L"\"";
  STARTUPINFOW start{};start.cb=sizeof(start);PROCESS_INFORMATION process{};
  if(!CreateProcessW(shell.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&start,&process))throw std::runtime_error("Impossible de démarrer l’installateur");CloseHandle(process.hThread);update_process=process.hProcess;
  out.AddMember("ok",true,out.GetAllocator());
 }catch(const std::exception& e){out.AddMember("ok",false,out.GetAllocator());text(out,"error",e.what());}});
ui.add_command("warlink-team-logo-save",[](const auto& request,rapidjson::Document& out){out.SetObject();try{const auto role=str(request,"role");if(role!="admin"&&role!="staff"&&role!="vip"&&role!="membre")throw std::runtime_error("Logo invalide");PWSTR folder=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_Downloads,0,nullptr,&folder)))throw std::runtime_error("Dossier Téléchargements inaccessible");const std::filesystem::path downloads(folder);CoTaskMemFree(folder);const auto source=utils::properties::get_appdata_path()/"data"/"launcher-ui"/"assets"/"img"/"team"/(role+"-original.png");auto target=downloads/("WARLINK-"+role+".png");for(int n=2;std::filesystem::exists(target);n++)target=downloads/("WARLINK-"+role+"-"+std::to_string(n)+".png");std::filesystem::copy_file(source,target);const auto args=L"/select,\""+target.wstring()+L"\"";ShellExecuteW(nullptr,L"open",L"explorer.exe",args.c_str(),nullptr,SW_SHOWNORMAL);out.AddMember("ok",true,out.GetAllocator());}catch(const std::exception& e){out.AddMember("ok",false,out.GetAllocator());text(out,"error",e.what());}});ui.add_command("warlink-update-status",[](const auto&,rapidjson::Document& out){out=update_status();});ui.add_command("warlink-discord-info",[](const auto&,rapidjson::Document& out){
 const auto r=utils::http::get_data("https://discord.com/api/v10/invites/eQaeYQgsp7?with_counts=true",{},{},{},8,0);out.SetObject();if(!r||r->response_code!=200){out.AddMember("ok",false,out.GetAllocator());return;}rapidjson::Document j;if(j.Parse(r->buffer).HasParseError()||!j.IsObject()){out.AddMember("ok",false,out.GetAllocator());return;}auto& a=out.GetAllocator();out.AddMember("ok",true,a);for(const auto* k:{"approximate_member_count","approximate_presence_count"})if(j.HasMember(k)&&j[k].IsNumber())out.AddMember(rapidjson::Value(k,a),rapidjson::Value(j[k],a),a);
});}
}
