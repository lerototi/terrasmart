# Arquitetura de descoberta e mesh ESP-NOW TerraSmart

## 1. Objetivo e escopo

Este documento detalha um plano para que os dispositivos TerraSmart descubram
automaticamente gateways ESP-NOW e, quando não houver alcance direto, possam
encaminhar mensagens por outros dispositivos até um gateway.

Objetivos:

- eliminar a dependência de `GTWY_MAC` fixo nos nodes;
- escolher o gateway direto com melhor sinal quando houver mais de um
  disponível;
- estender a cobertura por meio de nós roteadores sempre ligados;
- recuperar rotas após reinício, perda de sinal ou indisponibilidade de um
  gateway/roteador;
- manter cada leitura associada ao node de origem até o MQTT/Home Assistant;
- evitar loops, duplicatas, mudanças excessivas de rota e entrada de dispositivos
  não autorizados.

Este plano é para uma malha TerraSmart implementada sobre ESP-NOW. ESP-NOW não
fornece mesh nem retransmissão automática: descoberta, encaminhamento, controle
de duplicatas e recuperação de rota serão responsabilidade do protocolo da
aplicação.

## Premissas de hardware acordadas

O plano de implementação assume a seguinte composição:

- gateways de rádio ESP-NOW: ESP32;
- gateway Wi-Fi/MQTT: Wemos D1 mini baseado em ESP8266;
- nodes sensores/folhas: Wemos D1 mini baseado em ESP8266;
- primeiro roteador mesh de teste: ESP32 dedicado e sempre ligado.

O gateway MQTT é um equipamento separado do gateway ESP-NOW. A comunicação
entre os dois continuará pelo link de integração já usado/configurado no
firmware (atualmente UART para a topologia com gateway ESP32). O Wemos D1 mini
é a plataforma prevista para o front-end MQTT; as restrições de UART do ESP8266
precisam ser respeitadas durante os testes.

Os Wemos D1 mini sensores serão folhas na primeira versão, mesmo quando
alimentados continuamente. Assim, a validação do roteamento não depende de
compartilhar o papel de sensor e roteador. Um ESP32 dedicado fará o papel de
roteador nos testes de um salto e múltiplos saltos. Um sensor D1 mini só poderá
ser promovido a roteador em uma fase futura, mediante configuração explícita,
alimentação contínua e validação de memória/peers.

Todos os dispositivos usarão um canal ESP-NOW comum no MVP (canal 1). Mudança
de canal, busca por vários canais e roteamento multicanal ficam fora do MVP.

## 2. Estado atual do projeto

O projeto usa a biblioteca Farm Data Relay System (FDRS), fixada no commit
`33fca6e8c3bd8f8fe5c0dac4c04648c80c6a7723`. No fluxo atual:

- o endereço do gateway é formado estaticamente a partir de `GTWY_MAC`;
- `beginFDRS()` inicializa o ESP-NOW e cadastra esse endereço como peer;
- `addFDRS()` registra um controller em um gateway conhecido;
- o gateway mantém peers e encaminha leituras recebidas;
- não há descoberta de gateways, troca dinâmica de endereço/canal nem
  encaminhamento mesh de node para node;
- leituras FDRS contêm `id`, `t` e `d`, sem um identificador global obrigatório
  do node de origem.

A configuração atual do node define `GTWY_MAC 0x01` e canal 1. A arquitetura
de descoberta não pode ser habilitada apenas mudando essa configuração: será
necessário adicionar um protocolo e adaptar a integração com FDRS. As alterações
na biblioteca devem ser mantidas em um fork controlado ou em patches de build
versionados e cobertos por verificações.

## 3. Papéis dos dispositivos

### 3.1 Gateway ESP-NOW

É a saída da mesh para UART/MQTT/Home Assistant. Deve:

- possuir identidade lógica estável e única;
- anunciar sua presença, rede, canal, versão e capacidade;
- aceitar registros autenticados;
- receber mensagens locais ou encaminhadas;
- reconhecer a origem original da leitura;
- entregar dados ao backend e encaminhar comandos para a mesh;
- continuar atendendo os dispositivos locais durante falhas do MQTT, quando
  possível.

### 3.2 Nó roteador

É um ESP8266/ESP32 alimentado continuamente, com rádio disponível para receber
e retransmitir tráfego. Deve:

- anunciar ou propagar caminhos válidos até gateways;
- encaminhar mensagens de outros nodes;
- confirmar recebimento por salto, manter uma fila limitada e descartar
  duplicatas;
- deixar de anunciar uma rota quando o próximo salto expirar.

Um dispositivo em deep sleep não pode oferecer encaminhamento enquanto dorme.
Por isso, a primeira versão deve permitir roteamento apenas em dispositivos
sempre ligados. Nodes de bateria/deep sleep serão dispositivos folha.

### 3.3 Nó folha/sensor

Produz leituras e comandos de estado. Pode enviar diretamente para um gateway
ou usar um roteador descoberto. Não encaminha mensagens de terceiros na primeira
versão.

### 3.4 Gateway MQTT

Continua sendo o front-end de Wi-Fi/MQTT. Não participa diretamente do rádio
ESP-NOW, salvo se uma placa/firmware combinar explicitamente os dois papéis.
Gateways ESP-NOW encaminham os dados ao gateway MQTT pelo link já adotado no
projeto (por exemplo, UART).

## 4. Topologia e identidade

Exemplo de topologia:

```text
Sensor A ----------- Gateway ESP-NOW 01 ---- UART/MQTT ---- Home Assistant
    \
     Roteador ESP-NOW -------- Gateway ESP-NOW 02 ---- UART/MQTT ----/
           /
Sensor B
```

Cada dispositivo deve ter:

- um identificador de rede lógico, estável e diferente de um segredo;
- um ID de dispositivo globalmente único dentro dessa rede;
- o MAC de rádio real para endereçamento ESP-NOW e diagnóstico;
- um papel: gateway, roteador ou folha;
- uma versão do protocolo.

O `MAC_PREFIX` atual (`AA:BB:CC:DD:EE`) e o último byte configurado em
`UNIT_MAC`/`GTWY_MAC` não são suficientes para uma identidade global segura. O
prefixo é fixo na biblioteca; IDs lógicos duplicados causam ambiguidades mesmo
quando os MACs de rádio são diferentes. Um gateway deve ter ID persistente e
único, nunca gerado aleatoriamente em cada boot. O cadastro e a atribuição desses
IDs precisam ser definidos antes de instalar vários dispositivos.

O identificador do node precisa acompanhar os dados por todos os saltos. O
gateway MQTT não deve inferir a origem somente pelo gateway que recebeu o
pacote: diferentes nodes podem compartilhar um gateway e gateways redundantes
podem receber dados de uma mesma área.

## 5. Canal de rádio

ESP-NOW opera no canal atual do rádio. Um dispositivo só escuta um canal por
vez; portanto, um nó não descobre simultaneamente anúncios em canais diferentes.

### Recomendação para a primeira versão

- usar um canal comum em toda a mesh (canal 1 inicialmente, conforme o projeto);
- validar que todos os gateways e roteadores permanecem nesse canal;
- não fazer varredura de canal durante tráfego normal;
- registrar o canal como parte dos anúncios e da configuração persistida.

Usar canais diferentes pode reduzir interferência em redes planejadas, mas um
rádio único não consegue receber e retransmitir continuamente em dois canais.
Para suportar canais diferentes será preciso definir janelas de varredura,
reconexão e perda temporária de tráfego, ou usar hardware dedicado por canal.
Isso fica fora do MVP.

## 6. Descoberta e anúncios

### 6.1 Anúncio do gateway

O gateway transmite periodicamente um anúncio broadcast. O intervalo inicial
deve ser configurável; como ponto de partida para testes, usar 1–3 segundos
durante provisionamento e 10–30 segundos em operação estável. O anúncio deve
conter:

- tipo e versão da mensagem;
- ID da rede;
- ID lógico e MAC de rádio do gateway;
- canal;
- nonce/contador monotônico do gateway;
- estado/capacidade, se for possível medi-los de forma confiável;
- parâmetros de autenticação adequados à decisão de segurança.

O broadcast serve para descoberta, não deve conceder por si só acesso
permanente.

### 6.2 Descoberta pelo node

No primeiro provisionamento, o node:

1. inicializa no canal conhecido da instalação;
2. escuta anúncios por uma janela definida, com pequenas variações aleatórias
   para reduzir respostas sincronizadas;
3. valida rede, versão, identidade e autenticação;
4. coleta várias medições por candidato, não apenas um pacote;
5. tenta confirmação unicast com o candidato selecionado;
6. persiste a seleção somente após a confirmação.

O node guarda uma tabela limitada de candidatos com ID, MAC, canal, RSSI das
amostras, horário da última mensagem e resultado de confirmação. Entradas
expiram para evitar usar anúncios antigos.

### 6.3 Nós roteadores e anúncios de rota

Um roteador precisa anunciar que alcança um ou mais gateways. Para o MVP,
preferir um protocolo de vetor de distância simplificado, em que cada anúncio
de rota informe:

- ID do gateway de destino;
- ID do próximo salto;
- sequência/época originada pelo gateway, para distinguir informação nova;
- número de saltos;
- custo/qualidade acumulada ou estimada;
- tempo de validade;
- identidade e autenticação do anunciante.

O receptor não aceita uma rota anunciada por si mesmo como próximo salto, nem
rotas que excedam o limite de saltos. Sequência mais nova prevalece; para a
mesma sequência, comparar custo e desempatar deterministicamente pelo ID do
próximo salto. Rotas deixam de ser válidas após timeout sem atualização.

Gateways e roteadores não devem retransmitir anúncios sem limite. Usar
intervalo, jitter, supressão de anúncios equivalentes e validade finita.

## 7. Seleção de gateway e rota

### 7.1 Acesso direto

Quando vários gateways compatíveis forem ouvidos diretamente pelo node, a
preferência definida para o TerraSmart é escolher o gateway com sinal recebido
mais forte. O RSSI deve ser amostrado repetidamente e filtrado (por exemplo,
mediana ou média móvel) para reduzir variações instantâneas.

RSSI mais alto significa sinal mais forte (por exemplo, -55 dBm é mais forte
que -75 dBm). A leitura de RSSI é do pacote recebido pelo próprio dispositivo;
não se deve presumir que o gateway consegue informar com precisão o RSSI de
outros enlaces.

A disponibilidade da medição de RSSI deve ser confirmada para cada combinação
de ESP8266/ESP32 e versão do core Arduino. Se uma plataforma não expuser RSSI de
recepção confiável no callback utilizado, a seleção precisará usar sucesso e
tempo de ACK como aproximação, ou exigir uma API de rádio adequada.

### 7.2 Mesh

Em uma rota com vários saltos, o RSSI do primeiro enlace não revela a força do
sinal do gateway remoto. A rota deve ser escolhida com uma métrica de caminho:

- eliminar caminhos inválidos/expirados;
- preferir menor número de saltos como critério de simplicidade inicial;
- entre rotas equivalentes, preferir melhor qualidade observada dos enlaces;
- considerar falhas e atraso de confirmação;
- desempatar por ID estável.

O objetivo de selecionar o gateway mais forte aplica-se diretamente quando há
gateway visível. Via mesh, a decisão possível é selecionar o melhor caminho
estimado até um gateway, que pode não ser o gateway cujo sinal direto seria
mais forte em outra posição.

### 7.3 Histerese e estabilidade

Reavaliar candidatos periodicamente, mas não trocar a cada flutuação. Uma troca
só deve ocorrer quando:

- a rota atual falhar um número configurável de confirmações; ou
- outra rota/gateway superar de forma consistente um limiar de melhoria durante
  várias medições;
- tiver passado um intervalo mínimo desde a última troca, exceto em falha real.

Os valores exatos do limiar, quantidade de amostras e intervalo mínimo devem
ser medidos no local e permanecer configuráveis. Nunca persistir uma nova rota
antes de confirmar comunicação nela.

## 8. Formato e encaminhamento de dados

As leituras FDRS atuais não carregam identidade global do node nem campos de
roteamento. A camada mesh deve encapsular os dados FDRS em um envelope próprio,
sem alterar o significado de `id`, `t` e `d`.

Campos mínimos do envelope de dados:

- versão e tipo da mensagem;
- ID da rede;
- ID de origem e ID de destino/gateway;
- contador de sequência por origem;
- número de saltos restantes (TTL);
- quantidade/tamanho e payload FDRS;
- autenticação/integridade, quando definida.

Fluxo de encaminhamento:

1. O node cria uma mensagem com origem, sequência e TTL inicial.
2. Envia ao próximo salto da rota selecionada.
3. O próximo salto valida rede, integridade, TTL e duplicata.
4. Se for gateway de destino, entrega a leitura ao adaptador FDRS/MQTT.
5. Caso contrário, reduz TTL e encaminha ao próximo salto da própria tabela.
6. Cada salto confirma recepção; o originador repete em timeout de forma
   limitada e com atraso crescente.
7. Mensagens sem rota, expiradas ou repetidas são descartadas e contabilizadas.

A chave de deduplicação deve combinar pelo menos ID de origem e sequência. A
tabela de duplicatas deve ter limite de memória e expiração. TTL/hop-limit,
limite de tentativas e fila de envio devem ser finitos para evitar loops e
consumo ilimitado de memória.

Para comandos no sentido inverso, o envelope preserva o ID do node de destino e
o gateway usa a tabela de rotas para encaminhá-lo. O nó folha confirma o
comando; a confirmação também percorre o caminho de retorno ou uma rota válida
até o gateway.

## 9. Confiabilidade e recuperação

### Falha de um salto

- aguardar ACK por salto;
- repetir poucas vezes com backoff;
- marcar enlace degradado após falhas consecutivas;
- invalidar a rota e consultar alternativa;
- evitar bloquear o loop principal enquanto aguarda ACK.

### Perda de gateway

- gateways anunciam presença e rotas com timeout;
- rotas desatualizadas expiram;
- o nó escolhe outro gateway direto ou um caminho via roteador;
- persistir a nova rota apenas após confirmação ponta a ponta ou confirmação
  definida pelo protocolo.

### Reinício

O node persiste a última rota confirmada, mas usa isso como preferência de
arranque, não como verdade eterna. Ao iniciar, testa essa rota por tentativas
limitadas; sem resposta, inicia descoberta completa. Gateways reconstroem estado
de vizinhos e rotas a partir dos anúncios após reboot; não dependem de uma lista
de peers volátil para sempre.

### Filas e perda temporária do backend

Decidir quais mensagens podem ser armazenadas e por quanto tempo. Para a
primeira versão, preferir fila pequena em RAM e descarte explícito de leituras
antigas, em vez de gravar cada retransmissão em flash. Comandos de controle
podem precisar de política de prioridade e expiração diferente de telemetria.

## 10. Identidade de leituras e integração MQTT/Home Assistant

O encaminhamento não pode descartar a origem. O gateway deve exportar ao
backend um identificador de node além do ID do sensor, por exemplo:

```json
{
  "node_id": "node-002A",
  "gateway_id": "gw-01",
  "reading_id": 2,
  "type": 1,
  "data": 23.4,
  "sequence": 1042,
  "hops": 2
}
```

O formato acima é ilustrativo; a compatibilidade com a estrutura JSON FDRS
existente deve ser preservada ou versionada. O Home Assistant precisa conseguir
criar entidades estáveis por `node_id` e sensor, e distinguir:

- último dado recebido;
- disponibilidade do node;
- qualidade/rota usada, se exposta;
- disponibilidade do gateway MQTT, que é diferente da disponibilidade de cada
  node.

Disponibilidade de um node deve ser inferida no backend a partir do horário da
última mensagem ou de um heartbeat do próprio node. Um heartbeat do gateway
MQTT não prova que os sensores ESP-NOW estão ativos. Nodes silenciosos ou em
deep sleep exigem timeout maior que o intervalo normal entre transmissões.

## 11. Segurança e provisionamento

Antes de instalação em campo, definir:

- identificador da rede e segredo/chave com entropia adequada;
- como o segredo é inserido no gateway e nos nodes;
- janela de pareamento, botão físico ou autorização central para novos nodes;
- autenticação de anúncios, pedidos de rota, dados e comandos;
- criptografia ESP-NOW e gerenciamento de chaves suportados em cada plataforma;
- revogação de node perdido e rotação de credenciais;
- comportamento de recovery se as credenciais forem apagadas.

Um broadcast de descoberta não pode autorizar sozinho um dispositivo. Validar
todos os comprimentos/campos recebidos antes de copiar/processar dados, limitar
taxas e rejeitar origem, sequência, versão ou TTL inválidos.

## 12. Limites de recursos e comportamento de roteadores

ESP8266 tem memória e número máximo de peers mais limitados que ESP32. Antes da
implementação, medir limites reais da versão do core e da API ESP-NOW do projeto.
O protocolo deve limitar:

- tamanho da tabela de vizinhos e rotas;
- tamanho da tabela de deduplicação;
- quantidade e tamanho das mensagens em fila;
- número de saltos;
- peers cadastrados simultaneamente;
- taxa de anúncios e retransmissões.

Nodes roteadores são pontos de falha e consomem energia e tempo de rádio. Eles
devem anunciar capacidade/carga apenas se houver métrica útil e confiável; uma
indicação falsa de carga pode piorar a seleção. Uma primeira versão deve manter
roteadores fixos/sempre ligados e sem deep sleep.

## 13. Plano de implementação por fases

Cada fase deve gerar uma alteração pequena, um firmware compilável e um teste
reproduzível antes de iniciar a fase seguinte. Não ativar simultaneamente
descoberta, roteamento mesh e alteração de payload MQTT: testar cada camada
separadamente facilita isolar falhas. A subseção 13.1 organiza o trabalho como
uma sequência de PRs para revisão e entrega incremental. A descoberta direta
entre node e gateway é o objetivo inicial; mesh fica fora do caminho crítico da
migração.

### 13.1 Decomposição em pull requests

As fases abaixo são marcos técnicos; não devem ser executadas todas em uma única
PR. A sequência seguinte divide o trabalho em alterações revisáveis e testáveis.
Cada PR deve ter escopo próprio, firmware/build reproduzível e critério de
aceitação. Uma PR pode ajustar o plano após evidência de bancada, mas não deve
acumular a etapa seguinte.

#### PR 1 — baseline reproduzível e contrato de compatibilidade

- Seguir [production-baseline.md](production-baseline.md) para registrar builds,
  versões, topologia, payloads, tópicos, comandos, boards e resultados do teste
  ponta a ponta.
- Manter evidências de compilação separadas das evidências de bancada; build
  aprovado não comprova comunicação física.
- Não alterar o protocolo nem os firmwares instalados.

**Aceitação:** todos os ambientes de produção compilam ou têm falhas conhecidas
documentadas; o caminho sensor → ESP-NOW → UART → MQTT é registrado em bancada;
os contratos legados conhecidos estão documentados e nenhum segredo foi incluído.

**Contrato de comando após PR 8:** mensagens FDRS de controle usam o tópico
individual `terrasmart/gateways/<MAC>/command`, visível no portal/status. O tópico
legado `terrasmart/command` é aceito temporariamente para compatibilidade; ele é
compartilhado e pode atingir qualquer gateway que ainda o assine. Migre os
produtores de comando para os tópicos individuais antes de retirar essa
assinatura legada.

#### PR 2 — eliminar colisões entre gateways MQTT

- Derivar um client ID MQTT único e estável da identidade de fábrica de cada
  gateway MQTT.
- Publicar o status em um tópico próprio por gateway, preservando o tópico de
  dados e seu payload atuais.
- Configurar Last Will por gateway e limpar o status retido legado no primeiro
  contato da versão nova com o broker.
- Validar dois gateways MQTT conectados simultaneamente ao mesmo broker.

**Aceitação:** os gateways permanecem conectados ao broker ao mesmo tempo, cada
um mantém seu status independente e o consumidor atual de dados continua
recebendo o formato legado; a queda de cada gateway atualiza somente seu próprio
Last Will/status.

#### PR 3 — definir identidade automática e envelope versionado

- Especificar a identidade estável derivada da MAC de fábrica, separada dos IDs
  semânticos de sensores e das credenciais de segurança.
- Definir o envelope ESP-NOW e os campos de origem, gateway, sequência, versão e
  limite de tamanho.
- Definir o payload MQTT versionado para o broker compartilhado. O tópico de
  dados pode continuar comum, mas cada mensagem nova deve identificar `node_id`
  e `gateway_id`; o status permanece por gateway.
- Definir como comandos e atualizações OTA identificam o gateway/dispositivo
  alvo, para que gateways no mesmo broker não executem comandos alheios.
- Nesta PR, registrar contratos e exemplos; não ativar o novo formato em
  produção.

Contrato definido nesta etapa para orientar a implementação: [multi-gateway-protocol-v1.md](multi-gateway-protocol-v1.md).
Ele distingue o `gateway_id` do front-end MQTT do `radio_gateway_id` do ESP32,
mantém o tópico/payload legado e reserva `terrasmart/data/v1` para objetos
versionados. Segurança criptográfica, provisionamento de `network_id` e alvo OTA
individual por `node_id` continuam requisitos antes das respectivas fases de
ativação.

**Aceitação:** formatos, transição legado/novo, IDs e roteamento de comandos
estão especificados antes de alterar o tráfego dos dispositivos.

#### PR 4 — prova isolada de identidade e descoberta no rádio

- Criar ou estender firmware de diagnóstico para dois gateways ESP32 com
  identidade estável derivada da MAC de fábrica e anúncios versionados.
- Verificar recepção nos modelos de node em uso, canal e limites de peers.
- Medir o que os cores realmente oferecem para a qualidade de enlace; não
  assumir RSSI quando a API não o fornece.
- Manter os ambientes de produção intocados.

**Aceitação:** dois gateways são distinguidos após reinicializações e a
compatibilidade de rádio está demonstrada nos hardwares de produção.

#### PR 5 — descoberta experimental, sem persistência

- Implementar descoberta e resposta unicast em um ambiente de build
  experimental, inicialmente sem substituir o caminho fixo de produção.
- O node coleta candidatos, ignora versões/redes incompatíveis e seleciona um
  gateway usando a métrica de enlace validada na PR 4.
- Ainda não encaminhar leituras de produção nem gravar a associação em flash.

**Aceitação:** com dois gateways visíveis, o node seleciona e confirma um deles
repetidamente sem alternância instável; o firmware legado continua disponível.

#### PR 6 — associação persistente e recuperação direta

- Persistir a última associação confirmada em formato versionado e validado.
- Testar arranque em ordens diferentes, perda do gateway e redescoberta de outro
  gateway compatível.
- Limitar tentativas e escritas em flash; manter fallback para descoberta.

**Aceitação:** reinicialização ou perda do gateway não exige reconfiguração
manual e uma associação inválida não impede nova descoberta.

Implementação experimental desta etapa: `espnow_discovery_node.cpp` grava a
associação somente depois de três respostas unicast consecutivas. O registro
contém magic, versão, `network_id`, canal, MAC STA ativa do gateway e CRC-32;
comprimento, endereço unicast, checksum, rede e canal são validados no boot.
ESP32 usa NVS (`Preferences`); D1 mini usa a região EEPROM emulada. Após reboot,
o candidato persistido é sondado primeiro e continua sujeito a beacon recente e
confirmação da aplicação. Com três falhas ou expiração do beacon, a preferência
em RAM é invalidada e a descoberta geral retoma. A migração v1/vazio/corrompido
falha aberta para descoberta. Gravação só ocorre quando muda o MAC confirmado,
evitando writes por beacon; a associação não é credencial nem configuração de
produção.

Testes unitários do registro: `pio test -c platformio-native.ini -e native -f
test_discovery_association`. O build de D1 mini reserva 1 MB para EEPROM
emulada (`eagle.flash.4m1m.ld`); a área usada é o começo da partição EEPROM e
não grava de novo se a associação confirmada não mudar.

**Bancada de associação persistente, 05/11/2026:** com o D1 mini
`EC:FA:BC:D4:E7:33` em `ttyUSB3`, o node selecionou o gateway
`AA:BB:CC:DD:EE:02`, exibiu `Confirmed association saved` e, após reset, carregou
`Loaded persisted gateway preference: AA:BB:CC:DD:EE:02`, reconfirmando-o em
quatro respostas correspondentes no primeiro relatório. Mantendo o gateway 02
em reset, o node invalidou a preferência após falhas, confirmou o gateway 01
(`AA:BB:CC:DD:EE:01`) e salvou a nova associação. Um segundo reboot carregou a
preferência 01 e a confirmou em quatro sondagens. Não foram observados timeouts
para o gateway disponível nem descartes de fila. Os gateways e o node estavam
em firmwares experimentais; nenhum tráfego de produção foi alterado.

#### PR 7a — piloto isolado do envelope de dados ESP-NOW

- Implementar o codec binário do [contrato v1](multi-gateway-protocol-v1.md),
  com serialização explícita e validação de rede, destino, versão e comprimento.
- Usar ambientes de bancada independentes para um node D1 mini e um gateway
  ESP32. O node gera uma leitura FDRS sintética; o gateway decodifica e imprime
  identidade, boot, sequência e leitura, sem inicializar FDRS de produção.
- Manter endereçamento fixo ao MAC STA de fábrica do ESP32 de bancada. A
  associação dinâmica e o transporte de dados ainda não estão integrados.
- Testar bytes conhecidos, rejeição de quadros inválidos, identidade e sequência
  no rádio, incluindo reboot do node e mudança de `boot_id`.

**Aceitação desta subetapa:** builds ESP8266/ESP32 e testes do codec aprovados;
gateway piloto recebe o conteúdo e os identificadores esperados pela serial.
Não é uma prova sensor → MQTT nem uma ativação dos firmwares de produção.

#### PR 7b — ponte UART/MQTT versionada do piloto

**Objetivo:** transportar uma leitura v1 recebida pelo ESP32 até o tópico
`terrasmart/data/v1`, preservando a origem e o contrato legado.

1. Definir o framing UART v1 antes de implementar: delimitação, tamanho máximo,
   versão, CRC, timeout, ressincronização e comportamento diante de bytes
   parciais, quadros OTA e dados legados. Não assumir que o parser JSON UART
   existente reconhece o novo formato.
2. No gateway ESP32 piloto, enfileirar uma representação validada da leitura
   com `source_mac`, MAC de fábrica do gateway de rádio, `boot_id`, `sequence`
   e campos FDRS; limitar memória e contabilizar descarte por fila cheia.
3. No gateway MQTT D1 mini, identificar seu próprio `gateway_id`, validar o
   framing e publicar um objeto v1 por leitura conforme o contrato. A associação
   do ESP32 ao front-end MQTT deve ser configurada no par. Não inferir origem
   a partir do gateway MQTT. Usar `hops: 0` no enlace direto.
4. Manter `terrasmart/data` como array legado; se houver dupla publicação,
   usar apenas o formato legado nesse tópico e o objeto no tópico `/v1`.
   Definir explicitamente deduplicação por `node_id + boot_id + sequence`,
   filas, limites e política quando UART ou broker estiverem indisponíveis.
5. Testar parser com quadros truncados, CRC incorreto, versões desconhecidas,
   excesso de comprimento, fragmentação, sequência duplicada e coexistência OTA;
   em bancada, confirmar origem, sequência, valores e tópicos no broker e testar
   reinício/desconexão sem afetar clientes legados.

**Aceitação da PR 7b:** a mesma leitura do piloto chega ao broker com
`node_id`, `gateway_id`, `radio_gateway_id`, `boot_id`, sequência e dados
corretos; o payload legado e os canais de comando/OTA existentes permanecem
compatíveis. Autenticação/provisionamento e validação do D1 intermitente seguem
pendentes antes de qualquer migração para produção.

#### PR 8 — integração multi-gateway no broker compartilhado

- Validar pelo menos dois pares ESP-NOW/MQTT, cada par com sua própria UART e
  gateway MQTT conectado ao mesmo broker.
- Publicar dados no espaço comum acordado e incluir `node_id` e `gateway_id` no
  payload novo; status fica isolado por gateway.
- Direcionar comandos/OTA ao par e dispositivo corretos, com testes de que os
  outros gateways os ignoram.
- Usar tópicos de comando FDRS por gateway e exigir `gateway_id` nos comandos
  OTA globais; mostrar identidade e tópico de comando no portal de cada gateway.
- Definir deduplicação no consumidor se o mesmo node puder ser ouvido por mais
  de um gateway durante uma transição.

**Aceitação:** não há colisões de client ID/status, comandos executados pelo par
errado ou ambiguidade de origem no broker.

#### PR 9 — migração de produção e remoção do endereço fixo

- Preparar sequência de atualização, identificação de versões e rollback dos
  firmwares de gateways e nodes.
- Migrar por grupos após o piloto; confirmar recepção no Home Assistant, comandos
  e OTA antes de avançar cada grupo.
- Remover `GTWY_MAC`/endereçamento fixo somente depois de todos os dispositivos
  relevantes usarem e validarem o protocolo novo.

**Aceitação:** a frota migrada usa descoberta e IDs automáticos, com procedimento
documentado de recuperação para cada etapa.

#### Fora do escopo desta sequência — roteamento mesh

Repetidores, múltiplos saltos, propagação de rotas e recuperação de mesh não são
necessários para selecionar entre gateways diretamente alcançáveis. Permanecem
como uma iniciativa posterior, com proposta e testes próprios, após estabilizar
descoberta direta, identidade e integração MQTT multi-gateway. Não bloquear a
migração direta esperando implementação de mesh.

### Fase 0 — bancada, versões e baseline

**Hardware:** dois ESP32 para gateways ESP-NOW, um ESP32 para uso posterior como
roteador, um Wemos D1 mini sensor e um Wemos D1 mini para gateway MQTT.

**Implementação:**

- registrar versões do PlatformIO, ESP32 Arduino core, ESP8266 Arduino core e
  commit da FDRS;
- compilar os ambientes atuais `esp32dev_gateway`, `d1_mini_node`,
  `d1_mini_float_switch_node`, `d1_mini_relay_node` e
  `d1_mini_mqtt_gateway`;
- confirmar a fiação UART entre gateway ESP32 e gateway MQTT ESP8266, incluindo
  cruzamento RX/TX e níveis elétricos compatíveis;
- manter todos os rádios no canal 1;
- salvar os logs seriais de inicialização, ping, registro e envio de leitura;
- medir a taxa baseline de envio/ACK em distâncias e posições conhecidas.

**Testes:**

1. Um D1 mini sensor envia leituras ao ESP32 gateway existente.
2. Gateway ESP32 encaminha dados pelo UART e gateway MQTT publica no broker.
3. Confirmar que a telemetria chega ao tópico esperado sem alteração de payload.
4. Reiniciar cada placa isoladamente e registrar o tempo até voltar a comunicar.

**Aceitação para avançar:** todos os firmwares da topologia compilam e o caminho
sensor → ESP32 ESP-NOW → UART → D1 mini MQTT → broker está reproduzível.

**Comandos de build de referência:**

```bash
pio run -e esp32dev_gateway
pio run -e d1_mini_node
pio run -e d1_mini_float_switch_node
pio run -e d1_mini_relay_node
pio run -e d1_mini_mqtt_gateway
```

O upload e os testes de comunicação são feitos individualmente nas placas; não
subir um firmware experimental em todos os dispositivos ao mesmo tempo.

### Firmware de diagnóstico ESP-NOW/RSSI

Para a investigação da Fase 1, o repositório oferece firmwares isolados que não
alteram nem substituem os firmwares FDRS de produção:

```bash
pio run -e esp32dev_espnow_probe_gateway_1
pio run -e esp32dev_espnow_probe_gateway_2
pio run -e d1_mini_espnow_probe_receiver
```

Procedimento de bancada:

1. Gravar `esp32dev_espnow_probe_gateway_1` em um ESP32 e
   `esp32dev_espnow_probe_gateway_2` no outro. Os dois builds transmitem o mesmo
   protocolo; a identidade do gateway é a MAC STA de fábrica lida do rádio, não
   um ID constante de build.
2. Gravar `d1_mini_espnow_probe_receiver` no Wemos D1 mini que será o node.
3. Abrir os monitores dos ESP32 a 115200 baud e o monitor do receptor D1 mini a
   74880 baud. O ESP8266 do build de diagnóstico usa 74880 baud para que tanto
   as mensagens de boot quanto os marcadores de estágio da aplicação sejam
   legíveis. Cada ESP32 exibe a MAC STA de fábrica, canal, sequência transmitida
   e resultados do callback de envio.
4. O Wemos exibe uma entrada por MAC gateway observada, contagem de pacotes,
   última sequência e idade da última recepção. O callback apenas valida/copia
   para uma fila limitada; a atualização da tabela e impressão ocorrem no loop.
   Repetir mudando distância, posição e obstruções, e guardar os logs.
5. Repetir com cada transmissor individualmente e com os dois ativos.

O beacon de diagnóstico v1 tem 9 bytes: magic `TS`, versão, tipo, canal e
sequência `uint32_t`; a MAC de origem fornecida pelo callback é sua identidade
estável. O receptor valida tamanho, magic, versão, tipo e canal, e mantém até oito
MACs observadas. Quadros recebidos além da capacidade da fila/tabela são contados
como descartes, sem alocação dinâmica.

Nos cores efetivamente configurados (ESP8266 Arduino 3.1.2 e ESP32 Arduino
2.0.17), a assinatura do callback ESP-NOW usado recebe MAC, dados e comprimento;
não inclui RSSI. Por isso o firmware reporta RSSI como indisponível. Não
interpretar o status do callback de envio do transmissor como RSSI ou como
confirmação de que o receptor processou o anúncio. A prova de RSSI por sniffer ou
interface promiscuous continua fora desta PR, conforme a restrição de não usar
metadados privados/não documentados no firmware de produção.

**Resultado de bancada relatado:** dois ESP32 transmissores foram recebidos
simultaneamente pelo receptor de diagnóstico ESP32. Os logs mostraram os MACs
`38:3E:51:D4:0E:80` e `38:3E:51:D3:E3:74`, 551 callbacks de recepção, zero
pacotes inválidos, zero descartes de fila e zero pacotes sem rastreamento. Isso
valida broadcast ESP-NOW ESP32→ESP32 para o probe; não valida a recepção pelo D1
mini, RSSI, estabilidade após reboot, alcance ou o fluxo FDRS de produção.

#### Diagnóstico seguinte — enlace de descoberta com node D1 mini

Os probes de descoberta abaixo são experimentais e independentes do FDRS de
produção. Eles reproduzem o endereço lógico atual do gateway (`AA:BB:CC:DD:EE:01`)
e mantêm a MAC de fábrica do D1 mini como identidade/transmissor do node. O
gateway ESP32 imprime a MAC STA de fábrica e a MAC ativa após aplicar o endereço
FDRS; o node imprime sua MAC de fábrica e o MAC de destino configurado.

Builds:

```bash
pio run -e esp32dev_discovery_gateway_probe
pio run -e esp32dev_discovery_gateway_probe_2
pio run -e d1_mini_discovery_node_probe
pio run -e d1_mini_discovery_node_probe_2
```

Procedimento:

1. Gravar `esp32dev_discovery_gateway_probe` no ESP32 e
   `d1_mini_discovery_node_probe` no Wemos. Manter ambos no canal 1 e próximos
   durante a primeira execução.
2. Abrir o monitor ESP32 a 115200 baud e o monitor D1 mini a 74880 baud.
   Confirmar no ESP32 `Active STA MAC: AA:BB:CC:DD:EE:01`; a MAC de fábrica deve
   permanecer exibida separadamente. No node, confirmar a MAC de fábrica e o
   mesmo canal.
3. O gateway transmite beacon broadcast a cada segundo. O node conta beacons
   recebidos e envia uma sondagem unicast a cada dois segundos para o endereço
   ativo do gateway. O gateway responde com o mesmo número de sequência.
4. Comparar no node `Beacons received`, callbacks de envio, `replies`, timeouts e
   latência; no gateway comparar callbacks, sondagens válidas e falhas ao
   cadastrar/enfileirar/responder ao MAC de origem recebido.
5. Repetir com o gateway FDRS de produção e o node de produção somente após o
   protocolo experimental validar a diferença entre endereço STA de fábrica e
   endereço ESP-NOW ativo. Não operar os dois firmwares de gateway no mesmo canal
   ao mesmo tempo durante a comparação.
6. Para exercitar dois candidatos, gravar `esp32dev_discovery_gateway_probe`
   no primeiro ESP32 e `esp32dev_discovery_gateway_probe_2` no segundo; use um
   node com `d1_mini_discovery_node_probe`. Os endereços ativos serão
   `AA:BB:CC:DD:EE:01` e `AA:BB:CC:DD:EE:02`. O node alterna sondagens unicast
    entre os dois e reporta beacons, respostas e timeouts por MAC. Ambos os
    ambientes do node sondam os dois candidatos; o sufixo `_2` apenas altera o
    endereço de gateway mostrado na inicialização. Para um teste isolado,
    mantenha ligado somente o gateway desejado e considere os timeouts do outro.

Critério para iniciar a implementação de descoberta no node: beacons recebidos
repetidamente e respostas de sondagem com sequência correspondente, sem timeout
contínuo, com MAC/endereço ativo e canal registrados. O status do callback de
envio sozinho não conta como confirmação da aplicação.

**Bancada de 05/11/2026:** `ttyUSB2` (MAC de fábrica
`D4:E9:F4:02:E5:EC`) operou como gateway de diagnóstico com endereço ativo
`AA:BB:CC:DD:EE:01`, canal 1. Em um teste separado, `ttyUSB4` (MAC de fábrica
`38:3E:51:D3:E3:74`) executou o receptor ESP32 e recebeu 15 beacons válidos
desse gateway; o relatório registrou zero lacunas estimadas, duplicatas,
pacotes inválidos e descartes de fila. É uma amostra curta de broadcast
ESP32 → ESP32, sem prova de unicast ou do D1 mini. Ao final, `ttyUSB4` foi
regravado como gateway de diagnóstico `AA:BB:CC:DD:EE:02`.

O upload do probe para o D1 mini em `ttyUSB3` falhou duas vezes por timeout de
conexão. Após reset pela serial, o ROM informou `boot mode:(7,7)` e `waiting
for host`, portanto não houve execução do probe nem validação ESP8266 ↔ ESP32.
Verificar conexões externas e níveis de boot do ESP8266 (GPIO15/D8 baixo;
GPIO2/D4 alto; GPIO0/D3 alto para executar ou baixo para gravação), além de
RX/TX e reset automático, antes de repetir o upload. A comunicação direta
bidirecional com os dois gateways e o critério para avançar permanecem pendentes.

O resultado de bancada deve registrar separadamente se a camada de
recepção/monitor do rádio permite associar RSSI de forma confiável a cada
anúncio ESP-NOW nas placas reais. Não adotar modo promiscuous, layout privado de
metadados do SDK ou API não documentada no firmware de produção sem uma prova
isolada de coexistência e estabilidade. Se RSSI não puder ser obtido de forma
suportada no D1 mini, revisar a escolha de gateway para uma métrica de enlace
baseada em ACKs/entrega ou rever a plataforma antes da fase de seleção.

#### Descoberta dinâmica experimental

`src/espnow_discovery_node.cpp` implementa uma prova isolada para ESP32 e
ESP8266. O node descobre até quatro endereços de rádio a partir dos beacons do
probe de gateway acima, cadastra peers, sonda cada candidato em rodízio e exige
três respostas consecutivas com sequência correspondente antes de selecioná-lo.
Mantém a seleção enquanto o candidato anuncia e responde; três falhas
consecutivas ou seis segundos sem beacon invalidam a seleção. Candidatos
expirados liberam o peer. Depois da perda, um candidato já confirmado pode ser
selecionado. Caso vários já estejam confirmados, desempata pelo endereço MAC;
não troca enquanto o atual continua válido. A latência é apenas registrada:
esta versão não escolhe o melhor sinal nem compara qualidade de enlace.

O protocolo de descoberta experimental v2 é independente do envelope de dados
`multi-gateway-protocol-v1.md`. Ele usa exatamente 13 bytes, codificados
explicitamente em little-endian:

| Offset | Tamanho | Campo |
| ---: | ---: | --- |
| 0 | 2 | magic ASCII `TS` |
| 2 | 1 | versão `2` |
| 3 | 1 | tipo: beacon `1`, probe `2`, reply `3` |
| 4 | 1 | canal |
| 5 | 4 | `network_id` |
| 9 | 4 | sequência |

Node e gateway rejeitam comprimento, magic, versão, tipo, canal ou rede
incompatíveis; `wrong_network` conta os quadros de outra rede. O valor padrão
de bancada é `0x54530101`, substituível em cada build por
`-DTERRASMART_DISCOVERY_NETWORK_ID=<valor>`. Ele não é segredo nem prova de
autorização. O formato v2 rejeita o antigo v1 de 9 bytes, que continua utilizado
somente pelo probe independente `espnow_rssi_probe.cpp`; para testar descoberta,
regravar node e gateways juntos. As evidências anteriores referem-se ao v1.
O endereço de rádio é a MAC ativa do gateway e ainda não há identidade de fábrica
no payload ou autenticação. Esta prova não deve ser integrada ao transporte de
produção nem usada para autorizar dispositivos. A associação é somente em RAM.
A fila de recepção tem capacidade
efetiva de 15 eventos e a tabela tem quatro entradas; descartes e tabela cheia
são contados. O callback copia eventos, e o loop faz a seleção e mantém peers.

Compilação e bancada:

```bash
pio run -e esp32dev_discovery_node_experimental
pio run -e d1_mini_discovery_node_experimental
pio run -e esp32dev_discovery_gateway_probe
pio run -e esp32dev_discovery_gateway_probe_2
pio run -e esp32dev_discovery_gateway_other_network
pio test -c platformio-native.ini -e native -f test_discovery_packet
```

Gravar os dois gateways em ESP32 distintos e o node experimental em uma terceira
placa; usar baud 115200 para ESP32 e 74880 para ESP8266. Observar `Candidate
discovered`, os contadores de beacons e respostas e `Selection: ... confirmed`.
Desligar o gateway selecionado e acompanhar expiração e troca para o outro;
religá-lo e verificar que não ocorre alternância enquanto o selecionado responde.
Os dois candidatos devem ter endereços ativos diferentes. Confirmar também
reboot, beacons e respostas fora de ordem e falhas sustentadas antes de usar os
resultados para implementar seleção em produção.

**Bancada v2 com rede incompatível:** D1 mini `EC:FA:BC:D4:E7:33`
(`ttyUSB3`) e gateway `...:01` (`ttyUSB0`) usaram rede `0x54530101`;
gateway `...:02` (`ttyUSB2`) usou `0x54530102`. O node selecionou 01 e
confirmou 30/30 sondagens, enquanto o contador `wrong_network` chegou a 29
e 02 não entrou na tabela. Com o probe de endereços fixos, sondagens enviadas
à rede incompatível não geraram respostas: sete timeouts para 02 versus oito
respostas de oito sondagens para 01; o gateway incompatível contabilizou
quadros de outra rede e zero sondagens válidas. A entrega dos quadros unicast
foi observada apenas de forma agregada no contador do gateway, que também recebe
beacons da rede diferente. Depois, com os dois gateways na mesma rede, o node
confirmou 15/15 sondagens para cada um; ao colocar 01 em reset, selecionou 02
e permaneceu nele após o retorno e confirmação de 01. Outros quadros inválidos
foram observados e não tiveram origem identificada. Testes nativos cobrem layout,
sequência, tamanho, magic, versão, tipo, canal e rede incompatível.

**Bancada de 05/11/2026:** os ambientes de node ESP32 e ESP8266 compilaram. O
ESP32 de MAC `38:3E:51:D3:E3:74` recebeu o node experimental, mas após mudança
de porta/cabeamento reiniciou repetidamente com `Brownout detector was triggered`
ao ligar o Wi-Fi. O gateway `D4:E9:F4:02:E5:EC` inicializou e anunciou no canal 1.
Assim, não há ainda evidência de descoberta, unicast ou failover deste novo
firmware. Verificar alimentação/cabo/hub do node e repetir com um terceiro
dispositivo funcional para validar os dois gateways simultaneamente; o D1 mini
também precisa passar pelo teste físico descrito acima.

**Nova bancada com três ESP32:** os três firmwares foram gravados e executados
no canal 1. A numeração abaixo é a observada durante este ensaio e pode mudar
após reconexão USB:

| Papel | Porta | MAC STA de fábrica | MAC de rádio ativa |
| --- | --- | --- | --- |
| Gateway 01 | `/dev/ttyUSB2` | `D4:E9:F4:02:E5:EC` | `AA:BB:CC:DD:EE:01` |
| Gateway 02 | `/dev/ttyUSB0` | `38:3E:51:D3:E3:74` | `AA:BB:CC:DD:EE:02` |
| Node experimental | `/dev/ttyUSB1` | `38:3E:51:D4:0E:80` | MAC de fábrica |

O node identificou os dois candidatos e recebeu respostas unicast com a
sequência esperada. Em um relatório da primeira execução, gateway 01 tinha 26
sondagens e 26 respostas, e gateway 02 tinha 38 sondagens e 38 respostas; ambos
tinham zero timeouts. O node reportou zero descartes de fila, respostas sem
correspondência, tabela cheia e erros de peer/envio. Houve um pacote inválido
no node, sem diagnóstico adicional de sua origem. Não inferir taxa de perda de
broadcast apenas a partir dessas contagens.

Com 02 selecionado, mantê-lo em reset por 20 segundos levou à expiração do
candidato e à seleção de 01, previamente confirmado. Após liberar 02, o node o
redescobriu e continuou selecionando 01 mesmo depois de três respostas válidas
do 02. Repetido no sentido contrário, com 01 em reset por 18 segundos, o node
passou a 02 e permaneceu nele após o retorno e confirmação de 01. Isto testa
indisponibilidade de rádio por reset, não todas as causas de falha de enlace.

O contador do gateway foi corrigido para distinguir beacons válidos de outro
gateway de quadros inválidos; também foi adicionada exclusão mútua à fila de
respostas entre o callback Wi-Fi e o loop do ESP32. Ambos os gateways foram
regravados e continuaram respondendo às sondagens. A segunda execução de
failover foi realizada após essa alteração. Outros quadros inválidos foram
observados nos gateways, sem identificação da origem; não foram tratados como
falhas de entrega das sondagens. Estes resultados validam somente a descoberta
e a troca direta do protocolo de diagnóstico entre ESP32, sem RSSI, tráfego FDRS,
MQTT, mesh, autenticação ou teste em ESP8266.

**Bancada posterior com D1 mini ESP8266:** o node experimental já gravado em
`/dev/ttyUSB3` iniciou com MAC STA `E8:DB:84:93:4F:E0`. Com o gateway ESP32 de
MAC de fábrica `D4:E9:F4:02:E5:EC` no endereço ativo `AA:BB:CC:DD:EE:01`, o
node registrou 215 sondagens e 215 respostas correspondentes, sem timeouts ou
descartes no relatório observado. Depois de regravar esse mesmo ESP32 como
gateway `AA:BB:CC:DD:EE:02`, o D1 mini também o descobriu e confirmou 20 de 20
sondagens, sem timeouts. O teste comprova comunicação bidirecional direta com
essa placa nos dois endereços, não com dois gateways simultaneamente.

Com os gateways ativos simultaneamente, o D1 mini recebeu somente o gateway
`D4:E9:F4:02:E5:EC`, mesmo com o outro gateway visível ao node ESP32. O mesmo
ocorreu ao trocar os endereços ativos das placas, e ao colocar o gateway
`D4:E9:F4:02:E5:EC` em reset: o D1 mini expirou a seleção, mas não recebeu
anúncios do outro ESP32 durante a janela observada. Não foi identificada a causa;
o usuário confirmou que as placas já estavam próximas, separadas e alimentadas
por USB durante o teste. O caminho de rádio ainda precisa ser investigado.
Não tratar essa observação como prova de incompatibilidade geral do ESP8266.
Faltam recepção simultânea dos dois candidatos no D1 mini, respostas unicast de
ambos e teste de failover antes de declarar esse cenário aprovado.

**Isolamento adicional:** o receptor simples `d1_mini_espnow_probe_receiver`,
sem tabela de peers no caminho de recepção, também observou somente o gateway
02, enquanto o node ESP32 observou ambos. Com `d1_mini_discovery_node_probe`,
que sonda os endereços fixos mesmo sem receber beacon, o ESP32 com MAC de
fábrica `38:3E:51:D4:0E:80` recebeu sondagens da MAC do D1 e tentou responder,
mas o D1 contabilizou 44 sondagens para ele e nenhuma resposta; para o outro
gateway registrou 44 respostas de 44 sondagens. No gateway com falha, os
callbacks de envio de resposta falharam enquanto o ESP32 de diagnóstico estava
em reset, indicando que o problema não se limita à tabela de descoberta. Uma
tentativa experimental de fixar a taxa ESP-NOW em 1 Mbps e reduzir a potência
de transmissão nesse ESP32 não restabeleceu a recepção no D1. Não há evidência
suficiente para concluir se a causa é hardware, configuração ou outra diferença
na camada de rádio. Os testes temporários de taxa/potência foram removidos; o D1
foi novamente gravado com o node experimental e os dois ESP32 ficaram com os
gateways de diagnóstico 01 (`ttyUSB1`) e 02 (`ttyUSB2`).

**Investigação adicional do enlace:** o problema foi reproduzido também com o
ESP32 de MAC `38:3E:51:D3:E3:74` como gateway, usando o mesmo firmware e MAC
ativa `AA:BB:CC:DD:EE:01`. O D1 continuou recebendo somente o gateway de MAC
de fábrica `D4:E9:F4:02:E5:EC`; ao mantê-lo em reset, o node expirou sua
seleção e não descobriu o outro gateway durante 26 segundos. Num ensaio isolado
em canal 6, o D1 em monitor de rádio temporário não detectou quadros da placa
`38:3E:51:D4:0E:80`, enquanto um monitor ESP32, no mesmo canal, detectou seus
quadros. Esses monitores serviram apenas para diagnóstico; não foram incorporados
ao firmware. Os testes com MAC de fábrica, taxa de 1 Mbps e potência reduzida
não produziram recepção no D1. A causa raiz continua indeterminada: os resultados
apontam para o caminho de rádio, mas não permitem distinguir defeito de placa,
interferência ou incompatibilidade de configuração. Não houve correção validada;
permanecem pendentes recepção bidirecional e failover do D1 com dois gateways.

**Comparação com um segundo D1 mini:** gravou-se o mesmo ambiente
`d1_mini_discovery_node_experimental` no novo D1, MAC de fábrica
`EC:FA:BC:D4:E7:33`, conectado em `/dev/ttyUSB4`. Os dois D1 permaneceram
ligados simultaneamente no canal 1, assim como os gateways de diagnóstico
`AA:BB:CC:DD:EE:01` (`/dev/ttyUSB0`, MAC de fábrica
`38:3E:51:D3:E3:74`) e `AA:BB:CC:DD:EE:02` (`/dev/ttyUSB2`, MAC de fábrica
`D4:E9:F4:02:E5:EC`). As portas refletem somente esta sessão.

O novo D1 identificou os dois gateways e confirmou sondagens unicast com ambos:
em um relatório, 22 sondagens e 22 respostas por gateway, zero timeouts, quadros
inválidos, descartes de fila, respostas não correspondentes e erros de peer ou
envio. O D1 anterior, `E8:DB:84:93:4F:E0`, continuou observando somente o
gateway 02. Os dois receivers tinham o mesmo firmware e usavam posições e
conexões USB distintas; a comparação não determina qual diferença do enlace
causou o resultado.

No novo D1, manter o gateway 02 em reset por 22 segundos provocou seleção do
gateway 01, previamente confirmado; após o retorno de 02, o node o redescobriu
e permaneceu em 01. Repetir com 01 em reset por 22 segundos provocou seleção de
02, mantida após o retorno de 01. Durante a interrupção de 02, o D1 anterior
expirou sua seleção e permaneceu sem gateway até 02 voltar, quando confirmou
novamente o enlace. Os resultados validam descoberta e failover do **novo** D1
em bancada; não demonstram uma correção para o D1 anterior. Para isolar sua
falha, comparar as placas mantendo sua posição física e alternando as portas/cabos
USB entre elas. Em 05/11/2026, `/dev/ttyUSB3` identificou o D1 antigo
`E8:DB:84:93:4F:E0`; `/dev/ttyUSB4` identificou o novo D1
`EC:FA:BC:D4:E7:33`. Os gateways estavam em `/dev/ttyUSB0` (MAC de fábrica
`38:3E:51:D3:E3:74`, ativo `...:01`) e `/dev/ttyUSB2` (MAC de fábrica
`D4:E9:F4:02:E5:EC`, ativo `...:02`).

Foi feito um teste cruzado sem mover nenhum D1: os dois ESP32 mantiveram
beacons, e o D1 antigo continuou sem descobrir o gateway 01, enquanto o novo
D1 recebeu anúncios e respostas dos dois. Isolando os gateways pelo reset:

- Com gateway 01 disponível e 02 em reset, o novo D1 selecionou 01 e registrou
  25 respostas de 25 sondagens. O D1 antigo não recebeu anúncio 01.
- Com gateway 02 disponível e 01 em reset, o D1 antigo selecionou 02 e
  registrou 24/24 respostas; o novo D1 também manteve comunicação com 02.
- Com 01 e 02 ativos, o D1 antigo recebeu 25/25 respostas de 01 e 24/24 de 02
  na janela registrada, selecionou 01 e não alternou. Assim, a falha que parecia
  acompanhar sempre o gateway 01 não se reproduziu nesse ciclo controlado.

O resultado inicial do D1 antigo pode depender de uma condição transitória, mas
este teste não a identifica. A comparação controlada conclui que ambos os D1
podem descobrir e responder aos dois gateways na bancada; não identifica a
causa do comportamento intermitente anterior. A próxima investigação deve
repetir reinícios de ambos os gateways e do D1 antigo mantendo as placas imóveis,
registrando logs desde o boot e comparando a estabilidade da recepção.

### Fase 1 — identidade e compatibilidade de rádio

**Hardware:** os dois ESP32 gateways e um D1 mini folha.

**Implementação:**

- definir IDs lógicos únicos e persistentes para os dois gateways e identidade
  estável para nodes;
- especificar tamanho, versão e campos das mensagens de anúncio e controle;
- confirmar a API de recepção ESP-NOW em cada core e se o callback do ESP32
  fornece RSSI da mensagem recebida;
- verificar explicitamente a limitação do callback ESP-NOW usado no ESP8266:
  se ele não expuser RSSI do quadro, comparar opções suportadas antes de
  prometer seleção exata pelo sinal no Wemos D1 mini;
- fazer um firmware de bancada que apenas recebe anúncios e registra MAC,
  canal, versão e RSSI, sem modificar o fluxo FDRS em produção;
- validar comprimento de pacote e capacidade do ESP-NOW peer table nas versões
  de core efetivamente fixadas.

**Testes:**

1. Os dois gateways anunciam IDs diferentes e estáveis após vários reboots.
2. O D1 mini recebe os anúncios repetidamente, sem crash ou watchdog reset.
3. Variar a distância/atenuação e comparar RSSI com a mudança física observada.
4. Repetir a recepção com ESP32 e D1 mini para confirmar os metadados
   disponíveis em ambas as plataformas.

**Aceitação para avançar:** identidade não colide e está documentado como obter
uma medida comparável de qualidade do enlace no Wemos D1 mini. A implementação
confirmou por assinatura do core que os callbacks atuais não entregam RSSI, mas a
compatibilidade de rádio ainda requer execução física dos testes acima. Não
anunciar seleção exata pelo maior RSSI no D1 mini até definir e validar uma
alternativa suportada. Opções a avaliar são uma API documentada de recepção,
medição de sucesso/tempo de ACK ou migração dos nodes para uma placa cuja API
exponha RSSI. A PR 5 depende do resultado de bancada e da decisão dessa métrica.

### Fase 2 — descoberta e seleção direta, sem mesh

**Hardware:** dois ESP32 gateways, ambos no canal 1, e um D1 mini sensor.

**Implementação:**

- adicionar anúncio versionado aos gateways;
- no D1 mini, coletar vários anúncios numa janela de descoberta e manter uma
  tabela pequena de candidatos;
- filtrar várias amostras RSSI (mediana ou método simples equivalente);
- escolher o gateway direto de maior RSSI;
- confirmar por mensagem unicast e só então configurar/registrar o transporte
  FDRS para esse gateway;
- durante essa fase, manter opção de build ou modo de fallback ao gateway fixo.

**Testes:**

1. Ambos os gateways próximos: o node escolhe o de sinal medido mais forte.
2. Atenuar/deslocar o gateway atual: após a janela de descoberta, escolhe o
   outro.
3. Sinais próximos: várias execuções não provocam alternância a cada anúncio.
4. Gateway incompatível (versão/rede diferente) é ignorado.
5. Gateway desligado antes da confirmação: não persiste a seleção inválida.
6. Comparar tempo de descoberta e sucesso de envio com o baseline da Fase 0.

**Aceitação para avançar:** seleção reproduzível do melhor gateway diretamente
visível, confirmação unicast obrigatória e nenhuma regressão persistente no
envio das leituras.

### Fase 3 — persistência e recuperação da associação direta

**Hardware:** mesma bancada da Fase 2.

**Implementação:**

- persistir somente a identidade/endereço/canal da seleção confirmada;
- versionar o registro salvo e validar checksum/magic antes de utilizá-lo;
- no boot, tentar o último gateway por número e duração limitados de tentativas;
- se não confirmar, abrir descoberta e escolher o melhor gateway disponível;
- gravar a nova seleção depois do sucesso, protegendo flash contra escrita
  excessiva;
- não apagar a associação por uma falha transitória isolada.

**Testes:**

1. Reiniciar o node com gateway salvo presente: recupera comunicação sem espera
   pela janela completa de descoberta, se possível.
2. Inicializar node antes dos gateways: descobre e registra quando os gateways
   começam a anunciar.
3. Desligar o gateway salvo: migra ao segundo gateway compatível.
4. Restaurar gateway antigo: histerese evita retorno imediato se o node já
   confirmou outra associação.
5. Interromper energia durante gravação simulada: configuração inválida é
   detectada e ocorre descoberta segura.

**Aceitação para avançar:** reboot e ordem de inicialização não exigem reset ou
reconfiguração manual.

### Fase 4 — identidade de origem e envelope de mensagens

**Hardware:** dois D1 mini sensores, ESP32 gateways e gateway MQTT D1 mini.

**Implementação:**

- definir um envelope versionado com rede, origem, destino, sequência, tipo,
  tamanho e payload FDRS;
- transportar o identificador estável do node independente do ID de leitura;
- implementar limite de tamanho e validação antes de copiar payload;
- integrar o envelope primeiro no caminho direto, sem encaminhadores;
- atualizar o formato UART/MQTT de modo versionado, preservando suporte legado
  durante a migração;
- mapear node + ID de sensor para entidades estáveis no Home Assistant.

**Testes:**

1. Dois nodes usam o mesmo ID de leitura e aparecem como origens distintas.
2. Pacote truncado, tamanho excessivo ou versão desconhecida é descartado.
3. Mensagens duplicadas não geram publicação duplicada quando a política
   deduplicar por sequência.
4. Testar firmware novo com o caminho gateway MQTT e conferir payloads,
   comandos e automações existentes.

**Aceitação para avançar:** a origem permanece identificável ponta a ponta e a
compatibilidade de payload está definida/testada.

### Fase 5 — roteador ESP32 de um salto

**Hardware:** um D1 mini folha fora do alcance do gateway; um ESP32 sempre ligado
como roteador; um ESP32 gateway ESP-NOW; gateway MQTT D1 mini.

**Implementação:**

- dar ao ESP32 roteador papel explícito e ID único;
- anunciar gateway alcançável e próximo salto ao node folha;
- implementar encaminhamento de dados por exatamente um roteador;
- adicionar ACK por salto, timeout e tentativas limitadas;
- incluir sequência, cache finito de duplicatas e TTL/hop-limit;
- manter gateway e roteador no canal 1 durante o MVP;
- não permitir que o Wemos sensor em deep sleep seja usado como roteador.

**Testes:**

1. Confirmar que o node folha não alcança diretamente o gateway, mas ambos
   alcançam o roteador.
2. Enviar dados e confirmar origem, sequência e conteúdo no broker.
3. Desligar o roteador durante transmissão: pacote falha de forma limitada,
   sem travar ou reiniciar o node.
4. Reenviar o mesmo pacote: gateway/roteador não criam leituras duplicadas
   segundo a política definida.
5. Forçar TTL zero e pacote malformado: ambos são descartados.
6. Confirmar que rota direta passa a ser preferida quando o gateway torna-se
   alcançável e a política de seleção assim indicar.

**Aceitação para avançar:** tráfego passa por um salto com origem preservada,
sem loops, tempestade de retransmissão ou bloqueio do loop principal.

### Fase 6 — múltiplos saltos e recuperação de rota

**Hardware:** D1 mini folha, dois ESP32 roteadores e pelo menos um ESP32 gateway.

**Implementação:**

- propagar anúncios de rota com sequência/época do gateway, next-hop, custo,
  saltos e timeout;
- limitar o número máximo de saltos e tamanho das tabelas;
- selecionar caminho válido com regra determinística e histerese;
- detectar next-hop inválido por falhas consecutivas e expirar rotas antigas;
- testar caminho alternativo antes de habilitar mais de dois saltos;
- encaminhar comandos do gateway ao node, incluindo ACK e expiração.

**Testes:**

1. Topologia folha → roteador A → roteador B → gateway entrega telemetria.
2. Desligar A ou B e confirmar expiração e seleção de rota alternativa, quando
   houver caminho redundante.
3. Criar possibilidade de ciclo e verificar que TTL/deduplicação interrompem
   o tráfego.
4. Reiniciar roteador e gateway em ordens diferentes e medir convergência.
5. Enviar comando para node remoto; interromper caminho de retorno e verificar
   erro/expiração visível, sem repetição infinita.
6. Medir taxa de entrega, latência, uso de RAM, peers e ocupação do canal sob
   tráfego representativo.

**Aceitação para avançar:** a mesh recupera-se dentro do tempo definido, respeita
limites de recursos e não mantém rotas inválidas.

### Fase 7 — monitoramento e endurecimento

**Hardware:** topologia completa com nodes folha, gateways ESP32, roteadores
ESP32 e gateway MQTT Wemos D1 mini.

**Implementação:**

- exportar node ID, gateway final, saltos, última recepção e estado de
  disponibilidade;
- adicionar contadores de RSSI, ACK perdido, retransmissões, troca/expiração de
  rota e descarte por TTL/deduplicação;
- definir timeouts de disponibilidade maiores que o intervalo normal de envio e
  compatíveis com deep sleep;
- revisar autenticação, provisionamento, proteção contra replay, tamanhos e
  limites de taxa;
- documentar atualização coordenada de nodes, roteadores e gateways e o caminho
  de rollback.

**Testes:**

1. Gateway MQTT offline: rádio local continua roteando segundo a política
   definida; recuperação MQTT não duplica indevidamente dados.
2. Node desligado: Home Assistant marca somente esse node indisponível.
3. Gateway ESP-NOW desligado: nodes afetados descobrem outro gateway/rota;
   estado do gateway MQTT permanece independente.
4. Interferência e RSSI variável: rotas não oscilam continuamente.
5. Reboot coletivo após falta de energia: a rede converge sem pareamento manual.
6. Mensagens não autorizadas, replay e flood controlado são rejeitados sem
   afetar o restante da mesh.

**Aceitação final:** critérios de disponibilidade, segurança, recuperação,
identidade e carga de rádio são atendidos na instalação piloto.

### Artefatos esperados por fase

Para cada fase, manter no repositório:

- decisão/protocolo ou mudança documentada;
- firmware e configuração de build identificados por commit;
- roteiro do teste de bancada com topologia e posicionamento;
- logs seriais relevantes e payloads MQTT de evidência (sem senhas/chaves);
- tabela de resultado: passou/falhou, defeito encontrado e ação necessária;
- critério explícito de rollback para a versão estável anterior.

Não avançar se o critério da fase não passar; corrigir e repetir apenas os testes
afetados e os testes de regressão do fluxo sensor → Home Assistant.

## 14. Matriz mínima de testes

| Cenário | Resultado esperado |
| --- | --- |
| Um node e um gateway | Descoberta, registro e dados corretos |
| Dois gateways diretos com RSSI diferente | Seleciona o de sinal mais forte após filtrar amostras; se RSSI indisponível, usa a métrica de enlace definida para a plataforma |
| RSSI dos gateways muito próximo | Não alterna continuamente; aplica histerese/desempate |
| Gateway selecionado desligado | Descobre e confirma outro gateway |
| Um roteador entre node e gateway | Dados chegam com origem preservada e `hops` correto |
| Caminhos redundantes e ciclo possível | Sem loop; TTL e deduplicação descartam repetidos |
| Roteador reinicia durante envio | Rota expira e tráfego usa alternativa ou é reportado como falho |
| Gateway reinicia | Nodes refazem descoberta/registro sem reiniciar manualmente |
| MQTT indisponível | Mesh local continua operando conforme capacidade definida |
| Mensagem malformada/não autorizada | É descartada sem travar ou cadastrar o emissor |
| Node em deep sleep | É tratado como folha e não como roteador disponível |
| IDs de sensores repetidos em nodes diferentes | Origem mantém entidades distintas no Home Assistant |

## 15. Critérios de aceitação

- Um node novo encontra gateways autorizados sem depender de um único
  `GTWY_MAC` compilado.
- Entre gateways diretamente visíveis e compatíveis, escolhe o de RSSI recebido
  mais forte quando medição confiável estiver disponível, ou usa a métrica de
  enlace validada e documentada para a plataforma.
- A seleção é confirmada por comunicação unicast antes de ser persistida.
- Após reboot, tenta a última rota válida e redescobre se ela estiver fora do ar.
- Um node sem gateway direto consegue alcançar um gateway por roteador sempre
  ligado.
- Um roteador/gateway indisponível não causa loop nem exige reboot manual dos
  outros dispositivos.
- Nodes não oscilam entre rotas por pequenas mudanças no RSSI.
- Nodes e leituras permanecem identificáveis no MQTT/Home Assistant através de
  gateways e saltos diferentes.
- Dispositivos não autorizados não conseguem se passar por gateway ou inserir
  comandos/dados aceitos pela mesh.
- Limites de memória, peers e filas são respeitados nas placas alvo.

## 16. Premissas acordadas e decisões ainda necessárias

### Premissas acordadas para o plano

- gateways que participam da rede ESP-NOW usam ESP32;
- gateway MQTT separado usa Wemos D1 mini/ESP8266;
- cada gateway ESP-NOW tem seu próprio gateway MQTT, ligado por UART; todos os
  gateways MQTT conectam-se ao mesmo broker;
- sensores/nodes folha usam Wemos D1 mini/ESP8266;
- roteadores do MVP usam ESP32 dedicado, alimentado continuamente;
- todos os rádios usam canal 1 durante o MVP;
- seleção entre gateways diretamente visíveis busca o sinal mais forte, com
  filtragem e histerese;
- validar descoberta direta antes de considerar múltiplos saltos; mesh é uma
  iniciativa posterior e não requisito para a primeira migração.

### Decisões a fechar antes das fases correspondentes

1. Qual identidade global de node e gateway será usada e como será provisionada?
2. Como será autenticada a entrada de um dispositivo novo e como serão
   armazenadas/rotacionadas as chaves?
3. Qual formato MQTT versionado carregará node, gateway, sensor e sequência,
   preservando as automações existentes durante a migração?
4. Como direcionar comandos e OTA ao par gateway correto quando todos assinam
   tópicos no mesmo broker?
5. Como obter RSSI/qualidade comparável no D1 mini ESP8266 e nos ESP32 com os
   cores selecionados?
6. Quais RSSI, histerese e timeout são adequados após testes no local?
7. (Para uma fase futura de mesh) Qual máximo de saltos e comportamento de
   comandos se a rota de retorno desaparecer?

As recomendações deste documento são ponto de partida; números de intervalos,
limiares e capacidade devem ser validados em hardware e no local de instalação.
