project(pytriskel
  VERSION ${TRISKEL_VERSION}
  DESCRIPTION "Python bindings for the triskel CFG layout library"
  LANGUAGES CXX C)

# Force modern Python discovery before pybind11 is configured.
set(PYBIND11_FINDPYTHON ON CACHE BOOL "" FORCE)

# Pybind
find_package(Python COMPONENTS Development.Module Interpreter REQUIRED)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11
    GIT_TAG        a2e59f0e7065404b44dfe92a28aca47ba1378dc4 # 2.13.6
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(pybind11)


pybind11_add_module(pytriskel pytriskel.cpp)

target_link_libraries(pytriskel PRIVATE triskel)

install(TARGETS pytriskel LIBRARY DESTINATION .)
