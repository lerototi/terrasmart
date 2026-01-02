# 🌱 Terrasmart

**Terrasmart** é uma plataforma de automação inteligente voltada para **agroecologia, permacultura e casas inteligentes**, integrando dispositivos IoT ao **Home Assistant** de forma simples, modular e escalável.

O projeto tem como objetivo tornar a automação **acessível**, **robusta** e **descentralizada**, permitindo que usuários finais configurem sensores, atuadores e regras sem conhecimento técnico avançado.

---

## ✨ Visão Geral

O Terrasmart é composto por três grandes camadas:

1. **Dispositivos IoT**
   - ESP8266 / ESP32
   - Sensores e atuadores
   - Comunicação via MQTT

2. **Agente Local (Home Assistant)**
   - Add-ons personalizados
   - Broker MQTT
   - Integração e automação local
   - Funcionamento offline-first

3. **Camada SaaS (em evolução)**
   - Onboarding facilitado
   - Monitoramento remoto
   - Assistente inteligente (IA + WhatsApp)
   - Gestão de dispositivos e usuários

---

## 🧱 Arquitetura

┌──────────────┐
│ Dispositivos│
│ IoT (ESP) │
└──────┬───────┘
│ MQTT
┌──────▼────────┐
│ Home Assistant│
│ + Add-ons │
│ Terrasmart │
└──────┬─────────┘
│ API / MQTT
┌──────▼──────────┐
│ SaaS Terrasmart│
│ (opcional/remoto)│
└─────────────────┘

---

## 📦 Componentes do Projeto

### 🔌 Add-on Home Assistant
- Responsável por:
  - Comunicação MQTT
  - Descoberta de dispositivos
  - Publicação e escuta de eventos
  - Ponte entre HA e SaaS

### 📡 MQTT
- Canal principal de comunicação
- Tópicos padronizados
- Suporte a auto-discovery

### 🤖 Dispositivos
- Firmware customizado
- Logs de Wi-Fi e MQTT
- Suporte a onboarding automatizado
- Reset físico e lógico

---

## 🚀 Status do Projeto

⚠️ **Projeto em desenvolvimento ativo**

Esta branch (`development`) representa uma **nova arquitetura**, com mudanças significativas em relação à versão anterior.

- [x] Nova estrutura do repositório
- [x] Integração MQTT
- [x] Add-on Home Assistant (base)
- [ ] Auto-discovery completo
- [ ] Onboarding via app/mobile
- [ ] Integração SaaS
- [ ] Assistente via WhatsApp

---

## 🌿 Filosofia do Projeto

O Terrasmart nasce da união entre:

- 🌱 Agroecologia
- 🛠️ Tecnologia aberta
- 🌎 Sustentabilidade
- 🤝 Comunidade

A proposta é **empoderar pessoas**, e não criar dependência de plataformas fechadas.

---

## 🧑‍💻 Desenvolvimento

### Branches
- `master` → versão estável
- `development` → desenvolvimento ativo
- `feature/*` → novas funcionalidades

### Requisitos
- Docker
- Home Assistant
- Broker MQTT (Mosquitto)
- ESP8266 / ESP32

---

## 🛠️ Como começar (em breve)
Instruções detalhadas de instalação e uso serão adicionadas nas próximas versões.

---

## 📄 Licença
Este projeto é open-source e será licenciado sob uma licença compatível com software livre (a definir).

---

## 🤝 Contribuindo
Contribuições são bem-vindas!

- Fork o projeto
- Crie uma branch (`feature/minha-feature`)
- Commit suas mudanças
- Abra um Pull Request

---

## 📫 Contato
Projeto mantido por **Leonardo Torres**  
Produtor rural • Desenvolvedor • Agroecologia & Tecnologia

> “Automação a serviço da vida, não o contrário.”
