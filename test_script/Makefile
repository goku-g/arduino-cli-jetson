default_fqbn := esp32.esp32.esp32 # replace : with .
build-dir := build

target-dir := ${build-dir}/${default_fqbn}

compile:
	arduino-cli compile . --config-file ../config.yaml

upload:
	arduino-cli upload . --input-dir ${target-dir}

clean:
	rm -rf ${build-dir}