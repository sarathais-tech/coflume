// COFLUME v0.9 EXPERIMENTAL - Hardware fisico: ESP32-S3 DevKitC-1 N16R8 + ST7735 1,8" (160x128) + teclado 4x4
// Base: v0.8 experimental (simulations/wokwi/v0.8, intacta). Mesma Guardia IA, mesma politica, mesmo banco.
// Guardia IA: MLP mlp8-sint-20261101-e6a4cf00 (13 -> 8 -> 1), treinada com dados SINTETICOS,
// executada offline no ESP32-S3 (sem Wi-Fi, sem servidor). NAO e versao de producao:
// teste lacrado e bancada de latencia na placa pendentes; nao validada com criancas reais.
// Servo REAL no GPIO 12 via LEDC; LEDs permanecem simulados no monitor serial.
// Agente Secreto: painel USB + senha fisica, sem inferencias da Guardia nessa modalidade.
// Abertura: validador de 2/3 no quiz OU senha correta no Agente Secreto; nunca pela Guardia.
#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <SPI.h>
#include <Keypad.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <esp_random.h>
#include <esp_timer.h>

#include "guardia_modelo.h"
#include "guardia_runtime.h"
#include "guardia_autoteste.h"
#include "coflume_banco.h"
#include "coflume_politica.h"
#include "coflume_tela.h"
#include "coflume_politica_gradual.h"

// 1 = Guardia escolhe o nivel das perguntas 2 e 3; 0 = nivel fixo, como na v0.7.
#ifndef GUARDIA_ADAPTATIVA
#define GUARDIA_ADAPTATIVA 1
#endif
// Painel ST7735: INITR_BLACKTAB e o mais comum no 1,8" 128x160. Se as cores vierem
// trocadas (vermelho <-> azul) ou houver faixas na borda, teste INITR_GREENTAB ou INITR_REDTAB.
#ifndef TFT_PAINEL
#define TFT_PAINEL INITR_BLACKTAB
#endif
// Paisagem 160x128: 1; use 3 se a imagem ficar de cabeca para baixo.
#ifndef TFT_ROTACAO
#define TFT_ROTACAO 1
#endif
// Politica EXPERIMENTAL gradual-v2-exp (coflume-guardian-ai, docs/politica_gradual_exp.md):
// 0 = fluxo-0.8 (padrao, igual a r2); 1 = mudanca gradual entre perguntas, so com banco viavel.
#ifndef GUARDIA_POLITICA_GRADUAL
#define GUARDIA_POLITICA_GRADUAL 0
#endif
static_assert(GUARDIA_CONTRATO_MAJOR == 1, "Contrato da Guardia incompativel");

#define COFLUME_VERSAO "v0.9-agente-usb-experimental-r3"

// Pinagem validada no hardware fisico.
constexpr uint8_t TFT_CS=14, TFT_RST=16, TFT_DC=15, TFT_MOSI=17, TFT_SCLK=18;
constexpr uint32_t TEMPO_QUESTAO_MS=30000UL;
constexpr float TEMPO_LIMITE_S=30.0f;
constexpr uint32_t FEEDBACK_MS=1700UL;        // resposta correta (igual a v0.7)
constexpr uint32_t FEEDBACK_ERRO_MS=3500UL;   // erro: tempo para ler a alternativa correta e a dica
constexpr uint32_t BOOT_TELA_MS=1500UL;       // tela de diagnostico ao ligar
constexpr int ANGULO_FECHADO=90, ANGULO_ABERTO=0;
constexpr uint8_t SERVO_PIN = 12;
constexpr uint32_t SERVO_FREQ = 50;
constexpr uint8_t SERVO_RESOLUCAO = 14;

bool servoPronto = false;
constexpr int N_LATENCIA=1000;                // chamadas de prever() na medicao informativa


char mapa[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte linhas[4]={8,9,10,11};   // linhas do teclado
byte colunas[4]={4,5,6,7};    // colunas do teclado
Keypad teclado(makeKeymap(mapa),linhas,colunas,4,4);
Adafruit_ST7735 tft(&SPI,TFT_CS,TFT_DC,TFT_RST);

const char* const NOME_NIVEL[3]={"Explorador","Desafiante","Mestre"};
const char* const NOME_NIVEL_MAIUSC[3]={"EXPLORADOR","DESAFIANTE","MESTRE"};

enum Estado {MENU, PERGUNTA, RESULTADO, VITORIA, DERROTA, ERRO_BANCO, AGENTE};
Estado estado=MENU;
namespace agente { void tecla(char k); void iniciar(); }
int nivel=0, nivelInicial=0, indice=0, acertos=0, ultimoSegundo=-1;
char selecionada='\0';
bool dicaExibida=false, respostaCerta=false;
uint32_t inicioPergunta=0, inicioResultado=0, duracaoFeedback=FEEDBACK_MS;

// --- Estado da Guardia e da partida (igual a v0.8) -----------------------------
bool guardiaAtiva=false;
bool bancoOK=false;
bool bancoGradualOK=false;          // banco viavel para a politica gradual (so com GUARDIA_POLITICA_GRADUAL 1)
coflume::gradual::Anterior anteriorGradual;   // resultado da pergunta anterior, na mesma partida
coflume::gradual::Decisao decisaoGradual;     // ultima decisao gradual (campos de auditoria)
const char* nomePolitica(){return GUARDIA_POLITICA_GRADUAL?coflume::gradual::VERSAO:"fluxo-0.8";}
bool autotesteOK=false;
uint32_t sessaoId=0;
uint8_t ordemCategorias[coflume::N_CATEGORIAS];
uint8_t restantes[coflume::N_CATEGORIAS];
int nRestantes=0;
bool usada[COFLUME_BANCO_N];
int perguntaAtual=0;
coflume::Rodizio rodizio;
coflume::Decisao decisao;
guardia::Historico historico;
int64_t decisaoUs=0;            // tempo da ultima decisao (todas as inferencias), informativo
float latenciaPreverUs=0.0f;    // media medida ao ligar, informativa

// --- Posicoes das alternativas na tela atual (para o destaque da selecao) ------
int yAlternativa[4]={0,0,0,0};
int linhasAlternativa[4]={0,0,0,0};

// --- Servo real (LEDC) e LEDs simulados ---------------------------------------
int anguloServo=-1;
bool ledVermelho=false, ledVerde=false;

const coflume::Pergunta& questao(){return coflume::BANCO[perguntaAtual];}
uint32_t aleatorio(){return esp_random();}

float preverGuardia(void*, uint8_t nivelCandidato, uint8_t categoria){
  return guardia::prever(guardia::montar_entrada(historico,(uint8_t)nivelInicial,categoria,nivelCandidato));
}

void servoAngulo(int graus){
  if(!servoPronto) return;
  if(graus==anguloServo) return;

  graus = constrain(graus, 0, 180);

  uint32_t pulso = map(graus, 0, 180, 500, 2400);
  uint32_t duty = (pulso * 16383UL) / 20000UL;

  ledcWrite(SERVO_PIN, duty);
  anguloServo = graus;

  Serial.printf("SERVO,angulo=%d\n", graus);
}
void ledSimulado(const char* nome,bool& estadoLed,bool ligado){
  if(estadoLed==ligado) return;
  estadoLed=ligado;
  Serial.printf("SIMULACAO,%s=%d\n",nome,ligado?1:0);
}
void ledVermelhoSet(bool v){ledSimulado("led_vermelho",ledVermelho,v);}
void ledVerdeSet(bool v){ledSimulado("led_verde",ledVerde,v);}
void fechar(){servoAngulo(ANGULO_FECHADO);ledVerdeSet(false);}
void abrir(){servoAngulo(ANGULO_ABERTO);ledVerdeSet(true);ledVermelhoSet(false);}

// --- Desenho -------------------------------------------------------------------
void texto(int x,int y,int tam,uint16_t cor,const char* s){
  tft.setTextSize(tam);tft.setTextColor(cor);tft.setCursor(x,y);tft.print(s);
}
// Texto com quebra por palavra; devolve o y depois da ultima linha.
int paragrafo(int x,int y,int tam,uint16_t cor,const char* s,int recuo){
  const tela::Linhas L=tela::quebrar(s,tela::colunas(tam),recuo);
  for(int i=0;i<L.n;++i) texto(x,y+i*tela::CHAR_H*tam,tam,cor,L.texto[i]);
  return y+L.n*tela::CHAR_H*tam;
}
void limpar(){tft.fillScreen(ST77XX_BLACK);}

void telaBoot(){
  limpar();
  texto(tela::MARGEM_X,4,2,ST77XX_CYAN,"COFLUME");
  texto(tela::MARGEM_X,24,1,ST77XX_WHITE,"v0.9 experimental");
  texto(tela::MARGEM_X,44,1,ST77XX_WHITE,"Guardia IA (MLP 13-8-1)");
  char l[32];
  snprintf(l,sizeof(l),"Autoteste: %s %d vet.",autotesteOK?"OK":"FALHA",GUARDIA_AUTOTESTE_N);
  texto(tela::MARGEM_X,60,1,autotesteOK?ST77XX_GREEN:ST77XX_RED,l);
  snprintf(l,sizeof(l),"prever(): %.1f us",(double)latenciaPreverUs);
  texto(tela::MARGEM_X,72,1,ST77XX_WHITE,l);
  texto(tela::MARGEM_X,84,1,ST77XX_WHITE,guardiaAtiva?"Modo: adaptativo":"Modo: nivel fixo");
  texto(tela::MARGEM_X,96,1,GUARDIA_POLITICA_GRADUAL?ST77XX_CYAN:ST77XX_WHITE,
        GUARDIA_POLITICA_GRADUAL?"Politica: gradual (exp.)":"Politica: fluxo-0.8");
  texto(tela::MARGEM_X,108,1,ST77XX_YELLOW,"Dados sinteticos");
  if(servoPronto) texto(tela::MARGEM_X,118,1,ST77XX_YELLOW,"Servo real / LEDs simul.");
  else texto(tela::MARGEM_X,118,1,ST77XX_RED,"SERVO COM FALHA (GPIO 12)");
}
void menu(){
  estado=MENU;
  fechar();ledVermelhoSet(false);
  limpar();
  texto(tela::MARGEM_X,2,2,ST77XX_CYAN,"COFLUME");
  texto(tela::MARGEM_X,20,1,ST77XX_WHITE,"Conhecimento abre portas");
  tft.drawFastHLine(0,30,tela::LARGURA,ST77XX_CYAN);
  texto(tela::MARGEM_X,36,2,ST77XX_YELLOW,"1 Explorador");
  texto(tela::MARGEM_X,56,2,ST77XX_YELLOW,"2 Desafiante");
  texto(tela::MARGEM_X,76,2,ST77XX_YELLOW,"3 Mestre");
  if(servoPronto) texto(tela::MARGEM_X,97,1,ST77XX_CYAN,"A Agente Secreto (USB)");
  else texto(tela::MARGEM_X,97,1,ST77XX_RED,"A Agente: SERVO C/ FALHA");
  texto(tela::MARGEM_X,107,1,ST77XX_WHITE,"Escolha 1-3 ou A");
  texto(tela::MARGEM_X,116,1,guardiaAtiva?ST77XX_GREEN:ST77XX_YELLOW,
        guardiaAtiva?"Guardia IA: ativa":"Guardia IA: nivel fixo");
  Serial.println("MENU");
}
void erroBanco(const char* motivo){
  estado=ERRO_BANCO;
  fechar();ledVermelhoSet(true);
  limpar();
  texto(tela::MARGEM_X,4,2,ST77XX_RED,"BANCO");
  texto(tela::MARGEM_X,20,2,ST77XX_RED,"INCOMPLETO");
  paragrafo(tela::MARGEM_X,48,1,ST77XX_YELLOW,"Faltam perguntas para esta partida.",0);
  texto(tela::MARGEM_X,116,1,ST77XX_WHITE,"# volta ao menu");
  Serial.printf("GUARDIA,erro,%s,politica=%s\n",motivo,nomePolitica());
}
void desenharTempo(int s){
  // Contador no canto superior direito e barra proporcional ao tempo restante.
  char t[8];snprintf(t,sizeof(t),"%02ds",s);
  const int x=tela::LARGURA-tela::largura_texto(t,1);
  tft.fillRect(x,tela::Y_CABECALHO,tela::LARGURA-x,tela::CHAR_H,ST77XX_BLACK);
  texto(x,tela::Y_CABECALHO,1,s<=10?ST77XX_RED:ST77XX_YELLOW,t);
  const int cheio=(tela::LARGURA*s)/30;
  tft.fillRect(0,tela::Y_BARRA,cheio,tela::ALTURA_BARRA,s<=10?ST77XX_RED:ST77XX_GREEN);
  tft.fillRect(cheio,tela::Y_BARRA,tela::LARGURA-cheio,tela::ALTURA_BARRA,ST77XX_BLACK);
}
void desenharAlternativa(int a,bool destaque){
  const coflume::Pergunta& q=questao();
  char s[48];snprintf(s,sizeof(s),"%c) %s",'A'+a,q.alt[a]);
  const int h=linhasAlternativa[a]*tela::CHAR_H*2;
  tft.fillRect(0,yAlternativa[a],tela::LARGURA,h,destaque?ST77XX_YELLOW:ST77XX_BLACK);
  paragrafo(tela::MARGEM_X,yAlternativa[a],2,destaque?ST77XX_BLACK:ST77XX_GREEN,s,3);
}
void desenharRodape(){
  tft.fillRect(0,tela::Y_RODAPE,tela::LARGURA,tela::ALTURA-tela::Y_RODAPE,ST77XX_BLACK);
  if(dicaExibida){
    char s[64];snprintf(s,sizeof(s),"Dica: %s",questao().dica);
    paragrafo(tela::MARGEM_X,tela::Y_RODAPE,1,ST77XX_YELLOW,s,0);
  }else{
    texto(tela::MARGEM_X,tela::Y_RODAPE+4,1,ST77XX_WHITE,"A-D marca *dica #confirma");
  }
}

// --- Log serial: demonstra que a escolha veio das previsoes da MLP (igual a v0.8) -
void imprimirPrevisao(int n){
  if(decisao.avaliado[n]) Serial.printf(",p%d=%.6f",n,(double)decisao.previsoes[n]);
  else Serial.printf(",p%d=NA",n);
}
void logDecisao(){
  Serial.printf("GUARDIA,decisao,modelo=%s,sessao=%08lX,ordem=%d,nivel_inicial=%d,categoria_sorteada=%d,categoria=%d,"
                "categoria_substituida=%d,niveis_permitidos=",
                GUARDIA_MODELO_ID,(unsigned long)sessaoId,indice+1,nivelInicial,decisao.categoria_sorteada,
                decisao.categoria,decisao.categoria_substituida()?1:0);
  for(int i=0;i<decisao.n_permitidos;++i) Serial.printf("%s%d",i?"|":"",decisao.permitidos[i]);
  Serial.print(",niveis_disponiveis=");
  for(int i=0;i<decisao.n_disponiveis;++i) Serial.printf("%s%d",i?"|":"",decisao.disponiveis[i]);
  Serial.printf(",celula_vazia=%d",decisao.celula_vazia()?1:0);
  for(int n=0;n<coflume::N_NIVEIS;++n) imprimirPrevisao(n);
  Serial.printf(",alvo=0.70,nivel_escolhido=%d,p_escolhido=",decisao.nivel);
  if(decisao.p_previsto>=0.0f) Serial.printf("%.6f",(double)decisao.p_previsto); else Serial.print("INVALIDO");
  Serial.printf(",motivo=%s,pergunta=%s,decisao_us=%lld\n",coflume::nome_motivo(decisao.motivo),questao().id,
                (long long)decisaoUs);
  const guardia::Entrada e=guardia::montar_entrada(historico,(uint8_t)nivelInicial,decisao.categoria,decisao.nivel);
  Serial.printf("GUARDIA,features,sessao=%08lX,ordem=%d,nivel_candidato=%d,nivel_inicial=%d,n_respondidas=%d,categoria=%d,"
                "taxa_acerto_sessao=%.6f,taxa_acerto_categoria=%.6f,tempo_medio_norm=%.6f,dicas_por_questao=%.6f,"
                "resultado_ultima=%d,sequencia=%d\n",
                (unsigned long)sessaoId,indice+1,e.nivel_candidato,e.nivel_inicial,e.n_respondidas,e.categoria,
                (double)e.taxa_acerto_sessao,(double)e.taxa_acerto_categoria,(double)e.tempo_medio_norm,
                (double)e.dicas_por_questao,e.resultado_ultima,e.sequencia);
}

void mostrarQuestao(){
  estado=PERGUNTA;selecionada='\0';dicaExibida=false;ultimoSegundo=-1;
  limpar();
  char cab[24];snprintf(cab,sizeof(cab),"%s %d/3",NOME_NIVEL_MAIUSC[nivel],indice+1);
  texto(0,tela::Y_CABECALHO,1,ST77XX_CYAN,cab);
  const coflume::Pergunta &q=questao();
  int y=paragrafo(tela::MARGEM_X,tela::Y_CORPO,2,ST77XX_WHITE,q.enunciado,0);
  for(int a=0;a<4;++a){
    char s[48];snprintf(s,sizeof(s),"%c) %s",'A'+a,q.alt[a]);
    yAlternativa[a]=y;
    linhasAlternativa[a]=tela::quebrar(s,tela::colunas(2),3).n;
    desenharAlternativa(a,false);
    y+=linhasAlternativa[a]*tela::CHAR_H*2;
  }
  if(y>tela::Y_RODAPE) Serial.printf("TELA,aviso,pergunta=%s,invade_rodape_px=%d\n",q.id,y-tela::Y_RODAPE);
  desenharRodape();
  desenharTempo(30);
  Serial.printf("QUESTAO:%d NIVEL:%d ID:%s\n",indice+1,nivel+1,q.id);
  // O cronometro de 30 s comeca SO depois da decisao da Guardia e da tela desenhada.
  inicioPergunta=millis();
}

// Copia os campos comuns da decisao gradual para a decisao usada no resto do jogo e no log.
void copiarDecisaoGradual(const coflume::gradual::Decisao& g){
  decisao=coflume::Decisao();
  decisao.viavel=g.viavel;decisao.ordem=g.ordem;decisao.nivel_inicial=g.nivel_inicial;
  decisao.categoria_sorteada=g.categoria_sorteada;decisao.categoria=g.categoria;
  decisao.n_permitidos=g.n_permitidos;decisao.n_disponiveis=g.n_disponiveis;
  for(int i=0;i<coflume::N_NIVEIS;++i){
    decisao.permitidos[i]=g.permitidos[i];decisao.disponiveis[i]=g.disponiveis[i];
    decisao.previsoes[i]=g.previsoes[i];decisao.avaliado[i]=g.avaliado[i];
  }
  decisao.nivel=g.nivel;decisao.p_previsto=g.p_previsto;decisao.motivo=g.motivo;
}
void listaNiveis(const uint8_t* v,int n){for(int i=0;i<n;++i) Serial.printf("%s%d",i?"|":"",v[i]);if(!n)Serial.print("-");}
// Campos de auditoria da politica gradual (docs/politica_gradual_exp.md, secao 5).
void logGradual(){
  const coflume::gradual::Decisao& g=decisaoGradual;
  Serial.printf("GUARDIA,politica_gradual,politica=%s,sessao=%08lX,ordem=%d,anterior=",nomePolitica(),(unsigned long)sessaoId,indice+1);
  if(anteriorGradual.existe) Serial.printf("%d,anterior_acertou=%d",anteriorGradual.nivel,anteriorGradual.acertou?1:0);
  else Serial.print("-,anterior_acertou=-");
  Serial.print(",niveis_compativeis=");listaNiveis(g.compativeis,g.n_compativeis);
  Serial.print(",candidatos=");listaNiveis(g.candidatos,g.n_candidatos);
  Serial.printf(",categoria_fluxo08=%d,categoria=%d,nivel_sem_regra=%d,nivel=%d,limitou=%d,motivo=%s\n",
                g.categoria_fluxo08,g.viavel?g.categoria:-1,g.nivel_sem_regra,g.viavel?g.nivel:-1,g.limitou()?1:0,g.nome_motivo());
}
void prepararPergunta(){
  const int64_t t0=esp_timer_get_time();
  if(GUARDIA_POLITICA_GRADUAL){
    decisaoGradual=coflume::gradual::decidir_pergunta(indice,(uint8_t)nivelInicial,restantes,nRestantes,
                                                      coflume::gradual::tem_pergunta_banco,nullptr,
                                                      guardiaAtiva?preverGuardia:nullptr,nullptr,anteriorGradual);
    decisaoUs=esp_timer_get_time()-t0;
    logGradual();
    // Sem nivel compativel: recusa explicita. NUNCA volta ao fluxo-0.8 para contornar.
    if(!decisaoGradual.viavel){erroBanco("sem_nivel_compativel");return;}
    copiarDecisaoGradual(decisaoGradual);
  }else{
    decisao=coflume::decidir_pergunta(indice,(uint8_t)nivelInicial,restantes,nRestantes,
                                      guardiaAtiva?preverGuardia:nullptr,nullptr);
    decisaoUs=esp_timer_get_time()-t0;
  }
  if(!decisao.viavel){erroBanco("banco_inviavel");return;}
  const int idx=coflume::escolher_pergunta(rodizio,decisao.nivel,decisao.categoria,usada);
  if(idx<0){erroBanco("banco_inviavel");return;}
  perguntaAtual=idx;usada[idx]=true;nivel=decisao.nivel;
  int w=0;
  for(int k=0;k<nRestantes;++k) if(restantes[k]!=decisao.categoria) restantes[w++]=restantes[k];
  nRestantes=w;
  logDecisao();
  mostrarQuestao();
}

void iniciarPartida(int nivelEscolhido){
  nivelInicial=nivelEscolhido;indice=0;acertos=0;
  sessaoId=aleatorio();
  coflume::sortear_categorias(ordemCategorias,aleatorio);
  for(int k=0;k<coflume::N_CATEGORIAS;++k) restantes[k]=ordemCategorias[k];
  nRestantes=coflume::N_CATEGORIAS;
  for(int i=0;i<COFLUME_BANCO_N;++i) usada[i]=false;
  guardia::historico_limpar(historico);  // nada da partida anterior e herdado
  anteriorGradual=coflume::gradual::Anterior();
  Serial.printf("GUARDIA,sessao,sessao=%08lX,modelo=%s,adaptativa=%d,nivel_inicial=%d,ordem_categorias=%d%d%d%d,politica=%s\n",
                (unsigned long)sessaoId,GUARDIA_MODELO_ID,guardiaAtiva?1:0,nivelInicial,
                ordemCategorias[0],ordemCategorias[1],ordemCategorias[2],ordemCategorias[3],nomePolitica());
  if(!bancoOK){erroBanco("banco_inviavel");return;}
  // Politica gradual: o quiz so comeca com banco validado (verificado ao ligar).
  if(GUARDIA_POLITICA_GRADUAL&&!bancoGradualOK){erroBanco("banco_inviavel_gradual");return;}
  prepararPergunta();
}

void resultado(bool acertou,bool esgotou){
  estado=RESULTADO;inicioResultado=millis();respostaCerta=acertou;
  float gasto=esgotou?TEMPO_LIMITE_S:(float)(millis()-inicioPergunta)/1000.0f;
  if(gasto>TEMPO_LIMITE_S) gasto=TEMPO_LIMITE_S;
  const guardia::Resposta r{decisao.categoria,(uint8_t)nivel,acertou,gasto,TEMPO_LIMITE_S,dicaExibida};
  guardia::historico_adicionar(historico,r);
  anteriorGradual.existe=true;anteriorGradual.nivel=(uint8_t)nivel;anteriorGradual.acertou=acertou;
  if(acertou) acertos++;
  Serial.printf("GUARDIA,resposta,sessao=%08lX,ordem=%d,pergunta=%s,categoria=%d,nivel=%d,acertou=%d,usou_dica=%d,"
                "tempo_gasto_s=%.1f,tempo_limite_s=%.0f,esgotou=%d,historico_n=%d\n",
                (unsigned long)sessaoId,indice+1,questao().id,decisao.categoria,nivel,acertou?1:0,dicaExibida?1:0,
                (double)gasto,(double)TEMPO_LIMITE_S,esgotou?1:0,historico.n);
  duracaoFeedback=acertou?FEEDBACK_MS:FEEDBACK_ERRO_MS;
  ledVermelhoSet(!acertou);
  limpar();
  const coflume::Pergunta &q=questao();
  if(acertou){
    texto(tela::MARGEM_X,2,2,ST77XX_GREEN,"RESPOSTA");
    texto(tela::MARGEM_X,18,2,ST77XX_GREEN,"CORRETA!");
  }else{
    texto(tela::MARGEM_X,2,2,ST77XX_YELLOW,"VAMOS");
    texto(tela::MARGEM_X,18,2,ST77XX_YELLOW,"APRENDER!");
  }
  int y=36;
  if(esgotou){texto(tela::MARGEM_X,y,1,ST77XX_RED,"Tempo esgotado!");y+=12;}
  char s[24];snprintf(s,sizeof(s),"Resposta: %c",q.correta);
  texto(tela::MARGEM_X,y,2,ST77XX_WHITE,s);y+=16;
  y=paragrafo(tela::MARGEM_X,y,2,ST77XX_CYAN,q.alt[q.correta-'A'],0);
  if(!acertou){
    char d[64];snprintf(d,sizeof(d),"Dica: %s",q.dica);
    paragrafo(tela::MARGEM_X,y+4,1,ST77XX_WHITE,d,0);
  }
  char a[32];snprintf(a,sizeof(a),"Acertos: %d de %d",acertos,indice+1);
  texto(tela::MARGEM_X,118,1,ST77XX_YELLOW,a);
  Serial.printf("RESULTADO:%s\n",acertou?"ACERTO":"ERRO");
}
void finalizar(){
  // Validador do quiz: 2/3 acertos. A Guardia nao participa da abertura.
  const bool abre=coflume::desbloqueia(acertos);
  Serial.printf("GUARDIA,fim,sessao=%08lX,acertos=%d,desbloqueio=%s\n",(unsigned long)sessaoId,acertos,abre?"SIM":"NAO");
  limpar();
  if(abre){
    estado=VITORIA;abrir();
    texto(tela::MARGEM_X,8,2,ST77XX_GREEN,"COFRE");
    texto(tela::MARGEM_X,26,2,ST77XX_GREEN,"ABERTO!");
    paragrafo(tela::MARGEM_X,56,1,ST77XX_WHITE,"Parabens! Premio liberado.",0);
    Serial.println("WIN");
  } else {
    estado=DERROTA;fechar();ledVermelhoSet(true);
    texto(tela::MARGEM_X,8,2,ST77XX_YELLOW,"NOVA");
    texto(tela::MARGEM_X,26,2,ST77XX_YELLOW,"CHANCE!");
    paragrafo(tela::MARGEM_X,56,1,ST77XX_WHITE,"Tente novamente para aprender!",0);
    Serial.println("LOSE");
  }
  char a[32];snprintf(a,sizeof(a),"Acertos: %d de 3",acertos);
  texto(tela::MARGEM_X,84,1,ST77XX_YELLOW,a);
  texto(tela::MARGEM_X,116,1,ST77XX_WHITE,"# volta ao menu");
}
void proxima(){
  indice++;
  ledVermelhoSet(false);
  if(indice>=coflume::N_PERGUNTAS) finalizar(); else prepararPergunta();
}
void tecla(char k){
  Serial.printf("KEY:%c\n",k);
  if(estado==AGENTE){agente::tecla(k);return;}
  if(estado==MENU){
    if(k=='A'){agente::iniciar();return;}
    if(k>='1'&&k<='3') iniciarPartida(k-'1');
    return;
  }
  if(estado==VITORIA||estado==DERROTA||estado==ERRO_BANCO){if(k=='#')menu();return;}
  if(estado!=PERGUNTA)return;
  // Prazo tem prioridade, inclusive ao apertar a tecla na virada do cronometro.
  if(millis()-inicioPergunta>=TEMPO_QUESTAO_MS){resultado(false,true);return;}
  if(k>='A'&&k<='D'){
    if(selecionada!='\0') desenharAlternativa(selecionada-'A',false);
    selecionada=k;
    desenharAlternativa(k-'A',true);
  }else if(k=='*'){
    dicaExibida=true;
    desenharRodape();
  }else if(k=='#'&&selecionada!='\0'){
    resultado(selecionada==questao().correta,false);
  }
}

// --- Diagnostico da Guardia ao ligar (igual a v0.8) ----------------------------
// As linhas GUARDIA do boot ficam guardadas e sao reenviadas quando o painel USB
// se conecta (AG,HELLO), porque o painel costuma abrir a porta depois do boot.
// Sao linhas comuns de log: o protocolo AG nao muda.
char diagBoot[1024]="";
size_t nDiagBoot=0;
void registrarDiag(const char* linha){
  Serial.print(linha);
  const size_t n=strlen(linha);
  if(nDiagBoot+n<sizeof(diagBoot)){memcpy(diagBoot+nDiagBoot,linha,n);nDiagBoot+=n;diagBoot[nDiagBoot]='\0';}
}
void reenviarDiagBoot(){
  Serial.print("GUARDIA,reenvio_diagnostico_boot,motivo=painel_conectado\n");
  Serial.print(diagBoot);
}
bool autotesteGuardia(){
  float erroP=0.0f, erroF=0.0f;
  long falhas=0;
  guardia::Historico h;
  for(int k=0;k<GUARDIA_AUTOTESTE_N;++k){
    const GuardiaAutoteste& v=GUARDIA_AUTOTESTE[k];
    guardia::historico_limpar(h);
    for(int i=0;i<v.n_historico;++i) guardia::historico_adicionar(h,v.historico[i]);
    const guardia::Entrada e=guardia::montar_entrada(h,v.nivel_inicial,v.categoria,v.nivel_candidato);
    const guardia::Entrada& x=v.entrada_esperada;
    if(e.nivel_candidato!=x.nivel_candidato||e.nivel_inicial!=x.nivel_inicial||e.n_respondidas!=x.n_respondidas||
       e.categoria!=x.categoria||e.resultado_ultima!=x.resultado_ultima||e.sequencia!=x.sequencia) ++falhas;
    const float df[4]={fabsf(e.taxa_acerto_sessao-x.taxa_acerto_sessao),fabsf(e.taxa_acerto_categoria-x.taxa_acerto_categoria),
                       fabsf(e.tempo_medio_norm-x.tempo_medio_norm),fabsf(e.dicas_por_questao-x.dicas_por_questao)};
    for(float d:df) if(!(d<=erroF)) erroF=d;
    const float p=guardia::prever(e);
    if(!coflume::previsao_valida(p)) ++falhas;
    const float dp=fabsf(p-v.p_esperado);
    if(!(dp<=erroP)) erroP=dp;
  }
  guardia::historico_limpar(h);
  guardia::Entrada inv1=guardia::montar_entrada(h,1,0,1); inv1.nivel_candidato=3;
  guardia::Entrada inv2=guardia::montar_entrada(h,1,0,1); inv2.taxa_acerto_sessao=NAN;
  const int invalidosOk=(guardia::prever(inv1)==guardia::INVALIDO)+(guardia::prever(inv2)==guardia::INVALIDO);
  const bool idOk=strcmp(GUARDIA_AUTOTESTE_MODELO_ID,GUARDIA_MODELO_ID)==0;
  const bool ok=idOk&&falhas==0&&erroP<1e-5f&&erroF<=1e-6f&&invalidosOk==2;
  char l[256];
  snprintf(l,sizeof(l),"GUARDIA,autoteste,vetores=%d,erro_p_max=%.3g,erro_features_max=%.3g,falhas=%ld,invalidos_ok=%d/2,modelo_confere=%s,resultado=%s\n",
           GUARDIA_AUTOTESTE_N,(double)erroP,(double)erroF,falhas,invalidosOk,idOk?"sim":"nao",ok?"OK":"FALHA");
  registrarDiag(l);
  return ok;
}
// Latencia real de prever() NESTA placa (informativa; nao substitui a bancada da fase D).
void medirLatencia(){
  guardia::Historico h;guardia::historico_limpar(h);
  volatile float soma=0.0f;
  const int64_t t0=esp_timer_get_time();
  for(int i=0;i<N_LATENCIA;++i){
    const GuardiaAutoteste& v=GUARDIA_AUTOTESTE[i%GUARDIA_AUTOTESTE_N];
    soma=soma+guardia::prever(v.entrada_esperada);
  }
  const int64_t t1=esp_timer_get_time();
  latenciaPreverUs=(float)(t1-t0)/N_LATENCIA;
  char l[192];
  snprintf(l,sizeof(l),"GUARDIA,latencia_informativa,chamadas=%d,prever_us_media=%.2f,cpu_mhz=%lu,obs=medida_nesta_placa_nao_e_bancada_fase_D\n",
           N_LATENCIA,(double)latenciaPreverUs,(unsigned long)getCpuFrequencyMhz());
  registrarDiag(l);
}

#include "coflume_agente.h"

void setup(){
  Serial.begin(115200);
  servoPronto = ledcAttach(
    SERVO_PIN,
    SERVO_FREQ,
    SERVO_RESOLUCAO
  );

  const uint32_t t0=millis();
  while(!Serial&&millis()-t0<3000) delay(10);  // USB CDC: espera o monitor por ate 3 s
  // Impressa DEPOIS da espera: com USB CDC, antes dela a linha podia se perder.
  Serial.printf("SERVO,inicializacao=%s\n",
                servoPronto ? "OK" : "FALHA");
  SPI.begin(TFT_SCLK,-1,TFT_MOSI,TFT_CS);
  tft.initR(TFT_PAINEL);
  tft.setRotation(TFT_ROTACAO);
  tft.setTextWrap(false);  // a quebra de linha e feita por palavra em coflume_tela.h
  Serial.printf("COFLUME,versao=%s,banco=%s,perguntas=%d\n",COFLUME_VERSAO,COFLUME_BANCO_VERSAO,COFLUME_BANCO_N);
  Serial.printf("HARDWARE,placa=ESP32-S3 DevKitC-1 N16R8,chip=%s,tela=ST7735 %dx%d,rotacao=%d,"
                "tft_cs=%d,tft_dc=%d,tft_rst=%d,tft_mosi=%d,tft_sclk=%d,teclado_linhas=8|9|10|11,teclado_colunas=4|5|6|7,"
                "servo=REAL_GPIO12,leds=SIMULADOS\n",
                ESP.getChipModel(),(int)tft.width(),(int)tft.height(),TFT_ROTACAO,TFT_CS,TFT_DC,TFT_RST,TFT_MOSI,TFT_SCLK);
  char l[320];
  snprintf(l,sizeof(l),"GUARDIA,modelo,id=%s,hash=%s,contrato=%s,fonte=%s,uso=%s,parametros=%d\n",GUARDIA_MODELO_ID,
           GUARDIA_MODELO_HASH,GUARDIA_CONTRATO_VERSAO,GUARDIA_MODELO_FONTE,GUARDIA_MODELO_USO,
           GUARDIA_MODELO_N_PARAMETROS);
  registrarDiag(l);
  coflume::rodizio_limpar(rodizio);
  bancoOK=coflume::banco_viavel();
  bancoGradualOK=GUARDIA_POLITICA_GRADUAL&&coflume::gradual::banco_viavel(coflume::gradual::tem_pergunta_banco,nullptr);
  autotesteOK=autotesteGuardia();
  medirLatencia();
  guardiaAtiva=GUARDIA_ADAPTATIVA&&autotesteOK;
  snprintf(l,sizeof(l),"GUARDIA,estado,adaptativa_configurada=%d,banco_viavel=%d,guardia_ativa=%d,servo=%s,politica=%s,banco_viavel_gradual=%s\n",
           GUARDIA_ADAPTATIVA,bancoOK?1:0,guardiaAtiva?1:0,servoPronto?"OK":"FALHA",nomePolitica(),
           GUARDIA_POLITICA_GRADUAL?(bancoGradualOK?"1":"0"):"NA");
  registrarDiag(l);
  fechar();ledVermelhoSet(false);
  telaBoot();
  delay(BOOT_TELA_MS);
  menu();
}
void loop(){
  agente::lerSerial();
  agente::atualizar();
  char k=teclado.getKey();if(k!=NO_KEY)tecla(k);
  if(estado==AGENTE)return;
  if(estado==PERGUNTA){
    uint32_t passado=millis()-inicioPergunta;
    if(passado>=TEMPO_QUESTAO_MS){resultado(false,true);return;}
    int faltam=(TEMPO_QUESTAO_MS-passado+999)/1000;
    if(faltam!=ultimoSegundo){
      ultimoSegundo=faltam;desenharTempo(faltam);
      ledVermelhoSet(faltam<=10&&faltam%2==0);
    }
  }else if(estado==RESULTADO && millis()-inicioResultado>=duracaoFeedback){
    proxima();
  }
}
