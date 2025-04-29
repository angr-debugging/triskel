project(triskel-img
    VERSION ${TRISKEL_VERSION}
    DESCRIPTION "Image making binary for the triskel CFG layout library"
    LANGUAGES CXX C
)

add_executable(triskel-img main.cpp)

target_link_libraries(triskel-img
PRIVATE
  triskel
  gflags
  fmt::fmt
)

if (ENABLE_LINTING)
  find_program(CLANG_TIDY NAMES "clang-tidy" REQUIRED)
    set_target_properties(triskel-img PROPERTIES
      CXX_CLANG_TIDY ${CLANG_TIDY}
  )
endif()
