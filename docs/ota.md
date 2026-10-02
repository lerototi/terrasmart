# OTA TerraSmart

## Topologia suportada nesta primeira versão

```text
servidor HTTPS -> gateway MQTT ESP32 -> UART -> gateway ESP-NOW ESP32
                                      -> broadcast ESP-NOW -> nó selecionado
```

O gateway MQTT ESP32 também pode atualizar a si próprio. A implantação no
gateway MQTT ESP8266 continua usando ArduinoOTA na rede local; encaminhamento de
imagens dele para a UART ainda não é suportado, pois nessa placa a UART ligada
ao gateway ESP-NOW é a mesma interface usada para upload USB.

## Preparação

1. Grave por USB um firmware inicial de cada alvo OTA, incluindo o novo gateway
   MQTT ESP32 com a tabela `partitions_ota.csv`. Esse passo é obrigatório para
   criar as partições OTA; imagens gravadas com a tabela antiga não ganham slots
   OTA automaticamente.
2. Copie `include/terrasmart_ota_secrets.h.example` para
   `include/terrasmart_ota_secrets.h`, configure uma senha exclusiva do
   ArduinoOTA e substitua o certificado raiz pelo CA usado pelo servidor HTTPS.
   Esse arquivo é ignorado pelo Git.
3. O servidor HTTPS deve disponibilizar o `.bin` cru, com `Content-Length`
   conhecido. Calcule o tamanho e o CRC-32 IEEE do mesmo arquivo.
4. Proteja publicação no tópico `terrasmart/ota` por ACL no broker. Essa
   primeira versão requer TLS e validação de CA para baixar firmware.
5. Para o MQTT ESP8266, substitua também `-DFDRS_OTA_PASSWORD` no ambiente
   `d1_mini_mqtt_gateway` em `platformio.ini` com a senha de implantação.

## IDs OTA

O `target` é um identificador OTA de 8 bits, separado de `READING_ID`/`UNIT_MAC`.
Os valores inicializados são gateway ESP-NOW `0x01`, nó sensor `0x11`, nó de
boias `0x12` e nó de relés `0x14`. Se dois dispositivos iguais forem
implantados, atribua IDs exclusivos em suas configurações de build/firmware.

O gateway envia blocos broadcast ESP-NOW porque a tabela FDRS existente não
associa IDs lógicos OTA a MACs reais dos nodes. Só aceita e grava os blocos o nó
com o ID de destino; sua confirmação retorna unicast ao gateway.

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

O ESP8266 não tem partições A/B equivalentes às do ESP32. O core Arduino grava a
imagem OTA e seleciona o reboot loader; um corte de energia durante a escrita
pode exigir recuperação por USB. O binário para OTA precisa caber no espaço
reservado pelo layout de flash do board.

## Limitações / validação de bancada

- Fazer a primeira prova com gateway/nó ligados por USB e testar antes da
  implantação; o firmware e a compatibilidade do protocolo devem ser alinhados
  antes de atualizar o gateway ESP-NOW para que ele entenda os quadros UART.
- A OTA ESP-NOW é para um nó por vez, diretamente no alcance do gateway, acordado
  durante toda a transferência. Deep sleep, roteadores e mesh não participam.
- O CRC detecta corrupção de transporte, não autentica o binário. A autenticação
  depende do TLS com CA confiável e de ACL MQTT. Assinatura criptográfica dos
  firmwares e rollback automático após health-check serão etapas futuras.
- Confirme que a imagem compilada é compatível com o modelo de placa de destino;
  o comando não faz detecção de modelo nesta versão.
- O gateway MQTT ESP8266 não faz download/encaminhamento HTTPS remoto nesta
  implementação; para atualização remota indireta use o gateway MQTT ESP32.
