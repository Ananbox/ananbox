include(ExternalProject)
include(Talloc)
set(PROOT_SRC ${CMAKE_CURRENT_BINARY_DIR}/proot-prefix/src/proot/src)
set(PROOT_BIN ${CMAKE_CURRENT_BINARY_DIR}/proot-prefix/src/proot-build)
set(PROOT_C_FLAGS "-I${TALLOC_INCLUDE_DIRS} -I${PROOT_SRC} -D_GNU_SOURCE")
set(PROOT_LINKER_FLAGS "-L${TALLOC_BIN}/lib -ltalloc")
# specific correct clang target to use
if(${CMAKE_ANDROID_ARCH_ABI} STREQUAL "arm64-v8a")
    set(PROOT_C_COMPILER ${ANDROID_TOOLCHAIN_ROOT}/bin/aarch64-linux-android${CMAKE_SYSTEM_VERSION}-clang)
else()
    set(PROOT_C_COMPILER ${ANDROID_TOOLCHAIN_ROOT}/bin/${CMAKE_ANDROID_ARCH_ABI}-linux-android${CMAKE_SYSTEM_VERSION}-clang)
endif()
ExternalProject_Add(
        proot
        GIT_REPOSITORY https://github.com/Ananbox/proot.git
        GIT_TAG 50949de394a4661cda95d49aea7f0f20dd0153bd
        GIT_SHALLOW 1
        CONFIGURE_COMMAND cd ${PROOT_SRC} && make clean
        BUILD_COMMAND cd ${PROOT_SRC} && make V=1 CC=${PROOT_C_COMPILER} LD=${PROOT_C_COMPILER} STRIP=${CMAKE_STRIP} OBJCOPY=${CMAKE_OBJCOPY} OBJDUMP=${CMAKE_OBJDUMP} CFLAGS=${PROOT_C_FLAGS} LDFLAGS=${PROOT_LINKER_FLAGS}
        # hacked: only lib*.so can be packed into apk
        INSTALL_COMMAND cd ${PROOT_SRC} && cp -f ./proot ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libproot.so
        DEPENDS talloc
)