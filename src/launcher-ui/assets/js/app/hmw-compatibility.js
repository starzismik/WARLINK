(function(){
'use strict';
const esc=s=>GameUtils.escapeHtml(String(s??''));
async function open(gamePath=''){
 const modal=document.createElement('div');modal.className='warlink-product-overlay';modal.style.zIndex='11000';modal.setAttribute('role','dialog');modal.setAttribute('aria-modal','true');modal.setAttribute('aria-label','Cartes et langues HorizonMW');
 modal.innerHTML='<section class="warlink-product-card"><small>WARLINK / HORIZONMW</small><h2>Vérifier cartes et langues</h2><p data-language-status>Analyse des fichiers installés…</p><div data-language-details style="max-height:220px;overflow:auto;line-height:1.8"></div><p>Le contrôle porte sur les fichiers de langue des cartes MWR détectées et leur en-tête. Les autres fichiers du jeu se vérifient depuis « Vérifier les fichiers ».</p><div class="warlink-product-actions"><button class="mods-btn" data-language-repair hidden>Utiliser les fichiers anglais</button><button class="mods-btn" data-language-close>Fermer</button></div></section>';
 document.body.appendChild(modal);modal.querySelector('[data-language-close]').onclick=()=>modal.remove();
 const status=modal.querySelector('[data-language-status]'),details=modal.querySelector('[data-language-details]'),repair=modal.querySelector('[data-language-repair]');
 async function check(action){const r=await window.executeCommand('hmw-host',{action,gamePath});if(!r?.ok)throw Error(r?.error||'Vérification indisponible');status.textContent=(r.compatible?'Compatible — ':'Fichiers incomplets — ')+r.language+' · '+r.checked+' fichiers contrôlés';details.innerHTML=r.compatible?'<p>Les fichiers de langue attendus sont présents et leur en-tête est valide.</p>':'<ul>'+r.missing.map(x=>'<li>'+esc(x)+'</li>').join('')+'</ul><p>'+(r.englishAvailable?'Les fichiers anglais sont disponibles. La réparation change la langue du jeu en anglais ; elle ne traduit pas les fichiers. Le réglage précédent est sauvegardé. Ferme le jeu avant de continuer.':'Les fichiers anglais sont également incomplets : vérifie ou réinstalle les composants manquants du jeu.')+'</p>';repair.hidden=r.compatible||!r.englishAvailable;}
 repair.onclick=async()=>{repair.disabled=true;try{await check('language-repair');status.textContent='Réparation effectuée : HorizonMW utilisera les fichiers anglais au prochain lancement.';}catch(e){status.textContent=e.message;}finally{repair.disabled=false;}};
 try{await check('language-check');}catch(e){status.textContent=e.message;}
 modal.querySelector('[data-language-close]').focus();
}
window.HMWCompatibility={open};
})();
