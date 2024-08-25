.PHONY: deps
deps:
	mkdir -p deps/packages/RaspberryPi
	mkdir -p deps/libraries
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
build: deps
	cd .. && arduino-cli compile \
	-b RaspberryPi:piduino:bplus --log \
	--build-path PlasticPlayer/build \
	--build-cache-path PlasticPlayer/build/cache \
	--config-dir PlasticPlayer/deps \
	--libraries PlasticPlayer/deps/libraries \
	--build-property compiler.c.flags="-c -pipe -std=c99" \
	--build-property compiler.cpp.flags="-c -pipe -std=c++11 -fno-rtti -DNDEF_USE_SERIAL" \
	PlasticPlayer
