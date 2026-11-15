  // ===== MONITOR DA IA: janela de exposição (este arquivo aberto com ?monitor=ia) =====
  // Não usa USB, áudio nem pistas (o código do painel principal nem roda nesta janela).
  // Recebe do painel principal, por postMessage, um resumo pronto para exibição e reutiliza
  // o desenho e o animador da rede (CoflumeGuardia). Nunca recebe senha nem pistas.
  // Visual para leitura à distância: rede grande, textos simples; valores técnicos nos detalhes recolhidos.
  if(G.modoMonitor){
    const $m=id=>document.getElementById(id),E=G.esc;
    document.documentElement.classList.add('modo-monitor');
    document.title='Coflume — Monitor da Guardiã IA';
    const raiz=document.createElement('div');raiz.id='monitorIA';raiz.className='mon';
    raiz.innerHTML=`<header class="mon-topo"><div class="mon-marca">COFLUME / GUARDIÃ IA</div>
      <div class="mon-intro"><h1>Como a Guardiã escolhe o próximo desafio</h1>
      <p>A rede neural estima a chance de acerto. A política do jogo usa essa estimativa para escolher a dificuldade.</p></div>
      <div class="mon-topo-dir"><div id="monDemo" class="mon-demo" hidden></div><div id="monFonte" class="mon-chip"></div>
      <div id="monConexao" class="mon-chip"></div><button id="monTela" class="mon-btn" type="button">⛶ TELA CHEIA</button></div></header>
      <div id="monAviso" class="mon-aviso" hidden></div>
      <main class="mon-corpo" id="monCorpo">
        <section class="mon-card mon-rede">
          <div class="mon-rede-cab"><div><div class="mon-etapa">1 / A REDE FAZ A PREVISÃO</div>
          <div class="mon-titulo" id="monTitulo">Aguardando o painel principal…</div></div>
          <div class="mon-dados-box"><div class="mon-sub" id="monSubDados">Dados usados antes da pergunta</div>
          <div class="mon-dados" id="monDados"></div></div></div>
          <svg id="monSvg" viewBox="0 0 1000 610" preserveAspectRatio="xMidYMid meet" role="img" aria-label="Rede neural da Guardiã: 13 entradas, 8 neurônios ocultos, 1 saída"></svg>
          <div class="mon-rodape-rede"><span id="monEstado"></span>
          <span class="mon-lenta" id="monLenta" hidden>visualização em câmera lenta · o pulso nas conexões é recurso visual</span>
          <span class="mon-reproduzido">cálculo reproduzido no painel</span></div>
        </section>
        <aside class="mon-lado">
          <div class="mon-perg" id="monPerg"></div>
          <h2>Chance estimada de acerto</h2>
          <p class="mon-sub">Para cada dificuldade avaliada pela rede</p>
          <div id="monBarras" class="mon-barras"></div>
          <div class="mon-escala"><span>0%</span><span class="mon-escala-alvo">Alvo: 70%</span><span>100%</span></div>
          <div class="mon-decisao" id="monDecisao" hidden></div>
        </aside>
      </main>
      <footer class="mon-final" id="monFinal">
        <div><div class="mon-etapa" id="monPartidaCab">RESULTADO FINAL DA PARTIDA</div><div class="mon-final-v" id="monPartida">—</div></div>
        <div><div class="mon-etapa">REGRA DO COFRE: PELO MENOS 2 ACERTOS EM 3</div><div class="mon-final-v" id="monCofre">—</div></div>
        <div class="mon-final-nota">Previsões são estimativas, não certezas.<br>Modelo experimental treinado com dados sintéticos.</div>
      </footer>
      <details class="mon-tec" id="monTecDet"><summary>Detalhes técnicos e diagnóstico</summary>
        <div id="monTec"></div>
        <p class="mon-ctl"><label><input type="checkbox" id="monAnimar" checked> animar</label>
        <label>ritmo <select id="monRitmo"><option value="1">câmera lenta</option><option value="0.3">mais rápido</option></select></label>
        <button id="monRepetir" class="mon-btn" type="button">↻ repetir animação</button></p></details>`;
    document.body.prepend(raiz);
    // Rede real (pesos e ativações do CoflumeGuardia) em geometria grande, só nesta janela:
    // 13 nós de entrada sem rótulos (os nomes ficam nos detalhes técnicos), todas as 112 conexões reais,
    // círculos maiores e halo proporcional à ativação reproduzida (ativação zero = sem halo).
    const GEO={xe:105,xh:500,xs:885,ye:i=>78+i*42,yh:i=>92+i*66,ys:330,re:14,rh:28,rs:64,ycab:34,dvs:112,dvs2:136};
    const rede=G.criarRede($m('monSvg'),'mon',{espessura:0.7,opacidade:0.7,geo:GEO,rotulos:false,brilho:true,
      titulos:['Dados da partida','8 neurônios','Chance de acerto']});
    const svgM=$m('monSvg');
    const ROT=['dificuldade avaliada','dificuldade inicial','perguntas respondidas','acertos na partida','acertos na categoria','tempo médio','dicas por pergunta','última resposta','sequência','Matemática','Português','Lógica','Conh. gerais'];
    const sub=document.createElementNS('http://www.w3.org/2000/svg','text');
    sub.setAttribute('x',String(GEO.xs));sub.setAttribute('y',String(GEO.ys+GEO.rs+86));sub.setAttribute('text-anchor','middle');sub.setAttribute('class','gvr-cab mon-estimada');
    svgM.append(sub);
    try{$m('monAnimar').checked=!window.matchMedia('(prefers-reduced-motion: reduce)').matches;}catch(e){}
    let est=null,ultimaMsg=0,ultimoSeq=0,chave=null,ctl=null,primeiro=true,semPrincipal=false,revelados=3,animando=false;
    const nome=n=>G.NIVEIS[n]||'?';
    const parar=()=>{if(ctl){ctl.parar();ctl=null;}animando=false;$m('monLenta').hidden=true;};
    // ---- Dados usados antes da pergunta (da linha GUARDIA,features, convertidos no painel principal) ----
    function dados(p){
      $m('monSubDados').textContent=p?`Dados usados antes da pergunta ${p.ordem}`:'Dados usados antes da pergunta';
      const c=p&&p.contexto,bloco=(v,r)=>`<div><div class="mon-dados-v">${v}</div><div class="mon-dados-r">${r}</div></div>`;
      if(!p){$m('monDados').innerHTML='';return;}
      if(!c){$m('monDados').innerHTML=bloco('indisponível','dados da pergunta ausentes no log');return;}
      if(c.n===0){$m('monDados').innerHTML=bloco('—','primeira pergunta: ainda sem respostas');return;}
      const nd=c.n===null?'indisponível':c.n;
      const a=c.acertos!==null&&c.n!==null?bloco(`${c.acertos} de ${c.n}`,'acertos anteriores')
        :c.taxa_acertos!==null?bloco(G.pct(c.taxa_acertos),'taxa de acertos anteriores'):bloco('indisponível','acertos anteriores');
      const t=c.tempo_medio_s!==null?bloco(G.fmt(c.tempo_medio_s,1)+' s','tempo médio aproximado')
        :c.tempo_frac!==null?bloco(G.fmt(c.tempo_frac*100,0)+'% do limite','tempo médio por resposta'):bloco('indisponível','tempo médio');
      const d=c.dicas_total!==null?bloco(String(c.dicas_total),c.dicas_total===1?'dica usada (total)':'dicas usadas (total)')
        :c.dicas_media!==null?bloco(G.fmt(c.dicas_media,2),'dicas por pergunta (média)'):bloco('indisponível','dicas');
      $m('monDados').innerHTML=a+t+d+(c.n===null?bloco(nd,'perguntas respondidas'):'');
    }
    // ---- Chance estimada por dificuldade (valores registrados pelo ESP32) ----
    function reveladoNivel(n){
      if(!animando)return true;
      const avs=est&&est.pergunta&&est.pergunta.rede.avaliacoes;if(!avs)return true;
      const k=avs.findIndex(a=>a.nivel===n);return k<0||k<revelados;
    }
    function barras(p){
      const prev=p?p.previsoes:[null,null,null],alvo=p&&p.alvo!==null?p.alvo:0.7,mostrarEsc=!$m('monDecisao').hidden;
      let h='';
      for(let n=0;n<3;n++){
        let v=prev[n];if(p&&p.recusada&&typeof v!=='number')v='na';
        const ok=typeof v==='number',vis=ok&&reveladoNivel(n),esc=p&&mostrarEsc&&p.tem_decisao&&p.nivel_escolhido===n;
        h+=`<div class="mon-barra${esc?' mon-esc':''}${ok?'':' mon-na'}" data-nivel="${n}"><div class="mon-bcab"><span class="mon-bn">${nome(n)}</span>`
          +`<span class="mon-bv">${!p?'':!ok?(v==='na'?'Não avaliado':'indisponível'):vis?'≈ '+G.pct(v):'…'}</span></div>`
          +`<div class="mon-bt"><span class="mon-bf" style="width:${vis?(v*100).toFixed(1):0}%"></span><span class="mon-alvo" style="left:${(alvo*100).toFixed(1)}%"></span></div>`
          +(esc?'<div class="mon-esc-tag">DIFICULDADE ESCOLHIDA PELA POLÍTICA</div>':'')+'</div>';
      }
      $m('monBarras').innerHTML=h;
    }
    // ---- Decisão da política ----
    function decisao(p){
      const box=$m('monDecisao');
      if(!p||(!p.tem_decisao&&!p.recusada)){box.hidden=true;return;}
      const ex=p.explicacao||{texto:'Motivo indisponível no log.',detalhe:null};
      box.innerHTML=`<div class="mon-etapa">2 / A POLÍTICA ESCOLHE A DIFICULDADE <small>(não é a rede)</small></div>`
        +`<div class="mon-dec-nivel">${p.recusada?'Recusada':p.nivel_escolhido!==null?E(nome(p.nivel_escolhido)):'indisponível'}</div>`
        +`<div class="mon-dec-txt">${E(ex.texto)}</div>`+(ex.detalhe?`<div class="mon-dec-det">${E(ex.detalhe)}</div>`:'');
      box.hidden=false;
    }
    // ---- Resultado final da partida (separado dos dados usados antes da pergunta) ----
    function partida(){
      const s=est.partida,p=est.pergunta;
      $m('monPerg').textContent=p?`PERGUNTA ${p.ordem} DE 3${p.categoria?' / '+p.categoria.toUpperCase():''}`:'';
      const pl=(k,u,v)=>`${k} ${k===1?u:v}`;
      $m('monPartidaCab').textContent=s&&s.status!=='encerrada'?'PARTIDA ATÉ AGORA':'RESULTADO FINAL DA PARTIDA';
      if(!s){$m('monPartida').textContent='Aguardando o Quiz';$m('monCofre').textContent='—';return;}
      if(s.status==='encerrada'){
        $m('monPartida').textContent=s.acertos===null?'indisponível':pl(s.acertos,'acerto','acertos')+' em 3';
        $m('monCofre').textContent=s.desbloqueio==='SIM'?'Cofre aberto':s.desbloqueio==='NAO'?'Cofre fechado':'indisponível';
        $m('monCofre').className='mon-final-v'+(s.desbloqueio==='SIM'?' mon-aberto':s.desbloqueio==='NAO'?' mon-fechado':'');
      }else if(s.status==='interrompida'){
        $m('monPartida').textContent='Partida interrompida';$m('monCofre').textContent='indisponível';$m('monCofre').className='mon-final-v';
      }else{
        $m('monPartida').innerHTML=`${pl(s.acertos,'acerto','acertos')} em ${pl(s.respondidas,'respondida','respondidas')} <span class="mon-andamento">(em andamento)</span>`;
        $m('monCofre').textContent='Aguardando o fim da partida';$m('monCofre').className='mon-final-v';
      }
    }
    function tecnico(){
      const s=est,p=s.pergunta,r=p&&p.rede;
      let h=`<p>Modelo ${E(s.modelo.id)} (hash ${E(s.modelo.hash)}…) · política ${E(s.politica||'indisponível')} · fonte ${s.fonte==='demo'?'reprodução de log':'ao vivo (USB)'} · mensagem nº ${ultimoSeq}</p>`
        +'<p>A rede desenhada usa os pesos do firmware; as ativações são <b>calculadas no painel principal</b> a partir das entradas enviadas pelo ESP32 e conferidas com a previsão registrada pela placa. Não são medições internas da placa.</p>';
      if(r&&r.estado==='ok'&&r.avaliacoes.length){
        const raw=r.avaliacoes[0].raw;
        h+=`<p>Entradas enviadas pelo ESP32 (GUARDIA,features): n_respondidas=${raw[2]}, taxa_acerto_sessao=${raw[3]}, taxa_acerto_categoria=${raw[4]}, tempo_medio_norm=${raw[5]}, dicas_por_questao=${raw[6]}, resultado_ultima=${raw[7]}, sequencia=${raw[8]}</p>`;
        h+='<table><tr><th>dificuldade</th><th>P reproduzida no painel</th><th>P registrada pelo ESP32</th><th>|Δ|</th><th></th></tr>';
        for(const a of r.avaliacoes)h+=`<tr><td>${nome(a.nivel)}</td><td>${a.c?a.c.p.toFixed(6):'inválida'}</td><td>${E(a.bruto)}</td><td>${a.dif===null?'—':G.exp(a.dif)}</td><td class="${a.ok?'mon-ok':'mon-nok'}">${a.ok?'confere':'NÃO CONFERE'}</td></tr>`;
        h+='</table>';
        // As 13 entradas da rede (rótulos retirados do desenho principal): valor enviado e valor padronizado usado no cálculo.
        h+='<p><b>As 13 entradas da rede</b> (nós da coluna “Dados da partida”, de cima para baixo). Só a primeira muda entre as avaliações.</p>';
        h+='<table class="mon-entradas"><tr><th>#</th><th>entrada</th><th>valor enviado</th>'+r.avaliacoes.map(a=>`<th>padronizado · ${E(nome(a.nivel))}</th>`).join('')+'</tr>';
        for(let j=0;j<13;j++){
          const v=j===0?r.avaliacoes.map(a=>`${E(nome(a.nivel))} (${a.raw[0]})`).join(' / '):String(raw[j]);
          h+=`<tr><td>${j+1}</td><td>${E(ROT[j])}</td><td>${v}</td>`+r.avaliacoes.map(a=>`<td>${a.c?a.c.x[j].toFixed(4):'—'}</td>`).join('')+'</tr>';
        }
        h+='</table>';
      }else if(r)h+=`<p>Cálculo reproduzido indisponível: ${E(r.erro||r.estado)}</p>`;
      if(p&&p.contexto&&p.contexto.limite_s!==null)h+=`<p>Tempo limite registrado nas respostas: ${p.contexto.limite_s} s.</p>`;
      if(s.conf.n)h+=`<p>Conferência acumulada: ${s.conf.ok} de ${s.conf.n} previsões dentro da tolerância de ${G.exp(s.conf.tol)} (maior |Δ| = ${G.exp(s.conf.max)}).</p>`;
      if(s.demo)h+=`<p>${E(s.demo.info)}</p>`;
      $m('monTec').innerHTML=h;
    }
    function aviso(txt,classe){const a=$m('monAviso');a.hidden=!txt;a.className='mon-aviso'+(classe?' '+classe:'');a.innerHTML=txt||'';}
    // ---- Rede: anima cada pergunta nova uma vez; ao abrir/reabrir mostra o último resultado parado ----
    function redeEstatica(p){
      const r=p&&p.rede;
      $m('monEstado').textContent='';sub.textContent='';
      if(!r||r.estado!=='ok'||!r.avaliacoes.length){
        rede.desenhar(null,0,1);
        $m('monTitulo').textContent=!p?'Aguardando uma pergunta do Quiz':p.recusada?'Pergunta recusada pela regra gradual · a rede não foi usada':r&&r.estado==='sem_features'?'Previsão da rede indisponível nesta pergunta'
          :r&&r.estado==='erro'?'Cálculo da rede indisponível':r&&r.estado==='ok'?'A rede não foi avaliada nesta pergunta':'Aguardando a avaliação da rede';
        return;
      }
      const alvo=r.avaliacoes.find(a=>a.nivel===p.nivel_escolhido)||r.avaliacoes[r.avaliacoes.length-1];
      rede.desenhar(alvo,3,G.hmax(r.avaliacoes));sub.textContent='estimados';
      $m('monTitulo').innerHTML=`Última avaliação: ${E(nome(alvo.nivel))}`;
      $m('monEstado').textContent='Última avaliação · visualização parada';
    }
    function animar(p){
      parar();
      const avs=p.rede.avaliacoes,k=avs.length;
      animando=true;revelados=0;$m('monLenta').hidden=false;$m('monEstado').textContent='';sub.textContent='';
      $m('monDecisao').hidden=true;barras(p);
      ctl=G.animar(rede,avs,{ritmo:Number($m('monRitmo').value)||1,
        aoIniciar:i=>{sub.textContent='';$m('monTitulo').innerHTML=`Avaliando: ${E(nome(avs[i].nivel))} <small>${i+1} de ${k}</small>`;},
        aoSaida:i=>{revelados=i+1;sub.textContent='estimados';barras(p);},
        aoFim:()=>{ctl=null;animando=false;revelados=3;$m('monLenta').hidden=true;decisao(p);barras(p);redeEstatica(p);}});
    }
    // ---- Aplica um estado recebido ----
    function aplicar(s,completo){
      est=s;
      const demo=s.fonte==='demo';
      // Um único aviso de demonstração (com a nota do arquivo, se houver).
      $m('monDemo').hidden=!demo;
      if(demo)$m('monDemo').innerHTML=`<b>DADOS DE DEMONSTRAÇÃO</b> · reprodução de log<small>Não vêm do cofre conectado nem de crianças.</small>`
        +(s.demo.nota?`<small class="mon-demo-nota">${E(s.demo.nota)}</small>`:'');
      $m('monFonte').hidden=demo;$m('monFonte').textContent=demo?'':'AO VIVO · USB';
      const cx={conectado:'cofre conectado',identificando:'identificando o cofre…',desconectado:'cofre desconectado',perdida:'COMUNICAÇÃO COM O COFRE PERDIDA',indisponivel:'estado da conexão indisponível'}[s.conexao];
      $m('monConexao').hidden=demo;$m('monConexao').textContent=cx||'';$m('monConexao').className='mon-chip mon-cx-'+(s.conexao||'');
      tecnico();
      if(s.agente){ // Agente Secreto: sem Guardiã, sem animação, nada da missão
        parar();chave='agente';rede.desenhar(null,0,1);$m('monCorpo').classList.add('mon-oculto');$m('monFinal').classList.add('mon-oculto');
        aviso('<b>MODO AGENTE SECRETO</b><br>Esta modalidade não usa a Guardiã IA. A rede volta a ser exibida no próximo Quiz.','mon-aviso-agente');
        primeiro=false;return;
      }
      $m('monCorpo').classList.remove('mon-oculto');$m('monFinal').classList.remove('mon-oculto');
      const antigo=!demo&&(s.conexao==='perdida'||s.conexao==='desconectado');
      $m('monCorpo').classList.toggle('mon-antigo',antigo&&!!s.partida);$m('monFinal').classList.toggle('mon-antigo',antigo&&!!s.partida);
      if(antigo)parar();
      if(s.conexao==='perdida')aviso('<b>COMUNICAÇÃO COM O COFRE PERDIDA</b><br>'+(s.partida?'O que aparece abaixo é da última partida antes da desconexão, não é atual.':'Aguardando reconexão no painel principal.'),'mon-aviso-erro');
      else if(s.conexao==='desconectado')aviso(s.partida?'<b>Cofre desconectado</b> · o que aparece abaixo é da última partida, não é atual.':'Aguardando a conexão do cofre no painel principal.','');
      else if(s.erros.length)aviso(s.erros.map(e=>(e.texto?E(e.texto):'A Guardiã registrou um erro')+' <small>('+E(e.motivo)+')</small>').join('<br>'),'mon-aviso-erro');
      else aviso('');
      partida();
      const p=s.pergunta;
      dados(p);
      const k=[s.fonte,s.partida&&s.partida.id,p&&p.ordem,p&&p.rede.estado,p&&p.rede.avaliacoes?p.rede.avaliacoes.length:0].join('|');
      if(k!==chave){
        chave=k;parar();
        if(p&&p.rede.estado==='ok'&&p.rede.avaliacoes.length&&!completo&&!primeiro&&!antigo&&$m('monAnimar').checked)animar(p);
        else{decisao(p);barras(p);redeEstatica(p);}
      }else if(!animando){decisao(p);barras(p);}
      primeiro=false;
    }
    function semComunicacao(motivo){
      if(semPrincipal)return;
      semPrincipal=true;parar();
      $m('monCorpo').classList.add('mon-antigo');$m('monFinal').classList.add('mon-antigo');
      aviso(`<b>SEM COMUNICAÇÃO COM O PAINEL PRINCIPAL</b><br>${motivo} Os dados abaixo não são atuais.`,'mon-aviso-erro');
      $m('monConexao').hidden=false;$m('monConexao').textContent='painel principal sem comunicação';$m('monConexao').className='mon-chip mon-cx-perdida';
    }
    window.addEventListener('message',e=>{
      if(!window.opener||e.source!==window.opener)return;
      const d=e.data;if(!d||d.canal!==G.CANAL)return;
      ultimaMsg=performance.now();
      if(d.tipo==='principal_fechado'){semComunicacao('O painel principal foi fechado ou recarregado.');return;}
      if(semPrincipal){semPrincipal=false;$m('monCorpo').classList.remove('mon-antigo');$m('monFinal').classList.remove('mon-antigo');}
      if(d.tipo==='estado'){if(d.seq<ultimoSeq&&!d.completo)return;ultimoSeq=d.seq;aplicar(d.estado,d.completo);}
    });
    const pedir=()=>{try{window.opener.postMessage({canal:G.CANAL,tipo:'pronto'},'*');}catch(e){}};
    if(!window.opener){
      aviso('Esta janela é o <b>monitor da IA</b>. Abra-a pelo botão <b>ABRIR MONITOR DA IA</b> no painel principal, que é o único conectado à USB.','');
      $m('monCorpo').classList.add('mon-oculto');$m('monFinal').classList.add('mon-oculto');
    }else{
      pedir();
      setInterval(()=>{ // batidas do principal chegam a cada 1 s
        if(performance.now()-ultimaMsg>3500){
          if(ultimaMsg)semComunicacao(window.opener.closed?'O painel principal foi fechado.':'Nenhuma mensagem há mais de 3 segundos.');
          pedir();
        }
      },1000);
    }
    $m('monTela').onclick=()=>{const p=document.fullscreenElement?document.exitFullscreen():document.documentElement.requestFullscreen();p&&p.catch&&p.catch(()=>{});};
    document.addEventListener('fullscreenchange',()=>{$m('monTela').textContent=document.fullscreenElement?'⛶ SAIR DA TELA CHEIA':'⛶ TELA CHEIA';});
    $m('monRepetir').onclick=()=>{const p=est&&est.pergunta;if(p&&!est.agente&&p.rede.estado==='ok'&&p.rede.avaliacoes.length)animar(p);};
  }
