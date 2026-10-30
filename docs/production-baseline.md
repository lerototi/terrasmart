# Baseline de produção TerraSmart

## Objetivo

Este documento registra o comportamento atual antes de mudanças de identidade,
descoberta ESP-NOW ou integração multi-gateway. Ele serve como referência de
compatibilidade e roteiro de regressão para as PRs seguintes.

Este baseline não altera nem atualiza os firmwares instalados. Não registrar
senhas, chaves, URLs privadas de firmware nem outros segredos nos logs ou neste
documento.

## Escopo e topologia observada

Topologia de um par:

```text
node ESP8266 --ESP-NOW/canal 1--> gateway ESP-NOW ESP32
                                      |
                                UART 115200, 8N1
                                      |
                               gateway MQTT
                                      |
                              Wi-Fi 2.4 GHz
                                      |
                                  broker MQTT
```

Cada par tem seu próprio gateway MQTT conectado ao broker compartilhado. O
baseline de um par não valida por si só dois pares MQTT simultâneos; isso será
um teste de uma etapa posterior.

Ambientes definidos no `platformio.ini`:

| Papel | Ambiente | Placa | Fonte selecionada |
| --- | --- | --- | --- |
| Sensor ultrassônico | `d1_mini_node` | Wemos D1 mini / ESP8266 | `src/node_sensor.cpp` |
| Boias de nível | `d1_mini_float_switch_node` | Wemos D1 mini / ESP8266 | `src/float_switch_node.cpp` |
| Controle de relés | `d1_mini_relay_node` | Wemos D1 mini / ESP8266 | `src/relay_node.cpp` |
| Gateway de rádio | `esp32dev_gateway` | ESP32 DevKit | `src/gateway.cpp` |
| Gateway MQTT selecionado | `d1_mini_mqtt_gateway` | Wemos D1 mini Pro / ESP8266EX | `src/mqtt_gateway.cpp` |
| Alternativa MQTT | `esp32dev_mqtt_gateway` | ESP32 DevKit | `src/mqtt_gateway.cpp` |

Existe também `wemos_d1_mini32_node`, além de ambientes de diagnóstico ESP-NOW;
eles não substituem os ambientes da topologia de produção listados acima.

## Versões e identificação da execução

O `platformio.ini` fixa a biblioteca FDRS no commit
`33fca6e8c3bd8f8fe5c0dac4c04648c80c6a7723` e declara ArduinoJson `^7.0.0`.
As plataformas estão declaradas como `espressif32@^7.0.1` e
`espressif8266` (sem versão exata no arquivo). A versão local de PlatformIO Core
no momento da preparação deste baseline é `6.1.18`.

Antes de comparar resultados de bancada em outra máquina ou release, registrar:

- commit Git do firmware (`git rev-parse HEAD`);
- saída de `pio --version`;
- versões efetivamente instaladas de `espressif32`, `espressif8266`, Arduino
  cores e bibliotecas, obtidas da saída de build;
- nome do ambiente, placa usada, data, canal e configuração do par;
- se o teste foi somente compilação ou também executado em hardware.

As versões com `^` não garantem que uma instalação futura selecione exatamente
o mesmo pacote. Não atualizar dependências como parte de uma PR de baseline.

Build de referência executado durante esta PR:

- commit de firmware documentado: `7920145727506de3db5bfa774fb6b770f1572153`;
- PlatformIO Core `6.1.18`;
- plataforma `espressif32 7.0.1`, framework Arduino ESP32
  `3.20017.241212+sha.dcc1105b` (core 2.0.17);
- plataforma `espressif8266 4.2.1`, framework Arduino ESP8266
  `3.30102.0` (core 3.1.2);
- Farm-Data-Relay-System `2.0.1+sha.33fca6e`;
- ArduinoJson `7.4.3`;
- PubSubClient `2.8.0` nos gateways MQTT.

Essas são versões observadas no build local, não uma trava adicional de
dependências no projeto.

## Configuração de rádio e identidade atuais

- Canal ESP-NOW configurado: `1`.
- `UNIT_MAC` do gateway de rádio: `0x01`.
- `GTWY_MAC` nos firmwares de node atuais: `0x01`.
- O FDRS usa prefixo `AA:BB:CC:DD:EE` e substitui o endereço da interface de
  rádio ao inicializar ESP-NOW. Portanto, os endereços atuais são derivados do
  prefixo compilado e do último byte configurado, não da MAC de fábrica.
- IDs OTA atuais: gateway ESP-NOW `0x01`, node sensor `0x11`, boias `0x12`,
  relés `0x14` e gateway MQTT ESP32 `0x00` (atualização local do MQTT ESP32).
- IDs de leitura atuais: sensor `1`; boias `2` e `3`; relés usam `4` como
  `READING_ID`, com comandos de relé `101` e `102`.
- Os IDs OTA, endereços FDRS e IDs de leitura têm funções diferentes; não são
  identificadores globalmente únicos de hardware.

## Contratos de dados atuais

### ESP-NOW e FDRS

Os dados de leitura FDRS são estruturas binárias `DataReading` contendo:

- `d`: valor `float`;
- `id`: identificador de leitura `uint16_t`;
- `t`: tipo `uint8_t`.

Esse payload não carrega identidade global do node, gateway, sequência ou versão
de aplicação. Os nodes conhecem previamente o gateway por `GTWY_MAC`; o canal
também é configurado no firmware. O registro de peer FDRS não equivale a uma
descoberta automática de gateway.

### UART entre gateways

No ESP32, o gateway de rádio usa `Serial1` nos GPIO16 (RX) e GPIO17 (TX). O
gateway MQTT ESP32 usa os mesmos pinos para o link UART; a ligação deve cruzar
TX/RX e compartilhar GND. A velocidade é `115200`, formato `8N1`.

O ESP32 de rádio serializa leituras como JSON em uma linha terminada por newline.
Formato observado:

```json
[{"id":1,"type":1,"data":23.4}]
```

O gateway MQTT interpreta esse array e publica os mesmos campos como JSON no
MQTT. Mensagens de comando FDRS também atravessam a UART como array JSON, com
`cmd` e `param`. Linhas OTA com prefixo `!TSOTA:` coexistem no link UART quando
o firmware e o fluxo OTA correspondentes estão habilitados.

Na alternativa com gateway MQTT D1 mini, a UART principal também é usada pelo
USB/upload/monitor serial: desconectar TX/RX durante upload e o monitor de
debug UART não está disponível enquanto o link está conectado.

O ambiente `d1_mini_mqtt_gateway` usa o mesmo `src/mqtt_gateway.cpp`, identidade
MQTT por MAC, portal de configuração com persistência EEPROM, OTA pelo broker e
tópico individual de comando FDRS. Sua UART principal compartilha USB/debug e o
link com o gateway ESP32; desconectar TX/RX durante gravação serial.

### MQTT atual

O gateway publica dados em `terrasmart/data`. O payload é um array de leituras
com `id`, `type` e `data`, por exemplo:

```json
[{"id":1,"type":1,"data":23.4},{"id":1,"type":3,"data":65.2}]
```

O contrato legado registrado antes da PR 2 usava estado retido em
`terrasmart/status` e client ID fixo `FDRS_GATEWAY`. A PR
`feature/unique-mqtt-gateway-identity` muda somente a identificação de conexão e
status: client ID `ts-<MAC>` e tópico retido
`terrasmart/gateways/<MAC>/status`, com MAC em hexadecimal maiúsculo sem
separadores. O payload de status mantém eventos `online`, `heartbeat` e Last
Will `offline`, e agora inclui `gateway_id` (`gw-<MAC>`). O heartbeat é publicado
a cada 30 segundos. Na primeira conexão bem-sucedida, o firmware novo limpa o
estado retido legado em `terrasmart/status`. Dados seguem em `terrasmart/data`
sem alteração de payload.

Comandos FDRS são recebidos por `terrasmart/command` como JSON array contendo
`id`, `type` e `data`; o MQTT gateway encaminha as leituras pela UART para o
gateway ESP-NOW. O tópico de OTA é `terrasmart/ota`, com payload de comando
documentado em [ota.md](ota.md). Esses tópicos não incluem atualmente um
identificador de par/gateway no endereço do tópico. Assim, a identidade de
client/status não torna comandos e OTA direcionados por gateway; todos os
gateways conectados ainda compartilham essas assinaturas.

Segredos Wi-Fi/MQTT ficam nos headers locais ignorados pelo Git. Usar placeholders
ou remover credenciais ao guardar logs e exemplos.

## Procedimento de compilação

Executar na raiz do projeto e guardar a saída como evidência, registrando commit
e versão PlatformIO:

```bash
pio run -e esp32dev_gateway
pio run -e esp32dev_mqtt_gateway
pio run -e d1_mini_mqtt_gateway
pio run -e d1_mini_node
pio run -e d1_mini_float_switch_node
pio run -e d1_mini_relay_node
```

Compilar não comprova comunicação por rádio, UART, Wi-Fi ou MQTT. O resultado
deve dizer explicitamente quais ambientes passaram e quais testes físicos foram
executados.

### Resultado de build desta execução

Os seis ambientes acima compilaram com sucesso no ambiente de referência
registrado neste documento. O build emitiu avisos não fatais em dependências
FDRS/ArduinoJson e no core ESP8266; não foram alterados nesta PR documental.

O teste físico sensor → broker, comando e reinicialização **não foi executado**
nesta execução: `pio device list` não encontrou placas TerraSmart conectadas
(somente portas seriais virtuais). Portanto, a documentação e compilação estão
prontas, mas o critério de aceite de comunicação ponta a ponta ainda precisa
ser preenchido com o procedimento de bancada abaixo.

## Procedimento de bancada

### Preparação

1. Identificar e anotar as placas e seus papéis. Começar com um único par: um
   node, um ESP32 ESP-NOW e um MQTT gateway, mais um broker acessível.
2. Confirmar que os firmwares instalados correspondem aos ambientes registrados
   e que todos usam canal ESP-NOW `1`.
3. Conferir UART cruzada (TX→RX, RX←TX, GND comum) e níveis lógicos de 3,3 V.
   No par ESP32/ESP32, o link dedicado usa GPIO17 TX e GPIO16 RX em cada placa.
4. Confirmar que Wi-Fi/broker estão configurados localmente. Não copiar senha
   para o log de teste.
5. Iniciar monitores seriais a 115200 baud onde a ligação permite. Para D1 mini
   MQTT com UART conectada, registrar que o serial principal está ocupado pelo
   link.

### Caminho sensor → broker

1. Reiniciar o gateway ESP-NOW e registrar inicialização, endereço lógico e
   canal. Confirmar `ESP-NOW Initialized` e canal `1` no log.
2. Reiniciar o gateway MQTT e confirmar Wi-Fi conectado e MQTT conectado, pela
   página de status/serial e pelo broker.
3. Reiniciar o node. Confirmar o ping/registro ao gateway, quando exibido pelo
   firmware.
4. Produzir pelo menos uma leitura conhecida. Para o node ultrassônico, registrar
   uma leitura válida do A02YYUW; para as boias, acionar e liberar cada chave;
   para relés, registrar o estado reportado.
5. Observar o JSON de leitura saindo do gateway ESP-NOW pela UART (ou no log USB
   do firmware ESP32, que também imprime a linha) e confirmar sua publicação em
   `terrasmart/data`.
6. Comparar `id`, `type` e `data` entre origem, UART e broker. Registrar timestamp,
   resultado e qualquer transformação esperada.
7. Confirmar no consumidor atual/Home Assistant que a entidade ou automação
   continua recebendo o formato legado.

### Comandos e OTA

- Se o dispositivo for controlável, enviar um comando conhecido por
  `terrasmart/command`, confirmar ação física, ACK/retorno e leitura de estado.
- OTA não é requisito para aceitar o baseline de telemetria. Se testado, seguir
  [ota.md](ota.md), usar firmware de teste/alvo conhecido e registrar o target,
  sem publicar credenciais ou URL privada nos artefatos.

### Reinicialização e recuperação

Reiniciar cada placa separadamente: primeiro node, depois gateway ESP-NOW, por
fim MQTT gateway. Para cada caso, medir o tempo entre boot e primeira leitura
recebida no broker. Registrar se houve recuperação automática, ação manual ou
perda de dados esperada durante a interrupção.

## Folha de resultados

Copiar e preencher para cada execução; logs e payloads anexados devem ser
redigidos para retirar segredos.

```text
Commit:
Data/hora:
Operador:
PlatformIO Core:
Builds/versões efetivas:
Ambientes compilados:
Placas e papéis:
Gateway ESP-NOW UNIT_MAC/canal:
IDs de node/leituras/OTA relevantes:
Gateway MQTT (ESP32 ou D1 mini):
Broker (identificador não secreto):
UART/pinos/baud:
Leitura esperada:
Payload observado na UART:
Tópico/payload observado no broker:
Resultado no Home Assistant:
Comando testado/resultado (se aplicável):
OTA testado/resultado (se aplicável):
Tempo de recuperação após reboot:
Resultado: PASSOU / FALHOU / NÃO EXECUTADO
Logs/evidências redigidos:
Observações e desvios:
```

## Critério de aceite do baseline

- Todos os ambientes de produção listados no procedimento compilam, ou cada
  falha está reproduzida e documentada como bloqueio conhecido.
- Para aceitar o caminho ponta a ponta, uma leitura chega ao broker no tópico e
  formato atuais, com os campos preservados.
- Quando houver dispositivos de comando, um comando de referência produz a ação
  e retorno esperados; caso não tenha sido testado, registrar como não executado.
- Reinícios individuais e tempos de recuperação ficam registrados.
- Logs/exemplos não contêm credenciais.
- Nenhum protocolo, configuração, tópico, payload ou firmware de produção foi
  alterado como parte da documentação do baseline.
