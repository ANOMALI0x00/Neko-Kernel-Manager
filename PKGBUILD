# PKGBUILD para Neko Kernel Manager en Arch Linux
pkgname=neko-kernel-manager
pkgver=1.1.0
pkgrel=1
pkgdesc="Modern kernel manager for Void Linux (GUI client for Arch users/developers)"
arch=('x86_64')
url="https://github.com/ErzaGOD19/Neko-Kernel-Manager"
license=('GPL2')
depends=('qt6-base' 'qt6-declarative' 'fmt')
makedepends=('meson' 'ninja' 'rust' 'cargo')
source=("neko-kernel-manager::git+https://github.com/ErzaGOD19/Neko-Kernel-Manager.git")
sha256sums=('SKIP')

build() {
	cd "$srcdir/neko-kernel-manager"
	chmod +x build.sh
	./build.sh
}

package() {
	cd "$srcdir/neko-kernel-manager"
	install -Dm755 build/neko-kernel-manager "$pkgdir/usr/bin/neko-kernel-manager"
	mkdir -p "$pkgdir/usr/share/neko-kernel-manager"
	cp -r Data/* "$pkgdir/usr/share/neko-kernel-manager/"
}
