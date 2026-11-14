# SPDX-License-Identifier: Apache-2.0
# BK Open Framework: ESP-IDF Component CMake Bridge (idf_component_register -> Armino / Host CMake)
# Allows pure-C / middleware components from https://components.espressif.com/
# to register seamlessly inside bk-open-framework or Armino SMP builds.

if(NOT COMMAND idf_component_register)
    function(idf_component_register)
        cmake_parse_arguments(
            IDF_COMP
            ""
            ""
            "SRCS;SRC_DIRS;INCLUDE_DIRS;PRIV_INCLUDE_DIRS;REQUIRES;PRIV_REQUIRES;EXCLUDE_SRCS"
            ${ARGN}
        )

        set(_resolved_srcs ${IDF_COMP_SRCS})
        foreach(_sdir IN LISTS IDF_COMP_SRC_DIRS)
            file(GLOB _dir_c_srcs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/${_sdir}/*.c")
            list(APPEND _resolved_srcs ${_dir_c_srcs})
        endforeach()
        if(IDF_COMP_EXCLUDE_SRCS AND _resolved_srcs)
            list(REMOVE_ITEM _resolved_srcs ${IDF_COMP_EXCLUDE_SRCS})
        endif()

        # Map common ESP-IDF component dependency names to bk_esp_compat / bk-open-framework
        set(_mapped_reqs)
        foreach(_req IN LISTS IDF_COMP_REQUIRES IDF_COMP_PRIV_REQUIRES)
            if(_req MATCHES "^(esp_timer|esp_event|nvs_flash|esp_common|log|heap|freertos|driver|button|led_indicator)$")
                list(APPEND _mapped_reqs bk_esp_compat)
            else()
                list(APPEND _mapped_reqs ${_req})
            endif()
        endforeach()
        list(REMOVE_DUPLICATES _mapped_reqs)

        if(COMMAND armino_component_register)
            armino_component_register(
                SRCS ${_resolved_srcs}
                INCLUDE_DIRS ${IDF_COMP_INCLUDE_DIRS}
                PRIV_INCLUDE_DIRS ${IDF_COMP_PRIV_INCLUDE_DIRS}
                REQUIRES ${_mapped_reqs}
            )
        else()
            get_filename_component(_comp_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
            if(_resolved_srcs)
                add_library(${_comp_name} STATIC ${_resolved_srcs})
                foreach(_inc IN LISTS IDF_COMP_INCLUDE_DIRS)
                    target_include_directories(${_comp_name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${_inc}")
                endforeach()
                foreach(_pinc IN LISTS IDF_COMP_PRIV_INCLUDE_DIRS)
                    target_include_directories(${_comp_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${_pinc}")
                endforeach()
                if(_mapped_reqs)
                    target_link_libraries(${_comp_name} PUBLIC ${_mapped_reqs})
                endif()
            else()
                add_library(${_comp_name} INTERFACE)
                foreach(_inc IN LISTS IDF_COMP_INCLUDE_DIRS)
                    target_include_directories(${_comp_name} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/${_inc}")
                endforeach()
            endif()
        endif()
    endfunction()
endif()
