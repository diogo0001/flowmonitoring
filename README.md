# Monitorament de fluxo de pessoas

Realiza a contagem da passagem de pessoas, incrementando ao entrar no ambiente ou decrementando ao sair, com objetivo do controle do fluxo, bem como a obtenção dos dados de sua densidade, armazenando data e horário de cada passagem.

São utilizados:
- 2 [Módulos ultrassônicos](https://images-na.ssl-images-amazon.com/images/I/61dBXHczMuL._SX425_.jpg): para detectar a passagem.
- 1 [Arduino Uno](https://s3-sa-east-1.amazonaws.com/multilogica-files/arduino_uno_r3_4_M.jpg): para o processamento
- 2 [Módulos bluetoth](https://http2.mlstatic.com/modulo-bluetooth-hc-06-rs232-arduino-hc-06-D_NQ_NP_696305-MLB25000728948_082016-F.jpg): para a comunicação entre o módulo remoto e o central
- 1 [Módulo SDcard](http://lghttp.57222.nexcesscdn.net/803B362/magento/media/catalog/product/cache/1/image/650x/040ec09b1e35df139433887a97daa66f/m/_/m_dulo_sd_card-5.jpg): para armazenar os dados
- 1 [Módulo relógio](https://uploads.filipeflop.com/2014/06/Real_Time_Clock_DS13075.jpg): para monitoramento do tempo

O príncipio é haver um módulo remoto monitorando cada entrada de um ambiente, e estes se comunicam com uma central, que coordena a contagem, armazenando a data e horário em que houve cada passagem, armazenando em um SD card e enviando para um computador local em tempo real.


Módulo remoto: remoteSensor.c

Módulo central: CFPbase.c



