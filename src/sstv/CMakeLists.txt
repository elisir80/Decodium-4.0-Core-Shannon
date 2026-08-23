set (DECODIUM_SSTV_CORE_SOURCES
  core/SstvFskIdCodec.cpp
  core/SstvModeRegistry.cpp
  core/SstvModeSpec.cpp
  core/SstvTimingAccumulator.cpp
  core/SstvVisCodec.cpp
  )

set (DECODIUM_SSTV_CORE_HEADERS
  core/SstvFskIdCodec.h
  core/SstvModeRegistry.h
  core/SstvModeSpec.h
  core/SstvTimingAccumulator.h
  core/SstvTypes.h
  core/SstvVisCodec.h
  )

add_library (decodium_sstv_core STATIC
  ${DECODIUM_SSTV_CORE_SOURCES}
  ${DECODIUM_SSTV_CORE_HEADERS}
  )
add_library (Decodium::SstvCore ALIAS decodium_sstv_core)

target_include_directories (decodium_sstv_core
  PUBLIC
    ${CMAKE_SOURCE_DIR}
  )
target_compile_features (decodium_sstv_core PUBLIC cxx_std_17)
set_target_properties (decodium_sstv_core PROPERTIES
  AUTOMOC OFF
  AUTORCC OFF
  AUTOUIC OFF
  CXX_EXTENSIONS OFF
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  )
