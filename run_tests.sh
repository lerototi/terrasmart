#!/bin/bash
# Script para facilitar execução de testes

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "\n${BLUE}╔════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║  $1${NC}"
    echo -e "${BLUE}╚════════════════════════════════════╝${NC}\n"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ $1${NC}"
}

show_help() {
    cat << EOF
Uso: ./run_tests.sh [OPÇÃO]

Opções:
    all              Rodar todos os testes (padrão)
    config           Rodar testes do ConfigManager
    wifi             Rodar testes do WiFiManager
    mqtt             Rodar testes do MQTTManager
    verbose          Rodar todos os testes com output detalhado
    compile          Apenas compilar (sem rodar)
    coverage         Executar com cobertura de código
    help             Mostrar esta mensagem

Exemplos:
    ./run_tests.sh all
    ./run_tests.sh config
    ./run_tests.sh verbose

EOF
}

run_all_tests() {
    print_header "Executando Todos os Testes"
    platformio test -e native_test
}

run_config_tests() {
    print_header "Executando Testes - ConfigManager"
    platformio test -e native_test --filter test_config_manager
}

run_wifi_tests() {
    print_header "Executando Testes - WiFiManager"
    platformio test -e native_test --filter test_wifi_manager
}

run_mqtt_tests() {
    print_header "Executando Testes - MQTTManager"
    platformio test -e native_test --filter test_mqtt_manager
}

run_verbose() {
    print_header "Executando Todos os Testes (Verboso)"
    platformio test -e native_test -v
}

compile_only() {
    print_header "Compilando Testes"
    platformio run -e native_test --target build
    print_success "Compilação concluída"
}

case "${1:-all}" in
    all)
        run_all_tests
        ;;
    config)
        run_config_tests
        ;;
    wifi)
        run_wifi_tests
        ;;
    mqtt)
        run_mqtt_tests
        ;;
    verbose)
        run_verbose
        ;;
    compile)
        compile_only
        ;;
    help)
        show_help
        ;;
    *)
        print_error "Opção inválida: $1"
        show_help
        exit 1
        ;;
esac

print_success "Testes concluídos!"
