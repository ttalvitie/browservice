cmake_minimum_required(VERSION 3.14)

project(retrojsvice)

# Find dependencies
find_package(Poco REQUIRED Foundation Net Util JSON)
find_package(JPEG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
# We need to find browservice headers (vice_plugin_api.h)
include_directories(${CMAKE_SOURCE_DIR}) 

# Generate C++ code from HTML
find_package(Python3 REQUIRED COMPONENTS Interpreter)

file(GLOB HTML_FILES "html/*.html")

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/html.cpp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/gen
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_html_cpp.py > ${CMAKE_CURRENT_BINARY_DIR}/gen/html.cpp.tmp
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/gen/html.cpp.tmp ${CMAKE_CURRENT_BINARY_DIR}/gen/html.cpp
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_CURRENT_BINARY_DIR}/gen/html.cpp.tmp
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gen_html_cpp.py ${HTML_FILES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating html.cpp"
)

# Source files
file(GLOB SOURCES "src/*.cpp")
list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/gen/html.cpp)

add_library(retrojsvice SHARED ${SOURCES})

# Include directories
target_include_directories(retrojsvice PRIVATE src)
# For html.cpp which might need to include headers associated with it, or vice-versa?
# The python script likely generates a cpp file that might or might not include headers.
# Let's check the python script output if needed, but for now assuming it's self contained or uses standard includes.

target_link_libraries(retrojsvice PRIVATE
    Poco::Foundation Poco::Net Poco::Util Poco::JSON
    JPEG::JPEG
    ZLIB::ZLIB
    OpenSSL::Crypto
)

# Set output name
set_target_properties(retrojsvice PROPERTIES PREFIX "")
set_target_properties(retrojsvice PROPERTIES SUFFIX ".so") # Force .so extension even on macOS to match expectation if hardcoded, though we can change expectation.

# On macOS, we want this to be a loadable module (bundle) usually, but shared lib is fine for dlopen.
