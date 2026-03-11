# Maintainer: Your Name <your@email.com>
pkgname=gpu-benchmark
pkgver=0.4.0
pkgrel=1
pkgdesc='Cross-platform OpenGL GPU benchmark (GL 2.1+) with 12 tests, composite scoring, and stress mode'
arch=('x86_64')
license=('MIT')
depends=('sdl2' 'libgl')
makedepends=('cmake' 'git')
# source=("git+https://github.com/youruser/GPU_benchmark.git#tag=v${pkgver}")
source=()
sha256sums=()

prepare() {
    # When building from local source, symlink into srcdir
    if [[ ! -d "$srcdir/$pkgname" ]]; then
        ln -sf "$startdir" "$srcdir/$pkgname"
    fi
    cd "$srcdir/$pkgname"
    git submodule update --init --recursive
}

build() {
    cd "$srcdir/$pkgname"
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
}

package() {
    cd "$srcdir/$pkgname"
    DESTDIR="$pkgdir" cmake --install build
}
