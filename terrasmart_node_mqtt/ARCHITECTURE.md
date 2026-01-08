# Terrasmart Add-on - Estrutura Modular

## 📁 Estrutura de Diretórios

```
terrasmart_node_mqtt/
├── src/
│   ├── config/              # Configurações
│   │   ├── constants.js     # Constantes da aplicação
│   │   ├── index.js         # Carregador de configuração
│   │   └── sensorConfigs.js # Configurações de sensores (HA Discovery)
│   │
│   ├── utils/               # Utilitários
│   │   ├── logger.js        # Logger centralizado
│   │   └── espRegistry.js   # Gerenciador de dispositivos ESP
│   │
│   ├── services/            # Serviços
│   │   ├── mqttService.js   # Gerenciador de conexão MQTT
│   │   ├── discoveryService.js  # MQTT Discovery (Home Assistant)
│   │   └── httpServer.js    # Servidor HTTP (keep-alive/API)
│   │
│   └── handlers/            # Processadores de mensagens
│       ├── telemetryHandler.js  # Processa telemetria dos ESPs
│       ├── statusHandler.js     # Processa status dos ESPs (com debounce)
│       ├── commandHandler.js    # Processa comandos
│       └── messageRouter.js     # Roteia mensagens MQTT
│
├── index-new.js             # Novo ponto de entrada (refatorado)
├── index.js                 # Ponto de entrada atual (legado)
├── package.json
└── README.md
```

## 🎯 Responsabilidades dos Módulos

### 📦 config/
Gerencia todas as configurações e constantes da aplicação.

- **constants.js**: Todas as constantes em um único lugar
- **index.js**: Carrega configuração do arquivo JSON
- **sensorConfigs.js**: Mapeamento de tipos de sensores para Home Assistant

### 🔧 utils/
Utilitários reutilizáveis em toda a aplicação.

- **logger.js**: Logger estruturado em JSON
- **espRegistry.js**: Gerencia o registro de dispositivos ESP (singleton)

### 🚀 services/
Serviços principais da aplicação.

- **mqttService.js**: Gerencia conexão, publicação e inscrição MQTT
- **discoveryService.js**: Publica configurações de descoberta automática
- **httpServer.js**: Servidor HTTP com rotas de status e API

### 🎮 handlers/
Processadores de mensagens e lógica de negócio.

- **telemetryHandler.js**: Processa telemetria dos sensores
- **statusHandler.js**: Processa mudanças de status (com debounce)
- **commandHandler.js**: Processa comandos recebidos
- **messageRouter.js**: Roteia mensagens para handlers apropriados

## 🚀 Como Usar

### Testar a Nova Estrutura

```bash
# Usar o novo código refatorado
node index-new.js
```

### Migrar para a Nova Estrutura

Quando estiver satisfeito com os testes:

```bash
# Backup do código antigo
mv index.js index-old.js

# Ativar novo código
mv index-new.js index.js
```

## ✅ Vantagens da Nova Estrutura

### 1. **Separação de Responsabilidades**
Cada módulo tem uma única responsabilidade clara.

### 2. **Facilidade de Manutenção**
- Bugs são mais fáceis de localizar
- Mudanças são isoladas em módulos específicos
- Código mais testável

### 3. **Reutilização**
- Serviços podem ser usados em múltiplos lugares
- Evita duplicação de código

### 4. **Escalabilidade**
- Fácil adicionar novos handlers
- Fácil adicionar novos tipos de sensores
- Fácil adicionar novos comandos

### 5. **Legibilidade**
- Código mais fácil de entender
- Documentação inline clara
- Estrutura intuitiva

## 📝 Exemplos de Uso

### Adicionar um Novo Tipo de Sensor

1. Edite `src/config/sensorConfigs.js`
2. Adicione a configuração do sensor no objeto `configs`

```javascript
novo_sensor: {
  domain: "sensor",
  friendlyName: "Novo Sensor",
  device_class: "measurement",
  unit_of_measurement: "unit",
}
```

### Adicionar um Novo Comando

1. Edite `src/handlers/commandHandler.js`
2. Adicione um novo case no switch

```javascript
case "meu_comando":
  this._handleMeuComando();
  break;
```

### Adicionar uma Nova Rota HTTP

1. Edite `src/services/httpServer.js`
2. Adicione a rota no método `_setupRoutes()`

```javascript
this.app.get("/nova-rota", (req, res) => {
  res.json({ message: "Nova rota" });
});
```

## 🔄 Funcionalidades Mantidas

✅ Todas as funcionalidades do código original foram mantidas:
- Processamento dinâmico de telemetria
- MQTT Discovery automático
- Debounce de status
- Suporte a múltiplos sensores
- Compatibilidade legado
- Heartbeat
- Shutdown gracioso
- API HTTP

## 📊 Rotas HTTP Disponíveis

- `GET /` - Status geral do add-on
- `GET /health` - Health check
- `GET /devices` - Lista de dispositivos ESP conectados

## 🐛 Debugging

O logger centralizado facilita o debugging:

```javascript
logger.info("Mensagem", { dados: "extras" });
logger.error("Erro", { error: err.message });
logger.debug("Debug", { detalhes: "..." });
```

## 🔐 Boas Práticas Implementadas

- ✅ Singleton para serviços compartilhados
- ✅ Injeção de dependências implícita
- ✅ Separação clara de concerns
- ✅ Nomes descritivos e consistentes
- ✅ Documentação inline (JSDoc)
- ✅ Tratamento de erros
- ✅ Logs estruturados
