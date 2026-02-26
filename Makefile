# Makefile for building/installing pytriskel (mirrors the provided bash script)
ifeq ($(OS),Windows_NT)
  IS_WINDOWS := 1
else
  IS_WINDOWS := 0
endif

ifeq ($(IS_WINDOWS),0)
  SHELL := /usr/bin/env bash
  .SHELLFLAGS := -euo pipefail -c

  # ========= User-configurable paths =========
  PROJECT_DIR        ?= $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
  VENV_PYTHON         ?= /home/ubuntu/dAngr/venv/bin/python
  SITE_PACKAGES_DST   ?= /home/ubuntu/dAngr/venv/lib/python3.12/site-packages
  MODULE_SO_NAME      ?= pytriskel.cpython-312-x86_64-linux-gnu.so

  # ========= Tooling paths/versions =========
  VCPKG_ROOT          ?= /opt/vcpkg
  VCPKG_TOOLCHAIN     := $(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake
  VCPKG_TRIPLET       ?= x64-linux

  CMAKE_VERSION       ?= 4.2.3
  CMAKE_SH_URL        := https://github.com/Kitware/CMake/releases/download/v$(CMAKE_VERSION)/cmake-$(CMAKE_VERSION)-linux-x86_64.sh
  CMAKE_SH            := /tmp/cmake-$(CMAKE_VERSION).sh

  # ========= Build outputs =========
  BUILD_DIR           := $(PROJECT_DIR)/build
  SRC_SO              := $(BUILD_DIR)/bindings/python/$(MODULE_SO_NAME)
  DST_SO              := $(SITE_PACKAGES_DST)/$(MODULE_SO_NAME)

  # ========= Stamp files =========
  STAMP_DIR           := $(PROJECT_DIR)/.make-stamps
  STAMP_APT           := $(STAMP_DIR)/apt.ok
  STAMP_VCPKG         := $(STAMP_DIR)/vcpkg.ok
  STAMP_CAIRO         := $(STAMP_DIR)/cairo.ok
  STAMP_CMAKE         := $(STAMP_DIR)/cmake.ok
  STAMP_CONFIGURE     := $(STAMP_DIR)/configure.ok
  STAMP_BUILD         := $(STAMP_DIR)/build.ok
  STAMP_INSTALL       := $(STAMP_DIR)/install.ok

  # ========= Logging =========
  define log
	@echo -e "\n==> $(1)\n"
  endef

  .PHONY: all apt vcpkg cairo cmake configure build install clean distclean help

  all: install

  help:
	@echo "Targets:"
	@echo "  all        - full pipeline (install)"
	@echo "  apt        - install apt build deps (requires root)"
	@echo "  vcpkg      - clone/bootstrap vcpkg under /opt/vcpkg (requires root)"
	@echo "  cairo      - vcpkg install cairo:x64-linux"
	@echo "  cmake      - install CMake $(CMAKE_VERSION) to /usr/local (requires root)"
	@echo "  configure  - cmake configure into build/"
	@echo "  build      - build with cmake --build"
	@echo "  install    - copy built .so into venv site-packages"
	@echo "  clean      - remove build/ and configure/build/install stamps"
	@echo "  distclean  - clean + remove all stamps (keeps /opt/vcpkg and /usr/local cmake)"
	@echo ""
	@echo "Example:"
	@echo "  sudo -E make all"
	@echo ""
	@echo "Config vars (override like VAR=...):"
	@echo "  PROJECT_DIR, VENV_PYTHON, SITE_PACKAGES_DST, MODULE_SO_NAME"
	@echo "  VCPKG_ROOT, VCPKG_TRIPLET, CMAKE_VERSION"

  $(STAMP_DIR):
	@mkdir -p "$(STAMP_DIR)"

  # ========= apt deps (root) =========
  $(STAMP_APT): | $(STAMP_DIR)
	$(call log,Installing build tools & deps (apt))
	apt-get update -y
	DEBIAN_FRONTEND=noninteractive apt-get install -y \
	  ninja-build build-essential curl zip unzip tar pkg-config \
	  git wget autoconf autoconf-archive automake libtool \
	  gcc g++ perl python3 clang-tools-18 clang libc++-dev \
	  libc++abi-dev libpixman-1-dev \
	  libfontconfig1-dev libfreetype6-dev libexpat1-dev \
	  libpng-dev zlib1g-dev libbz2-dev libbrotli-dev
	@touch "$@"

  apt: $(STAMP_APT)

  # ========= vcpkg (root) =========
  $(STAMP_VCPKG): $(STAMP_APT) | $(STAMP_DIR)
	$(call log,Installing vcpkg to $(VCPKG_ROOT) (if missing))
	if [[ ! -d "$(VCPKG_ROOT)/.git" ]]; then \
	  mkdir -p "$(VCPKG_ROOT)"; \
	  rm -rf "$(VCPKG_ROOT)"; \
	  git clone https://github.com/microsoft/vcpkg.git "$(VCPKG_ROOT)"; \
	fi
	cd "$(VCPKG_ROOT)" && ./bootstrap-vcpkg.sh
	@touch "$@"

  vcpkg: $(STAMP_VCPKG)

  # ========= cairo via vcpkg =========
  $(STAMP_CAIRO): $(STAMP_VCPKG) | $(STAMP_DIR)
	$(call log,Installing cairo via vcpkg (triplet: $(VCPKG_TRIPLET)))
	export VCPKG_ROOT="$(VCPKG_ROOT)"; \
	export PATH="$(VCPKG_ROOT):$$PATH"; \
	vcpkg install cairo:$(VCPKG_TRIPLET)
	@touch "$@"

  cairo: $(STAMP_CAIRO)

  # ========= CMake install (root) =========
  $(STAMP_CMAKE): $(STAMP_APT) | $(STAMP_DIR)
	$(call log,Removing distro cmake (if present) and installing CMake $(CMAKE_VERSION) to /usr/local)
	apt-get remove -y cmake || true
	wget -O "$(CMAKE_SH)" "$(CMAKE_SH_URL)"
	chmod +x "$(CMAKE_SH)"
	"$(CMAKE_SH)" --skip-license --prefix=/usr/local
	hash -r
	@touch "$@"

  cmake: $(STAMP_CMAKE)

  # ========= configure =========
  $(STAMP_CONFIGURE): $(STAMP_CAIRO) $(STAMP_CMAKE) | $(STAMP_DIR)
	$(call log,Configuring with CMake (Ninja, clang-18, libc++, vcpkg toolchain))
	rm -rf "$(BUILD_DIR)"
	cd "$(PROJECT_DIR)" && \
	cmake -S . -B build -G Ninja \
	  -DCMAKE_C_COMPILER=clang-18 \
	  -DCMAKE_CXX_COMPILER=clang++-18 \
	  -DCMAKE_CXX_FLAGS="-stdlib=libc++" \
	  -DCMAKE_TOOLCHAIN_FILE="$(VCPKG_TOOLCHAIN)" \
	  -DVCPKG_TARGET_TRIPLET=$(VCPKG_TRIPLET) \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DENABLE_CAIRO=ON \
	  -DBUILD_PYTHON_BINDINGS=ON \
	  -DBUILD_JAVA_BINDINGS=OFF \
	  -DENABLE_LLVM=OFF \
	  -DENABLE_IMGUI=OFF \
	  -DPYBIND11_FINDPYTHON=ON \
	  -DPython_EXECUTABLE="$(VENV_PYTHON)" \
	  -DPython_FIND_VIRTUALENV=ONLY \
	  -DPython_FIND_STRATEGY=LOCATION \
	  -DCairo_INCLUDE_DIRS="$(VCPKG_ROOT)/installed/$(VCPKG_TRIPLET)/include" \
	  -DCairo_LIBRARY="$(VCPKG_ROOT)/installed/$(VCPKG_TRIPLET)/lib/libcairo.a" \
	  -DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++ -L$(VCPKG_ROOT)/installed/$(VCPKG_TRIPLET)/lib" \
	  -DCMAKE_SHARED_LINKER_FLAGS="-stdlib=libc++ -L$(VCPKG_ROOT)/installed/$(VCPKG_TRIPLET)/lib"
	@touch "$@"

  configure: $(STAMP_CONFIGURE)

  # ========= build =========
  $(STAMP_BUILD): $(STAMP_CONFIGURE)
	$(call log,Building)
	cmake --build "$(BUILD_DIR)"
	@touch "$@"

  build: $(STAMP_BUILD)

  # ========= install/copy module =========
  $(STAMP_INSTALL): $(STAMP_BUILD)
	$(call log,Copying built Python module into site-packages)
	if [[ ! -f "$(SRC_SO)" ]]; then \
	  echo "ERROR: Built module not found at: $(SRC_SO)"; \
	  exit 1; \
	fi
	mkdir -p "$(SITE_PACKAGES_DST)"
	cp "$(SRC_SO)" "$(DST_SO)"
	@echo -e "\n==> Done. Installed: $(DST_SO)\n"
	@touch "$@"

  install: $(STAMP_INSTALL)

  # ========= cleanup =========
  clean:
	$(call log,Cleaning build dir and build/configure/install stamps)
	rm -rf "$(BUILD_DIR)"
	rm -f "$(STAMP_CONFIGURE)" "$(STAMP_BUILD)" "$(STAMP_INSTALL)"

  distclean: clean
	$(call log,Removing all stamps)
	rm -rf "$(STAMP_DIR)"
else
	# Windows support not implemented yet
all:
	@echo "Windows support not implemented yet. Please run on Linux."
endif