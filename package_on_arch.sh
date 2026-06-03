#!/bin/bash
# Script para compilar en Arch y generar .xbps para Void Linux

echo "Compilando proyecto en Arch..."
chmod +x build.sh
./build.sh

echo "Preparando estructura de paquete..."
rm -rf pkg_root
mkdir -p pkg_root/usr/bin
mkdir -p pkg_root/usr/share/neko-kernel-manager
mkdir -p pkg_root/usr/share/applications
mkdir -p pkg_root/usr/share/pixmaps

cp build/neko-kernel-manager pkg_root/usr/bin/
cp -r Data/* pkg_root/usr/share/neko-kernel-manager/
cp Data/logo.png pkg_root/usr/share/pixmaps/neko-kernel-manager.png
cp neko-kernel-manager.desktop pkg_root/usr/share/applications/

echo "Añadiendo metadatos XBPS..."
cat << 'EOF' > pkg_root/props.plist
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>architecture</key>
	<string>x86_64</string>
	<key>maintainer</key>
	<string>ErzaGOD19 <gamingofdemon19@gmail.com></string>
	<key>pkgname</key>
	<string>neko-kernel-manager</string>
	<key>short_desc</key>
	<string>Modern kernel manager for Void Linux</string>
	<key>version</key>
	<string>1.1.0_1</string>
</dict>
</plist>
EOF

echo "Empaquetando en .xbps..."
tar -cJf ../neko-kernel-manager-1.1.0_1.x86_64.xbps -C pkg_root .
rm -rf pkg_root

echo "¡Hecho! Paquete generado en la carpeta superior."
