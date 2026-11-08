# Coflume — O Conhecimento Abre Portas

Cofre educacional inteligente desenvolvido com ESP32-S3 para transformar desafios de aprendizagem em uma experiência física interativa.

## Situação do projeto

- **v0.7 — Simulação Wokwi:** versão funcional de referência, com três níveis educativos (Explorador, Desafiante e Mestre), três perguntas por partida, teclado 4×4, servo e LEDs.
- **Testes manuais:** seleção dos três níveis, respostas corretas, abertura do servo, funcionamento dos LEDs e reinício por tecla relatados como aprovados em 28/10/2026.
- **v0.8 — Simulação Wokwi EXPERIMENTAL:** a Guardiã IA (rede neural MLP treinada com dados **sintéticos**) roda offline no ESP32-S3 e escolhe a dificuldade das perguntas 2 e 3; detalhes em [`simulations/wokwi/v0.8/`](simulations/wokwi/v0.8/README.md). Avaliação final do modelo e placa física pendentes; não foi validada com crianças reais.
- **v0.9 — Hardware físico EXPERIMENTAL (ESP32-S3 + ST7735):** firmware para a placa, com o quiz da Guardiã IA, o **Agente Secreto** (senha no teclado, pistas e música no painel USB do notebook), o servo real e os LEDs simulados.
  - [`firmware/v0.9/`](firmware/v0.9/README.md) **(r2):** base no sketch em uso na placa.
  - [`firmware/v0.9-r3/`](firmware/v0.9-r3/README.md) **(r3):** acrescenta a política gradual opcional, desligada.
  - [`firmware/v0.9-r4/`](firmware/v0.9-r4/README.md) **(r4, mais recente):** banco de **36 perguntas** e painel com a visualização da Guardiã e o **monitor da IA** para um segundo monitor. **A r4 ainda não foi testada na placa física**; situação dos testes em [`docs/testes/coflume-v0.9-r4.md`](docs/testes/coflume-v0.9-r4.md).
- **Guardiã IA na v0.7:** não integrada; o firmware v0.7 contém perguntas fixas e continua sendo a versão estável na simulação.
- **Hardware físico:** pinagem e configuração da placa documentadas; a montagem física (alimentação, trava, caixa) ainda não está documentada ([pendências](docs/montagem_pendencias.md)).

## Arquivos

- [Código da simulação v0.7](simulations/wokwi/v0.7/sketch.ino)
- [Diagrama Wokwi v0.7](simulations/wokwi/v0.7/diagram.json)
- [Bibliotecas da simulação](simulations/wokwi/v0.7/libraries.txt)
- [Relatório de testes v0.7](docs/testes/coflume-v0.7-wokwi.md)
- [v0.8 experimental com a Guardiã IA](simulations/wokwi/v0.8/README.md)
- [Relatório de testes v0.8](docs/testes/coflume-v0.8-wokwi.md)
- [v0.9-r4 experimental (placa física)](firmware/v0.9-r4/README.md) e [relatório de testes da r4](docs/testes/coflume-v0.9-r4.md)

## Guias

- [Guia para iniciantes](docs/guia_iniciantes.md): componentes, alimentação, pinagem, montagem, instalação, bibliotecas, configuração da placa, gravação e testes.
- [Guia de uso](docs/guia_de_uso.md): quizzes, Agente Secreto, música, conexão USB, dois monitores e demonstração sem hardware.
- [Pendências da montagem](docs/montagem_pendencias.md): fotos e informações ainda não registradas.

## Executar no Wokwi

Crie um projeto ESP32-S3 no [Wokwi](https://wokwi.com/), copie os arquivos da pasta `simulations/wokwi/v0.7/` para os arquivos correspondentes do projeto e execute a simulação.

**Atenção:** a v0.7 utiliza ILI9341 na simulação. Na placa física, a v0.9 usa o driver ST7735 (ver o [guia para iniciantes](docs/guia_iniciantes.md)). A rede neural é desenvolvida no projeto separado `coflume-guardian`; a integração experimental começou na v0.8.

## Créditos

Projeto de Sara Thais e colaboradores humanos do Coflume, desenvolvido com apoio de ferramentas de IA. A autoria individual de futuras contribuições deve ser mantida conforme sua origem.
