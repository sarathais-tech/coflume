  // ===== Visualização da Guardiã IA (proposta, somente leitura) =====
  // Observa as linhas recebidas. Não envia nada ao ESP32 e não altera o estado AG,
  // o relógio, o áudio, as pistas nem a área de logs. A reprodução de arquivo usa um
  // modelo separado, marcado como DADOS DE DEMONSTRAÇÃO, que nunca entra nos logs salvos.
  const GV_NIVEIS=['Explorador','Desafiante','Mestre'];
  const GV_CATEG=['Matemática','Português','Lógica','Conhecimentos gerais'];
  const GV_GRADUAL='gradual-v2-exp';
  const gvEsc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const gvIndisp=(motivo)=>`<span class="gv-na">indisponível${motivo?' · '+gvEsc(motivo):''}</span>`;
  function gvNovoModelo(){return {estado:null,modelo:null,autoteste:null,sessao:null,perguntas:{},fim:null,erros:[],agente:null,linhas:0,nota:null,conf:{n:0,ok:0,max:0}};}
  const gvModelos={usb:gvNovoModelo(),demo:gvNovoModelo()};
  let gvFonte='usb',gvSel=null,gvRaf=0,gvLinhasAoVivoDuranteDemo=0;
  function gvCampos(line){ // GUARDIA,tipo,k=v,...  (itens sem '=' ficam em c.pos, na ordem)
    const t=line.split(','),c={pos:[]};
    for(const x of t.slice(2)){const i=x.indexOf('=');if(i<0)c.pos.push(x);else c[x.slice(0,i)]=x.slice(i+1);}
    return {tipo:t[1]||'',c};
  }
  function gvNovaPartida(m,c){m.sessao=c;m.perguntas={};m.fim=null;m.erros=[];m.agente=null;gvSel=null;}
  // Devolve true quando a linha muda o que o quadro mostra.
  function gvLinha(m,line){
    line=line.trim();
    if(line.startsWith('AG,')){
      const v=line.split(',');let novo=m.agente;
      if(v[1]==='PREP')novo='PREP';else if(v[1]==='STATE'&&v[3])novo=v[3]==='IDLE'?null:v[3];else return false;
      if(novo===m.agente)return false;m.agente=novo;return true;
    }
    if(!line.startsWith('GUARDIA,'))return false;
    const {tipo,c}=gvCampos(line);m.linhas++;
    const mesma=()=>m.sessao&&c.sessao&&m.sessao.sessao===c.sessao;
    if(tipo==='estado')m.estado=c;
    else if(tipo==='modelo')m.modelo=c;
    else if(tipo==='autoteste')m.autoteste=c;
    else if(tipo==='sessao')gvNovaPartida(m,c);
    else if(tipo==='decisao'||tipo==='politica_gradual'||tipo==='resposta'||tipo==='features'){
      if(!m.sessao||(c.sessao&&!mesma()))gvNovaPartida(m,{sessao:c.sessao,nivel_inicial:c.nivel_inicial,parcial:'1'});
      const o=c.ordem||'?',q=m.perguntas[o]||(m.perguntas[o]={});
      q[tipo==='politica_gradual'?'gradual':tipo]=c;m.agente=null;
      if(tipo==='decisao'){gvSel=null;q.rep=null;}
      if(tipo==='features'&&q.decisao)gvrConferir(m,q); // cálculo reproduzido no painel (rede_guardia.js)
    }
    else if(tipo==='fim'){if(!m.sessao||(c.sessao&&!mesma()))gvNovaPartida(m,{sessao:c.sessao,parcial:'1'});m.fim=c;}
    else if(tipo==='erro')m.erros.push({motivo:c.pos[0],politica:c.politica});
    else return false;
    return true;
  }
  function gvPolitica(m){
    if(m.sessao&&m.sessao.politica)return {nome:m.sessao.politica,origem:'informada no início da partida'};
    for(const q of Object.values(m.perguntas))if(q.gradual&&q.gradual.politica)return {nome:q.gradual.politica,origem:'informada na linha politica_gradual'};
    if(m.estado&&m.estado.politica)return {nome:m.estado.politica,origem:'informada no estado do firmware'};
    return null;
  }
  const gvNivel=n=>{const i=Number(n);return n!==undefined&&n!==''&&Number.isInteger(i)&&GV_NIVEIS[i]?GV_NIVEIS[i]:null;};
  const gvCat=n=>{const i=Number(n);return n!==undefined&&n!==''&&Number.isInteger(i)&&GV_CATEG[i]?GV_CATEG[i]:null;};
  const gvLista=s=>s===undefined?null:s===''||s==='-'?[]:s.split('|').map(x=>gvNivel(x)||('nível '+x));
  const gvListaTxt=s=>{const l=gvLista(s);return l===null?gvIndisp('campo ausente no log'):l.length?gvEsc(l.join(', ')):'nenhum';};
  const gvNum=s=>{const x=Number(String(s).replace(',','.'));return s!==undefined&&s!==''&&Number.isFinite(x)?x:null;};
  const gvFmt=(x,d=1)=>x.toLocaleString('pt-BR',{minimumFractionDigits:d,maximumFractionDigits:d});
  function gvP(s){ // estimativa: número em [0,1], 'NA' (não avaliado), ausente ou inválida
    if(s===undefined)return {tipo:'ausente'};
    if(s==='NA')return {tipo:'na'};
    const x=gvNum(s);return x!==null&&x>=0&&x<=1?{tipo:'ok',v:x}:{tipo:'invalida',bruto:s};
  }
  // Exibição com 1 casa decimal. A escolha do firmware usa os valores completos do log.
  const gvPct=v=>gvFmt(v*100,1)+'%';
  const gvDistTxt=(v,alvo)=>gvFmt(Math.abs(v-alvo)*100,1);
  function gvPTxt(p,alvo){
    if(p.tipo==='ok'){
      let t='≈ '+gvPct(p.v);
      if(alvo!==null){const d=gvDistTxt(p.v,alvo);t+=d===gvFmt(0,1)?' · no alvo':' · '+d+' p.p. '+(p.v>alvo?'acima':'abaixo')+' do alvo';}
      return t;
    }
    if(p.tipo==='na')return gvIndisp('não avaliado nesta pergunta');
    if(p.tipo==='ausente')return gvIndisp('campo ausente no log');
    return gvIndisp('valor inválido no log: '+p.bruto);
  }
  const GV_MOTIVOS={
    nivel_inicial_pergunta_1:'Pergunta 1: usa o nível escolhido pela criança no menu. A estimativa é só registrada; a MLP não decide esta pergunta.',
    guardia:null, // texto depende da política, montado abaixo
    fallback_sem_modelo:'Modelo desativado ou reprovado no autoteste: a política usou o nível disponível mais próximo do inicial, sem usar estimativas.',
    fallback_previsao_invalida:'Alguma estimativa veio inválida: a política usou o nível disponível mais próximo do inicial, sem usar estimativas.',
    sem_nivel_compativel:'Nenhum nível compatível com a regra gradual: a Guardiã recusou a escolha em vez de violar a regra.',
  };
  function gvMotivoTxt(motivo,pol){
    if(motivo===undefined)return gvIndisp('motivo ausente no log');
    if(motivo==='guardia')return pol&&pol.nome===GV_GRADUAL
      ?'Entre os níveis compatíveis com a regra gradual e com pergunta disponível, escolheu o de estimativa mais próxima do alvo.'
      :pol&&pol.nome==='fluxo-0.8'?'Entre os níveis permitidos (inicial ± 1) com pergunta disponível, escolheu o de estimativa mais próxima do alvo.'
      :'Escolheu o nível de estimativa mais próxima do alvo entre os candidatos (política não informada no log).';
    if(GV_MOTIVOS[motivo])return GV_MOTIVOS[motivo];
    return 'Motivo registrado: <code>'+gvEsc(motivo)+'</code> '+gvIndisp('sem descrição neste painel');
  }
  const GV_ERROS={
    banco_inviavel:'Quiz bloqueado na verificação inicial: o banco não permite todas as partidas.',
    banco_inviavel_gradual:'Quiz bloqueado na verificação inicial: o banco não permite todas as partidas com a regra gradual (BANCO INCOMPLETO).',
    sem_nivel_compativel:'Guardiã recusou a próxima pergunta: nenhum nível compatível com a regra gradual tem pergunta no banco.',
  };
  function gvErroTxt(motivo){
    if(!motivo)return 'Erro registrado sem motivo: '+gvIndisp('motivo ausente no log');
    return GV_ERROS[motivo]?gvEsc(GV_ERROS[motivo])+' <code>'+gvEsc(motivo)+'</code>':'Erro registrado: <code>'+gvEsc(motivo)+'</code> '+gvIndisp('sem descrição neste painel');
  }
  function gvCaixa(classe,titulo,corpo){return `<div class="gv-box ${classe}"><h4>${titulo}</h4>${corpo}</div>`;}
  function gvMLP(d){
    const alvo=gvNum(d.alvo),esc=d.nivel_escolhido,perm=gvLista(d.niveis_permitidos)||[];
    let h='<div class="gv-bars">';
    for(let n=0;n<3;n++){
      const p=gvP(d['p'+n]),escolhido=String(n)===esc;
      const larg=p.tipo==='ok'?(p.v*100).toFixed(1):'100';
      h+=`<div class="gv-bar-row${escolhido?' gv-escolhido':''}" data-nivel="${n}"${p.tipo==='ok'?` title="valor registrado no log: p${n}=${gvEsc(d['p'+n])}"`:''}><span class="gv-bar-nome">${GV_NIVEIS[n]}</span>`
        +`<span class="gv-bar"><span class="gv-fill ${p.tipo==='ok'?'':'gv-fill-na'}" style="width:${larg}%"></span>${alvo!==null?`<span class="gv-alvo" style="left:${(alvo*100).toFixed(1)}%"></span>`:''}</span>`
        +`<span class="gv-bar-txt">${gvPTxt(p,alvo)}${escolhido?' <b class="gv-marca">◆ escolhido pela política</b>':''}</span></div>`;
    }
    h+='</div>';
    h+=gvEmpateVisual(d);
    h+=`<p class="gv-legenda">${alvo!==null?'Linha tracejada: alvo de '+Math.round(alvo*100)+'% de P(acerto).':gvIndisp('alvo ausente no log')} `
      +'São estimativas de um modelo experimental treinado com dados sintéticos: indicam tendência, não certeza, e não avaliam a criança.</p>';
    void perm;return h;
  }
  // Empate visual: o nível escolhido aparece com a mesma estimativa ou a mesma distância
  // do alvo (1 casa) que outro nível avaliado. Só faz sentido quando as estimativas decidiram.
  function gvEmpateVisual(d){
    const alvo=gvNum(d.alvo),esc=gvNum(d.nivel_escolhido);
    if(d.motivo!=='guardia'||alvo===null||esc===null)return '';
    const pe=gvP(d['p'+esc]);if(pe.tipo!=='ok')return '';
    const disp=gvLista(d.niveis_disponiveis);
    const iguais=[];
    for(let n=0;n<3;n++){
      if(n===esc)continue;
      const p=gvP(d['p'+n]);if(p.tipo!=='ok')continue;
      if(disp&&disp.length&&!disp.includes(GV_NIVEIS[n]))continue;
      if(gvPct(p.v)===gvPct(pe.v)||gvDistTxt(p.v,alvo)===gvDistTxt(pe.v,alvo))iguais.push(n);
    }
    if(!iguais.length)return '';
    const nomes=[esc,...iguais].map(n=>GV_NIVEIS[n]).join(' e ');
    const brutos=[esc,...iguais].map(n=>'p'+n+'='+gvEsc(d['p'+n])).join(', ');
    return `<p class="gv-empate" id="gvEmpate"><b>Empate visual:</b> ${gvEsc(nomes)} aparecem iguais com 1 casa decimal. `
      +`A escolha usa os valores sem arredondamento (${brutos}) e o desempate da política: menor distância exata do alvo; `
      +'se ainda empatar, o nível inicial; depois, o nível mais baixo.</p>';
  }
  function gvEscolha(d,pol){
    const nv=gvNivel(d.nivel_escolhido),p=gvP(d.p_escolhido),alvo=gvNum(d.alvo);
    let h=`<p class="gv-grande">${nv?gvEsc(nv):gvIndisp('nível ausente no log')}</p>`;
    h+=`<p>Estimativa da MLP para esse nível: ${gvPTxt(p,alvo)}</p>`;
    h+=`<p><b>Por quê:</b> ${gvMotivoTxt(d.motivo,pol)}</p>`;
    h+=`<p>Níveis permitidos: ${gvListaTxt(d.niveis_permitidos)} · com pergunta disponível: ${gvListaTxt(d.niveis_disponiveis)}</p>`;
    const cs=gvCat(d.categoria_sorteada),cu=gvCat(d.categoria);
    h+=`<p>Categoria: ${cu?gvEsc(cu):gvIndisp('ausente no log')}${d.categoria_substituida==='1'&&cs?' (sorteada: '+gvEsc(cs)+', trocada por falta de pergunta)':''}</p>`;
    return h;
  }
  function gvRegra(g){
    const s=gvNum(g.nivel_sem_regra),a=gvNum(g.anterior);
    if(s===null||a===null)return gvIndisp('não dá para deduzir dos campos do log');
    if(g.anterior_acertou==='0'&&s>a)return 'não subir de nível depois de erro ou tempo esgotado';
    if(Math.abs(s-a)>1)return 'no máximo 1 nível de diferença da pergunta anterior';
    return gvIndisp('não dá para deduzir dos campos do log');
  }
  function gvProtecao(g,pol,d){
    if(!pol)return {cls:'',txt:gvIndisp('o log não informa a política ativa'),curto:'indisponível'};
    if(pol.nome!==GV_GRADUAL)return {cls:'gv-off',txt:`<p>Não aplicável: a política ativa é <b>${gvEsc(pol.nome)}</b>, que não tem proteção gradual.</p>`,curto:'não se aplica'};
    if(!g)return {cls:'',txt:`<p>${gvIndisp('o log não traz a linha GUARDIA,politica_gradual desta pergunta')}</p>`,curto:'indisponível'};
    const ant=gvNivel(g.anterior);
    let h=`<p>Pergunta anterior: ${g.anterior==='-'?'nenhuma (primeira pergunta)':ant?gvEsc(ant)+(g.anterior_acertou==='1'?', acertou':g.anterior_acertou==='0'?', errou ou esgotou o tempo':', resultado '+gvIndisp()):gvIndisp('campo ausente no log')}</p>`;
    h+=`<p>Níveis compatíveis com a regra: ${gvListaTxt(g.niveis_compativeis)} · com pergunta no banco: ${gvListaTxt(g.candidatos)}</p>`;
    let cls='',curto='';
    if(g.motivo==='sem_nivel_compativel'||g.nivel==='-1'){
      cls='gv-recusa';curto='recusou';
      h=`<p class="gv-alerta">RECUSA · nenhum nível compatível. A Guardiã não escolhe um nível que viole a regra gradual.</p>`+h;
    }else if(g.limitou==='1'){
      cls='gv-atuou';curto='atuou';
      const sem=gvNivel(g.nivel_sem_regra),com=gvNivel(g.nivel);
      h=`<p class="gv-alerta">PROTEÇÃO ATUOU · sem a regra gradual, o fluxo-0.8 escolheria <b>${sem?gvEsc(sem):gvIndisp()}</b>; a regra limitou para <b>${com?gvEsc(com):gvIndisp()}</b>.</p>`
        +`<p>Regra que impediu (deduzida dos campos anterior e nivel_sem_regra): ${gvRegra(g)}.</p>`+h;
    }else if(g.limitou==='0'){
      curto='não precisou';
      h=(g.nivel_sem_regra==='-1'?'<p>Pergunta 1: nível escolhido pela criança; não há pergunta anterior para comparar.</p>'
        :`<p>Não precisou intervir: a escolha do fluxo-0.8 (${gvEsc(gvNivel(g.nivel_sem_regra)||g.nivel_sem_regra)}) já era compatível.</p>`)+h;
    }else{curto='indisponível';h=`<p>${gvIndisp('campo limitou ausente no log')}</p>`+h;}
    const c8=gvCat(g.categoria_fluxo08),cg=gvCat(g.categoria);
    if(c8&&cg&&g.categoria_fluxo08!==g.categoria)h+=`<p>Categoria: o fluxo-0.8 usaria ${gvEsc(c8)}; a regra usou ${gvEsc(cg)} (primeira da ordem sorteada com nível compatível).</p>`;
    void d;return {cls,txt:h,curto};
  }
  function gvResultado(r){
    if(!r)return null;
    if(r.esgotou==='1')return 'tempo esgotado';
    if(r.acertou==='1')return 'acertou';
    if(r.acertou==='0')return 'errou';
    return null;
  }
  function gvRender(){
    gvMonAgendar();
    const m=gvModelos[gvFonte],el=$('gvConteudo');
    $('gvFonte').textContent=gvFonte==='demo'?'REPRODUÇÃO DE LOG':'AO VIVO · USB';
    $('gvFonte').classList.toggle('gv-tag-demo',gvFonte==='demo');
    $('gvDemoNota').hidden=!(gvFonte==='demo'&&m.nota);$('gvDemoNota').textContent=gvFonte==='demo'&&m.nota?'NOTA DO ARQUIVO NESTE TRECHO: '+m.nota:'';
    $('gvAoVivo').textContent=gvFonte==='demo'&&gvLinhasAoVivoDuranteDemo?' · '+gvLinhasAoVivoDuranteDemo+' linhas ao vivo recebidas em segundo plano':'';
    if(m.agente){
      const est={PREP:'preparando',PLAYING:'missão em andamento',WIN:'missão cumprida',LOSE:'missão encerrada'}[m.agente]||m.agente;
      gvRedeSincronizar(m,null);
      el.innerHTML=`<div class="gv-agente" id="gvAgente"><strong>MODO AGENTE SECRETO · ${gvEsc(est)}</strong><p>A Guardiã IA não participa desta modalidade. A senha e o cronômetro são controlados pelo ESP32, e as pistas são geradas por este painel. O quadro da Guardiã volta a ser usado no Quiz.</p></div>`;
      return;
    }
    const pol=gvPolitica(m);let h='';
    const auto=m.autoteste&&m.autoteste.resultado,est=m.estado||{};
    h+=`<div class="gv-ctx"><span>Política ativa: ${pol?`<b>${gvEsc(pol.nome)}</b> <small>(${gvEsc(pol.origem)})</small>`:gvIndisp('não informada no log')}</span>`
      +`<span>Modelo: ${m.modelo&&m.modelo.id?gvEsc(m.modelo.id):m.sessao&&m.sessao.modelo?gvEsc(m.sessao.modelo):gvIndisp()}</span>`
      +`<span>Autoteste: ${auto?gvEsc(auto):gvIndisp()}</span>`
      +`<span>Banco viável: ${est.banco_viavel==='1'?'sim':est.banco_viavel==='0'?'NÃO':gvIndisp()}${pol&&pol.nome===GV_GRADUAL?' · para a regra gradual: '+(est.banco_viavel_gradual==='1'?'sim':est.banco_viavel_gradual==='0'?'NÃO':gvIndisp()):''}</span></div>`;
    for(const e of m.erros.slice(-3))h+=`<p class="gv-alerta gv-erro">${gvErroTxt(e.motivo)}${e.politica?' · política '+gvEsc(e.politica):''}</p>`;
    if(!m.sessao){
      gvRedeSincronizar(m,null);
      h+=`<p class="gv-vazio">Nenhuma partida de Quiz ${gvFonte==='demo'?'neste trecho do log':'recebida ainda'}. Escolha o Quiz no cofre (1, 2 ou 3)${gvFonte==='usb'?' ou abra um log para reproduzir':''}.</p>`;
      el.innerHTML=h;return;
    }
    const s=m.sessao,ordens=['1','2','3'];
    const ultima=[...ordens].reverse().find(o=>m.perguntas[o]&&(m.perguntas[o].decisao||m.perguntas[o].gradual));
    const sel=gvSel&&m.perguntas[gvSel]?gvSel:ultima;
    gvRedeSincronizar(m,sel);
    h+=`<p class="gv-partida">Partida de Quiz ${gvEsc(s.sessao||'?')} · ${m.fim?'encerrada':m.erros.length?'interrompida':'em andamento'} · nível inicial: ${gvNivel(s.nivel_inicial)?gvEsc(gvNivel(s.nivel_inicial)):gvIndisp()}${s.parcial?' · '+gvIndisp('início da partida ausente no log'):''}</p>`;
    if(sel){
      const q=m.perguntas[sel],d=q.decisao;
      if(d){
        const pr=gvProtecao(q.gradual,pol,d),cat=gvCat(d.categoria);
        h+=`<h3 class="gv-perg">PERGUNTA ${gvEsc(sel)} DE 3 · ${cat?gvEsc(cat):gvIndisp()} · <code>${gvEsc(d.pergunta||'?')}</code></h3><div class="gv-grid">`
          +gvCaixa('gv-mlp','1 · PREVISÃO DA MLP <small>P(acerto) estimada por nível</small>',gvMLP(d))
          +gvCaixa('gv-pol','2 · ESCOLHA DA POLÍTICA <small>'+(pol?gvEsc(pol.nome):'política indisponível')+'</small>',gvEscolha(d,pol))
          +gvCaixa('gv-grad '+pr.cls,'3 · PROTEÇÃO GRADUAL',pr.txt)+'</div>';
      }else{
        h+=`<h3 class="gv-perg">PERGUNTA ${gvEsc(sel)} DE 3</h3><p>${gvIndisp('linha GUARDIA,decisao desta pergunta ausente no log')}</p>`;
        if(q.gradual){const pr=gvProtecao(q.gradual,pol,null);h+='<div class="gv-grid gv-grid-1">'+gvCaixa('gv-grad '+pr.cls,'3 · PROTEÇÃO GRADUAL',pr.txt)+'</div>';}
      }
    }
    h+='<div class="gv-tabela"><table><thead><tr><th>Pergunta</th><th>Nível escolhido</th><th>Estimativa da MLP<br><small>p/ nível escolhido</small></th><th>Resultado</th><th>Tempo</th><th>Dica</th><th>Proteção gradual</th></tr></thead><tbody>';
    let acertos=0,respondidas=0,dicas=0,dicaOk=true,tempo=0,tempoOk=true;
    for(const o of ordens){
      const q=m.perguntas[o]||{},d=q.decisao,r=q.resposta;
      const falta=m.fim?gvIndisp('ausente no log'):m.erros.length?'<span class="gv-na">não aconteceu</span>':'<span class="gv-na">aguardando</span>';
      const res=gvResultado(r);
      if(r){respondidas++;if(r.acertou==='1')acertos++;if(r.usou_dica==='1')dicas++;else if(r.usou_dica!=='0')dicaOk=false;
        const t=gvNum(r.tempo_gasto_s);if(t===null)tempoOk=false;else tempo+=t;}
      const tg=r?gvNum(r.tempo_gasto_s):null,tl=r?gvNum(r.tempo_limite_s):null;
      h+=`<tr data-ordem="${o}" tabindex="0" class="${o===sel?'gv-sel':''}"><td>${o}/3</td>`
        +`<td>${d?(gvNivel(d.nivel_escolhido)?gvEsc(gvNivel(d.nivel_escolhido)):gvIndisp()):falta}</td>`
        +`<td>${d?gvPTxt(gvP(d.p_escolhido),null):falta}</td>`
        +`<td class="gv-res-${res?res.replace(' ','-'):'na'}">${r?(res||gvIndisp()):falta}</td>`
        +`<td>${r?(tg!==null?gvFmt(tg)+' s':gvIndisp())+(tl!==null?' de '+gvFmt(tl,0)+' s':''):falta}</td>`
        +`<td>${r?(r.usou_dica==='1'?'sim':r.usou_dica==='0'?'não':gvIndisp()):falta}</td>`
        +`<td>${d||q.gradual?gvProtecao(q.gradual,pol,d).curto:falta}</td></tr>`;
    }
    h+='</tbody></table></div>';
    const fa=m.fim?gvNum(m.fim.acertos):null,desb=m.fim&&m.fim.desbloqueio;
    h+=`<div class="gv-totais"><span>Acertos: <b>${m.fim?(fa!==null?fa+'/3':gvIndisp()):acertos+'/3'+(m.erros.length?'':' até agora')}</b></span>`
      +`<span>Dicas usadas: <b>${respondidas?(dicaOk?dicas:gvIndisp()):'0'}</b></span>`
      +`<span>Tempo somado das respostas: <b>${respondidas?(tempoOk?gvFmt(tempo)+' s':gvIndisp()):'0 s'}</b></span>`
      +`<span>Desbloqueio: <b>${m.fim?(desb==='SIM'?'SIM · cofre aberto':desb==='NAO'?'NÃO':gvIndisp()):m.erros.length?'NÃO · partida interrompida':'aguardando fim da partida'}</b></span></div>`;
    el.innerHTML=h;
  }
  function gvAgendarRender(){gvMonAgendar();if(!gvRaf)gvRaf=requestAnimationFrame(()=>{gvRaf=0;gvRender();});}
  // Gancho chamado por packet(): só lê a linha. Erros aqui nunca afetam o protocolo AG.
  function guardiaVisUSB(line){
    try{if(gvLinha(gvModelos.usb,line)){gvMonAgendar();if(gvFonte==='usb')gvAgendarRender();else{gvLinhasAoVivoDuranteDemo++;gvAgendarRender();}}}catch(e){}
  }
  // ---- Reprodução de arquivo de log (DADOS DE DEMONSTRAÇÃO) ----
  const gvRep={linhas:[],i:0,timer:null,tocando:false};
  function gvBotoes(){
    const tem=gvFonte==='demo',fim=gvRep.i>=gvRep.linhas.length;
    $('gvPlay').disabled=!tem||fim;$('gvPlay').textContent=gvRep.tocando?'❚❚ PAUSAR':'▶ REPRODUZIR';
    $('gvPasso').disabled=!tem||fim||gvRep.tocando;$('gvFim').disabled=!tem||fim;$('gvSair').disabled=!tem;
    $('gvProg').textContent=tem?`linha ${gvRep.i} de ${gvRep.linhas.length}`+(gvRep.i&&Number.isFinite(gvRep.linhas[gvRep.i-1].t)?' · '+new Date(gvRep.linhas[gvRep.i-1].t).toISOString().slice(11,19)+' UTC':''):'';
  }
  function gvCarregar(texto,nome){
    const brutas=texto.split(/\r?\n/);
    // Linhas '#' antes do primeiro dado formam o cabeçalho; as do meio viram notas do trecho.
    const inicio=brutas.findIndex(l=>l.trim()&&!l.startsWith('#'));
    const cab=brutas.slice(0,inicio<0?brutas.length:inicio).filter(l=>l.startsWith('#')).map(l=>l.replace(/^#\s*/,'')).join(' ');
    gvRep.linhas=(inicio<0?[]:brutas.slice(inicio)).filter(l=>l.trim()).map(l=>{
      if(l.startsWith('#'))return {t:NaN,s:'',nota:l.replace(/^#\s*/,'')};
      const x=/^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d+)?Z)\s+(.*)$/.exec(l);
      return x?{t:Date.parse(x[1]),s:x[2]}:{t:NaN,s:l.trim()};
    });
    clearTimeout(gvRep.timer);gvRep.i=0;gvRep.tocando=false;
    gvModelos.demo=gvNovoModelo();gvFonte='demo';gvSel=null;gvLinhasAoVivoDuranteDemo=0;
    $('gvDemo').hidden=false;
    $('gvDemoInfo').textContent=`Arquivo: ${nome} · ${gvRep.linhas.length} linhas. `+(cab?'Cabeçalho do arquivo: '+cab:'O arquivo não traz cabeçalho de origem.');
    gvBotoes();gvRender();
  }
  function gvPassoUm(){if(gvRep.i>=gvRep.linhas.length)return false;const l=gvRep.linhas[gvRep.i++];if(l.nota!==undefined){gvModelos.demo.nota=l.nota;return true;}return gvLinha(gvModelos.demo,l.s);}
  function gvAgendar(){
    clearTimeout(gvRep.timer);
    if(!gvRep.tocando)return;
    if(gvRep.i>=gvRep.linhas.length){gvRep.tocando=false;gvBotoes();return;}
    const a=gvRep.linhas[gvRep.i-1],b=gvRep.linhas[gvRep.i],vel=Number($('gvVel').value)||1;
    const dt=a&&Number.isFinite(a.t)&&Number.isFinite(b.t)?Math.max(0,b.t-a.t):250;
    gvRep.timer=setTimeout(()=>{
      gvPassoUm();
      while(gvRep.i<gvRep.linhas.length&&Number.isFinite(gvRep.linhas[gvRep.i].t)&&gvRep.linhas[gvRep.i].t===gvRep.linhas[gvRep.i-1].t)gvPassoUm();
      gvAgendarRender();gvBotoes();gvAgendar();
    },Math.min(dt/vel,1500));
  }
  const gvis=document.createElement('section');gvis.className='gvis';gvis.id='guardiaVis';gvis.setAttribute('aria-label','Atuação da Guardiã IA no Quiz');
  gvis.innerHTML=`<div class="gv-head"><strong>GUARDIÃ IA · ATUAÇÃO NO QUIZ</strong><span id="gvFonte" class="gv-tag">AO VIVO · USB</span></div>
  <div id="gvDemo" class="gv-demo" hidden><b>DADOS DE DEMONSTRAÇÃO</b> · reprodução de arquivo de log; não vêm do cofre conectado nem de crianças.<small id="gvDemoInfo"></small><small id="gvAoVivo"></small><strong id="gvDemoNota" class="gv-demo-nota" hidden></strong></div>
  <div id="gvConteudo" aria-live="polite"></div>
  <div class="gv-replay"><span>Reproduzir log:</span><label class="gv-btn gv-file">ABRIR ARQUIVO…<input type="file" id="gvArquivo" accept=".txt,.log,text/plain"></label><button class="gv-btn" id="gvPlay" disabled>▶ REPRODUZIR</button><button class="gv-btn" id="gvPasso" disabled>PRÓXIMO EVENTO</button><button class="gv-btn" id="gvFim" disabled>IR AO FIM</button><label>Velocidade <select id="gvVel"><option value="1">1×</option><option value="4" selected>4×</option><option value="16">16×</option><option value="64">64×</option></select></label><button class="gv-btn" id="gvSair" disabled>VOLTAR AO VIVO</button><span id="gvProg"></span></div>
  <p class="gv-nota">Somente leitura: este quadro não envia comandos ao ESP32 e não muda o Quiz nem a missão. No Agente Secreto a Guardiã não participa.</p>`;
  panel.insertBefore(gvis,$('guardiaLogs'));
  $('gvArquivo').onchange=()=>{const f=$('gvArquivo').files[0];if(!f)return;f.text().then(t=>gvCarregar(t,f.name));$('gvArquivo').value='';};
  $('gvPlay').onclick=()=>{gvRep.tocando=!gvRep.tocando;gvBotoes();gvAgendar();};
  $('gvPasso').onclick=()=>{while(gvRep.i<gvRep.linhas.length&&!gvPassoUm()){}gvRender();gvBotoes();};
  $('gvFim').onclick=()=>{gvRep.tocando=false;clearTimeout(gvRep.timer);while(gvRep.i<gvRep.linhas.length)gvPassoUm();gvRender();gvBotoes();};
  $('gvSair').onclick=()=>{gvRep.tocando=false;clearTimeout(gvRep.timer);gvFonte='usb';gvSel=null;$('gvDemo').hidden=true;gvBotoes();gvRender();};
  $('gvConteudo').addEventListener('click',e=>{const tr=e.target.closest('tr[data-ordem]');if(tr){gvSel=tr.dataset.ordem;gvRender();}});
  $('gvConteudo').addEventListener('keydown',e=>{const tr=e.target.closest('tr[data-ordem]');if(tr&&(e.key==='Enter'||e.key===' ')){e.preventDefault();gvSel=tr.dataset.ordem;gvRender();}});
  // gvRender() inicial fica no fim de rede_guardia.js, depois de a rede ser montada.
