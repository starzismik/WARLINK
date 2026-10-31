(function () {
    'use strict';
    const esc = value => GameUtils.escapeHtml(String(value == null ? '' : value));
    let overlay, profiles = [], selected = null, gamePath = '', maps = [], timer, polling = false, busy = false;
    const preview = ['127.0.0.1','localhost'].includes(location.hostname) || location.protocol === 'file:';
    const modes = {dm:'Chacun pour soi / 1v1',war:'Match à mort par équipe',dom:'Domination',sd:'Recherche et destruction',conf:'Élimination confirmée',hp:'Point stratégique',koth:'Quartier général',sab:'Sabotage',ctf:'Capture du drapeau',gun:'Jeu d’armes'};
    function field(name, label, type='text', value='', extra='') {
        return `<label class="hmwh-field"><span>${label}</span><input name="${name}" type="${type}" value="${esc(value)}" ${extra}></label>`;
    }
    async function api(action, data={}) {
        if (preview) {
            if(action==='list')return {ok:true,profiles,gamePath:'D:\\Jeux\\HorizonMW',maps:['mp_rust','mp_shipment','mp_terminal','mp_highrise','mp_crash']};
            if(action==='save'){profiles=profiles.filter(p=>p.id!==data.id);profiles.push({...data.profile,id:data.id});return{ok:true};}
            if(action==='status')return{ok:true,running:false,ready:false,log:'Aperçu navigateur. Les journaux réels apparaissent dans WARLINK.exe.'};
            throw Error('Aperçu navigateur : le démarrage et la console sont disponibles dans WARLINK.exe.');
        }
        const result = await window.executeCommand('hmw-host',{action,...data});
        if(!result || !result.ok)throw Error(result?.error || 'Le gestionnaire serveur ne répond pas.');
        return result;
    }
    function message(text, error=false){if(!overlay)return;const el=overlay.querySelector('.hmwh-message');el.textContent=text;el.classList.toggle('error',error);}
    function current(){return profiles.find(p=>p.id===selected);}
    function allocation(players, bots, humans, changed) {
        const integer = (v, min, max, fallback) => Math.max(min, Math.min(max, Number.isFinite(Number(v)) ? Math.round(Number(v)) : fallback));
        players=integer(players,2,18,12); bots=integer(bots,0,17,0);
        if(changed==='humans'){humans=integer(humans,1,18,1);bots=Math.min(bots,18-humans);players=Math.max(2,humans+bots);}
        else if(changed==='bots')players=Math.max(players,bots+1);
        else bots=Math.min(bots,players-1);
        return {players,bots,humans:players-bots};
    }
    function syncAllocation(changed) {
        const f=overlay?.querySelector('.hmwh-form'); if(!f)return;
        const n=allocation(f.elements.players.value,f.elements.bots.value,f.elements.humans.value,changed);
        for(const key of ['players','bots','humans'])f.elements[key].value=n[key];
        f.elements.difficulty.disabled=n.bots===0;
        overlay.querySelector('.hmwh-allocation').textContent=n.bots ? n.players+' places : '+n.humans+' pour les humains et '+n.bots+' bots maximum.' : n.players+' places humaines · aucun bot.';
    }
    function values() {
        syncAllocation();
        const f=overlay.querySelector('form'),data=new FormData(f),p={};
        for(const [k,v] of data)p[k]=v;
        for(const k of ['port','players','bots','scoreLimit','timeLimit','health','friendlyFire','announcementInterval','healthRegen','respawnDelay','lives','roundLimit','winLimit'])p[k]=Number(p[k]);
        for(const k of ['killstreaks','voting','headshots','welcomeRespawn','hardcore','constantUav','disableSprint','thirdPerson','collision'])p[k]=f.elements[k].checked;
        if(!p.difficulty)p.difficulty=f.elements.difficulty.value;
        p.rotation=p.rotation.split(/\s+/).filter(Boolean);p.id=selected;return p;
    }
    function renderList(){
        overlay.querySelector('.hmwh-profiles').innerHTML=profiles.map(p=>`<button type="button" data-profile="${esc(p.id)}" class="${p.id===selected?'active':''}"><strong>${window.WarlinkName.html(p.name)}</strong><small>UDP ${p.port} · ${esc(p.access==='public'?'Public':p.access==='local'?'Local':'Privé')}</small></button>`).join('')||'<p class="hmwh-muted">Crée ton premier serveur pour commencer.</p>';
    }
    function renderForm(profile){
        const p={name:'STARZISMIK · HorizonMW',gamePath,port:27017,access:'private',password:'',serverKey:'',rconPassword:'',players:12,bots:0,difficulty:'regular',mode:'war',rotation:['mp_rust','mp_terminal'],scoreLimit:75,timeLimit:10,health:100,friendlyFire:0,killstreaks:true,voting:true,headshots:false,motd:'Bienvenue sur WARLINK',messagePosition:'chat',healthRegen:5,respawnDelay:0,lives:0,roundLimit:1,winLimit:1,collision:true,advanced:'',...profile};
        if(!p.gamePath)p.gamePath=gamePath;
        overlay.querySelector('.hmwh-editor').innerHTML=`
            <form class="hmwh-form">
                <div class="hmwh-section-head"><h3>Configuration</h3><select name="preset" aria-label="Préréglage"><option value="">Appliquer un préréglage…</option value="duel">1v1 privé</option><option value="training">Entraînement avec bots</option><option value="public">Serveur public</option></select></div>
                <div class="hmwh-grid">
                    <div><input name="name" type="hidden" value="${esc(p.name)}"><div class="hmwh-name-editor"></div></div>
                    <label class="hmwh-field"><span>Accès</span><select name="access"><option value="local">Local · ce PC uniquement</option><option value="private">Privé · amis / réseau</option><option value="public">Public · Internet</option></select></label>
                    ${field('gamePath','Dossier HorizonMW (hmw-mod.exe)','text',p.gamePath,'required')}
                    ${field('port','Port du serveur · TCP et UDP','number',p.port,'min="1024" max="65535" required')}
                    ${field('password','Mot de passe pour rejoindre (privé)','text',p.password,'maxlength="64" autocomplete="off"')}
                    ${field('players','Places, humains et bots compris','number',p.players,'min="2" max="18" required')}
                    <label class="hmwh-field"><span>Mode de jeu</span><select name="mode">${Object.entries(modes).map(([id,name])=>`<option value="${id}">${name}</option>`).join('')}</select></label>
                    ${field('motd','Message de bienvenue en jeu','text',p.motd,'maxlength="160"')}
                    ${field('discord','Lien Discord affiché en jeu','text',p.discord||'','maxlength="160" placeholder="https://discord.gg/..."')}
                    ${field('announcement','Annonce périodique en jeu','text',p.announcement||'','maxlength="160"')}
                    ${field('announcementInterval','Annonces en secondes (0 = désactivé, minimum 10 s)','number',p.announcementInterval||0,'min="0" max="3600"')}
                    <label><input name="welcomeRespawn" type="checkbox" ${p.welcomeRespawn?'checked':''}> Réafficher bienvenue et Discord à chaque réapparition</label>
                </div>
                <h3>Affichage des messages</h3><label class="hmwh-field"><span>Position bienvenue, Discord et annonces</span><select name="messagePosition"><option value="chat">Bas à gauche · messages du jeu</option><option value="center">Centre · grand message</option><option value="left">Gauche · bandeau HUD</option></select></label>
                <h3>Cartes & rotation</h3><div class="hmwh-grid">
                    <label class="hmwh-field"><span>Ajouter une carte</span><select name="mapPick"><option value="">Choisir…</option>${maps.map(m=>`<option value="${esc(m)}">${esc(m)}</option>`).join('')}</select></label>
                    <label class="hmwh-field"><span>Rotation · un identifiant de carte par ligne</span><textarea name="rotation" rows="4" required>${esc(p.rotation.join('\n'))}</textarea></label>
                </div>
                <h3>Bots</h3><div class="hmwh-grid">
                    ${field('humans','Places réservées aux humains','number',Math.max(1,p.players-p.bots),'min="1" max="18" required')}
                    ${field('bots','Nombre de bots (0 = sans bots)','number',p.bots,'min="0" max="17" required')}
                    <label class="hmwh-field"><span>Difficulté</span><select name="difficulty"><option value="recruit">Recrue</option><option value="regular">Normal</option><option value="hardened">Vétéran</option><option value="veteran">Expert</option><option value="mixed">Mélangée</option></select></label>
                </div><p class="hmwh-muted hmwh-allocation" aria-live="polite"></p><p class="hmwh-muted">Les bots libèrent des places quand des humains rejoignent. Certaines cartes personnalisées n’ont pas de navigation pour les bots. XP normale ×1 ; la persistance du rang dépend du jeu.</p>
                <h3>Règles de partie</h3><div class="hmwh-grid">
                    ${field('scoreLimit','Limite de score (0 = illimité)','number',p.scoreLimit,'min="0" max="100000"')}
                    ${field('timeLimit','Durée en minutes (0 = illimité)','number',p.timeLimit,'min="0" max="1440"')}
                    ${field('health','Santé des joueurs','number',p.health,'min="1" max="1000"')}
                    <label class="hmwh-field"><span>Tir allié</span><select name="friendlyFire"><option value="0">Désactivé</option><option value="1">Activé</option><option value="2">Renvoi des dégâts</option><option value="3">Partagé</option></select></label>
                </div><div class="hmwh-checks">${[['killstreaks','Séries d’éliminations'],['voting','Votes'],['headshots','Tirs à la tête uniquement']].map(([id,label])=>`<label><input type="checkbox" name="${id}" ${p[id]?'checked':''}> ${label}</label>`).join('')}</div>
                <details><summary>Règles supplémentaires</summary><div class="hmwh-grid">
                    ${field('healthRegen','Régénération santé (secondes)','number',p.healthRegen,'min="0" max="120"')}
                    ${field('respawnDelay','Délai de réapparition (secondes)','number',p.respawnDelay,'min="0" max="120"')}
                    ${field('lives','Vies par joueur (0 = illimité)','number',p.lives,'min="0" max="100"')}
                    ${field('roundLimit','Limite de manches (0 = illimité)','number',p.roundLimit,'min="0" max="100"')}
                    ${field('winLimit','Manches à gagner','number',p.winLimit,'min="0" max="100"')}
                </div><div class="hmwh-checks">${[['hardcore','Hardcore'],['constantUav','Radar permanent'],['disableSprint','Désactiver le sprint'],['thirdPerson','Troisième personne'],['collision','Collision entre joueurs']].map(([id,label])=>`<label><input type="checkbox" name="${id}" ${p[id]?'checked':''}> ${label}</label>`).join('')}</div><p class="hmwh-muted">Les règles disponibles peuvent varier selon la carte et le mode. La section CFG permet les commandes supplémentaires du moteur.</p></details>
                <details><summary>Administration & paramètres avancés</summary><div class="hmwh-grid">
                    ${field('rconPassword','Mot de passe RCON (généré si vide)','password',p.rconPassword,'maxlength="64" autocomplete="off"')}
                    ${field('serverKey','Clé officielle de référencement (si fournie par HMW)','password',p.serverKey,'maxlength="256" autocomplete="off"')}
                </div><label class="hmwh-field"><span>Configuration CFG supplémentaire · appliquée au démarrage après les réglages ci-dessus</span><textarea name="advanced" rows="6" spellcheck="false">${esc(p.advanced)}</textarea></label><p class="hmwh-muted">Les commandes dépendent de la version d’HorizonMW. Un changement de configuration nécessite un redémarrage.</p></details>
                <button class="hmwh-primary" type="submit">Enregistrer le serveur</button>
            </form>`;
        const form=overlay.querySelector('form');
        window.WarlinkName.mount(form);
        for(const k of ['access','mode','difficulty','friendlyFire','messagePosition'])form.elements[k].value=String(p[k]);
        overlay.querySelector('.hmwh-live-map').innerHTML=maps.map(m=>`<option value="${esc(m)}">${esc(m)}</option>`).join('');
        syncAllocation();
        renderList();overlay.querySelector('.hmwh-log').textContent='Démarre le serveur puis consulte ici son chargement.';
        overlay.querySelector('.hmwh-status').textContent=profile?'Serveur sélectionné':'Nouveau profil · à enregistrer';
    }
    async function refreshList(){const r=await api('list');profiles=r.profiles;maps=r.maps;gamePath=r.gamePath;renderList();window.dispatchEvent(new Event('warlink-hmw-profiles-changed'));}
    async function poll(){
        if(!overlay||!selected||polling||busy||!current())return;
        const id=selected;
        polling=true;try{
            const r=await api('status',{id});
            if(!overlay||id!==selected)return;
            overlay.querySelector('.hmwh-status').textContent=r.ready?'● En ligne · prêt à rejoindre':r.running?'◌ Démarrage / chargement de la carte…':'○ Arrêté';
            overlay.querySelector('.hmwh-status').classList.toggle('ready',!!r.ready);
            overlay.querySelector('.hmwh-info').textContent=r.info?r.info.replace(/^infoResponse\s*/,'').replace(/\\/g,' · '):'';
            overlay.querySelector('.hmwh-publishing').textContent=(r.publication || '')+' Pour la liste communautaire WARLINK, utilise Publier sur WARLINK.';
            const log=overlay.querySelector('.hmwh-log');if(r.log){const atBottom=log.scrollHeight-log.scrollTop-log.clientHeight<60;log.textContent=r.log;if(atBottom)log.scrollTop=log.scrollHeight;}
        }catch(e){message(e.message,true);}finally{polling=false;}
    }
    async function act(action){
        if(busy)return;
        if(!selected||!current())throw Error('Enregistre d’abord un profil serveur.');
        busy=true;try{message('Opération en cours…');await api(action,{id:selected});window.dispatchEvent(new Event('warlink-hmw-profiles-changed'));message(action==='start'?'Démarrage lancé. Le premier chargement peut prendre plusieurs minutes.':action==='stop'?'Serveur arrêté.':'Redémarrage lancé.');}finally{busy=false;}
        await poll();
    }
    async function rcon(command){const r=await api('rcon',{id:selected,command});overlay.querySelector('.hmwh-rcon-output').textContent=r.output||'';}
    async function open(profileId){
        if(overlay)return;
        overlay=document.createElement('div');overlay.className='hmwh-overlay';overlay.setAttribute('role','dialog');overlay.setAttribute('aria-modal','true');overlay.setAttribute('aria-label','Mes serveurs HorizonMW');
        overlay.innerHTML=`<div class="hmwh-window"><header><div><span class="hmwh-eyebrow">WARLINK / HORIZONMW</span><h2>Mes serveurs</h2></div><button type="button" data-close aria-label="Fermer le gestionnaire">✕</button></header>
            ${preview?'<p class="hmwh-preview">Aperçu de l’interface : aucun serveur n’est lancé depuis ce navigateur.</p>':''}
            <div class="hmwh-body"><aside><button type="button" data-new>＋ Créer un serveur</button><div class="hmwh-profiles"></div><p class="hmwh-muted">Privé ou public, le serveur reste actif tant que ton PC est allumé. Fermer WARLINK ne l’arrête pas.</p><p class="hmwh-muted">Pour tes amis : autoriser le port choisi en TCP et UDP dans le pare-feu et sur la box. Le référencement officiel nécessite une clé HMW ; Public seul ne suffit pas.</p></aside>
            <main><div class="hmwh-message" role="status"></div><div class="hmwh-editor"></div></main>
            <section class="hmwh-monitor"><h3>Contrôle du serveur</h3><strong class="hmwh-status">Sélectionne un serveur</strong><div class="hmwh-controls"><button data-action="start">Démarrer</button><button data-action="stop">Arrêter</button><button data-action="restart">Redémarrer</button><button data-join>Rejoindre</button></div>
                <code class="hmwh-address"></code><p class="hmwh-publishing hmwh-muted"></p><p class="hmwh-muted">Pour jouer ici : 127.0.0.1. Pour tes amis : ton adresse publique et le même port. Le mot de passe du serveur se saisit dans le jeu.</p>
                <h3>Actions en direct</h3><div class="hmwh-live"><button data-cmd="status">Joueurs / statut</button><button data-cmd="map_rotate">Carte suivante</button><button data-cmd="map_restart">Relancer la partie</button><button data-bots>Appliquer les bots</button><button data-publish>Publier sur WARLINK</button><button data-unpublish>Retirer de WARLINK</button></div><div class="hmwh-rcon-form"><select class="hmwh-live-map" aria-label="Carte à charger"></select><button data-map>Charger la carte</button></div>
                <form class="hmwh-rcon-form"><label class="hmwh-field"><span>Console RCON · commande administrateur</span><input name="command" placeholder="status, map mp_rust, clientkick 2…" autocomplete="off"></label><button>Envoyer</button></form>
                <pre class="hmwh-rcon-output"></pre><h3>Journal de démarrage</h3><pre class="hmwh-log"></pre><details><summary>Réponse réseau</summary><pre class="hmwh-info"></pre></details>
            </section></div></div>`;
        document.body.appendChild(overlay);
        const run=fn=>Promise.resolve().then(fn).catch(e=>message(e.message,true));
        overlay.addEventListener('click',event=>{
            const b=event.target.closest('button');if(!b)return;
            if(b.hasAttribute('data-close')){clearInterval(timer);overlay.remove();overlay=null;return;}
            if(b.hasAttribute('data-new')){selected='srv_'+Date.now().toString(36);renderForm();message('Choisis un préréglage, ajuste les options puis enregistre.');return;}
            if(b.dataset.profile){selected=b.dataset.profile;renderForm(current());message('');updateAddress();run(poll);}
            if(b.dataset.action)run(()=>act(b.dataset.action));
            if(b.dataset.cmd)run(()=>rcon(b.dataset.cmd));
            if(b.hasAttribute('data-map'))run(()=>{const map=overlay.querySelector('.hmwh-live-map').value;if(!/^[a-zA-Z0-9_]+$/.test(map))throw Error('Choisis une carte.');return rcon('map '+map);});
            if(b.hasAttribute('data-bots'))run(()=>{const p=values();if(p.bots>p.players-1)throw Error('Il faut réserver au moins une place pour un joueur.');return rcon(`set warlink_bot_count ${p.bots}; set warlink_bot_difficulty ${p.difficulty}`);});
            if(b.hasAttribute('data-publish'))run(async()=>{const choice=await window.showMessageBox('Publier dans la communauté WARLINK','Ton adresse IP publique, le port, le nom du serveur, la carte et le nombre de joueurs seront visibles aux utilisateurs WARLINK. Les mots de passe ne sont pas publiés.',['Annuler','Publier']);if(choice!==1)return;const r=await api('publish',{id:selected});message(r.message);await window.ServersView.refreshCommunity();});
            if(b.hasAttribute('data-unpublish'))run(async()=>{const r=await api('unpublish',{id:selected});message(r.message);window.dispatchEvent(new Event('warlink-hmw-profiles-changed'));});
            if(b.hasAttribute('data-join'))run(async()=>{if(!current())throw Error('Choisis un profil enregistré.');const result=await api('join',{id:selected});message(result.message);});
        });
        overlay.addEventListener('submit',event=>{event.preventDefault();if(event.target.classList.contains('hmwh-rcon-form')){run(()=>rcon(event.target.elements.command.value));return;}run(async()=>{if(!event.target.reportValidity())return;const p=values();if(p.bots>p.players-1)throw Error('Réserve une place pour un humain : bots ≤ places − 1.');await api('save',{id:selected,profile:p});await refreshList();renderForm(current());updateAddress();message('Profil enregistré. Tu peux démarrer le serveur.');});});
        overlay.addEventListener('input',event=>{if(['players','bots','humans'].includes(event.target.name) && event.target.value !== '')syncAllocation(event.target.name);});
        overlay.addEventListener('change',event=>{
            if(['players','bots','humans'].includes(event.target.name))syncAllocation(event.target.name);
            const f=overlay.querySelector('.hmwh-form');if(!f)return;
            if(event.target.name==='mapPick'&&event.target.value){f.elements.rotation.value+=(f.elements.rotation.value.trim()?'\n':'')+event.target.value;event.target.value='';}
            if(event.target.name==='preset'){
                const v=event.target.value;f.elements.access.value=v==='public'?'public':'private';f.elements.players.value=v==='duel'?2:12;f.elements.bots.value=v==='training'?9:0;f.elements.mode.value=v==='duel'?'dm':'war';f.elements.rotation.value=v==='duel'?'mp_rust':'mp_rust\nmp_terminal';f.elements.scoreLimit.value=v==='duel'?20:75;f.elements.password.value=v==='public'?'':('amis-'+Math.random().toString(36).slice(2,8));
            }
            if(event.target.name==='preset')syncAllocation();
            updateAddress();
        });
        try{await refreshList();selected=profiles.find(p=>p.id===profileId)?.id||profiles[0]?.id||'srv_'+Date.now().toString(36);renderForm(current());updateAddress();await poll();timer=setInterval(poll,2500);}catch(e){message(e.message,true);}
    }
    function updateAddress(){const form=overlay?.querySelector('.hmwh-form');if(form)overlay.querySelector('.hmwh-address').textContent=`connect 127.0.0.1:${form.elements.port.value}`;}
    document.addEventListener('click',e=>{if(e.target.closest('[data-hmw-host]'))open();});
    window.HMWHost={open};
})();
