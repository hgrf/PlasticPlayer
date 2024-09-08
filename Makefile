.PHONY: deps
deps:
	mkdir -p deps/packages/RaspberryPi
	mkdir -p deps/libraries
	cd deps && \
		[ -d playerctl ] || \
		git clone https://github.com/altdesktop/playerctl.git
	cd deps/packages/RaspberryPi && \
		[ -d piduino ] || \
		(git clone https://github.com/me-no-dev/RasPiArduino piduino && \
		cd piduino && \
		git checkout 29fad57fa5d42cece2d5370831ba8f2a47b373c2 && \
		git apply ../../../../piduino.patch)
	cd deps/packages/RaspberryPi/piduino/tools && \
		[ -d arm-linux-gnueabihf ] || \
		(wget https://github.com/me-no-dev/RasPiArduino/releases/download/0.0.1/arm-linux-gnueabihf-osx.tar.gz && \
		tar -xzf arm-linux-gnueabihf-osx.tar.gz)
	cd deps/libraries && \
		[ -d MFRC522-spi-i2c-uart-async ] || \
		(git clone -b 1.5.1 https://github.com/MakerSpaceLeiden/rfid.git MFRC522-spi-i2c-uart-async && \
		cd MFRC522-spi-i2c-uart-async && \
		sed -i -e 's/Serial\.print/Console\.print/g' src/MFRC522.cpp)
	cd deps/libraries && \
		[ -d ssd1306 ] || \
		(git clone -b v1.8.5 https://github.com/lexus2k/ssd1306.git && \
		cd ssd1306 && \
		git apply ../../../raspi-io.patch)
	cd deps/libraries && \
		[ -d NDEF ] || \
		(git clone -b ndef-record-as-uri https://github.com/hgrf/NDEF.git && \
		cd NDEF && \
		sed -i -e 's/Serial\.print/Console\.print/g' src/*.*)

# TODO: check how much of this can be done with https://arduino.github.io/arduino-cli/0.29/sketch-project-file/

patch-toolchain:
	sed -i -e "s|SYSROOT=.*|SYSROOT=$$PWD/deps/sysroot|g" deps/packages/RaspberryPi/piduino/tools/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-pkg-config
# it seems like the linker is missing the standard library
# should link against libs here (or merge this into the sysroot?)
# /Users/holger/PlasticPlayer/deps/packages/RaspberryPi/piduino/tools/arm-linux-gnueabihf/bin/../arm-linux-gnueabihf/libc
	cd deps/packages/RaspberryPi/piduino/tools/arm-linux-gnueabihf/arm-linux-gnueabihf && \
		([ -d libc ] && mv libc libc.old); \
		ln -sf ../../../../../../sysroot libc && \
		([ -d lib ] && mv lib lib.old); \
		ln -s ../../../../../../sysroot/usr/lib/arm-linux-gnueabihf lib

unpatch-toolchain:
	cd deps/packages/RaspberryPi/piduino/tools/arm-linux-gnueabihf/arm-linux-gnueabihf && \
		([ -d libc.old ] && mv libc.old libc); \
		([ -d lib.old ] && mv lib.old lib)

build-playerctl: deps
	cd deps/playerctl && \
	rm -rf build && \
	CFLAGS="-std=c11 -D_POSIX_C_SOURCE=199309L" \
 		LDFLAGS="-lffi -lmount -lblkid -lpcre2-8 -lm -lz -lgmodule-2.0 -lselinux" \
 		meson setup --cross-file ../../meson.cross build \
 		-Dintrospection=false -Dgtk-doc=false \
		&& \
	meson compile -C build

.PHONY: build
build: patch-toolchain build-playerctl unpatch-toolchain
	cd .. && arduino-cli compile \
	-b RaspberryPi:piduino:bplus --log \
	--build-path PlasticPlayer/build \
	--build-cache-path PlasticPlayer/build/cache \
	--config-dir PlasticPlayer/deps \
	--libraries PlasticPlayer/deps/libraries \
	--build-property compiler.c.flags="-c -pipe -std=c99" \
	--build-property compiler.cpp.flags="-c -pipe -std=c++11 -fno-rtti -DNDEF_USE_SERIAL" \
	--build-property compiler.c.extra_flags="-I{build.path}/../deps/playerctl" \
	--build-property compiler.cpp.extra_flags="-I{build.path}/../deps/playerctl -I{build.path}/../deps/playerctl/build -I{build.path}/../deps/sysroot/usr/include/glib-2.0 -I{build.path}/../deps/sysroot/usr/lib/arm-linux-gnueabihf/glib-2.0/include" \
	--build-property compiler.c.elf.flags="-L{build.path}/../deps/playerctl/build/playerctl --sysroot={build.path}/../deps/sysroot -L {build.path}/../deps/sysroot/usr/lib/arm-linux-gnueabihf -lplayerctl -lm -lz -lpthread -lblkid -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lgmodule-2.0 -ldbus-1 -lpcre2-8 -lselinux -lmount  -lffi" \
	PlasticPlayer
