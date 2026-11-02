# Coflume — O Conhecimento Abre Portas

Cofre educacional inteligente desenvolvido com ESP32-S3 para transformar desafios de aprendizagem em uma experiência física interativa.

## Situação do projeto

- **v0.7 — Simulação Wokwi:** versão funcional de referência, com três níveis educativos (Explorador, Desafiante e Mestre), três perguntas por partida, teclado 4×4, servo e LEDs.
- **Testes manuais:** seleção dos três níveis, respostas corretas, abertura do servo, funcionamento dos LEDs e reinício por tecla relatados como aprovados em 28/10/2026.
- **v0.8 — Simulação Wokwi EXPERIMENTAL:** a Guardiã IA (rede neural MLP treinada com dados **sintéticos**) roda offline no ESP32-S3 e escolhe a dificuldade das perguntas 2 e 3; detalhes em [`simulations/wokwi/v0.8/`](simulations/wokwi/v0.8/README.md). Avaliação final do modelo e placa física pendentes; não foi validada com crianças reais.
- **Guardiã IA na v0.7:** não integrada; o firmware v0.7 contém perguntas fixas e continua sendo a versão estável.
- **Hardware físico:** montagem e validação ainda pendentes.

## Arquivos

- [Código da simulação v0.7](simulations/wokwi/v0.7/sketch.ino)
- [Diagrama Wokwi v0.7](simulations/wokwi/v0.7/diagram.json)
- [Bibliotecas da simulação](simulations/wokwi/v0.7/libraries.txt)
- [Relatório de testes v0.7](docs/testes/coflume-v0.7-wokwi.md)
- [v0.8 experimental com a Guardiã IA](simulations/wokwi/v0.8/README.md)
- [Relatório de testes v0.8](docs/testes/coflume-v0.8-wokwi.md)

## Executar no Wokwi

Crie um projeto ESP32-S3 no [Wokwi](https://wokwi.com/), copie os arquivos da pasta `simulations/wokwi/v0.7/` para os arquivos correspondentes do projeto e execute a simulação.

**Atenção:** a v0.7 utiliza ILI9341 na simulação. A tela física de 1,8 polegada deve ter o controlador confirmado (por exemplo, ST7735) e exigirá ajustes próprios de firmware e conexões. A rede neural é desenvolvida no projeto separado `coflume-guardian-ai`; a integração experimental está na v0.8.

## Créditos

Projeto de Sara Thais e colaboradores humanos do Coflume, desenvolvido com apoio de ferramentas de IA. A autoria individual de futuras contribuições deve ser mantida conforme sua origem.
