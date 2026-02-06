# Maintainer: ijuttt <zzudin.email@gmail.com>
# Contributor: Falint <kafkaxz2234@gmail.com>
pkgname=spiketrace-git
pkgver=0.1.0
pkgrel=1
pkgdesc="A system resources spike detection and tracing tool for anomaly processes detection"
arch=('x86_64')
url="https://github.com/ijuttt/spiketrace"
license=('GPL-2.0-only')
depends=('glibc')
makedepends=('go>=1.21' 'git' 'gcc' 'make')
optdepends=('systemd: for running as a service')
provides=('spiketrace')
conflicts=('spiketrace')
install=spiketrace.install
source=("git+https://github.com/ijuttt/spiketrace.git")
sha256sums=('SKIP')
backup=('etc/spiketrace/config.toml')

pkgver() {
    cd "spiketrace"
    git describe --long --tags 2>/dev/null | sed 's/\([^-]*-g\)/r\1/;s/-/./g' ||
    printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
    cd "spiketrace"
    
    export CGO_CFLAGS="${CFLAGS}"
    export CGO_LDFLAGS="${LDFLAGS}"
    export GOFLAGS="-buildmode=pie -trimpath -mod=vendor"
    
    make
}

package() {
    cd "spiketrace"
    
    make DESTDIR="$pkgdir" PREFIX=/usr SYSCONFDIR=/etc install
    
    install -dm0750 "$pkgdir/var/lib/spiketrace"
}
