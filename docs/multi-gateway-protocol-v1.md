# Contrato multi-gateway TerraSmart v1

## Estado e compatibilidade

Este documento define o contrato para implementação experimental. Sua publicação
não ativa o protocolo nos firmwares: o tráfego atual FDRS, UART, MQTT, comandos e
OTA permanece inalterado até uma etapa de implementação e migração própria.

O contrato v1 atende comunicação direta node → gateway ESP-NOW. Roteamento mesh,
autenticação criptográfica e seleção por RSSI não fazem parte deste formato e
continuam sujeitos a validação/decisão antes de uso em produção.

## Identidades

Identidades são derivadas dos seis bytes da MAC STA de fábrica, em hexadecimal
maiúsculo sem separadores. Elas não são segredo, chave de autenticação, `UNIT_MAC`,
`GTWY_MAC`, ID OTA nem ID de leitura.

| Campo/identificador | Forma textual | Significado |
| --- | --- | --- |
| `node_id` | `node-<MAC>` | Node físico de origem; estável mesmo se os IDs FDRS de leitura se repetirem |
| `gateway_id` | `gw-<MAC>` | Gateway MQTT do par, usando a identidade MQTT já implementada |
| `radio_gateway_id` | `rgw-<MAC>` | Gateway ESP-NOW físico que recebeu/encaminhou o quadro |

Exemplo: a MAC `24:6F:28:01:AB:CD` resulta em `node_id` `node-246F2801ABCD`,
`gateway_id` `gw-246F2801ABCD` ou `radio_gateway_id` `rgw-246F2801ABCD`, conforme
o papel do equipamento. O endereço binário do protocolo usa os seis bytes da
mesma MAC STA de fábrica, não a MAC FDRS reescrita para rádio.

`gateway_id` sempre identifica o front-end MQTT do par e é o valor já usado em
status, comandos individuais e filtro de OTA. `radio_gateway_id` evita confundir
esse front-end com a placa ESP32 separada que recebe ESP-NOW. A associação física
entre os dois é provisionada no par e não inferida por um node.

## Envelope binário ESP-NOW

Todo quadro da aplicação v1 tem cabeçalho fixo de 30 bytes, seguido por payload.
Inteiros multibyte usam little-endian. O tamanho total não pode exceder 250 bytes,
limite conservador comum às placas ESP8266/ESP32 suportadas; portanto, o payload
máximo é 220 bytes. Uma implementação deve validar o comprimento recebido antes
de ler o cabeçalho ou copiar o payload e rejeitar comprimentos inconsistentes.

| Offset | Tamanho | Campo | Valor/regra |
| ---: | ---: | --- | --- |
| 0 | 2 | `magic` | ASCII `TS` (`0x54 0x53`) |
| 2 | 1 | `version` | `1` |
| 3 | 1 | `type` | `0x01` data; outros valores reservados/rejeitados |
| 4 | 4 | `network_id` | Identificador público da instalação, provisionado; não é segredo |
| 8 | 6 | `source_mac` | MAC STA de fábrica do node de origem |
| 14 | 6 | `destination_mac` | MAC STA de fábrica do gateway ESP-NOW de destino |
| 20 | 4 | `sequence` | Contador crescente por origem e tipo de tráfego; wrap modular |
| 24 | 4 | `boot_id` | Valor aleatório novo a cada boot, obtido do RNG de hardware |
| 28 | 1 | `hop_limit` | `1` no caminho direto v1; zero é inválido para dados |
| 29 | 1 | `payload_length` | Comprimento exato em bytes após o cabeçalho, máximo 220 |

No tipo `0x01`, o payload da leitura tem exatamente 7 bytes, sem padding de
estrutura C/C++: `id` (`uint16_t`, little-endian), `t` (`uint8_t`) e `d` (IEEE
754 binary32 little-endian). A camada FDRS converte explicitamente de/para esse
formato e preserva o significado dos três campos. Quadros truncados, maiores que
o limite, com tipo/versão/rede incompatível, destino incorreto ou comprimento
divergente são descartados. A compilação deve confirmar que `float` tem 32 bits e
IEEE 754; caso contrário, o transporte v1 deve falhar fechado.

Descoberta, resposta unicast e associação serão definidos no firmware experimental
das etapas correspondentes. Este contrato de dados não autoriza associação
persistente nem permite tratar tipos reservados como confirmação.

O formato v1 não inclui autenticação nem criptografia. `network_id`, `magic` e MAC
de origem não autenticam um emissor. Até uma decisão de segurança e validação de
chaves, esse envelope deve permanecer em builds de diagnóstico/piloto isolado e
nunca ser considerado autorização para comandos ou provisionamento.

## Payload MQTT versionado

O formato atual continua em `terrasmart/data`, como array legado de leituras. O
formato v1 usa o tópico `terrasmart/data/v1` e um objeto JSON por leitura:

```json
{
  "schema_version": 1,
  "node_id": "node-246F2801ABCD",
  "gateway_id": "gw-246F2801ABCE",
  "radio_gateway_id": "rgw-246F2801ABCF",
  "boot_id": 3735928559,
  "sequence": 1042,
  "hops": 0,
  "reading": {
    "id": 2,
    "type": 1,
    "data": 23.4
  }
}
```

`sequence` é o contador `uint32_t` da origem carregado no envelope e avança uma
vez por leitura criada. `boot_id` é um identificador aleatório de 32 bits obtido
uma vez do RNG de hardware no boot; ele distingue reinícios sem escrita frequente
em flash. Não é credencial nem prova criptográfica. A comparação de sequência é
modular: `a` é posterior a `b` quando `int32_t(a - b) > 0`, desde que a janela
entre mensagens não alcance metade do espaço de sequência. Consumidores deduplicam
usando `node_id` + `boot_id` + `sequence`; após reboot, uma nova `boot_id` inicia
uma janela independente. `hops` é zero no caminho direto v1. O objeto `reading`
preserva os campos e seus significados FDRS; a identidade não é codificada em
`id`.

Durante a migração, firmware piloto poderá publicar no tópico legado somente o
array legado, sem campos novos, e no tópico `/v1` o objeto versionado. Consumidores
legados não devem ser apontados ao tópico v1. Não publicar os dois formatos no
mesmo tópico, pois a mudança de array para objeto quebra o contrato atual.
Retenção não é usada para telemetria.

O status continua no tópico individual já existente
`terrasmart/gateways/<MAC-do-MQTT>/status`, com `gateway_id` no payload. O tópico
`/v1` compartilha o broker, mas a origem fica explícita em cada mensagem, mesmo
quando um node puder ser ouvido por mais de um gateway.

## Comandos e OTA

- Comandos FDRS são enviados ao tópico individual existente
  `terrasmart/gateways/<MAC-do-MQTT>/command`; o payload legado `id`, `type` e
  `data` permanece durante a transição. O tópico compartilhado
  `terrasmart/command` é somente compatibilidade temporária e não identifica o
  par.
- Comandos novos que precisem selecionar um node devem carregar `node_id` além
  do conteúdo FDRS. O gateway deve validar que o alvo pertence ao seu domínio e
  encaminhar apenas ao node correspondente. No tópico privado, o formato
  versionado é um objeto com `schema_version: 1`, `gateway_id`, `node_id` e
  `command: {"id": ..., "type": ..., "data": ...}`. O gateway valida que o
  `gateway_id` corresponde ao próprio par, mesmo que o tópico já seja individual.
  Esse direcionamento por node exige implementação posterior; o comando legado
  não ganha esse comportamento por este contrato.
- Comandos OTA globais permanecem em `terrasmart/ota` e exigem `gateway_id` do
  par MQTT, como já implementado. `target` continua selecionando a classe/alvo
  OTA legado e não é identidade global. Para OTA de um node específico, a futura
  extensão exige também `node_id`; um ID OTA de 8 bits não distingue dois nodes
  do mesmo modelo. Targets de classes de nodes (atualmente `0x11`, `0x12` e
  `0x14`) sem `node_id` devem ser rejeitados no formato v1. Targets locais do
  gateway MQTT (`0x00`) e do gateway ESP-NOW do par (`0x01`) não usam `node_id`.
- `radio_gateway_id` é diagnóstico/roteamento de rádio; não substitui
  `gateway_id` nos tópicos MQTT nem no comando OTA.

Exemplo futuro de OTA dirigido a um node, mantendo os campos atuais do release:

```json
{
  "gateway_id": "gw-246F2801ABCE",
  "node_id": "node-246F2801ABCD",
  "target": 17,
  "url": "https://firmware.example.net/node.bin",
  "size": 304651,
  "crc32": 1234567890
}
```

Até a implementação dessa extensão, os comandos OTA de node continuam usando o
target OTA legado, que só é seguro quando o target identifica um único node no
par; não enviar o formato exemplificado a firmwares atuais.

## Regras de evolução

1. Rejeitar versões desconhecidas; não reinterpretar silenciosamente outro
   layout como v1.
2. Preservar os contratos legados até migração coordenada de produtor e
   consumidor.
3. Alterações incompatíveis incrementam `version` do envelope e
   `schema_version` do MQTT independentemente.
4. Antes de ativar em produção, definir autenticação/provisionamento e testar
   identidade STA de fábrica e RNG nas placas/core efetivamente usados. A MAC
   de fábrica deve ser capturada antes que a inicialização FDRS possa reescrever
   a identidade usada pelo rádio.
5. A implementação deve confirmar IEEE 754 binary32 e rejeitar o payload se o
   cabeçalho mais leitura exceder 250 bytes.
