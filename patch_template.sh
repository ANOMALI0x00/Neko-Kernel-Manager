#!/bin/bash
# Helper script to patch the template file safely using native Bash
TEMPLATE_FILE="$1"
SOURCE_TMPL="$2"
NAME="$3"

if [ ! -f "$TEMPLATE_FILE" ]; then
    echo "Error: Template file $TEMPLATE_FILE not found" >&2
    exit 1
fi

# Read file into variable
CONTENT=$(cat "$TEMPLATE_FILE")

# 1. Modificar pkgname
CONTENT=$(echo "$CONTENT" | sed "s|^pkgname=.*|pkgname=$NAME|")

# 2. Renombrar subpaquetes
CONTENT=$(echo "$CONTENT" | sed "s|${SOURCE_TMPL}-|${NAME}-|g")

# 3. Desactivar dbg y añadir nodebug=yes
# Si ya existe, lo reemplazamos, si no, lo añadimos
if echo "$CONTENT" | grep -q "nodebug="; then
    CONTENT=$(echo "$CONTENT" | sed "s|^nodebug=.*|nodebug=yes|")
else
    CONTENT="$CONTENT"$'\n'"nodebug=yes"
fi

# Eliminar la función de paquete dbg si existe para evitar que xbps-src la procese
CONTENT=$(echo "$CONTENT" | sed '/_dbg_package() {/,/}/d')

# 4. Parchear do_install() usando una estrategia de reemplazo de texto literal
TARGET="mv \${DESTDIR}/usr/lib/modules"
REPLACEMENT="cp -a \${DESTDIR}/usr/lib/modules/* \${DESTDIR}/usr/lib/modules/ 2>/dev/null || true; rm -rf \${DESTDIR}/usr/lib/modules/mv"

# Usamos awk para reemplazar la línea exacta, que es más seguro que sed para líneas con muchas barras
CONTENT=$(echo "$CONTENT" | awk -v search="$TARGET" -v replace="$REPLACEMENT" '{gsub(search, replace); print}')

# Guardar
echo "$CONTENT" > "$TEMPLATE_FILE"
