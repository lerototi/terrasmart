# 🔧 Guia de Debug - Portal Captivo

## O que foi corrigido:

### 1. **Configuração Estática do IP**
- IP do AP: `192.168.4.1`
- Gateway: `192.168.4.1`
- Subnet: `255.255.255.0`

### 2. **DNS Server Corrigido**
- Agora captura **TODAS** as requisições DNS
- Redireciona para o IP do AP automaticamente

### 3. **Rotas Adicionadas**
- `/` - Portal principal
- `/save` - POST para salvar config
- `/generate_204` - Android (detecção de portal)
- `/fwlink` - Windows (detecção de portal)
- `/connecttest.txt` - Windows
- `/hotspot-detect.html` - iOS
- `/canonical.html` - iOS

### 4. **Logs Melhorados**
```
╔════════════════════════════════════╗
║     MODO AP - PORTAL CAPTIVO       ║
╚════════════════════════════════════╝
[AP] SSID: terrasmart-setup
[AP] IP: 192.168.4.1
[AP] Gateway: 192.168.4.1
[AP] Conecte ao Wi-Fi 'terrasmart-setup'
[AP] Acesse: http://192.168.4.1 ou http://setup.local

[AP] Servidor Web iniciado na porta 80

[HTTP] Requisição: /
[HTTP] Client IP: 192.168.4.2
```

---

## 📋 Como Testar:

### Opção 1: Acessar pelo IP direto
1. Conectar ao Wi-Fi `terrasmart-setup` do seu celular
2. Abrir navegador e acessar `http://192.168.4.1`

### Opção 2: Portal Captivo Automático (Android/iOS)
1. Conectar ao Wi-Fi `terrasmart-setup`
2. O dispositivo deve detectar automaticamente e abrir uma janela com o portal

### Opção 3: Acessar pelo nome de domínio
1. Conectar ao Wi-Fi `terrasmart-setup`
2. Abrir navegador e acessar `http://setup.local`

---

## 🐛 Possíveis Problemas e Soluções:

### ❌ "ERR_CONNECTION_ABORTED" - Solução:
1. **Verificar se o ESP está em AP mode:**
   - Procure na saída serial por: `╔════════════════════════════════════╗`
   
2. **Verificar conexão Wi-Fi:**
   - O dispositivo está realmente conectado à rede `terrasmart-setup`?
   - Está recebendo IP na faixa 192.168.4.x?

3. **Verificar se recebe requisição DNS:**
   - Se vir `[HTTP] Requisição:` - o servidor HTTP está respondendo
   - Se NÃO vir nada - o DNS não está interceptando

4. **Limpar cache do navegador:**
   - Fazer reset de rede no celular
   - Tentar em navegador anônimo/incógnito

### ❌ ESP não inicia em AP mode:
- Verificar se `hasValidConfig()` retorna `false`
- Apagar arquivo `/config.json` do LittleFS

---

## 📊 Monitorar em Tempo Real:

Execute o comando abaixo para ver os logs:
```bash
screen /dev/ttyUSB0 115200
# Ou
minicom -D /dev/ttyUSB0 -b 115200
```

Procure por mensagens de:
- `[BOOT] Sem config` - iniciou sem config
- `[AP] SSID:` - AP iniciado
- `[HTTP] Requisição:` - portal acessado
- `[CAPTIVE] Config salva:` - config salva com sucesso

---

## 🔄 Fluxo Esperado:

```
[BOOT] Sem config → modo AP + Portal Captivo
╔════════════════════════════════════╗
║     MODO AP - PORTAL CAPTIVO       ║
╚════════════════════════════════════╝
[AP] SSID: terrasmart-setup
[AP] IP: 192.168.4.1
...
[HTTP] Requisição: /
[HTTP] Client IP: 192.168.4.2
[CAPTIVE] Config salva: SSID=MinhaRede
Reiniciando...
[BOOT] Config encontrada → modo operacional
[CONFIG] SSID: MinhaRede
[WIFI] Conectado: MinhaRede, IP: 192.168.x.x
```

---

## 📝 Notas Importantes:

1. A porta **80 é obrigatória** para o portal captivo funcionar
2. O DNS na porta **53** está ativo
3. O delay de `2ms` no loop mantém o sistema responsivo
4. O timeout de `1000ms` no inicio permite estabilização

---

## 🆘 Se Nada Funcionar:

1. Verificar compilação: `platformio run`
2. Verificar upload: `platformio run --target upload`
3. Ver logs: `platformio device monitor -b 115200`
4. Factory reset: Apagar todo o LittleFS antes do upload

