# OTA TerraSmart

## Topologia

```text
servidor HTTPS -> gateway MQTT ESP32 -> UART -> gateway ESP-NOW ESP32
                                      -> broadcast ESP-NOW -> nó selecionado
```

O gateway MQTT ESP32 também pode atualizar a si próprio. A implantação no
gateway MQTT ESP8266 continua usando ArduinoOTA na rede local; encaminhamento de
imagens dele para a UART ainda não é suportado, pois nessa placa a UART ligada
ao gateway ESP-NOW é a mesma interface usada para upload USB.

O gateway MQTT ESP32 baixa a imagem e a envia ao gateway ESP-NOW pela UART. Para
targets de nós, o gateway ESP-NOW transmite o início por broadcast para descobrir
o MAC do nó cujo ID corresponde ao target; os blocos seguintes seguem por unicast
até esse nó, que devolve os ACKs. A mesma sessão OTA
mantém o leitor serial genérico do FDRS pausado até o ACK do `End` ou `Abort`,
enquanto os serviços UART OTA e ESP-NOW continuam ativos.

O fluxo está integrado e compila para os gateways e nós configurados; a
transferência completa por rádio ainda precisa ser confirmada em bancada.

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
Os valores inicializados são gateway ESP-NOW `0x01`, nó sensor `0x11`, nó de
boias `0x12` e nó de relés `0x14`. Se dois dispositivos iguais forem
implantados, atribua IDs exclusivos em suas configurações de build/firmware.

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

Publique um JSON não retido em `terrasmart/ota`:

```json
{
  "target": 17,
  "url": "https://firmware.exemplo.net/terrasmart/d1_mini_node.bin",
  "size": 304651,
  "crc32": 1234567890
}
```

`crc32` é o CRC-32 IEEE do arquivo binário, reportado como inteiro decimal. O
gateway baixa o firmware, valida o tamanho e CRC, e só então envia Begin/Data/End.
Cada bloco de até 180 bytes deve ser confirmado antes do próximo; há até cinco
tentativas. O alvo reinicia depois da confirmação final. Para atualizar o
gateway MQTT ESP32, use `target: 0`.

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
  --www-dir /caminho/montado/config/www \
  --base-url http://10.0.0.216 \
  --payload-file .pio/ota/esp32dev_gateway.json
```

`--www-dir` é a pasta local montada que corresponde a `/config/www` no Home
Assistant; `--base-url` é o endereço HTTP acessível pelo ESP32. Esses argumentos
substituem os valores equivalentes de `ota_config.json`, permitindo executar o
script sem criar esse arquivo local.

Ambientes aceitos: `d1_mini_node`, `d1_mini_float_switch_node`,
`d1_mini_relay_node`, `esp32dev_gateway` e `esp32dev_mqtt_gateway`. Para publicar,
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
