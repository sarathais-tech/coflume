// COFLUME v0.7 - Simulacao Wokwi (ESP32-S3 + ILI9341)
// A tela REAL de 1,8 polegada usa ST7735 (ou compativel): firmware diferente.
// Versao educativa com perguntas FIXAS. Guardiã IA ainda nao integrada.
#include <Arduino.h>
#include <SPI.h>
#include <Keypad.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

constexpr uint8_t TFT_CS=16, TFT_DC=15, TFT_MOSI=17, TFT_SCK=18;
constexpr uint8_t SERVO_PIN=12, LED_VERMELHO=13, LED_VERDE=14;
constexpr uint32_t TEMPO_QUESTAO_MS=30000UL;
constexpr uint32_t FEEDBACK_MS=1700UL;
constexpr int ANGULO_FECHADO=90, ANGULO_ABERTO=0;
constexpr uint32_t PWM_FREQ=50;
constexpr uint8_t PWM_BITS=14;

char mapa[4][4]={{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
byte linhas[4]={4,5,6,7};
byte colunas[4]={8,9,10,11};
Keypad teclado(makeKeymap(mapa),linhas,colunas,4,4);
Adafruit_ILI9341 tela(TFT_CS,TFT_DC);

struct Questao {
  const char* texto;
  const char* a;
  const char* b;
  const char* c;
  const char* d;
  char correta;
  const char* dica;
};

const Questao banco[3][3]={
  { // Explorador
    {"Quanto e 6 x 7?", "36", "42", "48", "40", 'B', "6 grupos de 7"},
    {"Qual e um planeta?", "Lua", "Sol", "Marte", "Nuvem", 'C', "Orbita o Sol"},
    {"Qual palavra esta correta?", "Caza", "Casa", "Kasa", "Cassa", 'B', "Tem a letra S"}
  },
  { // Desafiante
    {"Quanto e 12 x 8?", "86", "94", "96", "108", 'C', "10x8 + 2x8"},
    {"Qual e um mamifero?", "Tubarao", "Golfinho", "Polvo", "Sardinha", 'B', "Respira ar"},
    {"Proximo: 2,4,8,16...", "18", "24", "30", "32", 'D', "Dobra a cada vez"}
  },
  { // Mestre
    {"Binario 1010 = decimal?", "8", "9", "10", "12", 'C', "8 + 2"},
    {"O que e phishing?", "Backup", "Golpe digital", "Antivirus", "Hardware", 'B', "Engana o usuario"},
    {"Qual senha e melhor?", "123456", "senha123", "nome2026", "G7!mP2#x", 'D', "Unica e aleatoria"}
  }
};

enum Estado {MENU, PERGUNTA, RESULTADO, VITORIA, DERROTA};
Estado estado=MENU;
int nivel=0, indice=0, acertos=0, ultimoSegundo=-1;
char selecionada='\0';
bool dicaExibida=false, respostaCerta=false;
uint32_t inicioPergunta=0, inicioResultado=0;
bool pwmOK=false;

void servoAngulo(int graus){
  if(!pwmOK) return;
  const uint32_t pulso=map(constrain(graus,0,180),0,180,500,2400);
  const uint32_t duty=(pulso*((1UL<<PWM_BITS)-1))/20000UL;
  ledcWrite(SERVO_PIN,duty);
}
void fechar(){servoAngulo(ANGULO_FECHADO);digitalWrite(LED_VERDE,LOW);}
void abrir(){servoAngulo(ANGULO_ABERTO);digitalWrite(LED_VERDE,HIGH);digitalWrite(LED_VERMELHO,LOW);}

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
  Serial.println("MENU");
}
void desenharTempo(int s){
  tela.fillRect(238,44,76,22,ILI9341_BLACK);
  tela.setTextColor(s<=10?ILI9341_RED:ILI9341_YELLOW);
  tela.setTextSize(2);tela.setCursor(244,47);tela.printf("%02ds",s);
}
void mostrarQuestao(){
  estado=PERGUNTA;selecionada='\0';dicaExibida=false;ultimoSegundo=-1;
  inicioPergunta=millis();
  char titulo[40];snprintf(titulo,sizeof(titulo),"QUESTAO %d/3",indice+1);
  cabecalho(titulo);
  const Questao &q=banco[nivel][indice];
  tela.setTextSize(2);tela.setTextColor(ILI9341_WHITE);
  tela.setCursor(12,78);tela.println(q.texto);
  tela.setTextSize(2);tela.setTextColor(ILI9341_GREEN);
  tela.setCursor(12,115);tela.printf("A) %s",q.a);
  tela.setCursor(165,115);tela.printf("B) %s",q.b);
  tela.setCursor(12,145);tela.printf("C) %s",q.c);
  tela.setCursor(165,145);tela.printf("D) %s",q.d);
  tela.setTextSize(1);tela.setTextColor(ILI9341_WHITE);
  tela.setCursor(12,198);tela.println("A-D: marca | #: confirma | *: dica");
  desenharTempo(30);
  Serial.printf("QUESTAO:%d NIVEL:%d\n",indice+1,nivel+1);
}
void resultado(bool acertou){
  estado=RESULTADO;inicioResultado=millis();respostaCerta=acertou;
  if(acertou) acertos++;
  digitalWrite(LED_VERMELHO,acertou?LOW:HIGH);
  cabecalho(acertou?"RESPOSTA CORRETA!":"VAMOS APRENDER!");
  tela.setTextColor(ILI9341_YELLOW);tela.setTextSize(2);
  tela.setCursor(15,108);tela.printf("Resposta: %c",banco[nivel][indice].correta);
  tela.setTextSize(1);tela.setCursor(15,165);tela.printf("Acertos: %d de %d",acertos,indice+1);
  Serial.printf("RESULTADO:%s\n",acertou?"ACERTO":"ERRO");
}
void finalizar(){
  if(acertos>=2){
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
  if(indice>=3) finalizar(); else mostrarQuestao();
}
void tecla(char k){
  Serial.printf("KEY:%c\n",k);
  if(estado==MENU){
    if(k>='1'&&k<='3'){
      nivel=k-'1';indice=0;acertos=0;mostrarQuestao();
    }
    return;
  }
  if(estado==VITORIA||estado==DERROTA){if(k=='#')menu();return;}
  if(estado!=PERGUNTA)return;
  // Prazo tem prioridade, inclusive ao apertar a tecla na virada do cronometro.
  if(millis()-inicioPergunta>=TEMPO_QUESTAO_MS){resultado(false);return;}
  if(k>='A'&&k<='D'){
    selecionada=k;
    tela.fillRect(12,176,300,19,ILI9341_BLACK);
    tela.setTextColor(ILI9341_CYAN);tela.setTextSize(2);
    tela.setCursor(12,176);tela.printf("Selecionada: %c",k);
  }else if(k=='*'){
    dicaExibida=true;
    tela.fillRect(12,212,300,20,ILI9341_BLACK);
    tela.setTextColor(ILI9341_YELLOW);tela.setTextSize(1);
    tela.setCursor(12,214);tela.printf("Dica: %s",banco[nivel][indice].dica);
  }else if(k=='#'&&selecionada!='\0'){
    resultado(selecionada==banco[nivel][indice].correta);
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
  menu();
}
void loop(){
  char k=teclado.getKey();if(k!=NO_KEY)tecla(k);
  if(estado==PERGUNTA){
    uint32_t passado=millis()-inicioPergunta;
    if(passado>=TEMPO_QUESTAO_MS){resultado(false);return;}
    int faltam=(TEMPO_QUESTAO_MS-passado+999)/1000;
    if(faltam!=ultimoSegundo){
      ultimoSegundo=faltam;desenharTempo(faltam);
      digitalWrite(LED_VERMELHO,(faltam<=10&&faltam%2==0)?HIGH:LOW);
    }
  }else if(estado==RESULTADO && millis()-inicioResultado>=FEEDBACK_MS){
    proxima();
  }
}