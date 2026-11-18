# OTA TerraSmart

## OTA local do gateway MQTT piloto

O piloto `uart_mqtt_pilot.cpp` para ESP32 e ESP8266 reutiliza o download,
validação e gravação OTA local de produção, com tópico isolado
`terrasmart/pilot/ota`. O comando exige o `gateway_id` exato mostrado no portal
e `target: 0` para o MQTT ou `target: 1` para o gateway de rádio do par;
outros targets são rejeitados. O piloto não assina
`terrasmart/ota`, e os firmwares atuais de produção usam esse tópico de produção,
não o tópico piloto. Confirmar também as assinaturas de firmwares antigos antes
de operar ambos no mesmo broker. A identidade é da placa física e pode ser a
mesma ao trocar entre firmwares; o tópico separa os ambientes, não autentica o
dispositivo. Restringir publicação nos dois tópicos por ACL.

Use o mesmo JSON da seção abaixo, mas publique **sem retenção** em
`terrasmart/pilot/ota`. `scripts/ota_release.py` reconhece os ambientes MQTT
piloto e seleciona esse tópico independentemente do `mqtt_topic` configurado
para produção. Por exemplo:

```bash
python3 scripts/ota_release.py --env esp32dev_uart_mqtt_pilot_local \
  --gateway-id gw-383E51D3E374 --www-dir /caminho/montado/config/www \
  --base-url http://IP_DO_SERVIDOR:8123
```

O script compila, copia e apresenta o comando; `--publish` efetua publicação.
Somente publicar imagem compatível com a placa e o firmware piloto. A imagem
ESP32 não pode exceder o limite atual de 1.200.000 bytes; no ESP8266, o limite
do código é 700.000 bytes e a imagem também precisa caber no espaço OTA real.

Durante a tentativa de atualização local, a publicação é pausada. Leituras v1
válidas recebidas pela UART nesse intervalo são descartadas e contabilizadas em
`ota_discarded`; o node e o gateway de rádio continuam transmitindo. Leitura
parcial, overflow da UART e reinício podem causar perdas adicionais não contadas.
A fila anterior permanece apenas em RAM: após falha, a publicação retoma e essa
fila pode ser enviada; após atualização bem-sucedida e reboot, ela é perdida.
O contador também reinicia no reboot. Não há ACK UART nem garantia de entrega.

O ESP32 piloto usa a tabela padrão do ambiente, com dois slots OTA de 1.280 KiB.
Antes da primeira OTA, conferir a tabela gravada e a partição ativa; se forem
incompatíveis, preparar a migração por USB preservando NVS e dados existentes.
O ESP8266 usa o layout do ambiente existente e mantém a configuração piloto no
slot EEPROM documentado no README. Os ambientes piloto de descoberta
não ganham OTA com esta etapa. O OTA do gateway de rádio é
descrito na próxima seção.

**Verificação de código:** compilaram o piloto MQTT ESP32 e ESP8266, os gateways
de produção de ambas plataformas e os ambientes de node afetados; os testes
nativos de tópico/target/identidade e os testes Python de seleção de tópico
passaram.

**Bancada ESP32, 17/11/2026:** gateway `gw-383E51D3E374` em `/dev/ttyUSB0`,
node D1 `EC:FA:BC:D4:E7:33` e gateway de rádio `D4:E9:F4:02:E5:EC`. A tabela
existente foi lida antes da gravação: dois slots OTA de 1.280 KiB e NVS preservada;
o firmware e a tabela padrão foram gravados por USB com hashes verificados. O
gateway reconectou e assinou `terrasmart/pilot/ota`. Comandos no tópico piloto
com outro `gateway_id` e com `target: 1` foram rejeitados sem download HTTP.
Uma imagem piloto de 1.027.296 bytes servida por HTTP privado com CRC errado
falhou, retomou a publicação e contou quatro leituras descartadas durante a
pausa. Com o CRC correto, a mesma imagem foi atualizada com sucesso: o gateway
reiniciou, reconectou ao Wi-Fi/MQTT, reassinou o tópico e um assinante
independente voltou a receber leituras do D1. A leitura posterior de `otadata`
mostrou sequência 2 para o segundo slot. A abertura da serial e a leitura da
flash reiniciaram o gateway adicionalmente. O teste verificou o ESP32 com a
mesma imagem, não uma mudança de versão.

Ainda faltam bancada do piloto ESP8266 e prova física de isolamento com um
gateway de produção conectado; também não foi enviado comando ao tópico global
de produção nessa bancada. O OTA dos nodes permanece para outra etapa.

## OTA do gateway de rádio piloto

Os ambientes `esp32dev_data_uart_bench` e `esp32dev_data_uart_pilot` passam a
receber pela UART o protocolo OTA já usado em produção (`!TSOTA:`), em blocos
de até 180 bytes com ACK e até cinco tentativas. O front-end MQTT e o rádio
precisam estar atualizados com esta versão. O comando é publicado em
`terrasmart/pilot/ota`, com `gateway_id` do front-end e `target: 1`; targets
de nodes permanecem rejeitados. Exemplo para preparar o binário de bancada:

```bash
python3 scripts/ota_release.py --env esp32dev_data_uart_bench \
  --gateway-id gw-383E51D3E374 --www-dir /caminho/montado/config/www \
  --base-url http://IP_DO_SERVIDOR:8123
```

O script compila, copia e mostra o JSON; `--publish` efetua a publicação. O
ambiente de rádio incorpora a MAC de bancada; configurar a identidade correta
antes da compilação e assegurar que a imagem corresponde à placa de destino.
No primeiro uso gravar por USB o firmware de rádio com OTA e verificar que a
tabela existente possui slots OTA adequados. A tabela padrão dos ambientes
piloto tem dois slots de 1.280 KiB; manter o layout existente ou fazer migração
planejada com backup de dados. Não substituir automaticamente a tabela por
`partitions_ota.csv`, que altera offsets e tamanhos das partições de aplicação
e dados. Também é preciso instalar o novo firmware MQTT piloto.

A UART usa um único leitor em cada extremo, com linhas `!TS1:` de telemetria,
`!TSOTA:` de OTA e sondagem `!TSPROBE:<nonce>` / resposta
`!TSREADY:<nonce>:<MAC>`. Antes de enviar dados OTA, o MQTT exige resposta
recente com a MAC de rádio configurada. Essa verificação identifica o par, mas
não autentica a placa; MAC, nonce e CRC não são credenciais. A transferência
pausa a publicação, descarta novas leituras e usa fila RAM como no OTA local;
o rádio descarta leituras enquanto grava e aborta uma sessão inativa após 60 s.
O contador `ota_drops` indica leituras descartadas no rádio; outras perdas podem
ocorrer antes da sessão iniciar ou durante reboot. Se o ACK final se perder,
o resultado é incerto: a imagem pode ter sido instalada mesmo com falha no
remetente. Verificar o boot/telemetria antes de repetir. No ESP8266, a UART
SoftwareSerial e sua operação cooperativa ainda exigem validação física.

**Bancada OTA do rádio, 18/11/2026:** gateway MQTT ESP32 `gw-383E51D3E374`
em `/dev/ttyUSB0`, rádio `D4:E9:F4:02:E5:EC` em `/dev/ttyUSB1` e node D1
`EC:FA:BC:D4:E7:33` em `/dev/ttyUSB2`, com UART física cruzada. O MQTT recebeu
por OTA a versão que encaminha ao rádio. A tabela existente do rádio foi lida:
dois slots OTA de 1.280 KiB; o firmware inicial foi gravado por USB com hashes
verificados. Abrir as portas seriais reiniciou ambos os ESP32 antes do ensaio.

No primeiro teste, o servidor HTTP interrompeu o download após 540 bytes de
uma imagem de 770.448 bytes. O MQTT verificou a identidade do rádio, reportou
timeout e falha do OTA e retomou a publicação; um assinante independente voltou
a receber telemetria. Na segunda tentativa, a imagem completa foi transmitida
em aproximadamente 1.871 segundos (31 minutos) a 9600 baud; o rádio confirmou
conclusão, reiniciou e o MQTT reportou `OTA target 1: complete`. O assinante
recebeu leituras novamente após o reboot. O relatório do rádio antes do reboot
registrou 939 leituras descartadas durante OTA; o MQTT registrou cinco rejeições
de framing ao longo dos testes, sem atribuição de causa. A leitura posterior de
`otadata` mostrou sequência 2 no segundo slot e reiniciou o rádio novamente.
O portal confirmou MQTT e Wi-Fi conectados. O ensaio atualizou para a mesma
imagem; não demonstrou uma mudança de versão.

Os builds dos pilotos MQTT ESP32/ESP8266, do rádio ESP32 e dos gateways de
produção compilaram; testes nativos exercitam enquadramento e ressincronização.
Ainda faltam testes físicos de MAC incorreta, CRC errado no encaminhamento e
front-end ESP8266, além da investigação das rejeições de framing. Não interpretar
esse ensaio isolado como validação dos protocolos legados ou de OTA para nodes.

## Plano incremental: OTA confiável e disponibilidade

Premissa: priorizar integridade e entrega de dados. A meta preferencial é OTA
em menos de cinco minutos, mas uma transferência comprovadamente mais confiável
pode levar mais tempo. Medir separadamente a duração total da atualização e a
interrupção de telemetria; velocidade não demonstra disponibilidade.

1. **Baseline e diagnóstico:** medir sequências na origem, no rádio e em um
   assinante MQTT independente; investigar as cinco rejeições de framing do
   ensaio anterior. Registrar tentativas, erros, filas, latência e perdas.
   Aceitação: resultados reproduzíveis e rejeições explicadas ou isoladas.
2. **UART mais rápida no piloto ESP32:** testar 115200 baud mantendo o protocolo
   OTA existente, confirmações e retransmissões. Adotar somente após testar
   integridade, interrupção, reboot, perdas e estabilidade em bancada; prever
   migração coordenada de ambos os extremos e recuperação por USB. Os ambientes
   existentes continuam em 9600 até uma decisão fundamentada. ESP8266 com
   SoftwareSerial exige testes físicos próprios antes de qualquer migração.
3. **Telemetria durante OTA:** atender dados e comandos entre blocos OTA com
   um único leitor UART; medir efeito das escritas em flash, filas, latência,
   perda e concorrência sob carga e falha. Reboot deve ser medido separadamente.
   A política de recuperação garantida de leituras após falhas ainda requer
   definição; as filas atuais em RAM e QoS 0 não oferecem essa garantia.
4. **Failover:** integrar descoberta ao transporte de dados do piloto e validar
   troca de gateway durante reboot, incluindo perdas, duplicatas e estabilidade
   após retorno. A descoberta experimental atual é separada do transporte fixo.
5. **Otimização adicional:** avaliar framing binário versionado apenas se os
   resultados justificarem; testar corrupção, retransmissão, ressincronização e
   migração sem quebrar os protocolos anteriores.

Cada etapa exige builds ESP32/ESP8266 afetados e evidências físicas registradas
separadamente. Não promover a produção com base somente em compilação ou num
único ensaio; definir volume, duração, carga e limite de perda antes da aceitação.

**Primeiros ensaios, 18/11/2026:** a 9600 baud, durante 85 segundos e após
reinicialização dos monitores seriais, assinante independente recebeu 42
sequências consecutivas 2–43 (`boot_id=1788948853`); rádio e MQTT reportaram
zero rejeições e descartes no período. O primeiro envio do node falhou durante
inicialização simultânea. Os novos contadores `uart_bad_data`, `uart_unknown`
e `uart_overflow` distinguem categorias de erro sem substituir
`framing_rejected`. Em uma interrupção simulada após 540 bytes de download OTA,
o envio abortou, a telemetria retornou e esses contadores permaneceram zero.
Isto não reproduz nem explica as cinco rejeições da transferência completa
anterior; investigar também em um novo ensaio completo.

Foram criados os ambientes **experimentais** `esp32dev_data_uart_fast_bench`
e `esp32dev_uart_mqtt_fast_bench` a 115200 baud. Ambos compilaram, assim como o
piloto MQTT ESP8266 com a taxa padrão de 9600; testes nativos e Python passaram.
Duas tentativas de gravar o rádio por USB falharam na sincronização do
bootloader antes da escrita. O firmware anterior de 9600 continuou recebendo e
encaminhando leituras; o MQTT não foi trocado para 115200. Repetir com a UART
entre as placas temporariamente desconectada, depois conectar novamente e
gravar também o MQTT rápido antes do teste ponta a ponta. Os dois lados precisam
usar a mesma taxa. Para retorno, regravar ambos os ambientes de bancada originais
a 9600. A mudança de baud não muda configurações persistidas nem o protocolo,
mas versões em velocidades diferentes não se comunicam. Ainda não há resultado
de OTA a 115200 nem evidência para considerá-lo estável.

**Tentativa com RX/TX desconectados:** o usuário desconectou os fios entre os
gateways, mas o upload em `/dev/ttyUSB1` voltou a falhar na sincronização do
bootloader antes da escrita. `esptool read_mac` também reportou ausência de
resposta de sincronização. Após reabrir a serial, o firmware anterior de rádio
inicializou e recebeu leituras do D1, confirmando o funcionamento da aplicação;
isso não comprova o caminho USB de transmissão para o bootloader. O gateway MQTT
não foi regravado, e a UART física entre os dois permanece desconectada para o
próximo ensaio de gravação. Testar entrada manual em modo bootloader e verificar
cabo/porta USB se necessário.

**Nova tentativa manual:** a gravação de `esp32dev_data_uart_fast_bench` em
`/dev/ttyUSB1` voltou a falhar na sincronização, antes da escrita. A saída
reportou modo de download detectado, mas sem resposta de sincronização; não foi
possível confirmar se BOOT estava pressionado durante essa tentativa. O gateway
MQTT não foi regravado e o teste de 115200 baud continua pendente.

**Primeira bancada a 115200 baud:** o usuário gravou os dois ambientes rápidos
e configurou a rede no portal. Os gateways voltaram a publicar leituras do D1;
relatórios observaram zero rejeições UART antes do ensaio. Uma interrupção
proposital após 540 bytes de download abortou e retomou a telemetria, com zero
rejeições registradas nos novos contadores. A primeira transferência completa
falhou com timeout HTTP após 256.976 de 770.528 bytes; a telemetria retomou.
Uma segunda tentativa completa não terminou após 600 segundos e foi interrompida
quando o servidor de teste encerrou; outra observação durou 230 segundos sem
conclusão e também foi interrompida. Não houve sucesso nessas tentativas
iniciais; a causa foi isolada depois em ensaio instrumentado (abaixo). A abertura
das seriais reiniciou as placas e a telemetria voltou. O teste rápido permanece
restrito à bancada até a decisão de promoção.

**Diagnóstico instrumentado e correções:** o ensaio com log de todas as linhas e
progresso HTTP mostrou que a transferência completa concluía em ~626 s, mas o
gateway não registrava a conclusão: o eco da própria linha OTA (387 bytes)
estourava o buffer RX UART padrão de 256 bytes enquanto `flush()` segurava o
mutex, descartando ACKs e gerando retries de 5 s. A correção
`pilotUart.setRxBufferSize(1024)` no gateway MQTT reduziu a transferência para
~223 s (3,7 min) no ensaio seguinte. Para absorver stalls transitórios de
TCP/Wi-Fi observados na bancada (a janela de recepção fecha enquanto o gateway
aguarda ACK e uma atualização de janela perdida pode atrasar o servidor por mais
de 10 s), a janela de leitura HTTP do OTA foi elevada de 10 s para 30 s e o
Wi-Fi do gateway piloto passou a operar sem modem sleep
(`WiFi.setSleep(false)`).

**Resultado final a 115200:** com o firmware corrigido, a interrupção proposital
após 540 bytes abortou em 30,4 s e retomou a telemetria (seq 9888), com zero
rejeições. Na sequência, a transferência completa de 770.528 bytes terminou em
242,4 s (~4 min), com `OTA target 1: complete`, telemetria retomada
(seq 10011), `uart_unknown=0`, `framing_rejected=0` e `uart_overflow=0`. Um
ensaio completo anterior nas mesmas condições levou 273,4 s e também retomou a
telemetria. As duas execuções ficaram abaixo da meta de 5 min e o par ESP32 a
115200 foi promovido à configuração validada do piloto em bancada; o padrão 9600
permanece para o ESP8266 e para o par de rollback, e a validação de campo ainda é
necessária antes de qualquer promoção em produção. O console USB do gateway
ocasionalmente corrompe o prefixo de uma linha de status (ex.:
`�A target 1: complete`), sem afetar a transferência nem a telemetria.

## Topologia

```text
servidor HTTPS -> gateway MQTT ESP32 -> UART -> gateway ESP-NOW ESP32
                                      -> broadcast ESP-NOW -> nó selecionado
```

Os gateways MQTT ESP32 e ESP8266 podem atualizar a si próprios por comando MQTT.
O Wemos ESP8266 também mantém ArduinoOTA local. Ele não possui rádio ESP-NOW;
targets do gateway ESP-NOW e nodes são encaminhados via UART ao ESP32 do par.

O gateway MQTT ESP32 baixa a imagem e a envia ao gateway ESP-NOW pela UART. Para
targets de nós, o gateway ESP-NOW transmite o início por broadcast para descobrir
o MAC do nó cujo ID corresponde ao target; os blocos seguintes seguem por unicast
até esse nó, que devolve os ACKs. A mesma sessão OTA
mantém o leitor serial genérico do FDRS pausado até o ACK do `End` ou `Abort`,
enquanto os serviços UART OTA e ESP-NOW continuam ativos.

O fluxo está integrado e compila para os gateways e nós configurados; a
transferência completa por rádio foi confirmada na bancada a 9600 baud e a
115200 baud com as correções descritas acima.

## Preparação

1. Grave por USB um firmware inicial de cada alvo OTA, incluindo o novo gateway
   MQTT ESP32 com a tabela `partitions_ota.csv`. Esse passo é obrigatório para
   criar as partições OTA; imagens gravadas com a tabela antiga não ganham slots
   OTA automaticamente.
2. Copie `include/terrasmart_secrets.h.example` para
   `include/terrasmart_secrets.h`. Mantenha nesse arquivo as credenciais Wi-Fi,
   MQTT e AP já usadas pelo gateway, configure a senha do ArduinoOTA e, para
   HTTPS, substitua o certificado raiz pelo CA do servidor. Esse arquivo é
   ignorado pelo Git. Durante a migração, os antigos headers específicos de
   MQTT e OTA continuam aceitos caso o header unificado ainda não exista.
3. O servidor HTTPS deve disponibilizar o `.bin` cru, com `Content-Length`
   conhecido. Calcule o tamanho e o CRC-32 IEEE do mesmo arquivo.
4. Proteja publicação no tópico `terrasmart/ota` por ACL no broker.
5. A senha OTA compartilhada é usada também pelo MQTT ESP8266; não é necessário
   definir uma senha separada em `platformio.ini`.

## IDs OTA

O `target` é um identificador OTA de 8 bits, separado de `READING_ID`/`UNIT_MAC`.
Os valores inicializados são gateway MQTT `0x00`, gateway ESP-NOW `0x01`, nó
sensor `0x11`, nó de boias `0x12` e nó de relés `0x14`. Se dois dispositivos
iguais forem implantados no mesmo par, atribua IDs exclusivos em suas
configurações de build/firmware.

O ID OTA é lógico e não está associado previamente a um MAC. Por isso, `Begin` é
enviado por broadcast; somente o nó correspondente ao target responde. O gateway
aprende o MAC pelo ACK e transmite os próximos blocos em unicast. Os ACKs retornam
pela UART ao gateway MQTT. Nós com outro `TERRASMART_OTA_DEVICE_ID` ignoram `Begin`.

O firmware do gateway ESP-NOW (ID `1`) pode ter até `0x1E0000` bytes, o tamanho
do slot OTA definido em `partitions_ota.csv`. Os demais alvos encaminhados
continuam limitados a 700.000 bytes. O tamanho real do firmware também precisa
ser menor ou igual ao slot OTA; grave inicialmente essa tabela de partições por
USB antes de usar OTA.

## Comando MQTT

Publique um JSON não retido no tópico OTA global `terrasmart/ota` ou envie um
comando FDRS de controle no tópico privado daquele gateway mostrado no portal.
Exemplo de atualização OTA:

```json
{
  "gateway_id": "gw-246F2801ABCD",
  "target": 17,
  "url": "https://firmware.exemplo.net/terrasmart/d1_mini_node.bin",
  "size": 304651,
  "crc32": 1234567890
}
```

`gateway_id` escolhe qual par MQTT executará o comando. Use o ID exibido no
portal/status, por exemplo `gw-246F2801ABCD`. Todos os gateways do mesmo broker
recebem o tópico global, mas ignoram atualizações destinadas a outro gateway.
Configure `ota_gateway_id` em `ota_config.json` ou informe `--gateway-id gw-...`
ao executar o script de release.

Para comandos de controle FDRS, cada portal também informa o tópico privado do
par, no formato `terrasmart/gateways/<MAC>/command`. Publique nesse tópico para
atingir somente os nodes conectados a esse gateway. O firmware runtime também
aceita temporariamente `terrasmart/command` para compatibilidade com instalações
existentes; esse tópico compartilhado não identifica o par e pode alcançar todos
os gateways que ainda o assinam.

Para atualizar o próprio gateway MQTT use `target: 0`. Para atualizar o gateway
ESP-NOW use `target: 1`; para nodes, use os targets documentados acima. No par
com Wemos, alvos 1/17/18/20 são encaminhados via UART ao ESP32 ESP-NOW, enquanto
target 0 atualiza o Wemos localmente. As tarefas e `scripts/ota_release.py`
sempre exigem o `gateway_id` do par para montar o comando.

No ESP8266 MQTT gateway, o OTA do próprio Wemos usa `Update` na partição OTA do
ESP8266; o OTA encaminhado para ESP-NOW continua passando pela UART para o ESP32.
O firmware Wemos não contém rádio ESP-NOW, portanto não processa `target: 1`
como atualização local. O layout OTA do D1 mini Pro oferece slots menores que a
partição ESP32; respeite o limite de imagem definido pelo firmware e a saída do
script antes de publicar.

`crc32` é o CRC-32 IEEE do arquivo binário, reportado como inteiro decimal. O
gateway baixa o firmware, valida tamanho e CRC, e então atualiza a si próprio ou
envia Begin/Data/End ao gateway ESP-NOW. Para OTA encaminhada, cada bloco de até
180 bytes deve ser confirmado antes do próximo; há até cinco tentativas. O alvo
reinicia depois da confirmação final.

### Preparar e publicar pelo VS Code

O repositório inclui tarefas para compilar o ambiente, copiar o binário para o
Home Assistant, calcular tamanho/CRC-32 e montar o JSON. Para habilitar a cópia
automática, o diretório `www` do Home Assistant precisa estar montado/acessível
como uma pasta local no computador onde o VS Code está executando. Por exemplo,
monte o compartilhamento Samba `config` e use o caminho local até `www`.

1. Copie `ota_config.example.json` para `ota_config.json` na raiz do projeto.
   Esse arquivo local é ignorado pelo Git.
2. Configure `home_assistant_www_dir` com o caminho local da pasta `www` montada
   e `home_assistant_base_url` com o endereço acessível pelo gateway, por exemplo
   a URL que abre o arquivo no navegador a partir da mesma rede; neste ambiente,
   `http://10.0.0.216` (sem porta). Configure também
   broker, porta e credenciais MQTT se quiser publicar pelo script.
3. No VS Code, abra **Terminal → Run Task** e selecione
   **TerraSmart OTA: build, stage and prepare payload**. Escolha o ambiente do
   dispositivo. A tarefa compila, copia o `.bin` com um nome conhecido e mostra
   o JSON com tamanho e CRC-32; ela não publica.
4. Depois de conferir dispositivo, URL e payload, selecione
   **TerraSmart OTA: build, stage and publish MQTT command** para compilar/copiar
   novamente e publicar no tópico configurado. A publicação é QoS 1 e não retida.

Para o gateway ESP-NOW há também a tarefa **TerraSmart OTA: gateway build, copy
and prepare payload**. Ela executa as três etapas em sequência e salva o JSON em
`.pio/ota/esp32dev_gateway.json`, sem publicar no MQTT. A tarefa solicita o
caminho local montado para `www` e a URL-base do Home Assistant acessível pelo
ESP32.

Também é possível executar pelo terminal:

```bash
python3 scripts/ota_release.py --env d1_mini_node
python3 scripts/ota_release.py --env d1_mini_node --publish
python3 scripts/ota_release.py --env esp32dev_gateway \
  --gateway-id gw-246F2801ABCD \
  --www-dir /caminho/montado/config/www \
  --base-url http://10.0.0.216 \
  --payload-file .pio/ota/esp32dev_gateway.json
```

`--www-dir` é a pasta local montada que corresponde a `/config/www` no Home
Assistant; `--base-url` é o endereço HTTP acessível pelo ESP32. Esses argumentos
substituem os valores equivalentes de `ota_config.json`, permitindo executar o
script sem criar esse arquivo local.

Ambientes aceitos: `d1_mini_node`, `d1_mini_float_switch_node`,
`d1_mini_relay_node`, `esp32dev_gateway`, `d1_mini_mqtt_gateway` e
`esp32dev_mqtt_gateway`. Para preparar OTA de um gateway MQTT, configure
`ota_gateway_id` em `ota_config.json` com o ID apresentado pelo gateway. Para publicar,
instale `mosquitto_pub` e configure os dados do broker em `ota_config.json`.
O script avisa e bloqueia a publicação do gateway ESP-NOW se o binário exceder
`0x1E0000` bytes; para os demais alvos encaminhados, o limite é 700.000 bytes.
Depois de qualquer alteração ou recompilação, use o payload recém-gerado, nunca
os valores de uma build anterior.

### Servidor local Home Assistant

Para uma prova dentro da LAN, copie o `.bin` para `/config/www` no Home Assistant.
Com Home Assistant OS, isso pode ser feito pelo add-on File editor (ou pelo
compartilhamento de arquivos `/config`). Um arquivo
`/config/www/d1_mini_node.bin` costuma ser servido como:

```text
http://IP_DO_HOME_ASSISTANT:8123/local/d1_mini_node.bin
```

Use o IP local do Home Assistant, por exemplo `10.0.0.216`, e confirme a URL de
outro dispositivo conectado à mesma rede antes de iniciar a OTA. A implementação
aceita HTTP somente para IPv4 privado RFC1918 (`10.x.x.x`, `172.16.x.x` a
`172.31.x.x`, `192.168.x.x`) ou hostname terminado em `.local`. Outros hosts
HTTP são rejeitados. HTTPS continua exigindo o CA confiável configurado no
arquivo de secrets. Para o primeiro teste, prefira o IP do Raspberry; resolução
de `.local` depende de suporte mDNS no ambiente.

O ESP8266 não tem partições A/B equivalentes às do ESP32. O core Arduino grava a
imagem OTA e seleciona o reboot loader; um corte de energia durante a escrita
pode exigir recuperação por USB. O binário para OTA precisa caber no espaço
reservado pelo layout de flash do board.

## Limitações / validação de bancada

- **UART exclusiva durante OTA:** no ESP32 ESP-NOW, `loopFDRS()` também lê a
  `Serial1` compartilhada com o gateway MQTT e tenta interpretar as linhas como
  JSON. `src/gateway.cpp` suspende `loopFDRS()` tanto para gravação local quanto
  durante uma sessão encaminhada a um nó, até o ACK terminal chegar pela UART.
  O serviço OTA UART e o serviço ESP-NOW permanecem ativos para encaminhar blocos
  e ACKs; somente o leitor genérico FDRS da UART é pausado.
- Se o nó deixar de responder, o gateway ESP-NOW encerra a sessão encaminhada
  após 60 segundos sem atividade e volta a processar o tráfego serial FDRS.
- O LED onboard do gateway ESP-NOW nesta placa é ativo em HIGH: `LOW` mantém o
  LED apagado e `HIGH` o acende. O firmware o mantém apagado fora de uma sessão
  OTA e pisca rapidamente durante a gravação.
- Fazer a primeira prova com gateway/nó ligados por USB e testar antes da
  implantação; o firmware e a compatibilidade do protocolo devem ser alinhados
  antes de atualizar o gateway ESP-NOW para que ele entenda os quadros UART.
- A OTA ESP-NOW é para um nó por vez, diretamente no alcance do gateway, acordado
  durante toda a transferência. Deep sleep, roteadores e mesh não participam.
- O CRC detecta corrupção de transporte, não autentica o binário. HTTPS valida a
  identidade do servidor pela CA configurada. HTTP local é uma exceção para
  bancada e não autentica nem cifra o download; use somente em uma LAN confiável
  e mantenha ACL MQTT. Assinatura criptográfica dos firmwares e rollback
  automático após health-check serão etapas futuras.
- Confirme que a imagem compilada é compatível com o modelo de placa de destino;
  o comando não faz detecção de modelo nesta versão.
- O gateway MQTT ESP8266 não faz download/encaminhamento HTTPS remoto nesta
  implementação; para atualização remota indireta use o gateway MQTT ESP32.
