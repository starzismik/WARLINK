#include "std_include.hpp"
#include "hmw_host.hpp"
#include "cef/cef_ui.hpp"
#include "game_config.hpp"
#include <utils/properties.hpp>
#include <utils/io.hpp>
#include <utils/string.hpp>
#include <utils/cryptography.hpp>
#include <utils/nt.hpp>
#include <utils/flags.hpp>
#include <utils/http.hpp>
#include <regex>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace hmw_host
{
    namespace
    {
        std::recursive_mutex mutex;
        std::string str(const rapidjson::Value& v, const char* k, const std::string& fallback = "")
        {
            return v.IsObject() && v.HasMember(k) && v[k].IsString() ? std::string(v[k].GetString(), v[k].GetStringLength()) : fallback;
        }
        int number(const rapidjson::Value& v, const char* k, int fallback, int low, int high)
        {
            const int n = v.IsObject() && v.HasMember(k) && v[k].IsInt() ? v[k].GetInt() : fallback;
            if (n < low || n > high) throw std::runtime_error(std::string("Valeur invalide : ") + k);
            return n;
        }
        bool boolean(const rapidjson::Value& v, const char* k, bool fallback = false)
        {
            return v.IsObject() && v.HasMember(k) && v[k].IsBool() ? v[k].GetBool() : fallback;
        }
        std::filesystem::path root() { return utils::properties::get_appdata_path() / "servers" / "hmw"; }
        std::filesystem::path profile_dir(const std::string& id)
        {
            if (!std::regex_match(id, std::regex("[a-z0-9_-]{1,48}"))) throw std::runtime_error("Identifiant serveur invalide");
            return root() / id;
        }
        rapidjson::Document read_json(const std::filesystem::path& file)
        {
            rapidjson::Document d; d.SetObject(); std::string data;
            if (utils::io::read_file(file, &data))
            {
                if (d.Parse(data.data(), data.size()).HasParseError() || !d.IsObject())
                    throw std::runtime_error("Configuration JSON invalide : " + file.filename().string());
            }
            return d;
        }
        void write_json(const std::filesystem::path& file, const rapidjson::Value& value)
        {
            rapidjson::StringBuffer b; rapidjson::PrettyWriter<rapidjson::StringBuffer> w(b); value.Accept(w);
            std::filesystem::create_directories(file.parent_path());
            auto temp = file; temp += ".tmp";
            if (!utils::io::write_file(temp, {b.GetString(), b.GetSize()}) ||
                !utils::io::move_file_replace(temp, file)) throw std::runtime_error("Impossible d'enregistrer la configuration");
        }
        std::string safe_text(const std::string& s, size_t limit = 160)
        {
            if (s.size() > limit || s.find_first_of("\r\n\";\\") != std::string::npos || s.find('\0') != std::string::npos)
                throw std::runtime_error("Texte invalide (guillemets, retours de ligne et points-virgules interdits)");
            return s;
        }
        std::string token(const std::string& s)
        {
            if (!std::regex_match(s, std::regex("[a-zA-Z0-9_]{1,64}"))) throw std::runtime_error("Carte ou mode invalide");
            return s;
        }
        std::string secret()
        {
            unsigned char bytes[24]; utils::cryptography::random::get_data(bytes, sizeof(bytes));
            std::string s; for (const auto b : bytes) { s += "0123456789abcdef"[b >> 4]; s += "0123456789abcdef"[b & 15]; } return s;
        }
        void set(rapidjson::Value& d, const char* key, const std::string& s, rapidjson::Document::AllocatorType& a)
        {
            if (d.HasMember(key)) d.RemoveMember(key);
            d.AddMember(rapidjson::Value(key, a), rapidjson::Value(s, a), a);
        }
        void set(rapidjson::Value& d, const char* key, int n, rapidjson::Document::AllocatorType& a)
        {
            if (d.HasMember(key)) d.RemoveMember(key);
            d.AddMember(rapidjson::Value(key, a), rapidjson::Value(n), a);
        }
        std::filesystem::path game_path(const rapidjson::Value& p)
        {
            auto text = str(p, "gamePath");
            if (text.empty())
            {
                if (const auto c = game_config::get_game_config("hmw"))
                    if (const auto install = c->get_install_path()) return *install;
                throw std::runtime_error("Installe HorizonMW ou indique son dossier");
            }
            return utils::string::utf8_to_path(text);
        }
        std::string config_name(const std::string& id) { return "warlink_" + id + ".cfg"; }
        std::filesystem::path server_exe(const rapidjson::Value& p)
        {
            return game_path(p) / ("warlink-hmw-" + str(p,"id") + ".exe");
        }
        int64_t utc_now() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
        std::string moderation_config(const std::string& id)
        {
            const auto bans=read_json(profile_dir(id)/"bans.json");std::string cfg="// WARLINK server exclusions\n";
            for(auto it=bans.MemberBegin();it!=bans.MemberEnd();++it) {
                const std::string guid=it->name.GetString();if(!std::regex_match(guid,std::regex("[a-f0-9]{1,32}")))continue;
                const auto& entry=it->value;if(!entry.IsObject()||!entry.HasMember("expires")||!entry["expires"].IsInt64())continue;
                const auto expiry=entry["expires"].GetInt64();cfg+="set warlink_ban_"+guid+" \""+std::to_string(expiry)+"\"\n";
            }
            return cfg;
        }
        void save_moderation_config(const std::string& id,const rapidjson::Value& p)
        {
            if(!utils::io::write_file(game_path(p)/"main"/("warlink_bans_"+id+".cfg"),moderation_config(id)))throw std::runtime_error("Écriture des exclusions impossible");
        }
        std::string make_config(const rapidjson::Value& p)
        {
            std::string cfg = "// WARLINK HorizonMW server - generated configuration\n";
            const auto line = [&cfg](const std::string& k, const std::string& v) { cfg += "set " + k + " \"" + v + "\"\n"; };
            const auto mode = token(str(p, "mode", "war"));
            const auto access = str(p, "access", "private");
            if(access!="local"&&access!="private"&&access!="public")throw std::runtime_error("Accès invalide");
            if(access=="private"&&str(p,"password").empty())throw std::runtime_error("Définis un mot de passe pour un serveur privé, ou choisis Local/Public");
            const auto players = number(p, "players", 18, 2, 18);
            line("sv_hostname", safe_text(str(p,"name","WARLINK HorizonMW")));
            line("g_password", safe_text(str(p,"password"),64));
            line("rcon_password", safe_text(str(p,"rconPassword"),64));
            line("sv_serverkey", safe_text(str(p,"serverKey"),256));
            line("sv_lanOnly", access == "public" ? "0" : "1");
            line("party_maxplayers",std::to_string(players)); line("sv_maxclients",std::to_string(players));
            line("sv_motd",safe_text(str(p,"motd","Bienvenue sur WARLINK")));
            line("warlink_welcome",safe_text(str(p,"motd","Bienvenue sur WARLINK"),160));
            line("warlink_discord",safe_text(str(p,"discord"),160));
            line("warlink_announcement",safe_text(str(p,"announcement"),160));
            line("warlink_announcement_interval",std::to_string(number(p,"announcementInterval",0,0,3600)));
            line("warlink_welcome_respawn",boolean(p,"welcomeRespawn")?"1":"0");
            line("g_log","hmw-mod/logs/warlink_" + str(p,"id") + ".log"); line("g_logSync","1");
            line("g_gametype",mode);
            line("scr_"+mode+"_scorelimit",std::to_string(number(p,"scoreLimit",75,0,100000)));
            line("scr_"+mode+"_timelimit",std::to_string(number(p,"timeLimit",10,0,1440)));
            line("scr_player_maxhealth",std::to_string(number(p,"health",100,1,1000)));
            line("scr_team_fftype",std::to_string(number(p,"friendlyFire",0,0,3)));
            line("scr_game_hardpoints",boolean(p,"killstreaks",true)?"1":"0");
            line("g_allowVote",boolean(p,"voting",true)?"1":"0");
            line("scr_game_onlyheadshots",boolean(p,"headshots")?"1":"0");
            line("scr_xpscale",std::to_string(number(p,"xpMultiplier",1,1,2)));
            line("scr_hardcore",boolean(p,"hardcore")?"1":"0");
            line("g_hardcore",boolean(p,"hardcore")?"1":"0");
            line("scr_game_forceuav",boolean(p,"constantUav")?"1":"0");
            line("scr_game_disablesprint",boolean(p,"disableSprint")?"1":"0");
            line("scr_thirdPerson",boolean(p,"thirdPerson")?"1":"0");
            line("g_playerCollision",boolean(p,"collision",true)?"1":"0");
            line("scr_player_healthregentime",std::to_string(number(p,"healthRegen",5,0,120)));
            line("scr_"+mode+"_playerrespawndelay",std::to_string(number(p,"respawnDelay",0,0,120)));
            line("scr_"+mode+"_numlives",std::to_string(number(p,"lives",0,0,100)));
            line("scr_"+mode+"_roundlimit",std::to_string(number(p,"roundLimit",1,0,100)));
            line("scr_"+mode+"_winlimit",std::to_string(number(p,"winLimit",1,0,100)));
            const auto position=str(p,"messagePosition","chat");
            if(position!="chat"&&position!="center"&&position!="left")throw std::runtime_error("Position de message invalide");
            line("warlink_message_position",position);
            line("warlink_managed_server","1");
            line("warlink_bot_count",std::to_string(std::min(number(p,"bots",0,0,17),players-1)));
            const auto diff = str(p,"difficulty","regular");
            if (diff!="recruit"&&diff!="regular"&&diff!="hardened"&&diff!="veteran"&&diff!="mixed") throw std::runtime_error("Difficulté invalide");
            line("warlink_bot_difficulty",diff);
            const auto weapons=str(p,"botWeapons","mixed");
            if(weapons!="mixed"&&weapons!="sniper"&&weapons!="knife"&&weapons!="none")throw std::runtime_error("Équipement bots invalide");
            line("warlink_bot_weapons",weapons);
            line("warlink_bot_balance",boolean(p,"botBalance",true)?"1":"0");
            line("warlink_allow_team_change",boolean(p,"allowTeamChange",true)?"1":"0");
            line("scr_teambalance",(boolean(p,"allowTeamChange",true)||boolean(p,"botBalance",true))?"0":"1");
            std::string rotation;
            if (p.HasMember("rotation") && p["rotation"].IsArray())
                for (const auto& map : p["rotation"].GetArray())
                {
                    if (!map.IsString() || rotation.size()>6000) throw std::runtime_error("Rotation invalide");
                    rotation += "gametype " + mode + " map " + token(map.GetString()) + " ";
                }
            if (rotation.empty()) rotation = "gametype " + mode + " map mp_rust";
            line("sv_mapRotation",rotation);
            const auto advanced = str(p,"advanced");
            if (advanced.size()>32000 || advanced.find('\0')!=std::string::npos) throw std::runtime_error("Configuration avancée trop longue");
            cfg += "\n// Advanced settings\n" + advanced + "\n";
            cfg += "exec warlink_bans_" + str(p,"id") + ".cfg\n";
            return cfg;
        }
        std::string udp(int port, const std::string& payload, int timeout = 900, const std::string& address = "127.0.0.1")
        {
            static const auto initialized = [] { WSADATA w; return WSAStartup(MAKEWORD(2,2),&w)==0; }();
            if (!initialized) throw std::runtime_error("Winsock indisponible");
            const SOCKET sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
            if(sock==INVALID_SOCKET) throw std::runtime_error("Socket indisponible");
            sockaddr_in target{}; target.sin_family=AF_INET; target.sin_port=htons(static_cast<u_short>(port)); target.sin_addr.s_addr=inet_addr(address.c_str());
            const std::string packet=std::string(4,'\xff')+payload;
            if(connect(sock,reinterpret_cast<const sockaddr*>(&target),sizeof(target))!=0 || send(sock,packet.data(),static_cast<int>(packet.size()),0)<0)
            { closesocket(sock); throw std::runtime_error("Envoi UDP impossible"); }
            std::string result;
            for(int i=0;i<16;i++)
            {
                fd_set reads; FD_ZERO(&reads); FD_SET(sock,&reads);
                timeval tv{}; tv.tv_sec=timeout/1000; tv.tv_usec=(timeout%1000)*1000;
                if(select(0,&reads,nullptr,nullptr,&tv)<=0) break;
                char b[65536]; const int n=recv(sock,b,sizeof(b),0); if(n<=0) break;
                if(n>=4 && std::string_view(b,4)==std::string_view("\xff\xff\xff\xff",4)) result.append(b+4,n-4);
                timeout=80;
                if(result.size()>128000) break;
            }
            closesocket(sock); return result;
        }
        HANDLE owned_process(const std::string& id, const rapidjson::Value& p)
        {
            const auto state=read_json(profile_dir(id)/"process.json");
            if(!state.HasMember("pid") || !state["pid"].IsInt()) return nullptr;
            const HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE|PROCESS_TERMINATE,FALSE,static_cast<DWORD>(state["pid"].GetInt()));
            if(!process) return nullptr;
            wchar_t name[32768]; DWORD length=32768; FILETIME created{},exit{},kernel{},user{};
            const auto exe=server_exe(p);
            std::error_code path_error;
            const auto close=[&] { CloseHandle(process); return static_cast<HANDLE>(nullptr); };
            if(WaitForSingleObject(process,0)!=WAIT_TIMEOUT || !QueryFullProcessImageNameW(process,0,name,&length) ||
                !std::filesystem::equivalent(std::filesystem::path(name),exe,path_error) || !GetProcessTimes(process,&created,&exit,&kernel,&user)) return close();
            const auto stamp=std::to_string(created.dwHighDateTime)+":"+std::to_string(created.dwLowDateTime);
            if(stamp!=str(state,"created")) return close();
            return process;
        }
        void stage(const rapidjson::Value& p)
        {
            const auto game=game_path(p); const auto id=str(p,"id");
            if(!utils::io::file_exists(game/"hmw-mod.exe")) throw std::runtime_error("hmw-mod.exe introuvable");
            // A distinct process name keeps the existing launcher from mistaking
            // the dedicated server for the playable client.
            std::filesystem::copy_file(game/"hmw-mod.exe",server_exe(p),std::filesystem::copy_options::overwrite_existing);
            const auto source=utils::nt::library{}.get_folder()/"warlink"/"data"/"server-assets"/"warlink_bots.gsc";
            // In non-portable mode assets also live under the active data root.
            std::string script;
            if(!utils::io::read_file(source,&script) && !utils::io::read_file(utils::properties::get_appdata_path()/"data"/"server-assets"/"warlink_bots.gsc",&script))
                throw std::runtime_error("Script de bots WARLINK manquant : extraire toute l'archive");
            std::filesystem::create_directories(game/"main");
            std::filesystem::create_directories(game/"user_scripts"/"mp");
            if(!utils::io::write_file(game/"main"/config_name(id),make_config(p))) throw std::runtime_error("Écriture de la configuration impossible");
            save_moderation_config(id,p);
            const auto script_path=game/"user_scripts"/"mp"/"warlink_bots.gsc";
            std::string old;
            if(utils::io::read_file(script_path,&old) && old!=script)
            {
                const auto backup=profile_dir(id)/("bots-backup-"+std::to_string(GetTickCount64())+".gsc");
                if(!utils::io::write_file(backup,old)) throw std::runtime_error("Sauvegarde du script impossible");
            }
            if(!utils::io::write_file(script_path,script)) throw std::runtime_error("Écriture du script de bots impossible");
        }
        void start(const std::string& id, rapidjson::Document& p)
        {
            if(const auto old=owned_process(id,p)) { CloseHandle(old); throw std::runtime_error("Ce serveur est déjà démarré"); }
            set(p,"gamePath",utils::string::path_to_utf8(game_path(p)),p.GetAllocator());
            if(str(p,"rconPassword").empty())set(p,"rconPassword",secret(),p.GetAllocator());
            write_json(profile_dir(id)/"profile.json",p);
            const int port=number(p,"port",27017,1024,65535);
            // Reserve-check the UDP port, then release immediately before the game binds it.
            static_cast<void>(udp(port,"getinfo warlink",80));
            const SOCKET check=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP); sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(static_cast<u_short>(port)); addr.sin_addr.s_addr=INADDR_ANY;
            if(check==INVALID_SOCKET || bind(check,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0)
            { if(check!=INVALID_SOCKET)closesocket(check); throw std::runtime_error("Ce port UDP est déjà utilisé"); }
            closesocket(check);
            stage(p);
            const auto game=game_path(p); const auto exe=server_exe(p).wstring();
            std::wstring command=L"\""+exe+L"\" \""+game.wstring()+L"\" -dedicated -memoryfix +set net_port "+std::to_wstring(port)+
                L" +set net_ip "+(str(p,"access")=="local"?std::wstring(L"127.0.0.1"):std::wstring(L"0.0.0.0"))+L" +exec "+utils::string::convert(config_name(id))+L" +map_rotate";
            SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
            const HANDLE log=CreateFileW((profile_dir(id)/"console.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(log==INVALID_HANDLE_VALUE) throw std::runtime_error("Journal serveur inaccessible");
            STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;si.hStdOutput=log;si.hStdError=log;si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
            PROCESS_INFORMATION pi{};
            const BOOL ok=CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,game.c_str(),&si,&pi);
            const auto error=GetLastError(); CloseHandle(log);
            if(!ok) throw std::runtime_error("Démarrage impossible, erreur Windows "+std::to_string(error));
            FILETIME created{},exit{},kernel{},user{};GetProcessTimes(pi.hProcess,&created,&exit,&kernel,&user);
            rapidjson::Document state;state.SetObject();auto& a=state.GetAllocator();set(state,"pid",static_cast<int>(pi.dwProcessId),a);set(state,"created",std::to_string(created.dwHighDateTime)+":"+std::to_string(created.dwLowDateTime),a);
            write_json(profile_dir(id)/"process.json",state);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
        }
        void launch_client(const std::filesystem::path& game, const std::string& endpoint, const std::string& pass)
        {
            const auto exe=game/"hmw-mod.exe";
            std::wstring cmd=L"\""+exe.wstring()+L"\" \""+game.wstring()+L"\" -memoryfix +set password \""+utils::string::convert(pass)+L"\" +connect "+utils::string::convert(endpoint);
            const auto logs=utils::properties::get_appdata_path()/"logs";std::filesystem::create_directories(logs);
            SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
            const HANDLE log=CreateFileW((logs/"horizon-client.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            const HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(log==INVALID_HANDLE_VALUE || input==INVALID_HANDLE_VALUE) {
                if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
                throw std::runtime_error("Impossible de préparer le journal HorizonMW");
            }
            STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=log;si.hStdError=log;si.hStdInput=input;PROCESS_INFORMATION pi{};
            const BOOL ok=CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,game.c_str(),&si,&pi);
            CloseHandle(log);CloseHandle(input);
            if(!ok)throw std::runtime_error("Impossible de lancer HorizonMW");
            const auto ended=WaitForSingleObject(pi.hProcess,1500);DWORD code=0;if(ended==WAIT_OBJECT_0)GetExitCodeProcess(pi.hProcess,&code);
            CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
            if(ended==WAIT_OBJECT_0)throw std::runtime_error("HorizonMW s'est fermé au démarrage (code "+std::to_string(code)+"). Journal : "+utils::string::path_to_utf8(logs/"horizon-client.log"));
        }
        void stop(const std::string& id, const rapidjson::Value& p)
        {
            const auto process=owned_process(id,p);if(!process)return;
            udp(number(p,"port",27017,1024,65535),"rcon "+str(p,"rconPassword")+" quit",200);
            if(WaitForSingleObject(process,1800)==WAIT_TIMEOUT)
            {
                if(!TerminateProcess(process,0) || WaitForSingleObject(process,5000)==WAIT_TIMEOUT)
                { CloseHandle(process); throw std::runtime_error("Le serveur ne s'est pas encore arrêté"); }
            }
            CloseHandle(process);
        }
    }

    rapidjson::Document execute(const rapidjson::Value& request)
    {
        std::lock_guard lock(mutex);
        rapidjson::Document out;out.SetObject();auto& a=out.GetAllocator();
        try
        {
            const auto action=str(request,"action","list");
            if(action=="network-info")
            {
                ULONG size=16384;std::vector<unsigned char> buffer(size);
                auto* adapters=reinterpret_cast<IP_ADAPTER_INFO*>(buffer.data());
                auto result=GetAdaptersInfo(adapters,&size);
                if(result==ERROR_BUFFER_OVERFLOW){buffer.resize(size);adapters=reinterpret_cast<IP_ADAPTER_INFO*>(buffer.data());result=GetAdaptersInfo(adapters,&size);}
                rapidjson::Value interfaces(rapidjson::kArrayType);
                if(result==NO_ERROR)for(auto* adapter=adapters;adapter;adapter=adapter->Next){
                    const std::string gateway=adapter->GatewayList.IpAddress.String;
                    if(gateway.empty()||gateway=="0.0.0.0")continue;
                    for(auto* item=&adapter->IpAddressList;item;item=item->Next){const std::string ip=item->IpAddress.String;if(ip=="0.0.0.0"||ip.starts_with("127.")||ip.starts_with("169.254."))continue;rapidjson::Value row(rapidjson::kObjectType);set(row,"ipv4",ip,a);set(row,"gateway",gateway,a);interfaces.PushBack(row,a);}
                }
                out.AddMember("interfaces",interfaces,a);
            }
            else if(action=="community-list")
            {
                const auto result=utils::http::get_data("https://warlink-community-production.up.railway.app/v1/servers",{},{},{},8,0);
                if(!result || result->response_code!=200)throw std::runtime_error("Annuaire WARLINK indisponible");
                rapidjson::Document data;if(data.Parse(result->buffer).HasParseError()||!data.IsObject()||!data.HasMember("servers")||!data["servers"].IsArray())throw std::runtime_error("Réponse annuaire invalide");
                out.AddMember("servers",rapidjson::Value(data["servers"],a),a);
            }
            else if(action=="join-remote")
            {
                auto endpoint=str(request,"endpoint");
                if(!std::regex_match(endpoint,std::regex("[0-9]{1,3}(\\.[0-9]{1,3}){3}:[0-9]{4,5}")))throw std::runtime_error("Adresse serveur invalide");
                const auto split=endpoint.find(':');const auto remote_ip=endpoint.substr(0,split);const int remote_port=std::stoi(endpoint.substr(split+1));
                if(inet_addr(remote_ip.c_str())==INADDR_NONE||remote_port>65535||remote_port<1024)throw std::runtime_error("Adresse serveur invalide");
                // Use loopback only for an endpoint returned by a successful publication
                // of this exact local profile. This avoids dependence on router NAT loopback.
                if(std::filesystem::exists(root()))for(const auto& item:std::filesystem::directory_iterator(root())) {
                    if(!item.is_directory() || !utils::io::file_exists(item.path()/"profile.json"))continue;
                    const auto owner=read_json(item.path()/"directory-owner.json");
                    if(str(owner,"endpoint")!=endpoint)continue;
                    const auto local=read_json(item.path()/"profile.json");
                    if(number(local,"port",27017,1024,65535)!=remote_port)continue;
                    if(const auto process=owned_process(item.path().filename().string(),local)) {
                        CloseHandle(process);endpoint="127.0.0.1:"+std::to_string(remote_port);break;
                    }
                }
                const auto target_ip=endpoint.substr(0,endpoint.find(':'));
                const auto remote_info=udp(remote_port,"getinfo warlink",900,target_ip);
                if(remote_info.find("\\sv_running\\1")==std::string::npos)throw std::runtime_error("Ce serveur ne répond pas ou charge sa carte. Actualise la liste puis réessaie.");
                const auto pass=safe_text(str(request,"password"),64);
                if(utils::nt::is_process_running("hmw-mod.exe"))set(out,"message","HorizonMW est ouvert : saisis dans sa console connect "+endpoint,a);
                else {
                    rapidjson::Document blank;blank.SetObject();const auto game=game_path(blank),exe=game/"hmw-mod.exe";
                    launch_client(game,endpoint,pass);set(out,"message","HorizonMW démarre vers "+endpoint,a);
                }
            }
            else if(action=="reset-view")
            {
                if(utils::nt::is_process_running("hmw-mod.exe"))throw std::runtime_error("Ferme HorizonMW pour appliquer la correction de visée");
                rapidjson::Document blank;blank.SetObject();const auto cfg=game_path(blank)/"players2"/"config_mp.cfg";std::string data;
                if(!utils::io::read_file(cfg,&data))throw std::runtime_error("Configuration joueur introuvable");
                const auto backup=cfg.parent_path()/"config_mp.warlink-before-view.cfg";
                if(!utils::io::file_exists(backup)&&!utils::io::write_file(backup,data))throw std::runtime_error("Sauvegarde de la vue impossible");
                if(data.find("seta cg_fovScale")==std::string::npos)data+="\nseta cg_fovScale \"1\"\n";
                data=std::regex_replace(data,std::regex("seta cg_fovScale \"[^\"]*\""),"seta cg_fovScale \"1\"");
                if(!utils::io::write_file(cfg,data))throw std::runtime_error("Enregistrement de la vue impossible");
                set(out,"message","Échelle de visée remise à 1 ; champ de vision conservé. Sauvegarde créée.",a);
            }
            else if(action=="list")
            {
                rapidjson::Value profiles(rapidjson::kArrayType),maps(rapidjson::kArrayType);
                std::filesystem::create_directories(root());
                bool has_profiles=false;
                for(const auto& item:std::filesystem::directory_iterator(root()))
                    if(item.is_directory()&&utils::io::file_exists(item.path()/"profile.json"))has_profiles=true;
                if(!has_profiles)
                {
                    for(const auto* id:{"entrainement","duel"})
                    {
                        const bool training=std::string(id)=="entrainement";
                        rapidjson::Document p;p.SetObject();auto& pa=p.GetAllocator();
                        set(p,"id",id,pa);set(p,"name",training?"STARZISMIK · Bots":"STARZISMIK · 1v1 privé",pa);
                        set(p,"gamePath","",pa);set(p,"port",training?27017:27018,pa);
                        set(p,"access",training?"local":"private",pa);set(p,"password",training?"":secret().substr(0,10),pa);
                        set(p,"mode",training?"war":"dm",pa);set(p,"players",training?12:2,pa);set(p,"bots",training?9:0,pa);
                        set(p,"difficulty","regular",pa);set(p,"scoreLimit",training?75:20,pa);set(p,"timeLimit",10,pa);
                        rapidjson::Value rotation(rapidjson::kArrayType);rotation.PushBack(rapidjson::Value("mp_rust",pa),pa);
                        if(training)rotation.PushBack(rapidjson::Value("mp_terminal",pa),pa);
                        p.AddMember("rotation",rotation,pa);write_json(profile_dir(id)/"profile.json",p);
                    }
                }
                for(const auto& item:std::filesystem::directory_iterator(root()))
                {
                    if(!item.is_directory() || !utils::io::file_exists(item.path()/"profile.json"))continue;
                    auto p=read_json(item.path()/"profile.json");
                    if(const auto process=owned_process(item.path().filename().string(),p)){p.AddMember("running",true,p.GetAllocator());CloseHandle(process);}else p.AddMember("running",false,p.GetAllocator());
                    const bool running=p["running"].GetBool();
                    const auto info=running?udp(number(p,"port",27017,1024,65535),"getinfo warlink",160):std::string{};
                    p.AddMember("ready",running && info.find("\\sv_running\\1")!=std::string::npos,p.GetAllocator());
                    profiles.PushBack(rapidjson::Value(p,a),a);
                }
                std::set<std::string> names;
                std::string game;
                if(const auto c=game_config::get_game_config("hmw"))if(const auto install=c->get_install_path())
                {
                    game=utils::string::path_to_utf8(*install);std::error_code ec;
                    // A matching load fastfile and main fastfile identify maps, excluding weapon/model fastfiles.
                    for(std::filesystem::directory_iterator it(*install,ec),end;!ec&&it!=end;it.increment(ec)) {
                        const auto filename=it->path().filename().string();
                        if(filename.size()>8 && filename.ends_with("_load.ff")) {
                            const auto name=filename.substr(0,filename.size()-8);
                            if(name.starts_with("mp_") && std::regex_match(name,std::regex("[a-zA-Z0-9_]{1,64}")) && utils::io::file_exists(*install/(name+".ff"))) names.insert(name);
                        }
                    }
                    ec.clear(); const auto custom=*install/"hmw-usermaps";
                    for(std::filesystem::directory_iterator it(custom,ec),end;!ec&&it!=end;it.increment(ec))
                        if(it->is_directory()&&std::regex_match(it->path().filename().string(),std::regex("[a-zA-Z0-9_]{1,64}")))if(utils::io::file_exists(it->path()/(it->path().filename().string()+".ff")))names.insert(it->path().filename().string());
                }
                for(const auto& name:names)maps.PushBack(rapidjson::Value(name,a),a);
                set(out,"gamePath",game,a);out.AddMember("profiles",profiles,a);out.AddMember("maps",maps,a);
            }
            else
            {
                const auto id=str(request,"id");const auto dir=profile_dir(id);
                auto p=read_json(dir/"profile.json");
                if(action=="save")
                {
                    if(const auto process=owned_process(id,p)){CloseHandle(process);throw std::runtime_error("Arrête le serveur avant de modifier sa configuration");}
                    if(!request.HasMember("profile")||!request["profile"].IsObject())throw std::runtime_error("Profil manquant");
                    const auto old_secret=str(p,"rconPassword");
                    p.CopyFrom(request["profile"],p.GetAllocator());
                    if(p.HasMember("running"))p.RemoveMember("running");
                    set(p,"id",id,p.GetAllocator());
                    set(p,"gamePath",utils::string::path_to_utf8(game_path(p)),p.GetAllocator());
                    if(str(p,"rconPassword").empty())set(p,"rconPassword",old_secret.empty()?secret():old_secret,p.GetAllocator());
                    safe_text(str(p,"rconPassword"),64);
                    if(str(p,"rconPassword").find_first_of(" \t")!=std::string::npos)throw std::runtime_error("Le mot de passe RCON ne doit pas contenir d'espace");
                    set(p,"bots",std::min(number(p,"bots",0,0,17),number(p,"players",18,2,18)-1),p.GetAllocator());
                    number(p,"port",27017,1024,65535);make_config(p);
                    std::filesystem::create_directories(dir);write_json(dir/"profile.json",p);
                }
                else
                {
                    if(!p.HasMember("id"))throw std::runtime_error("Profil serveur introuvable");
                    if(action=="publish"||action=="unpublish")
                    {
                        const auto settings=dir/"directory-owner.json";auto owner=read_json(settings);std::string key=utils::string::to_lower(str(owner,"token"));
                        if(key.empty()){key=utils::string::to_lower(utils::cryptography::sha256::compute(secret(),true));set(owner,"token",key,owner.GetAllocator());write_json(settings,owner);}
                        const auto port=number(p,"port",27017,1024,65535);const auto motd=safe_text(str(p,"motd","Bienvenue sur WARLINK"));
                        if(action=="publish"){
                            if(str(p,"access")!="public")throw std::runtime_error("Choisis Communauté WARLINK et redémarre le serveur avant publication");
                            if(udp(port,"getinfo warlink",300).find("\\sv_running\\1")==std::string::npos)throw std::runtime_error("Le serveur n'est pas encore prêt");
                            const auto proof=utils::string::to_lower(utils::cryptography::sha256::compute(key,true));
                            udp(port,"rcon "+str(p,"rconPassword")+" set sv_motd \"[WL:"+proof+"]\"");
                            std::this_thread::sleep_for(std::chrono::milliseconds(1300));
                        }
                        const std::string payload="{\"port\":"+std::to_string(port)+",\"token\":\""+key+"\"}";
                        const auto result=utils::http::get_data("https://warlink-community-production.up.railway.app/v1/"+(action=="publish"?std::string("publish"):std::string("unpublish")),payload,{{"Content-Type","application/json"}},{},12,0);
                        if(action=="publish")udp(port,"rcon "+str(p,"rconPassword")+" set sv_motd \""+motd+"\"");
                        if(!result)throw std::runtime_error("Annuaire WARLINK inaccessible");
                        rapidjson::Document reply;if(reply.Parse(result->buffer).HasParseError()||!reply.IsObject())throw std::runtime_error("Réponse annuaire invalide");
                        if(result->response_code!=200)throw std::runtime_error(str(reply,"error",str(reply,"message","Publication refusée")));
                        if(action=="publish" && reply.HasMember("server") && reply["server"].IsObject()) {
                            set(owner,"endpoint",str(reply["server"],"id"),owner.GetAllocator());write_json(settings,owner);
                        }
                        set(out,"message",action=="publish"?"Serveur publié : TCP et UDP vérifiés depuis Railway. Il restera listé tant qu'il répond.":"Serveur retiré de l'annuaire WARLINK",a);
                    }
                    else if(action=="start")start(id,p);
                    else if(action=="stop")stop(id,p);
                    else if(action=="restart"){stop(id,p);start(id,p);}
                    else if(action=="join")
                    {
                        const auto endpoint="127.0.0.1:"+std::to_string(number(p,"port",27017,1024,65535));
                        set(out,"connect","connect "+endpoint,a);
                        if(utils::nt::is_process_running("hmw-mod.exe"))
                            set(out,"message","HorizonMW est déjà ouvert. Dans sa console, saisis connect "+endpoint+" (et le mot de passe du serveur si demandé).",a);
                        else
                        {
                            const auto game=game_path(p),exe=game/"hmw-mod.exe";
                            const auto pass=safe_text(str(p,"password"),64);
                            launch_client(game,endpoint,pass);
                            set(out,"message","HorizonMW démarre et se connectera au serveur local.",a);
                        }
                    }
                    else if(action=="status")
                    {
                        bool running=false;if(const auto process=owned_process(id,p)){running=true;CloseHandle(process);}
                        const auto access=str(p,"access","private");
                        const auto publication=access=="local" ? "Local : ce serveur est accessible sur ce PC uniquement." : access!="public" ? "Privé : connexion directe ; absent de la liste Internet officielle." : str(p,"serverKey").empty() ? "Clé HMW absente : non référencé officiellement. Pour les amis, utiliser la connexion directe et rediriger ce port en TCP et UDP sur la box." : "Clé HMW renseignée. Le référencement doit encore être accepté par HMW et le port TCP/UDP accessible depuis Internet ; le démarrage local ne le prouve pas.";
                        set(out,"publication",publication,a);
                        out.AddMember("running",running,a);
                        const auto info=udp(number(p,"port",27017,1024,65535),"getinfo warlink",300);
                        out.AddMember("ready",running && info.find("infoResponse")!=std::string::npos && info.find("\\sv_running\\1")!=std::string::npos,a);set(out,"info",info,a);
                        std::ifstream log(dir/"console.log",std::ios::binary);
                        std::string text;if(log){log.seekg(0,std::ios::end);const auto size=log.tellg();log.seekg(size>std::streampos(24000)?size-std::streamoff(24000):std::streampos(0));text.assign(std::istreambuf_iterator<char>(log),{});}
                        const auto password=str(p,"rconPassword");for(size_t pos=0;!password.empty()&&(pos=text.find(password,pos))!=std::string::npos;pos+=10)text.replace(pos,password.size(),"[masqué]");
                        set(out,"log",text,a);
                    }
                    else if(action=="moderation-list" || action=="unban")
                    {
                        auto bans=read_json(dir/"bans.json");
                        if(action=="unban") {
                            const auto guid=utils::string::to_lower(str(request,"guid"));if(!std::regex_match(guid,std::regex("[a-f0-9]{1,32}")))throw std::runtime_error("Identité invalide");
                            if(!bans.HasMember(guid.c_str()))throw std::runtime_error("Exclusion introuvable");
                            bans.RemoveMember(guid.c_str());write_json(dir/"bans.json",bans);save_moderation_config(id,p);
                            if(const auto process=owned_process(id,p)){
                                CloseHandle(process);const auto port=number(p,"port",27017,1024,65535);
                                udp(port,"rcon "+str(p,"rconPassword")+" set warlink_ban_"+guid+" 0");std::this_thread::sleep_for(std::chrono::milliseconds(1100));
                                const auto check=udp(port,"rcon "+str(p,"rconPassword")+" warlink_ban_"+guid);
                                if(check.find("is: \"0\"")==std::string::npos)throw std::runtime_error("Exclusion retirée du fichier, mais révocation en direct non confirmée. Réessaie la commande ou redémarre ce serveur.");
                            }
                            set(out,"message","Exclusion levée sur ce serveur.",a);
                        }
                        rapidjson::Value entries(rapidjson::kArrayType);
                        for(auto it=bans.MemberBegin();it!=bans.MemberEnd();++it) {
                            if(!it->value.IsObject()||!it->value.HasMember("expires")||!it->value["expires"].IsInt64())continue;
                            const auto expiry=it->value["expires"].GetInt64();if(expiry!=-1&&expiry<=utc_now())continue;
                            rapidjson::Value v(it->value,a);set(v,"guid",it->name.GetString(),a);entries.PushBack(v,a);
                        }
                        out.AddMember("bans",entries,a);
                    }
                    else if(action=="moderate")
                    {
                        const auto guid=utils::string::to_lower(str(request,"guid")),kind=str(request,"kind");
                        if(kind=="rename")throw std::runtime_error("Renommage du client non pris en charge par cette version HorizonMW");const auto slot=number(request,"slot",0,0,17);
                        if(!std::regex_match(guid,std::regex("[a-f0-9]{1,32}")) || (kind!="kick"&&kind!="temporary"&&kind!="ban"&&kind!="rename"))throw std::runtime_error("Action de modération invalide");
                        auto reason=safe_text(str(request,"reason","Modération WARLINK"),120);if(reason.empty())reason="Modération WARLINK";
                        if(reason.find('|')!=std::string::npos)throw std::runtime_error("Le caractère | est interdit");
                        const auto port=number(p,"port",27017,1024,65535);
                        const auto status=udp(port,"rcon "+str(p,"rconPassword")+" status");
                        const std::regex row(R"(^\s*([0-9]+)\s+-?[0-9]+\s+No\s+[^\s]+\s+([0-9a-fA-F]+)\s+(.+?)\s+[^\s]+\s+[0-9]+\s*$)");
                        std::istringstream lines(status);std::string line,name;while(std::getline(lines,line)){std::smatch m;if(std::regex_match(line,m,row)&&std::stoi(m[1].str())==slot&&utils::string::to_lower(m[2].str())==guid){name=m[3].str();break;}}
                        if(name.empty())throw std::runtime_error("Ce joueur n'est plus connecté. Actualise la liste.");
                        int64_t expiry=0;
                        if(kind=="temporary"||kind=="ban") {
                            expiry=kind=="ban"?-1:utc_now()+number(request,"seconds",3600,10,31536000);
                            auto bans=read_json(dir/"bans.json");if(bans.MemberCount()>=500&&!bans.HasMember(guid.c_str()))throw std::runtime_error("Maximum 500 exclusions enregistrées");
                            rapidjson::Value entry(rapidjson::kObjectType);auto& ba=bans.GetAllocator();set(entry,"name",name,ba);set(entry,"reason",reason,ba);entry.AddMember("expires",expiry,ba);entry.AddMember("created",utc_now(),ba);
                            if(bans.HasMember(guid.c_str()))bans.RemoveMember(guid.c_str());bans.AddMember(rapidjson::Value(guid.c_str(),ba),entry,ba);write_json(dir/"bans.json",bans);save_moderation_config(id,p);

                        }
                        std::string value=reason;if(kind=="rename"){value=safe_text(str(request,"value"),24);if(value.empty()||value.find('|')!=std::string::npos)throw std::runtime_error("Pseudo invalide");}
                        const auto nonce=secret().substr(0,12);const auto payload=std::to_string(slot)+"|"+guid+"|"+kind+"|"+nonce+"|"+value+"|"+std::to_string(expiry);
                        std::this_thread::sleep_for(std::chrono::milliseconds(1100));udp(port,"rcon "+str(p,"rconPassword")+" set warlink_mod_request \""+payload+"\"");
                        bool applied=false;for(int i=0;i<3&&!applied;i++){std::this_thread::sleep_for(std::chrono::milliseconds(1200));const auto response=udp(port,"rcon "+str(p,"rconPassword")+" warlink_mod_result");applied=response.find(nonce+" ok")!=std::string::npos;if(response.find(nonce+" rejected")!=std::string::npos)break;}
                        if(!applied)throw std::runtime_error("Action non confirmée par le serveur. Les exclusions enregistrées restent conservées ; vérifie le journal et la liste des exclusions.");
                        rapidjson::Value audit(rapidjson::kObjectType);set(audit,"action",kind,a);set(audit,"guid",guid,a);set(audit,"name",name,a);set(audit,"reason",reason,a);audit.AddMember("time",utc_now(),a);rapidjson::StringBuffer buffer;rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);audit.Accept(writer);utils::io::write_file(dir/"moderation-audit.jsonl",std::string(buffer.GetString())+"\n",true);
                        set(out,"message",kind=="kick"?"Joueur expulsé ; il peut revenir.":kind=="temporary"?"Joueur exclu temporairement de ce serveur.":kind=="ban"?"Joueur banni de ce serveur jusqu'à révocation.":"Pseudo appliqué pour la session.",a);
                    }
                    else if(action=="team-players" || action=="team-move")
                    {
                        const auto port=number(p,"port",27017,1024,65535);
                        const auto status=udp(port,"rcon "+str(p,"rconPassword")+" status");
                        rapidjson::Value players(rapidjson::kArrayType);
                        const std::regex row(R"(^\s*([0-9]+)\s+-?[0-9]+\s+No\s+[^\s]+\s+([0-9a-fA-F]+)\s+(.+?)\s+[^\s]+\s+[0-9]+\s*$)");
                        std::istringstream lines(status);std::string line;
                        bool found=false;const auto slot=number(request,"slot",0,0,17);const auto guid=utils::string::to_lower(str(request,"guid"));
                        while(std::getline(lines,line)) {
                            std::smatch m;if(!std::regex_match(line,m,row))continue;
                            const int n=std::stoi(m[1].str());const auto player_guid=utils::string::to_lower(m[2].str());
                            rapidjson::Value v(rapidjson::kObjectType);set(v,"slot",n,a);set(v,"guid",player_guid,a);set(v,"name",m[3].str(),a);players.PushBack(v,a);
                            if(n==slot && player_guid==guid)found=true;
                        }
                        if(action=="team-players"){out.AddMember("players",players,a);std::istringstream all(status);int bots=0;while(std::getline(all,line))if(std::regex_search(line,std::regex(R"(^\s*[0-9]+\s+-?[0-9]+\s+Yes\s)")))bots++;set(out,"bots",bots,a);}
                        else {
                            const auto team=str(request,"team");if(team!="allies"&&team!="axis")throw std::runtime_error("Équipe invalide");
                            if(!found)throw std::runtime_error("Ce joueur n'est plus connecté. Actualise la liste.");
                            const auto nonce=secret().substr(0,12);
                            const auto command="set warlink_team_request \""+std::to_string(slot)+"|"+guid+"|"+team+"|"+nonce+"\"";
                            std::this_thread::sleep_for(std::chrono::milliseconds(1100));udp(port,"rcon "+str(p,"rconPassword")+" "+command);
                            bool applied=false;
                            for(int i=0;i<3 && !applied;i++) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(1200));const auto reply=udp(port,"rcon "+str(p,"rconPassword")+" warlink_team_result");
                                applied=reply.find(nonce+" ok")!=std::string::npos;
                                if(reply.find(nonce+" rejected")!=std::string::npos)break;
                            }
                            if(!applied)throw std::runtime_error("Changement refusé ou non confirmé. Active le choix d'équipe dans un mode par équipes et vérifie que le serveur utilise le nouveau script.");
                            set(out,"message","Équipe changée. Les bots rééquilibrent la partie sans déplacer les humains.",a);
                        }
                    }
                    else if(action=="rcon")
                    {
                        auto command=str(request,"command");
                        if(command.empty()||command.size()>2048||command.find('\0')!=std::string::npos)throw std::runtime_error("Commande invalide");
                        // HMW executes a single command per RCON datagram: a semicolon
                        // in a set command otherwise becomes part of the dvar value.
                        std::vector<std::string> commands;std::string part;bool quoted=false;
                        for(const char c:command)
                        {
                            if(c=='"')quoted=!quoted;
                            if(!quoted&&(c==';'||c=='\n'||c=='\r')){if(!part.empty())commands.push_back(part);part.clear();}
                            else part+=c;
                        }
                        if(!part.empty())commands.push_back(part);
                        if(commands.size()>16)throw std::runtime_error("Maximum 16 commandes à la fois");
                        std::string response;
                        static auto last=std::chrono::steady_clock::time_point{};
                        for(const auto& item:commands)
                        {
                            const auto next=last+std::chrono::milliseconds(1100);
                            if(std::chrono::steady_clock::now()<next)std::this_thread::sleep_until(next);
                            response+=udp(number(p,"port",27017,1024,65535),"rcon "+str(p,"rconPassword")+" "+item);
                            last=std::chrono::steady_clock::now();
                        }
                        set(out,"output",response.empty()?"Aucune réponse RCON (serveur en chargement ou indisponible)":response,a);
                    }
                    else throw std::runtime_error("Action inconnue");
                }
                if(action=="save")out.AddMember("profile",rapidjson::Value(p,a),a);
            }
            out.AddMember("ok",true,a);
        }
        catch(const std::exception& e){out.AddMember("ok",false,a);set(out,"error",e.what(),a);}
        return out;
    }
    void register_commands(cef::cef_ui& ui)
    {
        ui.add_command("hmw-host",[](const rapidjson::Value& request,rapidjson::Document& response){response=execute(request);});
    }
    int run_tool(const std::string& file)
    {
        game_config::import_cb_library();
        const auto request=read_json(utils::string::utf8_to_path(file));const auto result=execute(request);
        write_json(utils::string::utf8_to_path(file+".result.json"),result);
        return result.HasMember("ok")&&result["ok"].GetBool()?0:1;
    }
}
