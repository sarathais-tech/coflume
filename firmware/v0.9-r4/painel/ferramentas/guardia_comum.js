  // ===== Código comum da Guardiã: painel principal e janela de exposição (proposta) =====
  // Inferência reproduzida, desenho da rede e animador. Não acessa USB, áudio nem pistas.
  const G={};window.CoflumeGuardia=G;
  G.modoMonitor=new URLSearchParams(location.search).get('monitor')==='ia';
  G.CANAL='coflume-monitor-ia';
  G.modelo=GV_MODELO;
  G.NIVEIS=['Explorador','Desafiante','Mestre'];
  G.CATEG=['Matemática','Português','Lógica','Conhecimentos gerais'];
  G.esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  G.fmt=(x,d=1)=>x.toLocaleString('pt-BR',{minimumFractionDigits:d,maximumFractionDigits:d});
  G.pct=v=>G.fmt(v*100,1)+'%';
  G.exp=v=>v.toExponential(1).replace('.',',').replace('e-','×10⁻').replace(/⁻(\d+)/,(x,d)=>'⁻'+[...d].map(c=>'⁰¹²³⁴⁵⁶⁷⁸⁹'[c]).join(''));
  // ---- Inferência: espelho de guardia::prever() em float32 ----
  const F32=Math.fround;
  G.ROTULOS=['nível candidato','nível inicial','nº respondidas','acerto na sessão','acerto na categoria','tempo médio (norm.)','dicas por questão','última resposta','sequência','Matemática','Português','Lógica','Conh. gerais'];
  G.CAMPOS=['nivel_candidato','nivel_inicial','n_respondidas','taxa_acerto_sessao','taxa_acerto_categoria','tempo_medio_norm','dicas_por_questao','resultado_ultima','sequencia'];
  const R={mu:GV_MODELO.media.map(F32),sd:GV_MODELO.desvio.map(F32),w1:GV_MODELO.w1.map(l=>l.map(F32)),
    b1:GV_MODELO.b1.map(F32),w2:GV_MODELO.w2.map(F32),b2:F32(GV_MODELO.b2)};
  G.pesos=R;
  // Tolerância da comparação com o ESP32 (ver docs/rede_neural_avaliacao.md):
  // features contínuas em 6 casas (±5e-7) propagadas pela rede + previsão em 6 casas (±5e-7) + float32 (2e-7).
  G.TOL=(()=>{let s=0;for(let i=0;i<8;i++){let a=0;for(let j=3;j<=6;j++)a+=Math.abs(R.w1[i][j])*5e-7/R.sd[j];s+=Math.abs(R.w2[i])*a;}return 0.25*s+5e-7+2e-7;})();
  G.entrada=function(f,nivel,num){ // vetor bruto na ordem de vetorizar() do runtime
    const raw=[];
    for(let i=0;i<9;i++){const v=i===0?nivel:num(f[G.CAMPOS[i]]);if(v===null)return null;raw.push(v);}
    const cat=num(f.categoria);if(cat===null||!Number.isInteger(cat)||cat<0||cat>3)return null;
    for(let c=0;c<4;c++)raw.push(c===cat?1:0);
    return raw;
  };
  G.valida=function(raw){ // mesmas faixas de entrada_valida() do runtime
    const [nc,ni,nr,ts,tc,tm,dq,ru,sq]=raw,in01=v=>v>=0&&v<=1,int=(v,a,b)=>Number.isInteger(v)&&v>=a&&v<=b;
    return int(nc,0,2)&&int(ni,0,2)&&int(nr,0,8)&&in01(ts)&&in01(tc)&&in01(tm)&&in01(dq)&&int(ru,-1,1)&&int(sq,-3,3);
  };
  G.calcular=function(raw){
    const x=raw.map(F32);
    for(let i=0;i<9;i++)x[i]=F32(F32(x[i]-R.mu[i])/R.sd[i]);
    const h=[];
    for(let i=0;i<8;i++){let s=R.b1[i];for(let j=0;j<13;j++)s=F32(s+F32(R.w1[i][j]*x[j]));h.push(s>0?s:0);}
    let z=R.b2;for(let j=0;j<8;j++)z=F32(z+F32(R.w2[j]*h[j]));
    let p;
    if(z>=0)p=F32(1/F32(1+F32(Math.exp(-z))));else{const e=F32(Math.exp(z));p=F32(e/F32(1+e));}
    return {x,h,z,p:Math.min(1,Math.max(0,p))};
  };
  // ---- Desenho da rede em posições fixas (SVG). av = {nivel, raw, c:{x,h,z,p}} ----
  // Geometria padrão: a do painel principal. O monitor passa op.geo (posições e raios maiores),
  // op.titulos, op.rotulos:false (sem os 13 rótulos no desenho) e op.brilho (halo proporcional à ativação).
  // op.espessura e op.opacidade deixam as conexões mais discretas. Pesos e ativações nunca mudam.
  const NS='http://www.w3.org/2000/svg';
  const GEO_PADRAO={xe:200,xh:455,xs:650,ye:i=>40+i*29.5,yh:i=>62+i*47,ys:226,re:8,rh:14,rs:24,ycab:18,dvs:44,dvs2:60};
  G.criarRede=function(svg,prefixo,op){
    op=op||{};
    const fw=op.espessura||1,fo=op.opacidade||1,g=Object.assign({},GEO_PADRAO,op.geo||{}),rotulos=op.rotulos!==false,brilho=!!op.brilho;
    const XE=g.xe,XH=g.xh,XS=g.xs,yE=g.ye,yH=g.yh,YS=g.ys;
    const titulos=op.titulos||['13 entradas (padronizadas)','8 ocultos (ReLU)','saída (sigmoide)'];
    const el=(tag,at,pai)=>{const e=document.createElementNS(NS,tag);for(const k in at)e.setAttribute(k,at[k]);(pai||svg).append(e);return e;};
    const gA1=el('g',{class:'gvr-arestas',id:prefixo+'A1'}),gA2=el('g',{class:'gvr-arestas',id:prefixo+'A2'});
    const A1=[],A2=[],NE=[],VE=[],NH=[],VH=[];
    for(let i=0;i<8;i++){A1.push([]);for(let j=0;j<13;j++){const w=R.w1[i][j];
      A1[i].push(el('line',{x1:XE,y1:yE(j),x2:XH,y2:yH(i),stroke:w>=0?'#51ff9a':'#ff9a6b','stroke-width':((0.4+2.4*Math.abs(w))*fw).toFixed(2),opacity:(0.10*fo).toFixed(3)},gA1));}}
    for(let i=0;i<8;i++){const w=R.w2[i];A2.push(el('line',{x1:XH,y1:yH(i),x2:XS,y2:YS,stroke:w>=0?'#51ff9a':'#ff9a6b','stroke-width':((0.6+3*Math.abs(w))*fw).toFixed(2),opacity:(0.10*fo).toFixed(3)},gA2));}
    el('text',{x:XE,y:g.ycab,class:'gvr-cab','text-anchor':'middle'}).textContent=titulos[0];
    el('text',{x:XH,y:g.ycab,class:'gvr-cab','text-anchor':'middle'}).textContent=titulos[1];
    el('text',{x:XS,y:g.ycab,class:'gvr-cab','text-anchor':'middle'}).textContent=titulos[2];
    for(let j=0;j<13;j++){
      if(rotulos){
        el('text',{x:XE-14,y:yE(j)-1,class:'gvr-rot','text-anchor':'end'}).textContent=G.ROTULOS[j];
        VE.push(el('text',{x:XE-14,y:yE(j)+10,class:'gvr-val','text-anchor':'end'}));
      }else VE.push(null);
      NE.push(el('circle',{cx:XE,cy:yE(j),r:g.re,class:'gvr-no'}));
    }
    for(let i=0;i<8;i++){
      NH.push(el('circle',{cx:XH,cy:yH(i),r:g.rh,class:'gvr-no'}));
      el('text',{x:XH,y:yH(i)+g.rh*0.28,class:'gvr-nome','text-anchor':'middle'}).textContent='N'+(i+1);
      VH.push(el('text',{x:XH+g.rh+6,y:yH(i)-g.rh*0.57,class:'gvr-val'}));
    }
    const NS1=el('circle',{cx:XS,cy:YS,r:g.rs,class:'gvr-no'});
    el('text',{x:XS,y:YS+g.rs*0.17,class:'gvr-nome gvr-nome-s','text-anchor':'middle'}).textContent='P';
    const VS=el('text',{x:XS,y:YS+g.dvs,class:'gvr-val-s','text-anchor':'middle'});
    const VS2=el('text',{x:XS,y:YS+g.dvs2,class:'gvr-val','text-anchor':'middle'});
    const TIT=(e,t)=>{let x=e.querySelector('title');if(!x){x=document.createElementNS(NS,'title');e.append(x);}x.textContent=t;};
    const corEntrada=(x,on)=>{if(!on)return 'rgba(20,50,35,.9)';const a=Math.min(1,Math.abs(x)/2.2)*0.85+0.15;
      return x>=0?`rgba(255,211,107,${a.toFixed(2)})`:`rgba(107,200,255,${a.toFixed(2)})`;};
    // Halo proporcional ao valor real (0 = sem halo): torna perceptível a mudança de brilho, sem inventar atividade.
    const halo=(e,f,cor)=>{if(brilho)e.style.filter=f>0?`drop-shadow(0 0 ${(2+16*f).toFixed(1)}px rgba(${cor},${(0.25+0.7*f).toFixed(2)}))`:'none';};
    // estagio: 0 apagado, 1 entradas, 2 ocultos, 3 saída
    function desenhar(av,estagio,hmax){
      const c=av&&av.c;
      for(let j=0;j<13;j++){
        const on=!!c&&estagio>=1;
        NE[j].setAttribute('fill',corEntrada(on?c.x[j]:0,on));
        halo(NE[j],on?Math.min(1,Math.abs(c.x[j])/2.2):0,on&&c.x[j]<0?'107,200,255':'255,211,107');
        if(VE[j])VE[j].textContent=av?(j===0?G.NIVEIS[av.raw[0]]+' ('+av.raw[0]+')':j>=9?(av.raw[j]?'1 (categoria)':'0'):G.fmt(av.raw[j],j>=3&&j<=6?6:0)):'';
        if(c)TIT(NE[j],`${G.ROTULOS[j]}: valor enviado ${av.raw[j]} · padronizado ${c.x[j].toFixed(6)}`);
      }
      for(let i=0;i<8;i++){
        const a=c&&estagio>=2?c.h[i]:null,f=a===null?0:a/(hmax||1);
        NH[i].setAttribute('fill',a===null?'rgba(20,50,35,.9)':a===0?'rgba(10,25,18,.95)':`rgba(81,255,154,${(0.15+0.85*f).toFixed(2)})`);
        NH[i].classList.toggle('gvr-inativo',a===0);
        halo(NH[i],f,'81,255,154');
        VH[i].textContent=a===null?'':a===0?'0 · inativo':G.fmt(a,3);
        if(c)TIT(NH[i],`N${i+1}: ativação ReLU reproduzida ${c.h[i].toFixed(6)}`);
        for(let j=0;j<13;j++)A1[i][j].setAttribute('opacity',(fo*(c&&estagio>=1?Math.min(0.9,0.06+Math.abs(R.w1[i][j]*c.x[j])*0.9):0.10)).toFixed(3));
        A2[i].setAttribute('opacity',(fo*(c&&estagio>=2?Math.min(0.95,0.06+Math.abs(R.w2[i]*c.h[i])*1.2):0.10)).toFixed(3));
      }
      const p=c&&estagio>=3?c.p:null;
      NS1.setAttribute('fill',p===null?'rgba(20,50,35,.9)':`rgba(81,255,154,${(0.15+0.85*p).toFixed(2)})`);
      halo(NS1,p===null?0:p,'81,255,154');
      VS.textContent=p===null?'':'≈ '+G.pct(p);
      VS2.textContent=p===null?'':'reproduzida: '+p.toFixed(6);
      if(c)TIT(NS1,`saída reproduzida no painel: z=${c.z.toFixed(6)} · P=${c.p.toFixed(6)}`);
    }
    const pulso=(camada,on)=>(camada===1?gA1:gA2).classList.toggle('gvr-pulso',on);
    return {desenhar,pulso,svg,geo:g};
  };
  G.hmax=avs=>{let h=0;for(const a of avs)if(a.c)h=Math.max(h,...a.c.h);return h||1;};
  // ---- Animador: percorre as avaliações em sequência, em câmera lenta. Cancelável; nunca acumula. ----
  // o: {ritmo, aoIniciar(i), aoSaida(i), aoFim()}
  G.animar=function(rede,avs,o){
    const viva={v:true},timers=[],hmax=G.hmax(avs),f=o.ritmo||1;
    const depois=(ms,fn)=>timers.push(setTimeout(()=>{if(viva.v)fn();},ms));
    const T={ent:700*f,p1:1500*f,oc:600*f,p2:1100*f,sai:700*f,pausa:900*f};
    let t=0;
    avs.forEach((a,i)=>{
      depois(t,()=>{rede.desenhar(null,0,hmax);rede.desenhar(a,1,hmax);o.aoIniciar&&o.aoIniciar(i);});
      if(!a.c){depois(t+T.ent,()=>o.aoSaida&&o.aoSaida(i));t+=T.ent+T.pausa;return;}
      depois(t+T.ent,()=>rede.pulso(1,true));
      depois(t+T.ent+T.p1,()=>{rede.pulso(1,false);rede.desenhar(a,2,hmax);});
      depois(t+T.ent+T.p1+T.oc,()=>rede.pulso(2,true));
      depois(t+T.ent+T.p1+T.oc+T.p2,()=>{rede.pulso(2,false);rede.desenhar(a,3,hmax);o.aoSaida&&o.aoSaida(i);});
      t+=T.ent+T.p1+T.oc+T.p2+T.sai+T.pausa;
    });
    depois(t,()=>o.aoFim&&o.aoFim());
    return {parar(){viva.v=false;timers.forEach(clearTimeout);timers.length=0;rede.pulso(1,false);rede.pulso(2,false);}};
  };
