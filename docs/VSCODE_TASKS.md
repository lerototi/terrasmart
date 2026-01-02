# 🎯 VS Code Build Tasks - Guia Completo

## 📋 O que é?

O `.vscode/tasks.json` contém **7 tasks profissionais** que permitem compilar direto do VS Code com um clique, sem precisar de linha de comando!

---

## 🚀 Como Usar

### Método 1: Atalho de Teclado (Mais Rápido)

Pressione `Ctrl+Shift+B` (ou `Cmd+Shift+B` no macOS):

```
🎯 Escolher Build
├─ Build ESP8266      (Firmware)
├─ Run Tests          (42 Testes)
├─ Build ALL          (Firmware + Testes)
├─ Upload to ESP      (Fazer upload)
├─ Monitor Serial     (Ver logs)
└─ Clean Build        (Limpar arquivos)
```

### Método 2: Menu de Tasks

1. Abra a Paleta de Comandos: `Ctrl+Shift+P`
2. Digite: `Tasks: Run Task`
3. Escolha a task desejada

### Método 3: Terminal do VS Code

```bash
Ctrl+Shift+` 
# Depois digite (autocomplete disponível)
```

---

## 📝 Tasks Disponíveis

### 1️⃣ **🎯 Escolher Build** (Padrão)
```
Atalho: Ctrl+Shift+B
Função: Menu interativo para escolher
Opções:
  1 → Build ESP8266
  2 → Run Tests
  3 → Build ALL
  4 → Sair
```

### 2️⃣ **Build ESP8266**
```
Atalho: Via menu
Comando: platformio run -e wemos_d1_mini32
Resultado: Compila firmware para ESP8266
Tempo: ~2.4 segundos
```

### 3️⃣ **Run Tests**
```
Atalho: Via menu
Comando: platformio test -e native_test
Resultado: Roda 42 testes unitários
Tempo: ~0.7 segundos
```

### 4️⃣ **Build ALL**
```
Atalho: Via menu
Comando: Build ESP8266 + Run Tests
Resultado: Ambos sequencialmente
Tempo: ~3.1 segundos
```

### 5️⃣ **Upload to ESP**
```
Atalho: Via menu
Comando: platformio run -e wemos_d1_mini32 --target upload
Resultado: Faz upload do firmware
Tempo: ~3-5 segundos
```

### 6️⃣ **Monitor Serial**
```
Atalho: Via menu
Comando: platformio device monitor -b 115200
Resultado: Abre monitor serial (Ctrl+C para sair)
Tempo: Contínuo até fechar
```

### 7️⃣ **Clean Build**
```
Atalho: Via menu
Comando: Clean ambos ambientes
Resultado: Remove arquivos de build
Tempo: ~1 segundo
```

---

## 🎨 Workflow Recomendado

### Para Desenvolvimento

```mermaid
1. Editar código
   ↓
2. Ctrl+Shift+B → Escolher Build → 2 (Run Tests)
   ↓
3. Verificar testes passando
   ↓
4. Ctrl+Shift+B → Escolher Build → 1 (Build ESP8266)
   ↓
5. Verificar compilation OK
   ↓
6. Git Commit/Push
```

### Para Upload no Hardware

```
1. Ctrl+Shift+B → 3 (Build ALL)
   ├─ Compila firmware
   └─ Roda testes
   ↓
2. Ctrl+Shift+B → 5 (Upload to ESP)
   ↓
3. Ctrl+Shift+B → 6 (Monitor Serial)
   └─ Ver logs em tempo real
```

---

## 🎯 Exemplos de Uso

### ✅ Exemplo 1: Testar mudanças rápido
```
1. Editar src/config/config_manager.cpp
2. Ctrl+Shift+B → 2 (Run Tests)
3. Verificar se passou
4. Se OK → commit
```

### ✅ Exemplo 2: Deploy completo
```
1. Ctrl+Shift+B → 3 (Build ALL)
   ├─ Firmware compila ✅
   └─ 42 testes passam ✅
2. Ctrl+Shift+B → 5 (Upload to ESP)
3. Ctrl+Shift+B → 6 (Monitor Serial)
   └─ Acompanhar logs
```

### ✅ Exemplo 3: Limpeza completa
```
1. Ctrl+Shift+B → 7 (Clean Build)
2. Ctrl+Shift+B → 1 (Build ESP8266)
   └─ Build limpo, do zero
```

---

## 🔧 Personalizar Tasks

Se quiser adicionar ou modificar, edite `.vscode/tasks.json`:

```json
{
    "label": "Minha Task",
    "type": "shell",
    "command": "platformio",
    "args": ["run", "-e", "wemos_d1_mini32"],
    "problemMatcher": ["$platformio"],
    "group": {
        "kind": "build",
        "isDefault": false
    }
}
```

---

## ⚠️ Solução de Problemas

### Tasks não aparecem
```
1. Reinicie VS Code
2. Verifique se .vscode/tasks.json existe
3. Ctrl+Shift+P → Tasks: Run Task
```

### "Command not found: platformio"
```
✅ Solução:
1. Abra terminal: Ctrl+Shift+`
2. Instale: pip install platformio
3. Teste: pio --version
```

### Build falha com erro
```
1. Ctrl+Shift+B → 7 (Clean Build)
2. Tente novamente
3. Verifique logs no terminal
```

---

## 📊 Status Visual

Durante a execução, você verá:

```
[BUILD] Compilando para ESP8266...
[████████████░░░░░░░░░░░░░░░░] 50%

✅ Build concluído com sucesso!
```

---

## 🎓 Atalhos Úteis

| Ação | Atalho |
|------|--------|
| **Abrir Tasks** | `Ctrl+Shift+B` |
| **Abrir Terminal** | `Ctrl+Shift+`` |
| **Executar Tarefa** | `Ctrl+Shift+P` → Tasks |
| **Fechar Terminal** | `Ctrl+Shift+`` (toggled) |
| **Parar Tarefa** | `Ctrl+C` |
| **Monitor Serial** | Ctrl+Shift+B → 6 |

---

## 💡 Dicas Pro

### 1. Deixar Tasks sempre acessíveis
```
Na aba "Problems", clique em "Terminal"
Tasks rodadas aparecem no histórico
```

### 2. Verificar sucesso/erro
```
Cores na saída:
🟢 Verde = Sucesso
🔴 Vermelho = Erro
🟡 Amarelo = Aviso
```

### 3. Combinar com Launch Configs
Para debug, você pode criar `.vscode/launch.json`:
```json
{
    "configurations": [
        {
            "name": "PlatformIO Debug",
            "type": "gdb",
            "request": "launch",
            "preLaunchTask": "Build ESP8266"
        }
    ]
}
```

---

## ✅ Verificação

Para confirmar que está tudo funcionando:

```bash
# Terminal
Ctrl+Shift+`

# Digite
platformio --version

# Esperado output
PlatformIO, version X.X.X
```

---

## 🎉 Pronto!

Agora você pode:
✅ Build de qualquer ambiente com 1 clique  
✅ Escolher interativamente qual compilar  
✅ Rodar testes sem abrir terminal  
✅ Monitor serial integrado  
✅ Workflow profissional no VS Code  

**Comece agora: `Ctrl+Shift+B`** 🚀
