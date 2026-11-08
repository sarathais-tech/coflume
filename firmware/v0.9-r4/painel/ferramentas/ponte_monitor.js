  // ===== Ponte para o MONITOR DA IA (janela de exposição, proposta) =====
  // Só este painel fala com a USB. A janela de exposição é este mesmo arquivo aberto com ?monitor=ia.
  // Comunicação: window.open + postMessage (funciona com o HTML aberto localmente, file://).
  // O monitor recebe um resumo pronto para exibição: nunca a senha, as pistas nem linhas AG.
  // Fechar ou recarregar o monitor não afeta o jogo, o áudio nem a USB: aqui só se lê o estado.
  let gvMon=null,gvMonSeq=0,gvMonUltimo='',gvMonTimer=null;
  const gvMonUrl=()=>location.href.split(/[?#]/)[0]+'?monitor=ia';
  function gvMonAberto(){try{return !!gvMon&&!gvMon.closed;}catch(e){return false;}}
  function gvMonConexao(){ // leitura apenas; chamada só depois do carregamento (setTimeout/setInterval)
    try{
      if(connected)return ready?'conectado':'identificando';
      return $('status').textContent==='CONEXAO INTERROMPIDA'?'perdida':'desconectado';
    }catch(e){return 'indisponivel';}
  }
  // Explicação curta da decisão, em linguagem simples, só com o que os logs registram.
  function gvMonExplica(m,ordem,q,pol){
    const d=q.decisao,g=q.gradual;
    if(g&&(g.motivo==='sem_nivel_compativel'||g.nivel==='-1'))
      return {texto:'Nenhuma dificuldade compatível com a regra gradual tinha pergunta disponível.',detalhe:'A Guardiã recusou em vez de violar a regra.'};
    if(!d)return {texto:'Decisão indisponível no log.',detalhe:null};
    const nv=gvNivel(d.nivel_escolhido);
    if(d.motivo==='nivel_inicial_pergunta_1')return {texto:'Na primeira pergunta, a dificuldade é a escolhida no menu.',detalhe:'A rede só registra a previsão.'};
    if(d.motivo==='fallback_sem_modelo'||d.motivo==='fallback_previsao_invalida')
      return {texto:'Sem previsões válidas: a política usou a dificuldade disponível mais próxima da inicial.',detalhe:null};
    if(d.motivo==='guardia'){
      if(g&&g.limitou==='1'){
        const sem=gvNivel(g.nivel_sem_regra),s=gvNum(g.nivel_sem_regra),a=gvNum(g.anterior);
        const ant=m.perguntas[String(Number(ordem)-1)],r=ant&&ant.resposta;
        let texto='A proteção gradual limitou a escolha.';
        if(g.anterior_acertou==='0'&&s!==null&&a!==null&&s>a){
          const oque=r&&r.esgotou==='1'?'um tempo esgotado':r&&r.acertou==='0'?'um erro':'um erro ou tempo esgotado';
          texto=`A proteção gradual impediu aumentar a dificuldade após ${oque}.`;
        }else if(s!==null&&a!==null&&Math.abs(s-a)>1)texto='A proteção gradual impediu mudar mais de uma dificuldade de uma vez.';
        return {texto,detalhe:sem?`${sem} estava mais perto do alvo de 70%, mas fora da regra.`:null};
      }
      return {texto:`${nv||'Esta dificuldade'} tem a chance estimada mais próxima do alvo de 70%`
        +(pol&&pol.nome===GV_GRADUAL?' entre as compatíveis com a regra gradual.':' entre as permitidas.'),detalhe:null};
    }
    return {texto:d.motivo?'Motivo registrado: '+d.motivo+' (sem descrição).':'Motivo indisponível no log.',detalhe:null};
  }
  // "Dados usados antes da pergunta", a partir da linha GUARDIA,features (médias com 6 casas).
  // Totais só quando média × perguntas respondidas dá um inteiro; segundos só com o tempo limite registrado.
  function gvMonContexto(m,q){
    const f=q.features;if(!f)return null;
    const n=gvNum(f.n_respondidas),ts=gvNum(f.taxa_acerto_sessao),tm=gvNum(f.tempo_medio_norm),dq=gvNum(f.dicas_por_questao);
    const inteiro=x=>{if(x===null)return null;const k=Math.round(x);return Math.abs(x-k)<1e-3?k:null;};
    const tot=media=>n===null||media===null?null:n===0?0:inteiro(media*n);
    const lims=[...new Set(Object.values(m.perguntas).map(p=>p.resposta?gvNum(p.resposta.tempo_limite_s):null).filter(v=>v!==null))];
    const limite=lims.length===1?lims[0]:null;
    return {n,acertos:tot(ts),taxa_acertos:ts,dicas_total:tot(dq),dicas_media:dq,
      tempo_frac:n?tm:null,tempo_medio_s:n&&tm!==null&&limite!==null?tm*limite:null,limite_s:limite};
  }
  function gvMonEstado(){
    const m=gvModelos[gvFonte],pol=gvPolitica(m),ordens=['1','2','3'];
    const ultima=[...ordens].reverse().find(o=>m.perguntas[o]&&(m.perguntas[o].decisao||m.perguntas[o].gradual));
    const q=ultima&&m.perguntas[ultima];
    let pergunta=null;
    if(q){
      const d=q.decisao||{},r=q.rep;
      pergunta={ordem:ultima,categoria:gvCat(d.categoria),id:d.pergunta||null,alvo:gvNum(d.alvo),tem_decisao:!!q.decisao,
        nivel_escolhido:gvNum(d.nivel_escolhido),motivo:d.motivo||null,explicacao:gvMonExplica(m,ultima,q,pol),contexto:gvMonContexto(m,q),
        previsoes:[0,1,2].map(n=>{const p=gvP(d['p'+n]);return p.tipo==='ok'?p.v:p.tipo;}),
        protecao:q.decisao||q.gradual?gvProtecao(q.gradual,pol,q.decisao||null).curto:null,
        resultado:q.resposta?gvResultado(q.resposta):null,
        recusada:!!(q.gradual&&(q.gradual.motivo==='sem_nivel_compativel'||q.gradual.nivel==='-1')),
        rede:!q.decisao?{estado:'sem_decisao'}:!q.features?{estado:'sem_features'}:!r?{estado:'sem_dados'}:r.erro?{estado:'erro',erro:r.erro}
          :{estado:'ok',aviso:r.aviso||null,avaliacoes:r.avaliacoes.map(a=>({nivel:a.nivel,raw:a.raw,c:a.c&&{x:a.c.x,h:a.c.h,z:a.c.z,p:a.c.p},
            bruto:a.bruto,dif:a.dif,ok:a.ok,invalida:a.invalida}))}};
    }
    let acertos=0,respondidas=0;
    for(const o of ordens){const r=m.perguntas[o]&&m.perguntas[o].resposta;if(r){respondidas++;if(r.acertou==='1')acertos++;}}
    return {
      fonte:gvFonte,
      demo:gvFonte==='demo'?{info:$('gvDemoInfo').textContent,nota:m.nota||null}:null,
      conexao:gvFonte==='usb'?gvMonConexao():null,
      agente:m.agente||null,
      politica:pol?pol.nome:null,
      modelo:{id:GC.modelo.id,hash:GC.modelo.hash_parametros.slice(0,12)},
      partida:m.sessao?{id:m.sessao.sessao||null,nivel_inicial:gvNivel(m.sessao.nivel_inicial),
        status:m.fim?'encerrada':m.erros.length?'interrompida':'em andamento',
        acertos:m.fim?gvNum(m.fim.acertos):acertos,respondidas,desbloqueio:m.fim?m.fim.desbloqueio||null:null}:null,
      erros:m.erros.map(e=>({motivo:e.motivo||'sem motivo',texto:GV_ERROS[e.motivo]||null})),
      pergunta,
      conf:{n:m.conf.n,ok:m.conf.ok,max:m.conf.max,tol:GC.TOL},
    };
  }
  function gvMonEnviar(completo){
    if(!gvMonAberto())return;
    const est=gvMonEstado(),txt=JSON.stringify(est);
    if(!completo&&txt===gvMonUltimo)return;
    gvMonUltimo=txt;
    try{gvMon.postMessage({canal:GC.CANAL,tipo:'estado',seq:++gvMonSeq,completo:!!completo,estado:est},'*');}catch(e){}
  }
  function gvMonAgendar(){ // chamado a cada mudança; junta rajadas de linhas em um envio
    if(gvMonTimer||!gvMonAberto())return;
    gvMonTimer=setTimeout(()=>{gvMonTimer=null;gvMonEnviar(false);},60);
  }
  function gvMonBotao(){
    const b=$('gvMonitor');if(!b)return;
    const a=gvMonAberto();b.textContent=a?'MONITOR DA IA ABERTO · TRAZER À FRENTE':'ABRIR MONITOR DA IA';b.classList.toggle('gv-mon-aberto',a);
  }
  const gvMonBtn=document.createElement('button');gvMonBtn.className='gv-btn gv-mon-btn';gvMonBtn.id='gvMonitor';gvMonBtn.type='button';
  gvMonBtn.title='Abre uma janela de exposição para o segundo monitor. Ela recebe os dados deste painel; só este painel usa a USB.';
  $('gvFonte').before(gvMonBtn);gvMonBotao();
  gvMonBtn.onclick=()=>{
    if(gvMonAberto()){try{gvMon.focus();}catch(e){}gvMonEnviar(true);return;}
    const w=window.open(gvMonUrl(),'coflume_monitor_ia','popup,width=1280,height=760');
    if(!w){$('gvMonAviso').hidden=false;$('gvMonAviso').textContent='O navegador bloqueou a janela do monitor. Permita pop-ups para este arquivo e tente de novo.';return;}
    $('gvMonAviso').hidden=true;gvMon=w;gvMonUltimo='';gvMonBotao(); // o monitor pede o estado completo ao terminar de carregar
  };
  const gvMonAvisoEl=document.createElement('p');gvMonAvisoEl.id='gvMonAviso';gvMonAvisoEl.className='gv-alerta';gvMonAvisoEl.hidden=true;
  $('gvConteudo').before(gvMonAvisoEl);
  // O monitor avisa que carregou (inclusive depois de recarregar): responde com o estado completo.
  window.addEventListener('message',e=>{
    const d=e.data;
    if(!d||d.canal!==GC.CANAL||d.tipo!=='pronto'||!e.source||e.source===window)return;
    gvMon=e.source;gvMonEnviar(true);gvMonBotao();
  });
  // Batida a cada segundo: o monitor detecta perda de comunicação. Também capta mudança no estado da USB.
  setInterval(()=>{
    gvMonBotao();
    if(!gvMonAberto())return;
    gvMonEnviar(false);
    try{gvMon.postMessage({canal:GC.CANAL,tipo:'batida',seq:gvMonSeq},'*');}catch(e){}
  },1000);
  window.addEventListener('pagehide',()=>{if(gvMonAberto())try{gvMon.postMessage({canal:GC.CANAL,tipo:'principal_fechado'},'*');}catch(e){}});
  gvRender();
