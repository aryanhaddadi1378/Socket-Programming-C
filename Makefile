NPROC := $(shell nproc)
BUILD_DIR := build

all: release

release:
	mkdir -p $(BUILD_DIR) && \
	cd $(BUILD_DIR) && \
	cmake .. -B. -DCMAKE_BUILD_TYPE=Release && \
	make -j$(NPROC)

debug:
	mkdir -p $(BUILD_DIR) && \
	cd $(BUILD_DIR) && \
	cmake .. -B. -DCMAKE_BUILD_TYPE=Debug && \
	make -j$(NPROC)

clean:
	rm -rf $(BUILD_DIR)