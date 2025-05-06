if(BUILD_PYTHON_BINDINGS)
    add_subdirectory(python)
endif()

if(BUILD_JAVA_BINDINGS)
    add_subdirectory(java)
endif()
