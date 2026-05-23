#!/bin/bash
# Script para crear paquete .xbps en una máquina con Void Linux

echo "Preparando construcción de paquete XBPS..."

# 1. Compilar el proyecto localmente
chmod +x build.sh
./build.sh

# 2. Crear estructura del paquete
mkdir -p pkg/usr/bin
mkdir -p pkg/usr/share/neko-kernel-manager
mkdir -p pkg/usr/share/applications
mkdir -p pkg/usr/share/pixmaps

cp build/neko-kernel-manager pkg/usr/bin/
cp -r Data/* pkg/usr/share/neko-kernel-manager/
cp Data/logo.png pkg/usr/share/pixmaps/neko-kernel-manager.png
cp neko-kernel-manager.desktop pkg/usr/share/applications/

# 3. Crear el paquete .xbps
echo "Generando archivo .xbps..."
xbps-create -A x86_64 -n "neko-kernel-manager-1.1.0_1" \
    -s "Modern kernel manager for Void Linux" \
    -m "ErzaGOD19 <gamingofdemon19@gmail.com>" \
    pkg/

if [ $? -eq 0 ]; then
    echo "---------------------------------------------------"
    echo "¡ÉXITO! Paquete creado: neko-kernel-manager-1.1.0_1.x86_64.xbps"
    echo "Para instalarlo en Void, usa:"
    echo "sudo xbps-install --repository=. neko-kernel-manager"
    echo "---------------------------------------------------"
else
    echo "Error al crear el paquete. Asegúrate de tener 'xbps' instalado."
fi
