  // ===== Rede neural da Guardiã no painel principal · CÁLCULO REPRODUZIDO NO PAINEL (proposta) =====
  // O ESP32 envia as features (GUARDIA,features, 6 casas) e as previsões finais (GUARDIA,decisao, 6 casas).
  // A inferência é refeita com os mesmos pesos, padronização e funções (CoflumeGuardia, em guardia_comum.js)
  // e a saída é conferida com a previsão do ESP32. As ativações intermediárias NÃO são medições da placa.
  // Só lê o que já foi calculado: não envia nada ao ESP32 nem mexe em áudio, protocolo AG ou controles.
  const GC=window.CoflumeGuardia;
  const GVR_TOL=GC.TOL,gvrExp=GC.exp;
  // Refaz as avaliações de uma pergunta e confere com o ESP32. Chamado quando chega a linha features.
  function gvrConferir(m,q){
    const d=q.decisao,f=q.features,r={avaliacoes:[],erro:null};
    q.rep=r;
    const mod=d.modelo||(m.sessao&&m.sessao.modelo)||(m.modelo&&m.modelo.id);
    if(mod&&mod!==GC.modelo.id){r.erro='o log é do modelo '+mod+'; o painel tem os pesos de '+GC.modelo.id;return;}
    if(m.modelo&&m.modelo.hash&&m.modelo.hash!==GC.modelo.hash_parametros){r.erro='hash dos parâmetros do log diferente do modelo do painel';return;}
    if(!mod)r.aviso='o log não informa o modelo; usados os pesos de '+GC.modelo.id;
    if(d.categoria!==undefined&&f.categoria!==undefined&&d.categoria!==f.categoria){r.erro='categoria da linha features diferente da decisão';return;}
    if(d.nivel_escolhido!==undefined&&f.nivel_candidato!==undefined&&d.nivel_escolhido!==f.nivel_candidato)r.aviso='a linha features é de outro nível candidato; os demais campos foram usados';
    const ordem=(gvLista(d.niveis_disponiveis)||[]).length?d.niveis_disponiveis.split('|').map(Number).filter(n=>n>=0&&n<3):[0,1,2];
    for(const n of [0,1,2].filter(n=>!ordem.includes(n)))ordem.push(n);
    for(const n of ordem){
      const p=gvP(d['p'+n]);if(p.tipo==='na'||p.tipo==='ausente')continue;
      const raw=GC.entrada(f,n,gvNum);if(!raw){r.avaliacoes=[];r.erro='linha GUARDIA,features incompleta';return;}
      const valida=GC.valida(raw),c=valida?GC.calcular(raw):null,esp=p.tipo==='ok'?p.v:null;
      const dif=c&&esp!==null?Math.abs(c.p-esp):null;
      r.avaliacoes.push({nivel:n,raw,c,esp,bruto:d['p'+n],dif,ok:dif!==null&&dif<=GVR_TOL,invalida:!valida});
      if(dif!==null){m.conf.n++;if(dif<=GVR_TOL)m.conf.ok++;m.conf.max=Math.max(m.conf.max,dif);}
    }
  }
  const gvRede=document.createElement('details');gvRede.id='gvRede';gvRede.className='gv-rede';gvRede.open=true;
  gvRede.innerHTML=`<summary>REDE NEURAL DA GUARDIÃ · 13 → 8 → 1 · <b>cálculo reproduzido no painel</b></summary>
  <p class="gv-rede-aviso">As ativações são calculadas <b>neste computador</b>, com os mesmos pesos do firmware (<code>${gvEsc(GC.modelo.id)}</code>, hash ${gvEsc(GC.modelo.hash_parametros.slice(0,12))}…), a mesma padronização e as mesmas funções (ReLU e sigmoide), a partir das features enviadas pelo ESP32. <b>Não são medições internas da placa:</b> o ESP32 envia só as features e a previsão final, usada aqui para conferir o cálculo.</p>
  <p id="gvrMsg" class="gv-rede-msg" hidden></p>
  <div class="gv-rede-topo"><span id="gvrAvaliando" class="gv-rede-avaliando"></span><span class="gv-rede-lenta" id="gvrLenta">visualização em câmera lenta · o pulso nas conexões é recurso visual, não o tempo real do cálculo</span>
  <span class="gv-rede-ctl"><label><input type="checkbox" id="gvrAnimar"> animar</label><label>ritmo <select id="gvrRitmo"><option value="1">câmera lenta</option><option value="0.3">mais rápido</option></select></label><button class="gv-btn" id="gvrRepetir">↻ REPETIR</button></span></div>
  <div class="gv-rede-corpo"><div class="gv-rede-svg"><svg id="gvrSvg" viewBox="0 0 760 450" role="img" aria-label="Rede neural 13 entradas, 8 neurônios ocultos, 1 saída"></svg></div>
  <div class="gv-rede-lado"><p class="gv-rede-tit">Avaliações da rede nesta pergunta</p><ol id="gvrSeq" class="gv-rede-seq"></ol>
  <div id="gvrDecisao" class="gv-rede-decisao" hidden></div></div></div>
  <p id="gvrConf" class="gv-rede-conf"></p>
  <p class="gv-legenda">Cores: entrada padronizada acima da média em âmbar, abaixo em azul; neurônio oculto aceso conforme a ativação ReLU (apagado = 0); saída conforme P(acerto). Conexões: verde = peso positivo, laranja = negativo; espessura = |peso|. Passe o mouse para ver os valores.</p>`;
  $('gvConteudo').after(gvRede);
  const svg=$('gvrSvg'),gvrRede=GC.criarRede(svg,'gvr');
  // ---- Sequência, decisão e animação ----
  let gvrChave=null,gvrQ=null,gvrCtl=null;
  const gvrAnimadas=new WeakSet(); // cada pergunta anima uma vez; ao voltar a ela, estado final parado
  function gvrParar(){if(gvrCtl){gvrCtl.parar();gvrCtl=null;}gvrRede.pulso(1,false);gvrRede.pulso(2,false);}
  function gvrSeq(r,atual,feitas,d){
    const esc=d.nivel_escolhido,ol=$('gvrSeq');let h='';
    r.avaliacoes.forEach((a,k)=>{
      const st=k<feitas?'feita':k===atual?'avaliando':'pendente';
      let det='';
      if(k<feitas){
        if(a.invalida)det='entrada fora da faixa: o runtime devolveria INVALIDO';
        else det=`rede reproduzida: ${a.c.p.toFixed(6)} (≈ ${gvPct(a.c.p)}) · ESP32 registrou: ${gvEsc(a.bruto)}`
          +(a.dif===null?' · '+gvIndisp('previsão do ESP32 inválida'):` · |Δ| = ${gvrExp(a.dif)} · <b class="${a.ok?'gvr-ok':'gvr-nok'}">${a.ok?'confere':'NÃO CONFERE'}</b>`);
      }
      h+=`<li class="gvr-${st}" data-k="${k}" tabindex="0"><span class="gvr-seq-cab">${k+1} de ${r.avaliacoes.length} · ${GV_NIVEIS[a.nivel]}`
        +`${st==='avaliando'?' <b>▶ avaliando</b>':st==='pendente'?' · aguardando':''}${k<feitas&&String(a.nivel)===esc&&r.fim?' <span class="gvr-esc">◆ escolhido pela política</span>':''}</span>`
        +(det?`<span class="gvr-seq-det">${det}</span>`:'')+'</li>';
    });
    ol.innerHTML=h;
  }
  function gvrMostrarDecisao(d){
    const box=$('gvrDecisao'),nv=gvNivel(d.nivel_escolhido),pol=gvPolitica(gvModelos[gvFonte]);
    box.innerHTML=`<b>DECISÃO DA POLÍTICA</b> <small>(não é a rede)</small><br>${pol?gvEsc(pol.nome)+': ':''}<b>${nv?gvEsc(nv):gvIndisp()}</b><br><small>${gvMotivoTxt(d.motivo,pol)}</small>`;
    box.hidden=false;
  }
  function gvrMensagem(t){const e=$('gvrMsg');e.hidden=!t;e.innerHTML=t||'';}
  function gvrMostrarEscolhida(r,d,hmax,prefixo){
    const escolhida=r.avaliacoes.find(a=>String(a.nivel)===d.nivel_escolhido);
    const a=escolhida||r.avaliacoes[r.avaliacoes.length-1];
    gvrRede.desenhar(a,3,hmax);
    $('gvrAvaliando').textContent=a?prefixo+' · '+(escolhida?'mostrando a dificuldade escolhida pela política: ':'mostrando uma dificuldade avaliada: ')+GV_NIVEIS[a.nivel]+'; clique numa avaliação para rever':'';
  }
  function gvrAnimar(jaVista){
    const q=gvrQ;if(!q||!q.rep)return;
    gvrParar();
    const r=q.rep,d=q.decisao,hmax=GC.hmax(r.avaliacoes),k=r.avaliacoes.length;r.fim=false;
    $('gvrDecisao').hidden=true;
    const anima=$('gvrAnimar').checked&&!jaVista;
    $('gvrLenta').hidden=!anima;
    if(!anima){ // estado final, sem movimento
      r.fim=true;gvrSeq(r,-1,k,d);
      gvrMostrarEscolhida(r,d,hmax,jaVista?'Pergunta já animada':'Sem animação');gvrMostrarDecisao(d);return;
    }
    gvrCtl=GC.animar(gvrRede,r.avaliacoes,{ritmo:Number($('gvrRitmo').value)||1,
      aoIniciar:i=>{gvrSeq(r,i,i,d);$('gvrAvaliando').textContent=`Avaliando: ${GV_NIVEIS[r.avaliacoes[i].nivel]} (${i+1} de ${k})`;},
      aoSaida:i=>gvrSeq(r,-1,i+1,d),
      aoFim:()=>{r.fim=true;gvrCtl=null;gvrSeq(r,-1,k,d);gvrMostrarEscolhida(r,d,hmax,'Avaliações concluídas');gvrMostrarDecisao(d);}});
  }
  function gvrConfTxt(m){
    const c=m.conf,e=$('gvrConf');
    if(!c.n){e.textContent='';return;}
    const tudo=c.ok===c.n;
    e.className='gv-rede-conf'+(tudo?'':' gvr-nok');
    e.innerHTML=`Conferência acumulada (${gvFonte==='demo'?'reprodução de log':'ao vivo'}): ${c.ok} de ${c.n} previsões do ESP32 recalculadas no painel dentro da tolerância de ${gvrExp(GVR_TOL)} `
      +`(features e previsões chegam com 6 casas decimais; maior |Δ| = ${gvrExp(c.max)}).`+(tudo?'':' <b>Há previsões que não conferem: o cálculo do painel não reproduz a placa nesses casos.</b>');
  }
  // Chamado pelo gvRender(). Só reinicia a animação quando muda a pergunta mostrada; nunca acumula.
  function gvRedeSincronizar(m,sel){
    gvrConfTxt(m);
    if(m.agente){
      if(gvrChave==='agente')return;
      gvrChave='agente';gvrParar();$('gvrLenta').hidden=true;gvRede.classList.add('gvr-agente');
      gvrQ=null;gvrRede.desenhar(null,0,1);$('gvrSeq').innerHTML='';$('gvrDecisao').hidden=true;$('gvrAvaliando').textContent='';
      svg.classList.add('gvr-desligada');
      gvrMensagem('<b>Modo Agente Secreto:</b> a Guardiã IA não participa desta modalidade. A rede não é calculada nem animada.');
      return;
    }
    svg.classList.remove('gvr-desligada');gvRede.classList.remove('gvr-agente');
    const q=sel&&m.perguntas[sel];
    const recusada=q&&q.gradual&&(q.gradual.motivo==='sem_nivel_compativel'||q.gradual.nivel==='-1');
    const chave=gvFonte+'|'+(m.sessao&&m.sessao.sessao)+'|'+sel+'|'+(q&&q.decisao?'d':'-')+(q&&q.rep?'r':'-')+'|'+!!recusada;
    if(chave===gvrChave)return;
    gvrChave=chave;gvrParar();gvrQ=q||null;$('gvrLenta').hidden=true;
    gvrRede.desenhar(null,0,1);$('gvrSeq').innerHTML='';$('gvrDecisao').hidden=true;$('gvrAvaliando').textContent='';
    if(recusada){
      gvrMensagem('<b>Pergunta recusada pela regra gradual.</b> Nenhuma dificuldade compatível tinha pergunta disponível. A rede não foi usada nesta pergunta.');
      $('gvrDecisao').innerHTML='<b>DECISÃO DA POLÍTICA: RECUSADA</b><br><small>A partida foi interrompida em vez de violar a regra gradual.</small>';
      $('gvrDecisao').hidden=false;return;
    }
    if(!q||!q.decisao){gvrMensagem('Aguardando uma pergunta do Quiz com as linhas GUARDIA,decisao e GUARDIA,features.');return;}
    if(!q.features){gvrMensagem(gvIndisp('reprodução indisponível: linha GUARDIA,features desta pergunta ausente no log'));gvrMostrarDecisao(q.decisao);return;}
    const r=q.rep;
    if(!r||r.erro){gvrMensagem(gvIndisp('reprodução indisponível: '+(r?r.erro:'sem dados')));gvrMostrarDecisao(q.decisao);return;}
    if(!r.avaliacoes.length){gvrMensagem('A rede não foi avaliada nesta pergunta (nenhuma previsão registrada). '+gvMotivoTxt(q.decisao.motivo,gvPolitica(m)));gvrMostrarDecisao(q.decisao);return;}
    gvrMensagem(r.aviso?gvIndisp(r.aviso):'');
    const jaVista=gvrAnimadas.has(q);gvrAnimadas.add(q);
    gvrAnimar(jaVista);
  }
  $('gvrSeq').addEventListener('click',e=>{ // mostra uma avaliação parada
    const li=e.target.closest('li[data-k]');if(!li||!gvrQ||!gvrQ.rep)return;
    const r=gvrQ.rep,a=r.avaliacoes[Number(li.dataset.k)];if(!a||!r.fim)return;
    gvrRede.desenhar(a,3,GC.hmax(r.avaliacoes));$('gvrAvaliando').textContent='Revisando a dificuldade avaliada: '+GV_NIVEIS[a.nivel]+' · escolhida pela política: '+(gvNivel(gvrQ.decisao.nivel_escolhido)||'indisponível');
  });
  $('gvrRepetir').onclick=()=>{if(gvrQ&&gvrQ.rep&&!gvModelos[gvFonte].agente)gvrAnimar();};
  $('gvrAnimar').onchange=()=>{if(gvrQ&&gvrQ.rep&&!gvModelos[gvFonte].agente)gvrAnimar();};
  try{$('gvrAnimar').checked=!window.matchMedia('(prefers-reduced-motion: reduce)').matches;}catch(e){$('gvrAnimar').checked=true;}
