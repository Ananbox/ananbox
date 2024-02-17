include(ExternalProject)
set(TALLOC_SRC ${CMAKE_CURRENT_BINARY_DIR}/talloc-prefix/src/talloc)
set(TALLOC_BIN ${CMAKE_CURRENT_BINARY_DIR}/talloc-prefix/src/talloc-build)
set(TALLOC_STATIC_LIB ${TALLOC_BIN}/lib/libtalloc.a)
set(TALLOC_INCLUDE_DIRS ${TALLOC_BIN}/include)
# specific correct clang target to use
set(TALLOC_C_COMPILER ${ANDROID_TOOLCHAIN_ROOT}/bin/${CMAKE_ANDROID_ARCH_ABI}-linux-android${CMAKE_SYSTEM_VERSION}-clang)
ExternalProject_Add(
        talloc
        URL https://download.samba.org/pub/talloc/talloc-2.4.1.tar.gz
        CONFIGURE_COMMAND cd ${TALLOC_SRC} && ./configure CC=${TALLOC_C_COMPILER} LD=${TALLOC_C_COMPILER} --prefix=${TALLOC_BIN} --disable-rpath --disable-python --without-gettext --cross-compile --cross-answers=${CMAKE_CURRENT_SOURCE_DIR}/talloc/cross-answers.txt
        BUILD_COMMAND cd ${TALLOC_SRC} && make
        INSTALL_COMMAND cd ${TALLOC_SRC} && mkdir -p ${TALLOC_INCLUDE_DIRS} ${TALLOC_BIN}/lib && sh -c "${CMAKE_AR} rcs ${TALLOC_STATIC_LIB} bin/default/talloc.c*.o" && cp -f talloc.h ${TALLOC_INCLUDE_DIRS}
)