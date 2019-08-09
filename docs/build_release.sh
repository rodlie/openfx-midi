#!/bin/sh

CWD=`pwd`
TMP="$CWD/tmp"
MXE=/opt/mxe
ORIG_PATH=$PATH

if [ ! -f "$CWD/CMakeLists.txt" ]; then
   exit 1
fi

if [ -d "$TMP" ]; then
   rm -rf "$TMP" || exit 1
fi
mkdir -p "$TMP/linux" "$TMP/windows" || exit 1

export PATH="$MXE/usr/bin:$PATH"
cd "$TMP/windows" || exit 1

x86_64-w64-mingw32.static-cmake -DCMAKE_INSTALL_PREFIX=/ $CWD || exit 1
make || exit 1
x86_64-w64-mingw32.static-strip -s Midi.ofx || exit 1
make DESTDIR=`pwd` install || exit 1

export PATH="$ORIG_PATH"
cd "$TMP/linux" || exit 1

cmake3 -DCMAKE_INSTALL_PREFIX=/ $CWD || exit 1
make || exit 1
strip -s Midi.ofx || exit 1
make DESTDIR=`pwd` install || exit 1
cp -a "$TMP/windows/Midi.ofx.bundle" . || exit 1
zip -9 -r Midi.ofx.bundle.zip Midi.ofx.bundle || exit 1
tree -lah Midi.ofx.bundle
ls -lah Midi.ofx.bundle.zip
sha256sum Midi.ofx.bundle.zip
mv Midi.ofx.bundle.zip $CWD/
