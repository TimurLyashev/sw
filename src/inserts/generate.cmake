########################################

# we disable EXECUTABLE mode for executables with same configs
if (CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG)
    set(EXECUTABLE 0)
endif()

# CPPAN_BUILD_SHARED_LIBS has influence on config vars
if (EXECUTABLE)
    # TODO: try to work 0->1 <- why? maybe left as is?
    set(CPPAN_BUILD_SHARED_LIBS 0)
endif()

# after all settings
get_configuration_variables_unhashed()
get_configuration_variables()

set(build_dir_name build)
set(build_dir ${current_dir}/${build_dir_name}/${config_dir})
set(export_dir ${build_dir}/exports)
set(import ${export_dir}/${variable_name}.cmake)
#set(import_fixed ${export_dir}/${variable_name}-fixed.cmake) # old
set(import_fixed ${storage_dir_exp}/${config_dir}/${target}.cmake)
set(aliases_file ${export_dir}/${variable_name}-aliases.cmake)
set(variables_file ${build_dir}.gen.vars)
set(lock ${build_dir}/cppan_generate.lock)

########################################

if (NOT EXISTS ${import} OR NOT EXISTS ${import_fixed})
    file(
        LOCK ${lock}
        GUARD FILE # CMake bug workaround https://gitlab.kitware.com/cmake/cmake/issues/16295
        TIMEOUT 0
        RESULT_VARIABLE lock_result
    )
    if (NOT ${lock_result} EQUAL 0)
        message(STATUS "WARNING: Target: ${target}")
        message(STATUS "WARNING: Other project is being bootstrapped right now or you hit a circular deadlock.")
        message(STATUS "WARNING: If you aren't building other projects right now feel free to kill this process or it will be stopped in 90 seconds.")

        file(
            LOCK ${lock}
            GUARD FILE # CMake bug workaround https://gitlab.kitware.com/cmake/cmake/issues/16295
            TIMEOUT 90
            RESULT_VARIABLE lock_result
        )

        if (NOT ${lock_result} EQUAL 0)
            message(FATAL_ERROR "Lock error: ${lock_result}")
        endif()
    endif()

    # double check
    if (NOT EXISTS ${import} OR NOT EXISTS ${import_fixed})
        set(generator ${CMAKE_GENERATOR})

        # copy cmake cache for faster bootstrapping
        set(to ${build_dir}/CMakeFiles/${CMAKE_VERSION})
        if (NOT EXISTS ${to})
            if (EXECUTABLE)
                # TODO: fix executables bootstrapping
                # BUG: copying bad cmake config dirs (32 - 64 bits)
                #set(from ${storage_dir_cfg}/${config_dir}/CMakeFiles/${CMAKE_VERSION})
            else()
                set(from ${CMAKE_BINARY_DIR}/CMakeFiles/${CMAKE_VERSION})
            endif()

            if (EXISTS ${from})
                execute_process(
                    COMMAND ${CMAKE_COMMAND} -E copy_directory ${from} ${to}
                    RESULT_VARIABLE ret
                )
                check_result_variable(${ret})
                #message("Copied!")
            else()
                message(WARNING "From dir does not exist! ${from}")
            endif()
        else()
            #message("To dir ${to}")
        endif()

        # prepare variables for child process
        set(OUTPUT_DIR ${config}) # ???

        if (NOT CPPAN_COMMAND)
            message(FATAL_ERROR "cppan command '${CPPAN_COMMAND}' not found - ${CMAKE_CURRENT_LIST_FILE} - ${target}")
        endif()

        #
        clear_variables(GEN_CHILD_VARS)
        if (NOT EXECUTABLE)
            add_variable(GEN_CHILD_VARS CMAKE_BUILD_TYPE)
        endif()
        add_variable(GEN_CHILD_VARS OUTPUT_DIR)
        add_variable(GEN_CHILD_VARS CPPAN_BUILD_SHARED_LIBS)
        # if turned on, build exe with the same config (arch, toolchain, generator etc.)
        # do not pass this further, only projects with this option enabled will get it
        #add_variable(GEN_CHILD_VARS CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG)
        # if turned on, build exe with the same configiguration (debug, relwithdebinfo etc.)
        add_variable(GEN_CHILD_VARS CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIGURATION)
        add_variable(GEN_CHILD_VARS CPPAN_COMMAND)
        if (NOT EXECUTABLE)
            add_variable(GEN_CHILD_VARS CPPAN_MT_BUILD) # not for exe
        endif()
        add_variable(GEN_CHILD_VARS CPPAN_CMAKE_VERBOSE)
        add_variable(GEN_CHILD_VARS CPPAN_DEBUG_STACK_SPACE)
        add_variable(GEN_CHILD_VARS CPPAN_BUILD_VERBOSE)
        add_variable(GEN_CHILD_VARS CPPAN_BUILD_WARNING_LEVEL)
        add_variable(GEN_CHILD_VARS CPPAN_COPY_ALL_LIBRARIES_TO_OUTPUT)
        add_variable(GEN_CHILD_VARS XCODE)
        add_variable(GEN_CHILD_VARS VISUAL_STUDIO)
        add_variable(GEN_CHILD_VARS NINJA)
        write_variables_file(GEN_CHILD_VARS ${variables_file})
        #

        set(toolset)
        if (CMAKE_GENERATOR_TOOLSET)
            set(toolset "-T${CMAKE_GENERATOR_TOOLSET}")
        endif()

        message(STATUS "")
        message(STATUS "Preparing build tree for ${target} (${config_unhashed} - ${config_dir} - ${generator})")
        message(STATUS "")

        # call cmake
        if (EXECUTABLE)# AND NOT CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG)
                cppan_debug_message("COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DVARIABLES_FILE=${variables_file}")
                execute_process(
                    COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DVARIABLES_FILE=${variables_file}
                    RESULT_VARIABLE ret
                )
                check_result_variable(${ret})
        else()
            if (CMAKE_TOOLCHAIN_FILE)
                cppan_debug_message("COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                        -G \"${generator}\"
                        -DVARIABLES_FILE=${variables_file}")
                execute_process(
                    COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                        -G "${generator}"
                        -DVARIABLES_FILE=${variables_file}
                    RESULT_VARIABLE ret
                )
                check_result_variable(${ret})
            else(CMAKE_TOOLCHAIN_FILE)
                cppan_debug_message("COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                        -G \"${generator}\"
                        -DVARIABLES_FILE=${variables_file}")
                execute_process(
                    COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                        -G "${generator}"
                        ${toolset}
                        -DVARIABLES_FILE=${variables_file}
                    RESULT_VARIABLE ret
                )
                check_result_variable(${ret})
            endif(CMAKE_TOOLCHAIN_FILE)
        endif()

        # fix imports
        # TODO: move exports to exp dir
        file(WRITE ${aliases_file} "${aliases}")
        cppan_debug_message("COMMAND ${CPPAN_COMMAND} internal-fix-imports ${target} ${aliases_file} ${import} ${import_fixed}")
        execute_process(
            COMMAND ${CPPAN_COMMAND} internal-fix-imports ${target} ${aliases_file} ${import} ${import_fixed}
            RESULT_VARIABLE ret
        )
        check_result_variable(${ret})

        # create links to solution files
        # TODO: replace condition with if (VS generator)
        set(target_lnk ${storage_dir_lnk}/${config_dir}/${target}.sln.lnk)
        if (VISUAL_STUDIO AND NOT EXISTS ${target_lnk})
            cppan_debug_message("COMMAND ${CPPAN_COMMAND} internal-create-link-to-solution ${build_dir} ${target_lnk}")
            execute_process(
                COMMAND ${CPPAN_COMMAND} internal-create-link-to-solution ${build_dir}/${package_hash_short}.sln ${target_lnk}
                RESULT_VARIABLE ret
            )
            # no check_result_variable(): ignore errors
        endif()

        cppan_debug_message("-- Prepared  build tree for ${target} (${config_unhashed} - ${config_dir} - ${generator})")
    endif()

    file(LOCK ${lock} RELEASE)
endif()

########################################
