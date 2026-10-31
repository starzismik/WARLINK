(function () {
    'use strict';

    const STALE_MS = 30 * 1000;

    const state = {};
    const escapeHtml = value => GameUtils.escapeHtml(value);
    let searchTimer = null;
    let refreshTimer = null;
    const communityRequests = new WeakMap();

    function t(key, variables) {
        return window.LauncherI18n ? window.LauncherI18n.t(key, variables) : key;
    }

    function getState(gameId) {
        if (!state[gameId]) {
            const caps = window.ServersService.supports(gameId) || {};
            const prefs = window.ServersService.getViewPrefs(gameId);
            state[gameId] = {
                query: '',
                mode: (caps.modes || []).includes(prefs.mode) ? prefs.mode : 'all',
                sort: prefs.sort || 'players',
                region: prefs.region || 'all',
                hideEmpty: !!prefs.hideEmpty,
                hideFull: !!prefs.hideFull,
                favoritesOnly: !!prefs.favoritesOnly,
                featuredCollapsed: !!prefs.featuredCollapsed,
                servers: null,
                loading: false,
                error: null,
                lastUpdated: 0,
                caps
            };
        }
        return state[gameId];
    }

    function saveView(gameId) {
        const s = getState(gameId);
        window.ServersService.saveViewPrefs(gameId, {
            mode: s.mode,
            sort: s.sort,
            region: s.region,
            hideEmpty: s.hideEmpty,
            hideFull: s.hideFull,
            favoritesOnly: s.favoritesOnly,
            featuredCollapsed: s.featuredCollapsed
        });
    }

    function query(gameId, selector) {
        const panel = document.querySelector(`.servers-panel[data-game="${gameId}"]`);
        if (!panel) return null;
        return selector ? panel.querySelector(selector) : panel;
    }

    const hasPing = server => typeof server.ping === 'number';

    const COPY_SVG = '<svg class="server-copy-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true"><rect x="9" y="9" width="11" height="11" rx="2"/><path d="M5 15V6a2 2 0 0 1 2-2h9"/></svg>';
    const LOCK_SVG = '<svg class="server-lock" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="5" y="11" width="14" height="9" rx="1.5"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/></svg>';
    const STAR_SVG = '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 2.5l2.95 5.98 6.6.96-4.78 4.66 1.13 6.58L12 17.57l-5.9 3.11 1.13-6.58-4.78-4.66 6.6-.96z"/></svg>';
    const DISCORD_SVG = '<svg class="server-discord-icon" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M20.317 4.3698a19.7913 19.7913 0 00-4.8851-1.5152.0741.0741 0 00-.0785.0371c-.211.3753-.4447.8648-.6083 1.2495-1.8447-.2762-3.68-.2762-5.4868 0-.1636-.3933-.4058-.8742-.6177-1.2495a.077.077 0 00-.0785-.037 19.7363 19.7363 0 00-4.8852 1.515.0699.0699 0 00-.0321.0277C.5334 9.0458-.319 13.5799.0992 18.0578a.0824.0824 0 00.0312.0561c2.0528 1.5076 4.0413 2.4228 5.9929 3.0294a.0777.0777 0 00.0842-.0276c.4616-.6304.8731-1.2952 1.226-1.9942a.076.076 0 00-.0416-.1057c-.6528-.2476-1.2743-.5495-1.8722-.8923a.077.077 0 01-.0076-.1277c.1258-.0943.2517-.1923.3718-.2914a.0743.0743 0 01.0776-.0105c3.9278 1.7933 8.18 1.7933 12.0614 0a.0739.0739 0 01.0785.0095c.1202.099.246.1981.3728.2924a.077.077 0 01-.0066.1276 12.2986 12.2986 0 01-1.873.8914.0766.0766 0 00-.0407.1067c.3604.698.7719 1.3628 1.225 1.9932a.076.076 0 00.0842.0286c1.961-.6067 3.9495-1.5219 6.0023-3.0294a.077.077 0 00.0313-.0552c.5004-5.177-.8382-9.6739-3.5485-13.6604a.061.061 0 00-.0312-.0286zM8.02 15.3312c-1.1825 0-2.1569-1.0857-2.1569-2.419 0-1.3332.9555-2.4189 2.157-2.4189 1.2108 0 2.1757 1.0952 2.1568 2.419 0 1.3332-.9555 2.4189-2.1569 2.4189zm7.9748 0c-1.1825 0-2.1569-1.0857-2.1569-2.419 0-1.3332.9554-2.4189 2.1569-2.4189 1.2108 0 2.1757 1.0952 2.1568 2.419 0 1.3332-.946 2.4189-2.1568 2.4189Z"/></svg>';

    const TAG_KEYS = { official: 'servers.featuredOfficial', contributor: 'servers.featuredContributor', event: 'servers.featuredEvent' };
    const DISCORD_HOSTS = ['discord.gg', 'discord.com', 'www.discord.com'];

    function emptyHTML(key) {
        return `<div class="mods-empty">${escapeHtml(t(key))}</div>`;
    }

    function favTitle(favorite) {
        return t(favorite ? 'servers.favoriteRemove' : 'servers.favoriteAdd');
    }

    function render(gameId) {
        const panel = query(gameId);
        if (!panel) return;
        const s = getState(gameId);
        const chip = (attrs, active, label) => `<button class="chip${active ? ' active' : ''}" ${attrs}>${escapeHtml(label)}</button>`;
        const option = (current, value, label) => `<option value="${value}"${current === value ? ' selected' : ''}>${escapeHtml(label)}</option>`;
        const modeInfo = GameUtils.getModeInfo();
        const modes = s.caps.modes || [];

        panel.innerHTML = `
            ${gameId === 'hmw-mod' ? '<section class="hmw-owned-servers"><div class="servers-toolbar"><strong>Mes serveurs HorizonMW</strong><button class="mods-btn" data-hmw-host>＋ Créer / gérer mes serveurs</button><button class="mods-btn" data-hmw-guide>Guide réseau</button><button class="mods-btn" data-hmw-view>Corriger la visée</button></div><div class="hmw-owned-list">Chargement des profils locaux…</div><div class="hmw-community-head"><h3>Communauté WARLINK</h3><button class="mods-btn" data-community-refresh>↻ Actualiser</button><small class="hmw-community-updated" role="status">Actualisation automatique · 15 s</small></div><p>Serveurs vérifiés depuis notre annuaire, indépendants de la liste officielle HMW.</p><div class="hmw-community-list">Chargement de la communauté…</div></section>' : ''}
            ${gameId === 'hmw-mod' ? `<details class="hmw-official-servers" ${s.horizonExpanded?'open':''}><summary><span class="hmw-expand-icon" aria-hidden="true">+</span><span class="hmw-official-label">${s.horizonExpanded?'Masquer':'Afficher'} les serveurs HorizonMW</span><small>Liste HorizonMW</small></summary>` : ''}
            <div class="servers-toolbar">
                <div class="search-field">
                    <input type="text" class="servers-search" placeholder="${escapeHtml(t('servers.searchPlaceholder'))}" value="${escapeHtml(s.query)}" />
                    <button type="button" class="search-clear servers-search-clear"${s.query ? '' : ' hidden'}>&times;</button>
                </div>
                <div class="filter-chips servers-filters">
                    ${modes.length > 1 ? chip('data-mode="all"', s.mode === 'all', t('servers.filterAll')) : ''}
                    ${modes.length > 1 ? modes.map(mode => chip(`data-mode="${mode}"`, s.mode === mode, (modeInfo[mode] || {}).name || mode.toUpperCase())).join('') : ''}
                    ${chip('data-toggle="favoritesOnly"', s.favoritesOnly, t('servers.favorites'))}
                    ${chip('data-toggle="hideEmpty"', s.hideEmpty, t('servers.hideEmpty'))}
                    ${chip('data-toggle="hideFull"', s.hideFull, t('servers.hideFull'))}
                </div>
                <select class="cdn-select servers-region">
                    ${option(s.region, 'all', t('servers.allRegions'))}
                    ${window.ServersService.REGIONS.map(region => option(s.region, region, region)).join('')}
                </select>
                <select class="cdn-select servers-sort">
                    ${option(s.sort, 'players', t('servers.sortPlayers'))}
                    ${option(s.sort, 'ping', t('servers.sortPing'))}
                    ${option(s.sort, 'name', t('servers.sortName'))}
                </select>
                <button class="mods-btn servers-refresh-btn">${escapeHtml(t('servers.refresh'))}</button>
            </div>
            <div class="servers-meta">
                <span class="servers-count"></span>
                <span class="servers-updated"></span>
            </div>
            <div class="servers-list-host"></div>
            ${gameId === 'hmw-mod' ? '</details>' : ''}
        `;

        if(gameId === 'hmw-mod'){loadOwnedServers(panel);loadCommunity(panel);}
        panel.querySelector('[data-community-refresh]')?.addEventListener('click',()=>refreshCommunity(panel));
        const official=panel.querySelector('.hmw-official-servers');
        if(official)official.addEventListener('toggle',()=>{s.horizonExpanded=official.open;official.querySelector('.hmw-official-label').textContent=(official.open?'Masquer':'Afficher')+' les serveurs HorizonMW';});
        const input = panel.querySelector('.servers-search');
        const clear = panel.querySelector('.servers-search-clear');
        const setQuery = value => {
            s.query = value;
            input.value = value;
            clear.hidden = !value;
        };
        input.addEventListener('input', () => {
            setQuery(input.value);
            clearTimeout(searchTimer);
            searchTimer = setTimeout(() => renderList(gameId), 250);
        });
        clear.addEventListener('click', () => {
            setQuery('');
            renderList(gameId);
            input.focus();
        });
        panel.querySelectorAll('.chip[data-mode]').forEach(button => {
            button.addEventListener('click', () => {
                s.mode = button.dataset.mode;
                panel.querySelectorAll('.chip[data-mode]').forEach(b => b.classList.toggle('active', b === button));
                saveView(gameId);
                renderList(gameId);
            });
        });
        panel.querySelectorAll('.chip[data-toggle]').forEach(button => {
            button.addEventListener('click', () => {
                s[button.dataset.toggle] = !s[button.dataset.toggle];
                button.classList.toggle('active', s[button.dataset.toggle]);
                saveView(gameId);
                renderList(gameId);
            });
        });
        panel.querySelector('.servers-region').addEventListener('change', event => {
            s.region = event.target.value;
            saveView(gameId);
            renderList(gameId);
        });
        panel.querySelector('.servers-sort').addEventListener('change', event => {
            s.sort = event.target.value;
            saveView(gameId);
            renderList(gameId);
        });
        panel.querySelector('.servers-refresh-btn').addEventListener('click', () => loadServers(gameId));
        bindListEvents(gameId, panel.querySelector('.servers-list-host'));

        ensureTimers();
        if (s.servers === null || Date.now() - s.lastUpdated > STALE_MS) {
            loadServers(gameId);
        } else {
            renderList(gameId);
        }
    }

    async function loadCommunity(panel) {
        const host=panel.querySelector('.hmw-community-list');if(!host)return;
        const requestId=(communityRequests.get(panel)||0)+1;communityRequests.set(panel,requestId);
        const button=panel.querySelector('[data-community-refresh]'),updated=panel.querySelector('.hmw-community-updated');
        const current=()=>communityRequests.get(panel)===requestId && host.isConnected && panel.querySelector('.hmw-community-list')===host;
        if(button){button.disabled=true;button.textContent='Actualisation…';}
        try {
            const r=await window.executeCommand('hmw-host',{action:'community-list'});if(!current())return;if(!r?.ok)throw Error(r?.error||'Annuaire indisponible');
            host.innerHTML=r.servers.map(p=>`<div class="mods-row"><strong class="warlink-colored-name">${window.WarlinkName.html(p.name)}</strong><span class="hmw-state is-active">Actif · vérifié</span><span> · ${escapeHtml(p.map)} · ${p.players}/${p.maxPlayers} joueurs · ${escapeHtml(p.id)}${p.locked?' · mot de passe':''}</span><button class="mods-btn" data-community-join="${escapeHtml(p.id)}" data-locked="${p.locked?'1':'0'}">Rejoindre</button></div>`).join('') || '<p>Aucun serveur communautaire en ligne. Démarre ton serveur public puis utilise « Publier sur WARLINK » dans son gestionnaire.</p>';
            host.dataset.loaded='true';if(updated)updated.textContent='Mis à jour à '+new Date().toLocaleTimeString('fr-FR')+' · auto 15 s';
            host.querySelectorAll('[data-community-join]').forEach(b=>b.addEventListener('click',async()=>{try{
                let password='';if(b.dataset.locked==='1'){const choice=await window.showMessageBox('Mot de passe','<input id="hmw-community-password" type="password" maxlength="64" autocomplete="off">',['Annuler','Rejoindre']);if(choice!==1)return;password=document.getElementById('hmw-community-password').value;}
                const result=await window.executeCommand('hmw-host',{action:'join-remote',endpoint:b.dataset.communityJoin,password});if(!result?.ok)throw Error(result?.error||'Connexion impossible');window.showMessageBox('Rejoindre HorizonMW',escapeHtml(result.message),['OK']);
            }catch(e){window.showToast(e.message,'error');}}));
        }catch(e){if(!current())return;if(updated)updated.textContent='Actualisation impossible · nouvel essai automatique';if(!host.dataset.loaded)host.textContent='Annuaire communautaire : '+e.message;}
        finally{if(current()&&button){button.disabled=false;button.textContent='↻ Actualiser';}}
    }
    async function refreshCommunity(panel=query('hmw-mod')) {
        if(!panel)return;
        await Promise.allSettled([loadOwnedServers(panel),loadCommunity(panel)]);
    }
    document.addEventListener('click',async event=>{
        if(event.target.closest('[data-hmw-guide]'))window.showMessageBox('Héberger un serveur WARLINK',
        "<div class=\"hmw-network-guide\"><p class=\"hmw-guide-intro\">De la création du serveur à sa publication dans la communauté.</p><ol class=\"hmw-guide-steps\"><li><div><h4>Préparer ton serveur</h4><p>Configure HorizonMW dans la bibliothèque, puis crée un profil serveur. Choisis <strong>Public</strong> et un port libre, par exemple <code>27018</code>.</p></div></li><li><div><h4>Autoriser le pare-feu Windows</h4><p>Autorise uniquement <code>warlink-hmw-&lt;profil&gt;.exe</code> sur le port choisi, en <strong>TCP et UDP</strong>.</p></div></li><li><div><h4>Configurer la box</h4><p>Dans <strong>NAT / redirection de ports</strong>, redirige ce même port externe TCP et UDP vers l’adresse IPv4 locale du PC et le même port interne. <strong>Ne désactive pas le pare-feu et n’active pas de DMZ.</strong></p></div></li><li><div><h4>Conserver la bonne adresse</h4><p>Réserve l’adresse IPv4 de ton PC dans le DHCP de la box. Si ton opérateur utilise une IPv4 partagée / CGNAT, demande une IPv4 publique ou utilise un hébergement adapté.</p></div></li><li><div><h4>Publier dans WARLINK</h4><p>Démarre le serveur, attends le statut <strong>Actif</strong>, puis clique <strong>Publier sur WARLINK</strong>. L’annuaire vérifie TCP et UDP depuis Internet. Ton IP publique, le port et les informations du serveur seront visibles.</p></div></li><li><div><h4>Inviter les joueurs</h4><p>Ils ouvrent <strong>Serveurs → HorizonMW → Communauté WARLINK → Rejoindre</strong>. Aucune clé HMW n’est nécessaire pour cette liste ; le navigateur officiel du jeu reste indépendant.</p></div></li></ol><aside class=\"hmw-guide-note\"><strong>À garder en tête</strong><p>Le PC doit rester allumé. WARLINK actualise la communauté toutes les 15 secondes ; les vérifications réseau de l’annuaire ont lieu environ toutes les 30 secondes. Un serveur hors ligne disparaît en moins de 2 minutes. Ne partage jamais ton mot de passe RCON.</p></aside></div>",['Fermer']);
        if(event.target.closest('[data-hmw-view]')){const choice=await window.showMessageBox('Visée et champ de vision','Une échelle cg_fovScale élevée éloigne visuellement l’arme. Ce bouton remet uniquement cg_fovScale à 1, conserve ton FOV et sauvegarde la configuration précédente. Ferme le jeu avant d’appliquer.',['Annuler','Rétablir une visée normale']);if(choice!==1)return;try{const r=await window.executeCommand('hmw-host',{action:'reset-view'});if(!r?.ok)throw Error(r?.error);window.showToast(r.message,'success');}catch(e){window.showToast(e.message,'error');}}
    });

    async function loadOwnedServers(panel) {
        const host=panel.querySelector('.hmw-owned-list');if(!host)return;
        try {
            const r=await window.executeCommand('hmw-host',{action:'list'});
            if(!r?.ok)throw Error(r?.error || 'Lecture impossible');
            host.innerHTML=r.profiles.map(p=>`<div class="mods-row"><strong class="warlink-colored-name">${window.WarlinkName.html(p.name)}</strong>${p.ready?'<span class="hmw-state is-active">Actif</span>':p.running?'<span class="hmw-state is-starting">Chargement</span>':'<span class="hmw-state is-stopped">Arrêté</span>'}<span>127.0.0.1:${p.port}</span><button class="mods-btn" data-own-profile="${escapeHtml(p.id)}">Gérer / rejoindre</button></div>`).join('') || '<p>Aucun serveur créé. Utilise le bouton ci-dessus.</p>';
            host.querySelectorAll('[data-own-profile]').forEach(b=>b.addEventListener('click',()=>window.HMWHost.open(b.dataset.ownProfile)));
        } catch(error) {host.textContent='Profils locaux indisponibles : '+error.message;}
    }

    window.addEventListener('warlink-hmw-profiles-changed',()=>{const panel=query('hmw-mod');if(panel?.querySelector('.hmw-owned-list'))refreshCommunity(panel);});

    async function loadServers(gameId, silent) {
        if(gameId === 'hmw-mod'){const panel=query(gameId);if(panel){loadOwnedServers(panel);loadCommunity(panel);}}
        const s = getState(gameId);
        if (s.loading) return;

        s.loading = true;
        s.error = null;
        const host = query(gameId, '.servers-list-host');
        if (host && !silent) {
            host.innerHTML = `<div class="mods-loading"><div class="spinner"></div><span>${escapeHtml(t('servers.loading'))}</span></div>`;
        }

        try {
            s.servers = await window.ServersService.getServers(gameId);
            s.lastUpdated = Date.now();
        } catch (error) {
            console.error(error);
            // A silent background refresh keeps showing the last good list.
            if (!silent || s.servers === null) s.error = error;
        }
        s.loading = false;
        renderList(gameId);
    }

    function applyFilters(s, favorites) {
        const needle = s.query.trim().toLowerCase();
        return s.servers
            .filter(server => (s.mode === 'all' || server.mode === s.mode)
                && (s.region === 'all' || server.region === s.region)
                && (!needle || server.name.toLowerCase().includes(needle) || server.map.toLowerCase().includes(needle))
                && (!s.hideEmpty || server.players > 0)
                && (!s.hideFull || server.players < server.maxPlayers)
                && (!s.favoritesOnly || favorites.has(server.id)))
            .sort((a, b) => {
                const pingOf = server => hasPing(server) ? server.ping : Infinity;
                if (s.sort === 'ping') return pingOf(a) - pingOf(b);
                if (s.sort === 'name') return a.name.localeCompare(b.name);
                return (b.players - a.players) || (pingOf(a) - pingOf(b));
            });
    }

    // The worker already vets these; checked again so a bad list can never open anything but Discord.
    function discordLink(value) {
        try {
            const url = new URL(String(value));
            return url.protocol === 'https:' && DISCORD_HOSTS.includes(url.hostname) ? url.href : null;
        } catch (_) {
            return null;
        }
    }

    const isFeatured = server => !!server.tag && typeof server.tag.featured === 'number';

    function tagPillHTML(tag) {
        const key = TAG_KEYS[tag.label];
        if (!key) return '';
        return `<span class="server-featured-tag is-${escapeHtml(tag.label)}" title="${escapeHtml(t(`${key}Hint`))}">${escapeHtml(t(key))}</span>`;
    }

    function serverRowHTML(server, favorite, canJoin) {
        const tag = server.tag || null;
        const discord = tag && discordLink(tag.discord);
        const name = tag && tag.note
            ? `<span class="server-name-stack"><span>${escapeHtml(server.name)}</span><small class="server-note">${escapeHtml(tag.note)}</small></span>`
            : `<span>${escapeHtml(server.name)}</span>`;
        const copyTitle = escapeHtml(t('servers.copyConnectTitle'));
        const action = canJoin
            ? `<button class="mods-btn mods-icon-btn server-copy-btn" title="${copyTitle}" aria-label="${copyTitle}">${COPY_SVG}</button>
               <button class="mods-btn server-join-btn">${escapeHtml(t('servers.join'))}</button>`
            : `<button class="mods-btn server-copy-btn" title="${copyTitle}">${escapeHtml(t('servers.copyConnect'))}</button>`;
        const pinged = hasPing(server);
        const pingClass = !pinged ? 'is-off' : server.ping < 80 ? 'is-good' : server.ping < 150 ? 'is-mid' : 'is-bad';
        const playersTitle = server.bots > 0 ? ` title="${escapeHtml(t('servers.bots', { count: server.bots }))}"` : '';
        return `
            <div class="server-row${tag && tag.note ? ' has-note' : ''}" data-server-id="${escapeHtml(server.id)}">
                <button class="server-fav-btn${favorite ? ' is-fav' : ''}" title="${escapeHtml(favTitle(favorite))}">${STAR_SVG}</button>
                <div class="server-name">${server.locked ? `<span title="${escapeHtml(t('servers.passworded'))}">${LOCK_SVG}</span>` : ''}${tag ? tagPillHTML(tag) : ''}${name}</div>
                <span class="server-map">${escapeHtml(server.map)}</span>
                <span class="server-mode"><span class="badge server-mode-badge">${escapeHtml(`${server.mode.toUpperCase()} · ${server.gametype.toUpperCase()}`)}</span></span>
                <span class="server-region"${server.countryName ? ` title="${escapeHtml(server.countryName)}"` : ''}>${escapeHtml(server.region)}</span>
                <span class="server-players"${playersTitle}>${server.players}/${server.maxPlayers}</span>
                <span class="server-ping ${pingClass}">${pinged ? `${server.ping}ms` : '—'}</span>
                <span class="server-actions">
                    ${discord ? `<button class="mods-btn mods-icon-btn server-discord-btn" title="${escapeHtml(t('servers.featuredDiscord'))}">${DISCORD_SVG}</button>` : ''}
                    ${action}
                </span>
            </div>`;
    }

    function renderList(gameId) {
        const s = getState(gameId);
        const host = query(gameId, '.servers-list-host');
        if (!host) return;
        renderUpdated(gameId);
        const count = query(gameId, '.servers-count');
        if (count) count.textContent = '';

        if (s.loading) return;
        if (s.error) {
            host.innerHTML = `<div class="mods-empty servers-error">
                <span>${escapeHtml(t('servers.loadFailed'))}</span>
                <button class="mods-btn servers-retry-btn">${escapeHtml(t('servers.retry'))}</button>
            </div>`;
            host.querySelector('.servers-retry-btn').addEventListener('click', () => loadServers(gameId));
            return;
        }
        if (s.servers === null) return;
        if (!s.servers.length) {
            host.innerHTML = emptyHTML('servers.empty');
            return;
        }

        const favorites = new Set(window.ServersService.getFavorites(gameId));
        const shown = applyFilters(s, favorites);
        if (!shown.length) {
            const noFavorites = s.favoritesOnly && !favorites.size;
            host.innerHTML = emptyHTML(noFavorites ? 'servers.noFavorites' : 'servers.noMatches');
            return;
        }

        const totalPlayers = shown.reduce((sum, server) => sum + server.players, 0);
        if (count) count.textContent = `${t('servers.count', { shown: shown.length, total: s.servers.length })} · ${t('servers.countPlayers', { count: totalPlayers })}`;

        // Pinned featured rows leave the main list; collapsed, they sit in it at their usual place.
        const featured = shown.filter(isFeatured).sort((a, b) => a.tag.featured - b.tag.featured);
        const pinned = featured.length > 0 && !s.featuredCollapsed;
        const rest = pinned ? shown.filter(server => !isFeatured(server)) : shown;
        const rows = list => list.map(server => serverRowHTML(server, favorites.has(server.id), !!s.caps.join)).join('');
        const featuredBox = featured.length ? `
                <section class="servers-featured${pinned ? '' : ' is-collapsed'}">
                    <button class="servers-featured-head" type="button" aria-expanded="${pinned}" title="${escapeHtml(t(pinned ? 'servers.featuredCollapse' : 'servers.featuredExpand'))}">
                        <span class="servers-featured-title">${escapeHtml(t('servers.featured'))}</span>
                        <span class="servers-featured-count">${featured.length}</span>
                        <span class="servers-featured-chevron"></span>
                    </button>
                    ${pinned ? rows(featured) : ''}
                </section>` : '';

        host.innerHTML = `
            <div class="servers-list">
                <div class="servers-head">
                    <span></span>
                    <span>${escapeHtml(t('servers.colName'))}</span>
                    <span>${escapeHtml(t('servers.colMap'))}</span>
                    <span>${escapeHtml(t('servers.colMode'))}</span>
                    <span>${escapeHtml(t('servers.colRegion'))}</span>
                    <span>${escapeHtml(t('servers.colPlayers'))}</span>
                    <span>${escapeHtml(t('servers.colPing'))}</span>
                    <span></span>
                </div>
                ${featuredBox}
                ${rows(rest)}
            </div>
        `;
    }

    // One delegated listener per panel instead of two per row.
    function bindListEvents(gameId, host) {
        host.addEventListener('click', event => {
            if (event.target.closest('.servers-featured-head')) {
                const s = getState(gameId);
                s.featuredCollapsed = !s.featuredCollapsed;
                saveView(gameId);
                renderList(gameId);
                return;
            }

            const row = event.target.closest('.server-row');
            if (!row || !host.contains(row)) return;
            const id = row.dataset.serverId;

            const favButton = event.target.closest('.server-fav-btn');
            if (favButton) {
                const added = window.ServersService.toggleFavorite(gameId, id);
                if (getState(gameId).favoritesOnly) {
                    renderList(gameId);
                    return;
                }
                favButton.classList.toggle('is-fav', added);
                favButton.title = favTitle(added);
                return;
            }

            if (event.target.closest('.server-discord-btn')) {
                openDiscord(gameId, id);
            } else if (event.target.closest('.server-join-btn')) {
                joinServer(gameId, id);
            } else if (event.target.closest('.server-copy-btn')) {
                copyConnect(gameId, id);
            }
        });
    }

    function openDiscord(gameId, id) {
        const server = (getState(gameId).servers || []).find(entry => entry.id === id);
        const url = server && server.tag && discordLink(server.tag.discord);
        if (url) window.executeCommand('open-url', { url });
    }

    async function copyConnect(gameId, id) {
        const server = (getState(gameId).servers || []).find(entry => entry.id === id);
        if (!server) return;
        const command = window.ServersService.connectCommand(server);
        const ok = await window.copyTextToClipboard(command);
        window.showToast(ok ? t('servers.connectCopied', { command }) : t('servers.copyFailed'), ok ? 'success' : 'error');
    }

    async function joinServer(gameId, id) {
        const s = getState(gameId);
        const server = (s.servers || []).find(entry => entry.id === id);
        if (!server || !s.caps.join) return;

        const gameState = window.GameStateManager && window.GameStateManager.gameStates[gameId];
        if (!gameState || gameState.installStatus !== 'installed') {
            window.showToast(t('servers.joinInstallFirst'), 'info');
            if (window.showSetupFlow) window.showSetupFlow(gameId);
            return;
        }

        try {
            const result = await window.ServersService.joinServer(gameId, server);
            if (!result || !result.success) {
                throw new Error((result && result.error) || 'Failed to join the server.');
            }
            window.showToast(t('servers.joiningToast', { name: server.name }), 'info');
        } catch (error) {
            console.error(error);
            window.showToast(String(error.message || error), 'error');
        }
    }

    function renderUpdated(gameId) {
        const s = getState(gameId);
        const label = query(gameId, '.servers-updated');
        if (!label) return;
        if (!s.lastUpdated) {
            label.textContent = '—';
            return;
        }
        const seconds = Math.max(0, Math.floor((Date.now() - s.lastUpdated) / 1000));
        if (seconds < 60) {
            label.textContent = t('servers.updatedSecondsAgo');
        } else if (seconds < 3600) {
            label.textContent = t('servers.updatedMinutesAgo', { minutes: Math.floor(seconds / 60) });
        } else {
            label.textContent = t('servers.updatedHoursAgo', { hours: Math.floor(seconds / 3600) });
        }
    }

    // Refresh only while the Servers page is showing a game; polling for that beats
    // having the page signal deactivation.
    function activeServersGame() {
        if (document.visibilityState !== 'visible') return null;
        const panel = [...document.querySelectorAll('.servers-panel[data-game]')].find(p=>p.offsetParent!==null);
        if (!panel || panel.offsetParent === null) return null;
        return panel.dataset.game;
    }

    function ensureTimers() {
        if (refreshTimer) return;
        refreshTimer = setInterval(() => {
            const gameId = activeServersGame();
            if (gameId && state[gameId]) loadServers(gameId, true);
        }, STALE_MS);
        setInterval(() => {if(activeServersGame()==='hmw-mod')refreshCommunity();},15000);
        const refreshVisible=()=>{if(activeServersGame()==='hmw-mod')refreshCommunity();};
        window.addEventListener('focus',refreshVisible);document.addEventListener('visibilitychange',refreshVisible);
        setInterval(() => {
            const gameId = activeServersGame();
            if (gameId && state[gameId]) renderUpdated(gameId);
        }, 10000);
    }

    window.ServersView = {
        render,
        refreshCommunity,
        supports: gameId => !!(window.ServersService && window.ServersService.supports(gameId))
    };
})();
