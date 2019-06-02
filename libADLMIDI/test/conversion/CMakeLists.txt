
set(CMAKE_CXX_STANDARD 11)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../common
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src)

add_executable(ConversionTest conversion.cpp $<TARGET_OBJECTS:Catch-objects>)

target_compile_definitions(ConversionTest PRIVATE GSL_THROW_ON_CONTRACT_VIOLATION)
add_test(NAME ConversionTest COMMAND ConversionTest)
