# 🔨 Guia de Compilação - terraSmart Device

## ⚠️ Importante: Especifique o Environment

O projeto tem **2 ambientes diferentes**:
- `wemos_d1_mini32` - Para ESP8266 (hardware real)
- `native_test` - Para testes no PC (sem hardware)

**SEMPRE especifique qual ambiente usar!**

---

## 🏗️ Para Compilar para ESP8266

```bash
# Compilar apenas (sem upload)
platformio run -e wemos_d1_mini32

# Compilar e fazer upload
platformio run -e wemos_d1_mini32 --target upload

# Compilar com verbose (mostra detalhes)
platformio run -e wemos_d1_mini32 -v
```

**Resultado esperado:**
```
RAM:   [====      ]  41.2% (used 33776 bytes from 81920 bytes)
Flash: [===       ]  34.0% (used 354651 bytes from 1044464 bytes)
========================= [SUCCESS] Took X.XX seconds ==========================
```

---

## 🧪 Para Rodar Testes

```bash
# Rodar todos os testes
platformio test -e native_test

# Modo verbose (mostra cada teste)
platformio test -e native_test -v

# Teste específico
platformio test -e native_test --filter "ParseValidJSON"
```

**Ou use o script helper:**
```bash
./run_tests.sh all
./run_tests.sh verbose
./run_tests.sh config
```

**Resultado esperado:**
```
42 test cases: 42 succeeded in 0.71 seconds
========================= [SUCCESS] Took X.XX seconds ==========================
```

---

## ❌ O QUE NÃO FAZER

### ❌ Errado: Compilar sem especificar environment
```bash
# ❌ ERRO: Tenta compilar TUDO (wemos_d1_mini32 E native_test)
platformio run

# ❌ ERRO: Mesmo resultado
pio run
```

**Resultado:**
```
Error: Nothing to build. Please put your source code files to the '/path/to/src' folder
=================================== [FAILED] ===================================
```

### ✅ Correto: Especifique o ambiente
```bash
# ✅ CORRETO: Compila só para ESP8266
platformio run -e wemos_d1_mini32

# ✅ CORRETO: Roda só os testes
platformio test -e native_test
```

---

## 📋 Comandos Mais Usados

### Desenvolvimento ESP8266
```bash
# Compilar
platformio run -e wemos_d1_mini32

# Upload
platformio run -e wemos_d1_mini32 --target upload

# Monitorar serial
platformio device monitor -b 115200

# Limpar build
platformio run -e wemos_d1_mini32 --target clean
```

### Testes
```bash
# Todos os testes
./run_tests.sh all

# Por módulo
./run_tests.sh config
./run_tests.sh wifi
./run_tests.sh mqtt

# Modo verbose
./run_tests.sh verbose

# Apenas compilar (sem rodar)
./run_tests.sh compile
```

---

## 🚀 Fluxo de Trabalho Recomendado

### 1️⃣ Desenvolvimento
```bash
# Fazer mudanças no código em src/

# Compilar
platformio run -e wemos_d1_mini32

# Se tudo OK, fazer upload
platformio run -e wemos_d1_mini32 --target upload

# Monitorar
platformio device monitor -b 115200
```

### 2️⃣ Testes
```bash
# Fazer mudanças no teste em test/

# Rodar testes
./run_tests.sh all

# Se falhar, debug
./run_tests.sh verbose
```

### 3️⃣ Antes de Commit
```bash
# Garantir que tudo compila
platformio run -e wemos_d1_mini32

# Garantir que testes passam
./run_tests.sh all

# Se tudo OK, fazer commit
git add .
git commit -m "feature: sua feature aqui"
```

---

## 📊 Estrutura de Environments

```
platformio.ini
│
├─ [env:wemos_d1_mini32]     ← Para ESP8266
│   ├─ platform: espressif8266
│   ├─ board: d1_mini
│   ├─ framework: arduino
│   └─ Compila código real para hardware
│
└─ [env:native_test]         ← Para Testes no PC
    ├─ platform: native
    ├─ test_framework: googletest
    ├─ Roda em PC (sem hardware)
    └─ build_src_filter: só compila test/
```

---

## ⚡ Quick Reference

```bash
# PARA FAZER (Compilation)
platformio run -e wemos_d1_mini32

# PARA TESTAR (Testing)
platformio test -e native_test

# PARA UPLOAD (Hardware)
platformio run -e wemos_d1_mini32 --target upload

# PARA MONITORAR (Serial)
platformio device monitor -b 115200

# HELPER SCRIPTS
./run_tests.sh all          # Todos os testes
./run_tests.sh config       # Testes de config
./run_tests.sh wifi         # Testes de WiFi
./run_tests.sh mqtt         # Testes de MQTT
```

---

## 🆘 Troubleshooting

### "Error: Nothing to build"
**Causa:** Você rodou `platformio run` sem especificar `-e`

**Solução:**
```bash
# ❌ Errado
platformio run

# ✅ Correto
platformio run -e wemos_d1_mini32
```

### "Compilation failed"
**Causa:** Erro de sintaxe ou dependência faltando

**Solução:**
```bash
# Limpar build anterior
platformio run -e wemos_d1_mini32 --target clean

# Compilar novamente
platformio run -e wemos_d1_mini32 -v  # com verbose
```

### "Tests not running"
**Causa:** Usando `platformio run` em vez de `platformio test`

**Solução:**
```bash
# ❌ Errado
platformio run -e native_test

# ✅ Correto
platformio test -e native_test

# Ou use o script
./run_tests.sh all
```

### "Serial monitor não conecta"
**Causa:** Porta errada ou sem permissão

**Solução:**
```bash
# Listar portas disponíveis
ls /dev/ttyUSB*  # Linux
ls /dev/ttyACM*  # Linux alternativo
ls /dev/cu.*     # macOS

# Conectar (descobrir porta automaticamente)
platformio device monitor -b 115200
```

---

## 📚 Mais Informações

- [PlatformIO Documentation](https://docs.platformio.org/)
- [Environments in platformio.ini](https://docs.platformio.org/page/projectconf/sections/env.html)
- [Native Platform for Testing](https://docs.platformio.org/page/platforms/native.html)

---

**Importante:** Sempre especifique `-e nome_do_environment` ao usar `platformio run` ou `platformio test`!

---

**Criado:** 2 de Janeiro, 2026
