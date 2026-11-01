#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <regex>
#include <utils/io.hpp>
#include <utils/properties.hpp>
#include <utils/hash.hpp>
namespace hmw_compatibility {
struct report { std::string language; std::vector<std::string> missing; int checked=0; };
inline const std::map<std::string,std::string> languages{{"english","eng"},{"french","fra"},{"german","ger"},{"spanish","spa"},{"italian","ita"},{"russian","rus"},{"polish","pol"}};
inline bool valid_zone(const std::filesystem::path& p) {
    std::ifstream f(p,std::ios::binary);char magic[8]{};f.read(magic,8);
    std::error_code ec;const auto size=std::filesystem::file_size(p,ec);
    return f.gcount()==8 && std::string(magic,8)=="S1ff0100" && !ec && size>44;
}
inline std::string language(const std::filesystem::path& game) {
    std::string text;utils::io::read_file(game/"players2/default/language",&text);
    text=std::regex_replace(text,std::regex("^\\s+|\\s+$"),"");
    std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(languages.contains(text))return text;
    return "unknown";
}
inline std::filesystem::path locate(const std::filesystem::path& game,const std::string& lang,const std::string& name) {
    for(const auto& base:{game,game/"zone",game/"hmw-mod",game/"hmw-mod/zone"}){
        if(std::filesystem::exists(base/lang/name))return base/lang/name;
        if(std::filesystem::exists(base/name))return base/name;
    }
    return game/lang/name;
}
inline report scan(const std::filesystem::path& game,std::string lang={},bool full=false) {
    static const std::regex manifest_pattern("english/eng_((mp_[a-zA-Z0-9_]+|patch_mp_[a-zA-Z0-9_]+|code_pre_gfx_mp|patch_code_pre_gfx_mp|common_mp|patch_common_mp)\\.ff)");
    static const std::regex local_pattern("eng_(mp_[a-zA-Z0-9_]+|patch_mp_[a-zA-Z0-9_]+|code_pre_gfx_mp|patch_code_pre_gfx_mp|common_mp|patch_common_mp)\\.ff");
    report r;r.language=lang.empty()?language(game):lang;
    if(!languages.contains(r.language)){r.missing.push_back("Langue du jeu non détectée : choisir une langue installée.");return r;}
    const auto prefix=languages.at(r.language);
    std::set<std::string> zones;
    std::map<std::string,std::string> hashes;
    std::string manifest;
    if(utils::io::read_file(utils::properties::get_appdata_path()/"manifest/mwr.json",&manifest)){
        rapidjson::Document d;
        if(!d.Parse(manifest.c_str()).HasParseError()&&d.IsObject()&&d.HasMember("files")&&d["files"].IsArray())for(const auto& row:d["files"].GetArray()){
            if(!row.IsArray()||row.Size()<3||!row[0].IsString()||!row[2].IsString())continue;
            const std::string name=row[0].GetString();
            if(!name.starts_with("english/eng_"))continue;
            std::smatch match;
            if(std::regex_match(name,match,manifest_pattern)){zones.insert(match[1].str());hashes[match[1].str()]=row[2].GetString();}
        }
    }
    for(const auto& base:{game/"english",game/"zone/english",game/"hmw-mod/english",game/"hmw-mod/zone/english"}){
        if(!std::filesystem::is_directory(base))continue;
        for(const auto& e:std::filesystem::directory_iterator(base)){
            const auto n=e.path().filename().string();
            if(e.is_regular_file()&&std::regex_match(n,local_pattern))zones.insert(n.substr(4));
        }
    }
    if(zones.empty()){r.missing.push_back("Catalogue de fichiers anglais introuvable : vérifier les fichiers du jeu.");return r;}
    for(const auto& zone:zones){++r.checked;const auto name=prefix+"_"+zone;const auto file=locate(game,r.language,name);if(!valid_zone(file)||(full&&r.language=="english"&&hashes.contains(zone)&&utils::hash::get_file_hash(file)!=hashes.at(zone)))r.missing.push_back(r.language+"/"+name);}
    return r;
}
inline void require_compatible(const std::filesystem::path& game) {
    const auto r=scan(game);
    if(!r.missing.empty())throw std::runtime_error("HorizonMW : fichiers de langue incomplets ("+r.language+"). "+r.missing.front()+". Ouvre « Vérifier cartes et langues » dans Mes serveurs avant de relancer le jeu.");
}
inline void use_english(const std::filesystem::path& game) {
    if(!scan(game,"english",true).missing.empty())throw std::runtime_error("Les fichiers anglais sont incomplets ou invalides. Utilise Vérifier les fichiers avant de changer de langue.");
    const auto file=game/"players2/default/language",backup=game/"players2/default/language.warlink-before-compatibility";
    std::filesystem::create_directories(file.parent_path());
    if(std::filesystem::exists(file)&&!std::filesystem::exists(backup))std::filesystem::copy_file(file,backup);
    if(!utils::io::write_file(file,"english"))throw std::runtime_error("Impossible de sauvegarder la langue HorizonMW.");
}
}
