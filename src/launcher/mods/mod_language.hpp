#pragma once
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
namespace mods::language {
inline const std::vector<std::string> codes{"en","fr","ja","tc","sc","es","ge","it","po","ru","bp","ea"};
inline std::string lower(std::string s) { std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));}); return s; }
inline std::vector<std::filesystem::path> files(const std::filesystem::path& root) {
 namespace fs=std::filesystem; std::vector<fs::path> dirs{root}, out;
 if (fs::is_symlink(fs::symlink_status(root))) throw std::runtime_error("Les liens de dossiers ne peuvent pas etre modifies.");
 for(const auto& e:fs::directory_iterator(root)) if(e.is_directory() && !e.is_symlink()) dirs.push_back(e.path());
 for(const auto& d:dirs) for(const auto& e:fs::directory_iterator(d)) if(e.is_regular_file() && !e.is_symlink()) {
 auto ext=lower(e.path().extension().string()); if(ext==".ff" || ext==".xpak") out.push_back(e.path());
 } return out;
}
inline std::vector<std::string> detect(const std::filesystem::path& root) {
 std::vector<std::string> out; const auto all=files(root);
 for(const auto& c:codes) if(std::any_of(all.begin(),all.end(),[&](const auto& p){return lower(p.filename().string()).starts_with(c+"_");})) out.push_back(c);
 return out;
}
inline size_t change(const std::filesystem::path& root,const std::string& from,const std::string& to) {
 namespace fs=std::filesystem;
 if(from==to || std::find(codes.begin(),codes.end(),from)==codes.end() || std::find(codes.begin(),codes.end(),to)==codes.end()) throw std::runtime_error("Langues invalides.");
 std::vector<std::pair<fs::path,fs::path>> plan;
 for(const auto& p:files(root)) if(lower(p.filename().string()).starts_with(from+"_")) {
 auto target=p.parent_path()/(to+p.filename().string().substr(from.size()));
 if(fs::exists(target)) throw std::runtime_error("Un fichier de la langue cible existe deja : aucun fichier modifie.");
 plan.emplace_back(p,target);
 }
 if(plan.empty()) throw std::runtime_error("Aucun fichier dans la langue source.");
 size_t done=0;
 try { for(const auto& [a,b]:plan) { if(!MoveFileW(a.c_str(),b.c_str())) throw std::runtime_error("Renommage impossible. Fermez le jeu et verifiez les droits du dossier."); ++done; } }
 catch(...) { while(done) { --done; const auto& [a,b]=plan[done]; if(!MoveFileW(b.c_str(),a.c_str())) throw std::runtime_error("Echec du retour arriere : verifiez les fichiers du contenu."); } throw; }
 return plan.size();
}
}
