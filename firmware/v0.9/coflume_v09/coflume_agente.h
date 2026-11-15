#pragma once
// Protocolo COFLUME_AGENT/1. O ESP32 e a autoridade da partida.
// Este modulo nao chama a Guardia nem altera seus pesos ou historico.
namespace agente {
enum Fase { ESPERA, PREPARANDO, JOGANDO, GANHOU, PERDEU };
Fase fase=ESPERA;
constexpr uint32_t LIMITE_MS=90000, CONEXAO_MS=5000;
uint32_t inicio=0, ultimoPing=0, ultimaPublicacao=0, preparou=0, id=0;
uint16_t tentativas=0;
char senha[5]="", anterior[5]="", digitada[5]="";
uint8_t n=0;
char linha[96]; size_t pos=0; bool descartando=false;
const char* motivoFim="";  // ultimo motivo de encerramento (so para o texto da tela)
const char* nome(){
  switch(fase){case PREPARANDO:return "PREP";case JOGANDO:return "PLAYING";case GANHOU:return "WIN";case PERDEU:return "LOSE";default:return "IDLE";}
}
uint32_t restante(){if(fase!=JOGANDO)return 0;const uint32_t p=millis()-inicio;return p>=LIMITE_MS?0:LIMITE_MS-p;}
void publicar(){
  Serial.printf("AG,STATE,%08lX,%s,%lu,%u\n",(unsigned long)id,nome(),(unsigned long)restante(),tentativas);
  ultimaPublicacao=millis();
}
void prepararPainel(){Serial.printf("AG,PREP,%08lX,%s\n",(unsigned long)id,senha);}
void tela(){
  limpar();
  texto(3,3,1,ST77XX_CYAN,"COFLUME / AGENTE SECRETO");
  if(fase==JOGANDO){
    char t[24];snprintf(t,sizeof(t),"Tempo: %lus",(unsigned long)((restante()+999)/1000));
    texto(3,25,2,ST77XX_YELLOW,t);
    char mascara[5]="____";for(uint8_t i=0;i<n;++i)mascara[i]='*';
    texto(44,58,3,ST77XX_WHITE,mascara);
    texto(3,100,1,ST77XX_WHITE,"* apaga  # confirma");
    texto(3,115,1,ST77XX_WHITE,"D cancela a missao");
  }else if(fase==GANHOU){
    texto(3,30,2,ST77XX_GREEN,"MISSAO");texto(3,49,2,ST77XX_GREEN,"CUMPRIDA!");
    texto(3,83,1,ST77XX_WHITE,"Cofre desbloqueado.");texto(3,113,1,ST77XX_WHITE,"# fecha e volta ao menu");
  }else if(fase==PERDEU&&strcmp(motivoFim,"PANEL_TIMEOUT")==0){
    // Aviso: o painel USB nao confirmou as pistas (nao conectado ou fechado).
    texto(3,30,2,ST77XX_RED,"PAINEL USB");texto(3,49,2,ST77XX_RED,"AUSENTE");
    texto(3,77,1,ST77XX_WHITE,"Conecte o painel no");texto(3,87,1,ST77XX_WHITE,"notebook e tente de novo.");
    texto(3,113,1,ST77XX_WHITE,"# volta ao menu");
  }else if(fase==PERDEU&&strcmp(motivoFim,"DISCONNECTED")==0){
    // Aviso: a comunicacao com o painel caiu durante a missao.
    texto(3,30,2,ST77XX_RED,"CONEXAO");texto(3,49,2,ST77XX_RED,"PERDIDA");
    texto(3,77,1,ST77XX_WHITE,"O painel USB parou de");texto(3,87,1,ST77XX_WHITE,"responder.");
    texto(3,113,1,ST77XX_WHITE,"# volta ao menu");
  }else if(fase==PERDEU){
    texto(3,30,2,ST77XX_YELLOW,"MISSAO");texto(3,49,2,ST77XX_YELLOW,"ENCERRADA");
    texto(3,83,1,ST77XX_WHITE,"Veja o painel do agente.");texto(3,113,1,ST77XX_WHITE,"# volta ao menu");
  }else{
    texto(3,34,2,ST77XX_WHITE,"AGUARDANDO");texto(3,55,2,ST77XX_WHITE,"COMPUTADOR");
    texto(3,89,1,ST77XX_YELLOW,"Inicie no painel USB.");texto(3,113,1,ST77XX_WHITE,"D volta ao menu");
  }
}
void terminar(bool venceu,const char* motivo){
  if(fase!=JOGANDO&&fase!=PREPARANDO)return;
  fase=venceu?GANHOU:PERDEU;motivoFim=motivo;
  if(venceu)abrir();else{fechar();ledVermelhoSet(true);}
  Serial.printf("AG,EVENT,%08lX,%s\n",(unsigned long)id,motivo);
  publicar();tela();
}
void entrar(){estado=AGENTE;fase=ESPERA;n=0;digitada[0]='\0';fechar();ledVermelhoSet(false);tela();publicar();}
void sair(){
  if(fase==JOGANDO||fase==PREPARANDO)terminar(false,"CANCELLED");
  fase=ESPERA;menu();publicar();
}
void iniciar(){
  if(estado!=MENU&&estado!=AGENTE){Serial.println("AG,ERROR,BUSY");return;}
  if(fase==JOGANDO||fase==PREPARANDO){if(fase==PREPARANDO)prepararPainel();publicar();return;}
  if(!servoPronto){Serial.println("AG,ERROR,SERVO");return;}
  estado=AGENTE;fechar();ledVermelhoSet(false);n=0;digitada[0]='\0';tentativas=0;
  do{for(int i=0;i<4;++i)senha[i]='0'+(esp_random()%10);senha[4]='\0';}while(strcmp(senha,anterior)==0);
  strcpy(anterior,senha);do{id=esp_random();}while(id==0);
  fase=PREPARANDO;preparou=ultimoPing=millis();tela();prepararPainel();publicar();
}
bool mesmaSessao(const char* s){
  if(!s||strlen(s)!=8)return false;
  for(int i=0;i<8;++i)if(!isxdigit((unsigned char)s[i]))return false;
  return strtoul(s,nullptr,16)==id;
}
void comando(char* s){
  char* contexto=nullptr;
  const char* prefixo=strtok_r(s,",",&contexto);if(!prefixo||strcmp(prefixo,"AG")!=0)return;
  const char* cmd=strtok_r(nullptr,",",&contexto);if(!cmd)return;
  if(strcmp(cmd,"HELLO")==0){
    Serial.printf("AG,HELLO,1,%d\n",servoPronto?1:0);
    if(estado==AGENTE&&(fase==PREPARANDO||fase==JOGANDO))prepararPainel();
    publicar();
    reenviarDiagBoot();  // linhas GUARDIA para a area de logs do painel (fora do protocolo AG)
    return;
  }
  if(strcmp(cmd,"START")==0){iniciar();return;}
  const char* sessao=strtok_r(nullptr,",",&contexto);
  if(!mesmaSessao(sessao))return;
  if(strcmp(cmd,"PING")==0){ultimoPing=millis();return;}
  if(strcmp(cmd,"READY")==0){
    if(estado==AGENTE&&fase==PREPARANDO){fase=JOGANDO;inicio=ultimoPing=millis();publicar();tela();}
    else publicar();
    return;
  }
  if(strcmp(cmd,"CANCEL")==0&&estado==AGENTE){
    if(fase==JOGANDO||fase==PREPARANDO)terminar(false,"CANCELLED");
    else sair();
  }
}
void lerSerial(){
  // Limite por loop: ruido serial nao pode monopolizar o cronometro/teclado.
  for(int i=0;i<192&&Serial.available();++i){
    const char c=(char)Serial.read();if(c=='\r')continue;
    if(c=='\n'){
      if(!descartando&&pos){linha[pos]='\0';comando(linha);}pos=0;descartando=false;
    }else if(!descartando){
      if(pos<sizeof(linha)-1)linha[pos++]=c;else{descartando=true;pos=0;}
    }
  }
}
void tecla(char k){
  if(k=='D'){sair();return;}
  if(fase==GANHOU||fase==PERDEU){if(k=='#')sair();return;}
  if(fase!=JOGANDO)return;
  if(!restante()){terminar(false,"TIMEOUT");return;}
  if(k>='0'&&k<='9'&&n<4){digitada[n++]=k;digitada[n]='\0';tela();}
  else if(k=='*'){if(n){digitada[--n]='\0';tela();}}
  else if(k=='#'){
    if(n!=4){Serial.printf("AG,EVENT,%08lX,INCOMPLETE\n",(unsigned long)id);return;}
    ++tentativas;
    if(strcmp(digitada,senha)==0)terminar(true,"CORRECT");
    else{
      n=0;digitada[0]='\0';
      Serial.printf("AG,EVENT,%08lX,WRONG\n",(unsigned long)id);publicar();tela();
    }
  }
}
void atualizar(){
  if(estado!=AGENTE)return;
  if(fase==PREPARANDO&&millis()-preparou>=CONEXAO_MS){terminar(false,"PANEL_TIMEOUT");return;}
  if(fase!=JOGANDO)return;
  if(!restante()){terminar(false,"TIMEOUT");return;}
  if(millis()-ultimoPing>=CONEXAO_MS){terminar(false,"DISCONNECTED");return;}
  if(millis()-ultimaPublicacao>=250){
    // Atualiza o relogio sem redesenhar toda a tela.
    tft.fillRect(0,25,tela::LARGURA,16,ST77XX_BLACK);
    char t[24];snprintf(t,sizeof(t),"Tempo: %lus",(unsigned long)((restante()+999)/1000));
    texto(3,25,2,restante()<=15000?ST77XX_RED:ST77XX_YELLOW,t);publicar();
  }
}
} // namespace agente
