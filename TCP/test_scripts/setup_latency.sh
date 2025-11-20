#!/bin/bash
# Script para configurar latencias con tc (Traffic Control)

INTERFACE="eth0"  # loopback para pruebas locales

function show_usage() {
    echo "Uso: $0 [lan|internacional|lunar|clear]"
    echo ""
    echo "Escenarios:"
    echo "  lan           - Sin latencia (LAN local)"
    echo "  internacional - 90ms delay (RTT ~180ms)"
    echo "  lunar         - 1280ms delay (RTT ~2.56s)"
    echo "  clear         - Eliminar todas las configuraciones"
    echo ""
    echo "Nota: Requiere permisos sudo"
}

function clear_tc() {
    echo "Eliminando configuraciones de tc..."
    sudo tc qdisc del dev $INTERFACE root 2>/dev/null
    echo "✓ Configuración limpiada"
}

function setup_lan() {
    clear_tc
    echo "✓ Escenario LAN: Sin latencia adicional"
}

function setup_internacional() {
    clear_tc
    echo "Configurando latencia internacional (90ms delay, RTT ~180ms)..."
    sudo tc qdisc add dev $INTERFACE root netem delay 90ms
    echo "✓ Escenario Internacional configurado"
}

function setup_lunar() {
    clear_tc
    echo "Configurando latencia lunar (1280ms delay, RTT ~2.56s)..."
    sudo tc qdisc add dev $INTERFACE root netem delay 1280ms
    echo "✓ Escenario Lunar configurado"
}

# Main
case "$1" in
    lan)
        setup_lan
        ;;
    internacional)
        setup_internacional
        ;;
    lunar)
        setup_lunar
        ;;
    clear)
        clear_tc
        ;;
    *)
        show_usage
        exit 1
        ;;
esac

# Mostrar configuración actual
echo ""
echo "Configuración actual de tc:"
sudo tc qdisc show dev $INTERFACE
