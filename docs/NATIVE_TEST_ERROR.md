# ⚠️ Erro: native_test com platformio run

## 🐛 Problema

Quando você tenta rodar:
```
platformio run --environment native_test
```

Recebe o erro:
```
Error: Nothing to build. Please put your source code files to the '/path/to/src' folder
```

---

## 🔍 Por que acontece?

O projeto tem **2 ambientes diferentes**:

| Ambiente | Comando Correto | Descrição |
|----------|-----------------|-----------|
| `wemos_d1_mini32` | `platformio run -e wemos_d1_mini32` | Compila firmware para ESP8266 |
| `native_test` | `platformio test -e native_test` | **NÃO** use `run`, use `test`! |

O `native_test` é um ambiente **especial** configurado APENAS para testes com Google Test framework, não para compilação normal (`run`).

---

## ✅ Soluções

### Opção 1: Use VS Code Tasks (Recomendado)

Pressione `Ctrl+Shift+B` e escolha **"Run Tests"**

```
✅ Correto e fácil
✅ Menu interativo
✅ Sem linha de comando
```

### Opção 2: Use o comando correto na linha de comando

```bash
# ✅ CORRETO para testes
platformio test -e native_test

# ❌ ERRADO (vai falhar!)
platformio run -e native_test
```

### Opção 3: Use o script auxiliar

```bash
./run_tests.sh all
```

---

## 📊 Diferença de Configuração

### wemos_d1_mini32 (Hardware)
```ini
[env:wemos_d1_mini32]
platform = espressif8266      ← Hardware específico
framework = arduino            ← Framework Arduino
lib_deps = ... (libraries do ESP8266)

Comando: platformio run -e wemos_d1_mini32
```

### native_test (Testes no PC)
```ini
[env:native_test]
platform = native              ← Computador local (PC)
test_framework = googletest     ← Framework de testes
build_src_filter = -<*> +<test/>  ← APENAS código de teste

Comando: platformio test -e native_test  ⚠️ USE TEST, NÃO RUN!
```

---

## 🎯 Fluxo Correto

### ❌ ERRADO
```
Ctrl+Shift+B
↓
"Build ALL" ou seleciona "native_test" no VS Code
↓
❌ Erro: "Nothing to build"
```

### ✅ CORRETO
```
Ctrl+Shift+B
↓
Escolhe "🎯 Escolher Build"
↓
Escolhe opção "2) Run Tests"
↓
✅ 42 testes executam corretamente
```

---

## 📚 Documentação Relacionada

- **[VSCODE_TASKS.md](VSCODE_TASKS.md)** - Guia completo de tasks
- **[BUILD_GUIDE.md](BUILD_GUIDE.md)** - Guia de compilação
- **[TESTING_GUIDE.md](TESTING_GUIDE.md)** - Como rodar testes
- **[platformio.ini](../../platformio.ini)** - Configuração completa

---

## 💡 Resumo Rápido

| Ação | Comando | Status |
|------|---------|--------|
| Build Firmware | `platformio run -e wemos_d1_mini32` | ✅ |
| Build Firmware (VS Code) | `Ctrl+Shift+B → 1` | ✅ |
| Rodar Testes | `platformio test -e native_test` | ✅ |
| Rodar Testes (VS Code) | `Ctrl+Shift+B → 2` | ✅ |
| Build + Testes | `platformio run -e wemos_d1_mini32 && platformio test -e native_test` | ✅ |
| Build + Testes (VS Code) | `Ctrl+Shift+B → 3` | ✅ |
| **Rodar native com run** | `platformio run -e native_test` | ❌ **ERRO!** |

---

## 🚀 Próximos Passos

1. **Use VS Code Tasks:** `Ctrl+Shift+B`
2. **Para testes:** Escolha "Run Tests" (opção 2)
3. **Para firmware:** Escolha "Build ESP8266" (opção 1)
4. **Para ambos:** Escolha "Build ALL" (opção 3)

**Pronto!** Assim você nunca mais verá esse erro. 🎉
