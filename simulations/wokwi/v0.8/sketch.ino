// COFLUME v0.8 EXPERIMENTAL - Simulacao Wokwi (ESP32-S3 + ILI9341) com Guardia IA
// A tela REAL de 1,8 polegada usa ST7735 (ou compativel): firmware diferente.
// Base: v0.7 (simulations/wokwi/v0.7, intacta). Mesmos pinos, menu, teclado, LEDs e servo.
// Guardia IA: MLP mlp8-sint-20261101-e6a4cf00 (13 -> 8 -> 1), treinada com dados SINTETICOS,
// executada offline no ESP32-S3 (sem Wi-Fi, sem servidor). NAO e versao de producao:
// teste lacrado e placa fisica pendentes; nao validada com criancas reais.
// A Guardia so estima P(acerto) para escolher o nivel das perguntas 2 e 3.
// O servo e acionado SOMENTE pelo validador do jogo (2 ou mais acertos em 3).
#include <Arduino.h>
#include <SPI.h>
#include <Keypad.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <esp_random.h>

#include "guardia_modelo.h"
#include "guardia_runtime.h"
#include "guardia_autoteste.h"
#include "coflume_banco.h"
#include "coflume_politica.h"

// 1 = Guardia escolhe o nivel das perguntas 2 e 3; 0 = nivel fixo, como na v0.7.
#ifndef GUARDIA_ADAPTATIVA
#define GUARDIA_ADAPTATIVA 1
#endif
static_assert(GUARDIA_CONTRATO_MAJOR == 1, "Contrato da Guardia incompativel");

#define COFLUME_VERSAO "v0.8-experimental"

constexpr uint8_t TFT_CS=16, TFT_DC=15, TFT_MOSI=17, TFT_SCK=18;
constexpr uint8_t SERVO_PIN=12, LED_VERMELHO=13, LED_VERDE=14;
constexpr uint32_t TEMPO_QUESTAO_MS=30000UL;
constexpr float TEMPO_LIMITE_S=30.0f;
constexpr uint32_t FEEDBACK_MS=1700UL;        // resposta correta (igual a v0.7)
constexpr uint32_t FEEDBACK_ERRO_MS=3500UL;   // erro: tempo para ler a alternativa correta e a dica
constexpr int ANGULO_FECHADO=90, ANGULO_ABERTO=0;
constexpr uint32_t PWM_FREQ=50;
constexpr uint8_t PWM_BITS=14;
constexpr int ALT_MAX_TAMANHO_2=9;            // alternativas maiores usam letra tamanho 1

char mapa[4][4]={{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
byte linhas[4]={4,5,6,7};
byte colunas[4]={8,9,10,11};
Keypad teclado(makeKeymap(mapa),linhas,colunas,4,4);
Adafruit_ILI9341 tela(TFT_CS,TFT_DC);

const char* const NOME_NIVEL[3]={"Explorador","Desafiante","Mestre"};
const char* const NOME_CATEGORIA[4]={"Matematica","Portugues","Logica","Conhecimentos gerais"};

enum Estado {MENU, PERGUNTA, RESULTADO, VITORIA, DERROTA, ERRO_BANCO};
Estado estado=MENU;
int nivel=0, nivelInicial=0, indice=0, acertos=0, ultimoSegundo=-1;
char selecionada='\0';
bool dicaExibida=false, respostaCerta=false;
uint32_t inicioPergunta=0, inicioResultado=0, duracaoFeedback=FEEDBACK_MS;
bool pwmOK=false;

// --- Estado da Guardia e da partida -------------------------------------------
bool guardiaAtiva=false;      // GUARDIA_ADAPTATIVA e autoteste aprovado
bool bancoOK=false;           // todas as 72 combinacoes de partida viaveis
uint32_t sessaoId=0;
uint8_t ordemCategorias[coflume::N_CATEGORIAS];
uint8_t restantes[coflume::N_CATEGORIAS];
int nRestantes=0;
bool usada[COFLUME_BANCO_N];
int perguntaAtual=0;          // indice em coflume::BANCO
coflume::Rodizio rodizio;
coflume::Decisao decisao;
guardia::Historico historico; // limpo no inicio de cada partida

const coflume::Pergunta& questao(){return coflume::BANCO[perguntaAtual];}

uint32_t aleatorio(){return esp_random();}

// P(acerto) estimada pela MLP para o nivel candidato, com o historico da partida.
float preverGuardia(void*, uint8_t nivelCandidato, uint8_t categoria){
  return guardia::prever(guardia::montar_entrada(historico,(uint8_t)nivelInicial,categoria,nivelCandidato));
}

void servoAngulo(int graus){
  if(!pwmOK) return;
  const uint32_t pulso=map(constrain(graus,0,180),0,180,500,2400);
  const uint32_t duty=(pulso*((1UL<<PWM_BITS)-1))/20000UL;
  ledcWrite(SERVO_PIN,duty);
}
void fechar(){servoAngulo(ANGULO_FECHADO);digitalWrite(LED_VERDE,LOW);}
void abrir(){servoAngulo(ANGULO_ABERTO);digitalWrite(LED_VERDE,HIGH);digitalWrite(LED_VERMELHO,LOW);}

// --- Autoteste de inicializacao (subconjunto dos 400 vetores da bancada) ------
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
  // Contrato, secao 4, item 2: entradas fora da faixa retornam INVALIDO.
  guardia::historico_limpar(h);
  guardia::Entrada inv1=guardia::montar_entrada(h,1,0,1); inv1.nivel_candidato=3;
  guardia::Entrada inv2=guardia::montar_entrada(h,1,0,1); inv2.taxa_acerto_sessao=NAN;
  const int invalidosOk=(guardia::prever(inv1)==guardia::INVALIDO)+(guardia::prever(inv2)==guardia::INVALIDO);
  const bool idOk=strcmp(GUARDIA_AUTOTESTE_MODELO_ID,GUARDIA_MODELO_ID)==0;
  const bool ok=idOk&&falhas==0&&erroP<1e-5f&&erroF<=1e-6f&&invalidosOk==2;
  Serial.printf("GUARDIA,autoteste,vetores=%d,erro_p_max=%.3g,erro_features_max=%.3g,falhas=%ld,invalidos_ok=%d/2,modelo_confere=%s,resultado=%s\n",
                GUARDIA_AUTOTESTE_N,(double)erroP,(double)erroF,falhas,invalidosOk,idOk?"sim":"nao",ok?"OK":"FALHA");
  return ok;
}

void cabecalho(const char* titulo){
  tela.fillScreen(ILI9341_BLACK);
  tela.setTextWrap(true);
  tela.setTextSize(2);
  tela.setTextColor(ILI9341_CYAN);
  tela.setCursor(12,8);tela.print("COFLUME");
  tela.setTextSize(1);tela.setCursor(160,14);tela.print("Conhecimento abre portas");
  tela.drawFastHLine(10,34,300,ILI9341_CYAN);
  tela.setTextColor(ILI9341_WHITE);
  tela.setTextSize(2);tela.setCursor(12,47);tela.println(titulo);
}
void menu(){
  estado=MENU;
  fechar();digitalWrite(LED_VERMELHO,LOW);
  cabecalho("ESCOLHA UM NIVEL");
  tela.setTextSize(2);tela.setTextColor(ILI9341_YELLOW);
  tela.setCursor(20,95);tela.println("1 - Explorador");
  tela.setCursor(20,125);tela.println("2 - Desafiante");
  tela.setCursor(20,155);tela.println("3 - Mestre");
  tela.setTextSize(1);tela.setTextColor(ILI9341_WHITE);
  tela.setCursor(20,210);tela.println("Use 1, 2 ou 3 no teclado");
  tela.setCursor(20,225);
  tela.print(guardiaAtiva?"Guardia IA: ativa (experimental)":"Guardia IA: desativada (nivel fixo)");
  Serial.println("MENU");
}
void erroBanco(){
  estado=ERRO_BANCO;
  fechar();digitalWrite(LED_VERMELHO,HIGH);
  cabecalho("BANCO INCOMPLETO");
  tela.setTextSize(1);tela.setTextColor(ILI9341_YELLOW);
  tela.setCursor(20,100);tela.println("Faltam perguntas para esta partida.");
  tela.setCursor(20,210);tela.println("Pressione # para voltar ao menu");
  Serial.println("GUARDIA,erro,banco_inviavel");
}
void desenharTempo(int s){
  tela.fillRect(238,44,76,22,ILI9341_BLACK);
  tela.setTextColor(s<=10?ILI9341_RED:ILI9341_YELLOW);
  tela.setTextSize(2);tela.setCursor(244,47);tela.printf("%02ds",s);
}
void desenharAlternativa(int x,int y,char letra,const char* texto){
  const bool longa=strlen(texto)>(size_t)ALT_MAX_TAMANHO_2;
  tela.setTextSize(longa?1:2);
  tela.setCursor(x,longa?y+4:y);tela.printf("%c) %s",letra,texto);
}

// --- Log serial: demonstra que a escolha veio das previsoes da MLP -----------
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
  Serial.printf(",motivo=%s,pergunta=%s\n",coflume::nome_motivo(decisao.motivo),questao().id);
  // Features da entrada do nivel apresentado (contrato 1.0.0), para auditoria.
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
  char titulo[40];snprintf(titulo,sizeof(titulo),"QUESTAO %d/3",indice+1);
  cabecalho(titulo);
  tela.setTextSize(1);tela.setTextColor(ILI9341_CYAN);
  tela.setCursor(150,52);tela.print(NOME_NIVEL[nivel]);
  const coflume::Pergunta &q=questao();
  tela.setTextSize(2);tela.setTextColor(ILI9341_WHITE);
  tela.setCursor(12,78);tela.println(q.enunciado);
  tela.setTextColor(ILI9341_GREEN);
  desenharAlternativa(12,115,'A',q.alt[0]);
  desenharAlternativa(165,115,'B',q.alt[1]);
  desenharAlternativa(12,145,'C',q.alt[2]);
  desenharAlternativa(165,145,'D',q.alt[3]);
  tela.setTextSize(1);tela.setTextColor(ILI9341_WHITE);
  tela.setCursor(12,198);tela.println("A-D: marca | #: confirma | *: dica");
  desenharTempo(30);
  Serial.printf("QUESTAO:%d NIVEL:%d ID:%s\n",indice+1,nivel+1,q.id);
  // O cronometro de 30 s comeca SO depois da decisao da Guardia e da tela desenhada.
  inicioPergunta=millis();
}

// Decide categoria e nivel (Guardia nas perguntas 2 e 3) e apresenta a pergunta.
void prepararPergunta(){
  decisao=coflume::decidir_pergunta(indice,(uint8_t)nivelInicial,restantes,nRestantes,
                                    guardiaAtiva?preverGuardia:nullptr,nullptr);
  if(!decisao.viavel){erroBanco();return;}
  const int idx=coflume::escolher_pergunta(rodizio,decisao.nivel,decisao.categoria,usada);
  if(idx<0){erroBanco();return;}
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
  Serial.printf("GUARDIA,sessao,sessao=%08lX,modelo=%s,adaptativa=%d,nivel_inicial=%d,ordem_categorias=%d%d%d%d\n",
                (unsigned long)sessaoId,GUARDIA_MODELO_ID,guardiaAtiva?1:0,nivelInicial,
                ordemCategorias[0],ordemCategorias[1],ordemCategorias[2],ordemCategorias[3]);
  if(!bancoOK){erroBanco();return;}
  prepararPergunta();
}

void resultado(bool acertou,bool esgotou){
  estado=RESULTADO;inicioResultado=millis();respostaCerta=acertou;
  float gasto=esgotou?TEMPO_LIMITE_S:(float)(millis()-inicioPergunta)/1000.0f;
  if(gasto>TEMPO_LIMITE_S) gasto=TEMPO_LIMITE_S;
  // Historico usado pela Guardia nas proximas perguntas da MESMA partida.
  const guardia::Resposta r{decisao.categoria,(uint8_t)nivel,acertou,gasto,TEMPO_LIMITE_S,dicaExibida};
  guardia::historico_adicionar(historico,r);
  if(acertou) acertos++;
  Serial.printf("GUARDIA,resposta,sessao=%08lX,ordem=%d,pergunta=%s,categoria=%d,nivel=%d,acertou=%d,usou_dica=%d,"
                "tempo_gasto_s=%.1f,tempo_limite_s=%.0f,esgotou=%d,historico_n=%d\n",
                (unsigned long)sessaoId,indice+1,questao().id,decisao.categoria,nivel,acertou?1:0,dicaExibida?1:0,
                (double)gasto,(double)TEMPO_LIMITE_S,esgotou?1:0,historico.n);
  duracaoFeedback=acertou?FEEDBACK_MS:FEEDBACK_ERRO_MS;
  digitalWrite(LED_VERMELHO,acertou?LOW:HIGH);
  cabecalho(acertou?"RESPOSTA CORRETA!":"VAMOS APRENDER!");
  const coflume::Pergunta &q=questao();
  if(esgotou){
    tela.setTextColor(ILI9341_RED);tela.setTextSize(1);
    tela.setCursor(15,88);tela.print("Tempo esgotado!");
  }
  tela.setTextColor(ILI9341_YELLOW);tela.setTextSize(2);
  tela.setCursor(15,108);tela.printf("Resposta: %c",q.correta);
  tela.setCursor(15,130);tela.print(q.alt[q.correta-'A']);
  if(!acertou){
    tela.setTextSize(1);tela.setTextColor(ILI9341_WHITE);
    tela.setCursor(15,152);tela.printf("Dica: %s",q.dica);
  }
  tela.setTextSize(1);tela.setTextColor(ILI9341_YELLOW);
  tela.setCursor(15,165);tela.printf("Acertos: %d de %d",acertos,indice+1);
  Serial.printf("RESULTADO:%s\n",acertou?"ACERTO":"ERRO");
}
void finalizar(){
  // Validador do jogo: SO ele aciona o servo. A Guardia nao participa desta regra.
  const bool abre=coflume::desbloqueia(acertos);
  Serial.printf("GUARDIA,fim,sessao=%08lX,acertos=%d,desbloqueio=%s\n",(unsigned long)sessaoId,acertos,abre?"SIM":"NAO");
  if(abre){
    estado=VITORIA;abrir();cabecalho("COFRE ABERTO!");
    tela.setTextColor(ILI9341_GREEN);tela.setTextSize(2);
    tela.setCursor(20,110);tela.println("Parabens! Premio liberado.");
    Serial.println("WIN");
  } else {
    estado=DERROTA;fechar();digitalWrite(LED_VERMELHO,HIGH);
    cabecalho("NOVA CHANCE!");
    tela.setTextColor(ILI9341_YELLOW);tela.setTextSize(2);
    tela.setCursor(20,110);tela.println("Tente novamente para aprender!");
    Serial.println("LOSE");
  }
  tela.setTextSize(1);tela.setTextColor(ILI9341_WHITE);
  tela.setCursor(20,210);tela.println("Pressione # para voltar ao menu");
}
void proxima(){
  indice++;
  digitalWrite(LED_VERMELHO,LOW);
  if(indice>=coflume::N_PERGUNTAS) finalizar(); else prepararPergunta();
}
void tecla(char k){
  Serial.printf("KEY:%c\n",k);
  if(estado==MENU){
    if(k>='1'&&k<='3') iniciarPartida(k-'1');
    return;
  }
  if(estado==VITORIA||estado==DERROTA||estado==ERRO_BANCO){if(k=='#')menu();return;}
  if(estado!=PERGUNTA)return;
  // Prazo tem prioridade, inclusive ao apertar a tecla na virada do cronometro.
  if(millis()-inicioPergunta>=TEMPO_QUESTAO_MS){resultado(false,true);return;}
  if(k>='A'&&k<='D'){
    selecionada=k;
    tela.fillRect(12,176,300,19,ILI9341_BLACK);
    tela.setTextColor(ILI9341_CYAN);tela.setTextSize(2);
    tela.setCursor(12,176);tela.printf("Selecionada: %c",k);
  }else if(k=='*'){
    dicaExibida=true;
    tela.fillRect(12,212,300,20,ILI9341_BLACK);
    tela.setTextColor(ILI9341_YELLOW);tela.setTextSize(1);
    tela.setCursor(12,214);tela.printf("Dica: %s",questao().dica);
  }else if(k=='#'&&selecionada!='\0'){
    resultado(selecionada==questao().correta,false);
  }
}
void setup(){
  Serial.begin(115200);
  pinMode(LED_VERMELHO,OUTPUT);pinMode(LED_VERDE,OUTPUT);
  digitalWrite(LED_VERMELHO,LOW);digitalWrite(LED_VERDE,LOW);
  SPI.begin(TFT_SCK,-1,TFT_MOSI,TFT_CS);
  tela.begin();tela.setRotation(1);
  pwmOK=ledcAttach(SERVO_PIN,PWM_FREQ,PWM_BITS);
  Serial.printf("PWM:%s\n",pwmOK?"OK":"FALHA");
  Serial.printf("COFLUME,versao=%s,banco=%s,perguntas=%d\n",COFLUME_VERSAO,COFLUME_BANCO_VERSAO,COFLUME_BANCO_N);
  Serial.printf("GUARDIA,modelo,id=%s,hash=%s,contrato=%s,fonte=%s,uso=%s,parametros=%d\n",GUARDIA_MODELO_ID,
                GUARDIA_MODELO_HASH,GUARDIA_CONTRATO_VERSAO,GUARDIA_MODELO_FONTE,GUARDIA_MODELO_USO,
                GUARDIA_MODELO_N_PARAMETROS);
  coflume::rodizio_limpar(rodizio);
  bancoOK=coflume::banco_viavel();
  const bool autoteste=autotesteGuardia();
  guardiaAtiva=GUARDIA_ADAPTATIVA&&autoteste;
  Serial.printf("GUARDIA,estado,adaptativa_configurada=%d,banco_viavel=%d,guardia_ativa=%d\n",GUARDIA_ADAPTATIVA,
                bancoOK?1:0,guardiaAtiva?1:0);
  menu();
}
void loop(){
  char k=teclado.getKey();if(k!=NO_KEY)tecla(k);
  if(estado==PERGUNTA){
    uint32_t passado=millis()-inicioPergunta;
    if(passado>=TEMPO_QUESTAO_MS){resultado(false,true);return;}
    int faltam=(TEMPO_QUESTAO_MS-passado+999)/1000;
    if(faltam!=ultimoSegundo){
      ultimoSegundo=faltam;desenharTempo(faltam);
      digitalWrite(LED_VERMELHO,(faltam<=10&&faltam%2==0)?HIGH:LOW);
    }
  }else if(estado==RESULTADO && millis()-inicioResultado>=duracaoFeedback){
    proxima();
  }
}
