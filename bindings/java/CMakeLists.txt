# Set up swig
find_package(SWIG REQUIRED)
find_package(Java REQUIRED)
find_package(JNI REQUIRED)
include(UseJava)
include(UseSWIG)
include_directories(${JNI_INCLUDE_DIRS})

set(CMAKE_SWIG_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/jtriskel" )
set(CMAKE_SWIG_FLAGS "-v;-package;jtriskel")
set_source_files_properties(./triskel.i PROPERTIES
    CPLUSPLUS ON
    COMPILE_DEFINITIONS TRISKEL_CAIRO)

# TODO: hack
include_directories("${CMAKE_SOURCE_DIR}/lib/include")

# Add swig module
swig_add_module(jtriskel java ./triskel.i)
target_link_libraries(jtriskel triskel cairo)



SET(JAVA_RUNTIME  $ENV{JAVA_HOME}/bin/java)
SET(JAVA_ARCHIVE  $ENV{JAVA_HOME}/bin/jar)
SET(JAVA_COMPILE  $ENV{JAVA_HOME}/bin/javac)

ADD_CUSTOM_COMMAND(TARGET jtriskel
                   POST_BUILD
                   COMMAND cmake -E echo "Compiling Java files..."
                   COMMAND ${JAVA_COMPILE} ${CMAKE_CURRENT_BINARY_DIR}/jtriskel/*.java -d ${CMAKE_CURRENT_BINARY_DIR}
                   COMMAND cmake -E echo "Creating jar file..."
                   COMMAND ${JAVA_ARCHIVE} cvf jtriskel.jar jtriskel
)

