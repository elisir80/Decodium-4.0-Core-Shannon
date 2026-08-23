function (add_decodium_sstv_test test_name)
  add_executable (${test_name} ${ARGN})
  target_link_libraries (${test_name}
    PRIVATE
      Decodium::SstvCore
      Qt6::Core
      Qt6::Test
    )
  set_target_properties (${test_name} PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    )
  add_test (NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
  set_tests_properties (${test_name} PROPERTIES LABELS "sstv;unit")
endfunction ()

add_decodium_sstv_test (test_sstv_mode_registry test_sstv_mode_registry.cpp)
add_decodium_sstv_test (test_sstv_timing test_sstv_timing.cpp)
add_decodium_sstv_test (test_sstv_vis test_sstv_vis.cpp)
add_decodium_sstv_test (test_sstv_fskid test_sstv_fskid.cpp)
add_decodium_sstv_test (test_sstv_resampler test_sstv_resampler.cpp)
add_decodium_sstv_test (test_sstv_audio_buffer test_sstv_audio_buffer.cpp)
add_decodium_sstv_test (test_sstv_tone_generator test_sstv_tone_generator.cpp)
add_decodium_sstv_test (test_sstv_rx_state_machine test_sstv_rx_state_machine.cpp)
add_decodium_sstv_test (test_sstv_tone_detector test_sstv_tone_detector.cpp)
add_decodium_sstv_test (test_sstv_image_core test_sstv_image_core.cpp)
