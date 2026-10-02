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
separadamente facilita isolar falhas.

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

1. Gravar `esp32dev_espnow_probe_gateway_1` no primeiro ESP32 e
   `esp32dev_espnow_probe_gateway_2` no segundo.
2. Gravar `d1_mini_espnow_probe_receiver` no Wemos D1 mini que será o node.
3. Abrir monitores seriais a 115200 baud. Cada ESP32 deve exibir o ID do
   anúncio, a sequência transmitida e o resultado do callback de envio.
4. O Wemos deve exibir a quantidade de pacotes recebidos, ID/sequência e MAC de
   origem. Repetir mudando distância, posição e obstruções, e guardar os logs.
5. Repetir com cada transmissor individualmente e com os dois ativos.

O callback ESP-NOW fornecido pelo core ESP8266 atualmente fixado recebe somente
MAC, dados e comprimento; não inclui RSSI. O callback da versão ESP32 usada pelo
projeto também não inclui RSSI. Por isso, o firmware de diagnóstico valida
descoberta, recepção e identidade dos anúncios, mas reporta RSSI como indisponível.
Não interpretar o status do callback de envio do transmissor como RSSI ou como
confirmação de que o receptor processou o anúncio.

O resultado desta primeira prova deve registrar separadamente se a camada de
recepção/monitor do rádio permite associar RSSI de forma confiável a cada
anúncio ESP-NOW nas placas reais. Não adotar modo promiscuous, layout privado de
metadados do SDK ou API não documentada no firmware de produção sem uma prova
isolada de coexistência e estabilidade. Se RSSI não puder ser obtido de forma
suportada no D1 mini, revisar a escolha de gateway para uma métrica de enlace
baseada em ACKs/entrega ou rever a plataforma antes da fase de seleção.

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
uma medida comparável de qualidade do enlace no Wemos D1 mini. O callback
ESP-NOW legado do ESP8266 pode não entregar RSSI do quadro diretamente; se isso
se confirmar no core escolhido, não anunciar seleção exata pelo maior RSSI até
definir e validar uma alternativa suportada. Opções a avaliar são adaptação da
camada de recepção, medição de sucesso/tempo de ACK ou migração dos nodes para
uma placa cuja API exponha RSSI.

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
| Dois gateways diretos com RSSI diferente | Seleciona o de sinal mais forte após filtrar amostras |
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

- Um node novo encontra gateways autorizados sem `GTWY_MAC` compilado.
- Entre gateways diretamente visíveis e compatíveis, escolhe o de RSSI recebido
  mais forte segundo a janela de medição definida.
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
- sensores/nodes folha usam Wemos D1 mini/ESP8266;
- roteadores do MVP usam ESP32 dedicado, alimentado continuamente;
- todos os rádios usam canal 1 durante o MVP;
- seleção entre gateways diretamente visíveis busca o sinal mais forte, com
  filtragem e histerese;
- primeiro validar um salto antes de habilitar múltiplos saltos.

### Decisões a fechar antes das fases correspondentes

1. Qual identidade global de node e gateway será usada e como será provisionada?
2. Como será autenticada a entrada de um dispositivo novo e como serão
   armazenadas/rotacionadas as chaves?
3. Um ou vários gateways ESP-NOW podem convergir para o mesmo gateway MQTT, e
   como evitar duplicação se ambos receberem a mesma mensagem?
4. Qual formato MQTT versionado carregará node, sensor, gateway, sequência e
   saltos sem quebrar automações existentes?
5. Como obter RSSI/qualidade comparável no D1 mini ESP8266 e nos ESP32 com os
   cores selecionados?
6. Quais RSSI, histerese, timeout e máximo de saltos são adequados após testes
   no local?
7. Qual é o comportamento de comandos se a rota de retorno desaparecer?

As recomendações deste documento são ponto de partida; números de intervalos,
limiares e capacidade devem ser validados em hardware e no local de instalação.
