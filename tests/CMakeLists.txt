find_package (Qt6Test 5 REQUIRED)

#
# Compiler options
#
set (CMAKE_CXX_STANDARD 11)
add_compile_options ("$<$<COMPILE_LANGUAGE:Fortran>:-Wall;-Wno-conversion;-fno-second-underscore;-fno-f2c>")
add_compile_options ("$<$<AND:$<COMPILE_LANGUAGE:Fortran>,$<CONFIG:Debug>>:-fbounds-check>")
add_compile_options ("$<$<AND:$<COMPILE_LANGUAGE:Fortran>,$<NOT:$<CONFIG:Debug>>>:-funroll-all-loops>")
add_compile_options ("$<$<AND:$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>,$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:AppleClang>>>:-Wall;-Wextra>")
add_compile_options ("$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU>>:-Wno-pragmas>")
add_compile_options ("$<$<AND:$<OR:$<C_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:GNU>>,$<NOT:$<CONFIG:Debug>>>:-fdata-sections;-ffunction-sections>")
if (${OPENMP_FOUND} OR APPLE)
  add_compile_options ("$<$<AND:$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>,$<C_COMPILER_ID:GNU>>:${OpenMP_C_FLAGS}>")
endif ()

# Tell CMake to run moc when necessary
set (CMAKE_AUTOMOC ON)
include_directories (
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}
  ${DECODIUM_SOURCE_DIRS}
  )

add_executable (test_qt_helpers
  test_qt_helpers.cpp
  ../Detector/MSK144DecodeWorker.cpp
  ../Sequencer/MessageTokenRules.cpp)
target_include_directories (test_qt_helpers PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries (test_qt_helpers wsjt_qt wsjt_cxx wsjt_fort Qt6::Test)
add_test (NAME test_qt_helpers COMMAND $<TARGET_FILE:test_qt_helpers>)
set_tests_properties (test_qt_helpers PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable (test_qso_sequencer_rules
  test_qso_sequencer_rules.cpp
  ../Sequencer/QsoSequencerRules.cpp
  ../Sequencer/MessageTokenRules.cpp
  ../src/radio/Radio.cpp)
target_link_libraries (test_qso_sequencer_rules Qt6::Test Qt6::Core)
add_test (NAME test_qso_sequencer_rules COMMAND $<TARGET_FILE:test_qso_sequencer_rules>)

add_executable (ft4_stage_compare ft4_stage_compare.cpp)
target_link_libraries (ft4_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)

add_executable (ft8_stage_compare ft8_stage_compare.cpp)
target_link_libraries (ft8_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
if (MINGW)
  target_link_options (ft8_stage_compare PRIVATE -Wl,--stack,67108864)
endif ()

add_executable (ft2_stage_compare ft2_stage_compare.cpp)
target_link_libraries (ft2_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_dependencies (test_qt_helpers ft2_stage_compare)

add_executable (q65_stage_compare q65_stage_compare.cpp ../Detector/Q65DecodeWorker.cpp)
target_include_directories (q65_stage_compare PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries (q65_stage_compare wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)

add_executable (ft2_equalized_compare ft2_equalized_compare.cpp)
target_link_libraries (ft2_equalized_compare wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_dependencies (test_qt_helpers ft2_equalized_compare)

add_executable (ft2_make_test_wav ft2_make_test_wav.cpp)
target_link_libraries (ft2_make_test_wav wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_dependencies (test_qt_helpers ft2_make_test_wav)

add_executable (test_tx_pipeline test_tx_pipeline.cpp)
target_link_libraries (test_tx_pipeline wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_test (NAME test_tx_pipeline COMMAND $<TARGET_FILE:test_tx_pipeline>)

add_executable (test_jt4_roundtrip test_jt4_roundtrip.cpp)
target_link_libraries (test_jt4_roundtrip wsjt_qt wsjt_cxx Qt6::Core)
add_test (NAME test_jt4_roundtrip COMMAND $<TARGET_FILE:test_jt4_roundtrip>)

add_executable (test_dx_cluster_band test_dx_cluster_band.cpp ../src/services/DecodiumDxCluster.cpp)
target_link_libraries (test_dx_cluster_band Qt6::Test Qt6::Network Qt6::Core)
add_test (NAME test_dx_cluster_band COMMAND $<TARGET_FILE:test_dx_cluster_band>)

add_executable (test_map_layer_service
  test_map_layer_service.cpp
  ../src/services/MapIntelligenceService.cpp
  ../src/services/MapIntelligenceService.h
  ../src/services/MapBaseMapService.cpp
  ../src/services/MapBaseMapService.h
  ../src/services/MapPskFeedService.cpp
  ../src/services/MapPskFeedService.h
  ../src/services/MapOperationsService.cpp
  ../src/services/MapOperationsService.h
  ../src/services/RotatorService.cpp
  ../src/services/RotatorService.h
  ../src/services/MapExternalOverlayService.cpp
  ../src/services/MapExternalOverlayService.h
  ../src/services/DxccLookup.cpp
  ../src/services/DxccLookup.h
  ../src/services/MapLayerModel.cpp
  ../src/services/MapLayerModel.h)
target_link_libraries (test_map_layer_service Qt6::Test Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql Qt6::Concurrent)
qt_add_resources (test_map_layer_service test_map_layer_service_award_resources
  PREFIX "/"
  BASE "${CMAKE_SOURCE_DIR}/resources/map"
  FILES
    "${CMAKE_SOURCE_DIR}/resources/map/decodium-awards-catalog.json"
  )
add_test (NAME test_map_layer_service COMMAND $<TARGET_FILE:test_map_layer_service>)

add_executable (test_sgp4
  test_sgp4.cpp
  ../src/services/satellite/sgp4/SGP4.cpp
  ../src/services/satellite/sgp4/SGP4.h)
target_link_libraries (test_sgp4 Qt6::Test Qt6::Core)
add_test (NAME test_sgp4 COMMAND $<TARGET_FILE:test_sgp4>)

add_executable (test_rotator_service
  test_rotator_service.cpp
  ../src/services/RotatorService.cpp
  ../src/services/RotatorService.h)
target_link_libraries (test_rotator_service Qt6::Test Qt6::Core Qt6::Gui Qt6::Network)
add_test (NAME test_rotator_service COMMAND $<TARGET_FILE:test_rotator_service>)

add_executable (test_cat4om_manager
  test_cat4om_manager.cpp
  ../src/radio/DecodiumCat4OmManager.cpp
  ../src/radio/DecodiumCat4OmManager.h)
target_link_libraries (test_cat4om_manager Qt6::Test Qt6::Core Qt6::Network Qt6::WebSockets)
add_test (NAME test_cat4om_manager COMMAND $<TARGET_FILE:test_cat4om_manager>)

add_executable (test_udp_client_id test_udp_client_id.cpp)
target_link_libraries (test_udp_client_id wsjt_qt Qt6::Test Qt6::Network Qt6::Core)
add_test (NAME test_udp_client_id COMMAND $<TARGET_FILE:test_udp_client_id>)

add_executable (test_ftx_weak_decode test_ftx_weak_decode.cpp)
target_link_libraries (test_ftx_weak_decode wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_test (NAME test_ftx_weak_decode COMMAND $<TARGET_FILE:test_ftx_weak_decode>)
set_tests_properties (test_ftx_weak_decode PROPERTIES ENVIRONMENT "DECODIUM_WEAK_TEST_SNR=-15")
add_test (NAME test_ft8_weak_decode_deep COMMAND $<TARGET_FILE:test_ftx_weak_decode>)
set_tests_properties (test_ft8_weak_decode_deep PROPERTIES
  ENVIRONMENT "DECODIUM_WEAK_TEST_MODE=FT8;DECODIUM_WEAK_TEST_SNR=-26;DECODIUM_WEAK_TEST_TRIALS=1")
add_test (NAME test_ft8_hinted_decode_very_deep COMMAND $<TARGET_FILE:test_ftx_weak_decode>)
set_tests_properties (test_ft8_hinted_decode_very_deep PROPERTIES
  ENVIRONMENT "DECODIUM_WEAK_TEST_MODE=FT8;DECODIUM_WEAK_TEST_SNR=-27;DECODIUM_WEAK_TEST_TRIALS=1;DECODIUM_WEAK_TEST_ACCEPT_MISS=1"
  LABELS "accepted-limit")

add_executable (test_ft2_qso_sim test_ft2_qso_sim.cpp)
target_link_libraries (test_ft2_qso_sim wsjt_qt wsjt_cxx wsjt_fort Qt6::Core)
add_test (NAME test_ft2_qso_sim COMMAND $<TARGET_FILE:test_ft2_qso_sim>)

add_executable (test_secure_settings test_secure_settings.cpp)
target_link_libraries (test_secure_settings wsjt_qt wsjt_cxx Qt6::Test)
add_test (NAME test_secure_settings COMMAND $<TARGET_FILE:test_secure_settings>)

add_executable (test_callsign_intelligence
  test_callsign_intelligence.cpp
  ../src/services/CallsignIntelligenceService.cpp
  ../src/services/CallsignIntelligenceService.h
  ../src/services/DxccLookup.cpp
  ../src/services/DxccLookup.h
  ../src/security/SecureSettings.cpp)
target_link_libraries (test_callsign_intelligence Qt6::Test Qt6::Core Qt6::Network Qt6::Sql Qt6::Gui)
add_test (NAME test_callsign_intelligence COMMAND $<TARGET_FILE:test_callsign_intelligence>)

add_executable (test_filedownload test_filedownload.cpp)
target_link_libraries (test_filedownload wsjt_qt wsjt_cxx Qt6::Test)
add_test (NAME test_filedownload COMMAND $<TARGET_FILE:test_filedownload>)

# 1.0.234 Sprint 3 / Phase 2 performance roadmap:
# Test header-only per StreamingListModel<T> in lib/ui/. Nessuna dipendenza
# da wsjt_qt / wsjt_cxx -- solo Qt6::Test + Qt6::Concurrent (QtConcurrent::run
# per il caso thread-safety). include_directories aggiunge ${CMAKE_SOURCE_DIR}
# alla riga 19, quindi #include "lib/ui/StreamingListModel.h" risolve.
find_package (Qt6 COMPONENTS Concurrent REQUIRED)
add_executable (test_streaming_list_model test_streaming_list_model.cpp)
target_link_libraries (test_streaming_list_model Qt6::Test Qt6::Concurrent)
add_test (NAME test_streaming_list_model COMMAND $<TARGET_FILE:test_streaming_list_model>)

add_executable (test_decode_list_model
  test_decode_list_model.cpp
  ../src/models/DecodeListModel.cpp)
target_link_libraries (test_decode_list_model Qt6::Test Qt6::Core)
add_test (NAME test_decode_list_model COMMAND $<TARGET_FILE:test_decode_list_model>)

add_executable (test_ft_decode_thread_budget test_ft_decode_thread_budget.cpp)
target_link_libraries (test_ft_decode_thread_budget Qt6::Test Qt6::Core)
add_test (NAME test_ft_decode_thread_budget COMMAND $<TARGET_FILE:test_ft_decode_thread_budget>)

add_executable (test_legacy_decode_window test_legacy_decode_window.cpp)
target_link_libraries (test_legacy_decode_window Qt6::Test Qt6::Core)
add_test (NAME test_legacy_decode_window COMMAND $<TARGET_FILE:test_legacy_decode_window>)

add_executable (test_ft2link test_ft2link.cpp)
target_link_libraries (test_ft2link ft2link_core Qt6::Test)
add_test (NAME test_ft2link COMMAND $<TARGET_FILE:test_ft2link>)

add_executable (test_ft2link_qml_adapter
  test_ft2link_qml_adapter.cpp
  ../controllers/FT2LinkQmlAdapter.cpp
  ../controllers/FT2LinkQmlAdapter.hpp)
target_link_libraries (test_ft2link_qml_adapter ft2link_core Qt6::Test Qt6::Core)
target_compile_definitions (test_ft2link_qml_adapter
  PRIVATE
    DECODIUM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    DECODIUM_FT2LINK_QML_ADAPTER_STANDALONE=1)
add_test (NAME test_ft2link_qml_adapter COMMAND $<TARGET_FILE:test_ft2link_qml_adapter>)

add_executable (ft2link_rflab_corpus
  ft2link_rflab_corpus.cpp
  ../controllers/FT2LinkQmlAdapter.cpp
  ../controllers/FT2LinkQmlAdapter.hpp)
target_link_libraries (ft2link_rflab_corpus ft2link_core Qt6::Core)
target_compile_definitions (ft2link_rflab_corpus
  PRIVATE
    DECODIUM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    DECODIUM_FT2LINK_QML_ADAPTER_STANDALONE=1)

if (WSJT_BUILD_UTILS)
  # CLI smoke tests for utility binaries that are built in the main project.
  foreach(_utility_usage_target IN ITEMS
      test_q65 test_snr testEchoCall encode77 hash22calc msk144code msk144sim
      fst4sim ldpcsim240_101 ldpcsim240_74 sfoxsim sfrx sftx)
    if(TARGET ${_utility_usage_target})
      set(_usage_regex "Usage:")
      if(_utility_usage_target STREQUAL "hash22calc")
        set(_usage_regex "Usage:|Given a valid callsign")
      endif()
      add_test (NAME ${_utility_usage_target}_usage COMMAND $<TARGET_FILE:${_utility_usage_target}>)
      set_tests_properties (${_utility_usage_target}_usage PROPERTIES PASS_REGULAR_EXPRESSION "${_usage_regex}")
    endif()
  endforeach()
endif ()
