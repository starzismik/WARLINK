// Native bot identities, scoped to a verified WARLINK-owned HMW process.
struct bot_memory {
    HANDLE h{}; DWORD pid{};
    bot_memory(const std::string& id,const rapidjson::Value& p) {
        const auto verified=owned_process(id,p);if(!verified)return;
        pid=GetProcessId(verified);CloseHandle(verified);
        h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_VM_OPERATION,FALSE,pid);
        if(!h)throw std::runtime_error("Accès au serveur refusé pour les identités des bots");
    }
    ~bot_memory(){if(h)CloseHandle(h);}
    template<class T>T read(uintptr_t address)const{T value{};SIZE_T n{};if(!ReadProcessMemory(h,reinterpret_cast<void*>(address),&value,sizeof(value),&n)||n!=sizeof(value))throw std::runtime_error("Version HMW incompatible ou carte en chargement");return value;}
    void write(uintptr_t address,const std::array<char,32>& value)const{SIZE_T n{};if(!WriteProcessMemory(h,reinterpret_cast<void*>(address),value.data(),value.size(),&n)||n!=value.size())throw std::runtime_error("Modification du bot refusée");}
};
struct native_bot {int slot;uintptr_t entity,session,network;std::string name;};
std::vector<native_bot> native_bots(const bot_memory& m) {
    std::vector<native_bot> bots;if(!m.h)return bots;
    const auto clients=m.read<uintptr_t>(0x142DC3390ull);
    const int count=m.read<int>(0x142DC338Cull);
    if(count<2||count>18||clients<0x10000)throw std::runtime_error("Structure serveur HMW non reconnue");
    for(int slot=0;slot<count;slot++) {
        const auto net=clients+slot*1011960ull,ent=0x1471F19E0ull+slot*736ull;
        if(m.read<int>(net)!=5||m.read<int>(net+48)!=0)continue; // CS_ACTIVE and NA_BOT. Never humans.
        if(m.read<uintptr_t>(net+663168)!=ent)throw std::runtime_error("Structure bot non reconnue");
        const auto session=m.read<uintptr_t>(ent+0x158);if(session<0x10000)continue;
        const auto a=m.read<std::array<char,32>>(session+18834),b=m.read<std::array<char,32>>(net+663176);
        if(a!=b||a.back()!=0)continue;
        const std::string name(a.data());if(name.empty())continue;
        bots.push_back({slot,ent,session+18834,net+663176,name});
    }
    return bots;
}
std::string identity_text(const std::string& text,size_t max,bool empty=false) {
    if((text.empty()&&!empty)||text.size()>max||(!text.empty()&&!std::regex_match(text,std::regex("[A-Za-z0-9 _.-]+"))))throw std::runtime_error("Identité invalide : lettres sans accents, chiffres, espaces, point, tiret ou underscore uniquement");
    return text;
}
std::pair<std::string,std::string> split_bot_name(const std::string& value){const auto end=value.find(']');if(value.starts_with("[")&&end!=std::string::npos)return{value.substr(1,end-1),value.substr(end+1)};return{"",value};}
std::map<std::string,std::string> bot_identity_cache;
std::string bot_cache_key(const std::string& id,DWORD pid,int slot){return id+":"+std::to_string(pid)+":"+std::to_string(slot);}
void rename_native_bot(bot_memory& memory,const native_bot& bot,const std::string& name) {
    if(name.empty()||name.size()>31)throw std::runtime_error("L’identité complète dépasse 31 caractères");
    // Revalidate both fields and bot ownership immediately before the write.
    const auto current=native_bots(memory);const auto found=std::find_if(current.begin(),current.end(),[&](const auto& b){return b.slot==bot.slot&&b.name==bot.name&&b.session==bot.session;});
    if(found==current.end())throw std::runtime_error("Le bot a changé, actualise la liste");
    std::array<char,32> next{},old{};std::copy(name.begin(),name.end(),next.begin());std::copy(bot.name.begin(),bot.name.end(),old.begin());
    memory.write(bot.network,next);
    try {memory.write(bot.session,next);}catch(...){memory.write(bot.network,old);throw;}
    if(memory.read<std::array<char,32>>(bot.network)!=next||memory.read<std::array<char,32>>(bot.session)!=next)throw std::runtime_error("Identité non confirmée");
}
std::vector<std::string> identity_list(const rapidjson::Value& source,const char* key,size_t max) {
    std::vector<std::string> result;if(!source.HasMember(key))return result;
    if(!source[key].IsArray()||source[key].Size()>128)throw std::runtime_error("Maximum 128 éléments par liste");
    for(const auto& v:source[key].GetArray()){if(!v.IsString())throw std::runtime_error("Liste invalide");const auto value=identity_text(v.GetString(),max);if(std::find(result.begin(),result.end(),value)==result.end())result.push_back(value);}return result;
}
void sync_bot_identities(const std::string& id,const rapidjson::Value& p,const rapidjson::Value& settings,bool shuffle=false) {
    if(!boolean(settings,"enabled")&&!shuffle)return;
    auto tags=identity_list(settings,"tags",8),names=identity_list(settings,"names",20);
    if(tags.empty()&&names.empty())return;
    bot_memory memory(id,p);if(!memory.h)return;
    const auto bots=native_bots(memory);std::set<std::string> used;
    for(const auto& bot:bots){const auto key=bot_cache_key(id,memory.pid,bot.slot);if(!shuffle&&bot_identity_cache[key]==bot.name)used.insert(bot.name);}
    std::mt19937 rng(std::random_device{}());std::shuffle(tags.begin(),tags.end(),rng);std::shuffle(names.begin(),names.end(),rng);
    size_t index=0;
    for(const auto& bot:bots){const auto key=bot_cache_key(id,memory.pid,bot.slot);if(!shuffle&&bot_identity_cache[key]==bot.name)continue;
        const auto previous=split_bot_name(bot.name);const auto tag=tags.empty()?previous.first:tags[index%tags.size()];const auto base=names.empty()?previous.second.substr(0,20):names[index%names.size()];
        std::string candidate=(tag.empty()?"":"["+tag+"]")+base;const auto original=candidate;int suffix=2;
        while(used.contains(candidate)){const auto ending=" "+std::to_string(suffix++);candidate=original.substr(0,31-ending.size())+ending;}
        rename_native_bot(memory,bot,candidate);bot_identity_cache[key]=candidate;used.insert(candidate);index++;
    }
}
