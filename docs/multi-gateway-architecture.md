# Arquitetura de Múltiplos Gateways

## Objetivo

Distribuir vários gateways ESP-NOW pela propriedade, formando pequenas áreas
de cobertura. Cada node deve ser atendido pelo gateway mais adequado ao local,
mesmo que ocasionalmente consiga ouvir mais de um gateway.

O gateway será o **coordenador local** da sua rede de rádio. Um servidor,
Home Assistant ou gateway MQTT poderá atuar como coordenador global para
cadastro, configuração e visualização, mas não será necessário para o node
entrar na rede local.

## Arquitetura prevista

```text
Nodes de sensores -- ESP-NOW --> Gateway local/coordenador
                                      |
                                      +--> MQTT/UART --> servidor central
```

Cada gateway será responsável por:

- anunciar a própria presença;
- responder a solicitações de descoberta;
- registrar os nodes que escolheram a rede;
- enviar configurações aos nodes;
- receber e encaminhar as leituras;
- manter o vínculo local durante falhas temporárias do servidor central.

## Identidade da rede

O FDRS atual não usa diretamente o MAC físico da placa para endereçamento.
Ele monta um endereço com `MAC_PREFIX` e o byte final configurado em
`UNIT_MAC` ou `GTWY_MAC`.

Regras:

- todos os dispositivos da mesma rede lógica devem usar o mesmo `MAC_PREFIX`;
- cada gateway deve ter um `UNIT_MAC` diferente;
- `GTWY_MAC` não deve continuar fixo no firmware quando a descoberta for
  implementada;
- o endereço físico original da placa deve continuar disponível como
  identificador de hardware;
- gateways que não devem pertencer à mesma rede podem usar outro prefixo;
- não reutilizar o mesmo endereço lógico em gateways que possam se ouvir.

Exemplo:

```text
Gateway reservatório norte: MAC_PREFIX + 0x01
Gateway reservatório sul:   MAC_PREFIX + 0x02
Gateway estufa:             MAC_PREFIX + 0x03
```

O `UNIT_MAC` será atribuído uma vez durante o cadastro do gateway. Ele não
deve ser gerado aleatoriamente a cada inicialização.

## Canal ESP-NOW

ESP-NOW opera no canal de rádio atual. Um node não consegue descobrir gateways
em canais diferentes simultaneamente.

A solução prevista é:

- definir uma lista pequena de canais permitidos, inicialmente 1, 6 e 11;
- o node alternar entre esses canais durante a descoberta;
- cada gateway anunciar o canal usado;
- depois da seleção, o node permanecer no canal do gateway escolhido;
- gateways fisicamente próximos preferencialmente usarem canais diferentes;
- a configuração do canal ser persistida junto com o gateway selecionado.

Se todos os gateways tiverem cobertura sem sobreposição, pode-se usar o mesmo
canal e a descoberta será mais simples. Em áreas com sobreposição, canais
diferentes reduzem a interferência, mas tornam a descoberta mais lenta.

## Processo de descoberta

O mecanismo será implementado fora do fluxo atual de `GTWY_MAC` fixo.

### Primeiro provisionamento

1. O node inicia em modo de descoberta.
2. Ele transmite uma solicitação broadcast no canal atual.
3. Cada gateway compatível responde com um anúncio.
4. O node coleta as respostas por uma janela curta de tempo.
5. O node valida a rede, a versão do protocolo e a capacidade do gateway.
6. O node faz ping ou registro unicast no candidato escolhido.
7. O gateway confirma o vínculo e envia a configuração.
8. O node salva a seleção em memória não volátil.

### Inicializações seguintes

1. O node tenta primeiro o gateway salvo e o canal salvo.
2. Se receber confirmação, continua usando esse gateway.
3. Se o gateway não responder após tentativas limitadas, inicia nova
   descoberta.
4. O node não deve trocar de gateway por causa de uma única perda de pacote.

O anúncio deve conter, no mínimo:

- identificador da rede;
- endereço lógico do gateway;
- canal ESP-NOW;
- versão do protocolo;
- prioridade ou local do gateway;
- capacidade/carga atual, se disponível;
- nonce ou identificador da solicitação de descoberta.

## Regra de seleção do gateway

Quando mais de um gateway responder, o node deve selecionar nesta ordem:

1. Gateway autorizado para o local ou perfil do node, se houver essa regra.
2. Gateway salvo anteriormente, enquanto estiver disponível.
3. Gateway com rede e versão compatíveis.
4. Gateway com melhor qualidade de enlace, usando RSSI e confirmação de
   ping/registro.
5. Gateway com menor carga, se essa informação estiver disponível.
6. Maior prioridade configurada.
7. Menor endereço lógico como desempate determinístico.

O RSSI isolado não deve ser a única decisão. A seleção final precisa ser
confirmada por uma comunicação unicast bem-sucedida.

### Histerese e estabilidade

Para evitar que um node oscile entre dois gateways próximos:

- manter o gateway atual enquanto o enlace estiver aceitável;
- só trocar após várias falhas consecutivas;
- exigir que o novo candidato seja significativamente melhor;
- impor um tempo mínimo entre trocas;
- persistir o novo vínculo somente após confirmação.

## Identidade dos nodes e leituras

O endereço de rádio do node e o ID da leitura são conceitos diferentes.

O endereço físico/chip ID pode identificar a placa, mas o FDRS atual transporta
as leituras principalmente por `id`, `type` e `data`. Portanto, repetir os
mesmos IDs de leitura em vários nodes pode causar colisão no sistema central.

Antes de habilitar muitos nodes iguais, será necessário escolher uma destas
estratégias:

- atribuir IDs de leitura globalmente únicos durante o provisionamento;
- incluir o identificador do node no envelope encaminhado pelo gateway;
- criar um cadastro central que traduza node + sensor para um ID único.

Para o node de duas boias, os canais mínimo e máximo continuam sendo sensores
distintos. A configuração atual planejada usa `LEVEL_T`, com IDs 2 e 3 para o
protótipo. Esses IDs deverão ser atribuídos por node quando houver mais de uma
unidade instalada.

## Cadastro e configuração

O gateway deve aceitar um node novo somente durante uma janela de
provisionamento ou quando a rede estiver configurada para cadastro aberto.

Configurações candidatas a serem enviadas pelo gateway:

- endereço lógico e canal do gateway;
- intervalo de transmissão;
- IDs das leituras;
- perfil do node e dos sensores;
- polaridade das entradas;
- debounce das boias;
- uso de deep sleep;
- versão e parâmetros de atualização;
- nome/local do node.

O node deve armazenar a configuração recebida em memória não volátil e manter
uma versão/configuração válida anterior para recuperação.

## Segurança

O broadcast de descoberta não deve, sozinho, dar acesso permanente à rede.
Antes da implementação em campo, definir:

- identificador da rede que não seja apenas o `MAC_PREFIX`;
- token de provisionamento ou botão físico de pareamento;
- autenticação da resposta do gateway;
- criptografia ESP-NOW, se suportada pela implementação escolhida;
- rejeição de gateways desconhecidos após o provisionamento.

Isso é importante porque um dispositivo externo pode responder a um broadcast
e tentar atrair nodes para uma rede falsa.

## Limitações do FDRS atual

A versão usada no projeto atualmente:

- monta `gatewayAddress` estaticamente com `GTWY_MAC`;
- registra o gateway no ESP-NOW durante `beginFDRS()`;
- possui mensagens de ping e registro, mas não possui descoberta inicial;
- não expõe uma API para trocar o gateway em tempo de execução;
- usa `MAC_PREFIX` fixo no arquivo da biblioteca;
- não resolve sozinha a unicidade dos IDs das leituras entre nodes.

Para implementar a arquitetura, será necessário adicionar um pequeno protocolo
de descoberta e uma camada de configuração dinâmica no projeto ou em um fork
controlado do FDRS.

## Ordem de implementação

1. Corrigir e validar a identidade única de cada gateway.
2. Definir canais e localização/prioridade dos gateways.
3. Criar mensagens de descoberta e anúncio.
4. Tornar o endereço e o canal do gateway mutáveis no node.
5. Implementar ping, registro e confirmação do vínculo.
6. Persistir configuração no node.
7. Implementar seleção com RSSI, prioridade e histerese.
8. Implementar IDs globais ou envelope com identidade do node.
9. Adicionar autenticação ao provisionamento.
10. Testar com gateways próximos e perda/intermitência de sinal.

## Critérios de aceitação

- Um node novo encontra um gateway compatível sem `GTWY_MAC` compilado.
- Um node próximo de dois gateways escolhe sempre o candidato definido pela
  regra de seleção.
- O node não alterna de gateway em pequenas variações de sinal.
- Após reiniciar, o node recupera o gateway e o canal salvos.
- Após desligar o gateway salvo, o node encontra outro gateway autorizado.
- Dois gateways próximos não usam o mesmo endereço lógico.
- Leituras de nodes diferentes permanecem identificáveis no sistema central.
