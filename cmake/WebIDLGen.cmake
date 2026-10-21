# Generates V8 bindings from one or more WebIDL files.
#
#   webidl_gen(<target> HEADER <out.h> SOURCE <out.cpp> IDL <idl> [<idl> ...])

function(webidl_gen target_name)
    cmake_parse_arguments(WEBIDL "" "HEADER;SOURCE" "IDL" ${ARGN})

    if(NOT WEBIDL_HEADER OR NOT WEBIDL_SOURCE OR NOT WEBIDL_IDL)
        message(FATAL_ERROR
            "webidl_gen(${target_name}): HEADER, SOURCE and IDL are required")
    endif()

    set(WEBIDLGEN_EXECUTABLE $<TARGET_FILE:webidlgen>)

    get_filename_component(output_header_dir ${WEBIDL_HEADER} DIRECTORY)
    get_filename_component(output_source_dir ${WEBIDL_SOURCE} DIRECTORY)
    file(MAKE_DIRECTORY ${output_header_dir})
    file(MAKE_DIRECTORY ${output_source_dir})

    add_custom_command(
        OUTPUT ${WEBIDL_HEADER} ${WEBIDL_SOURCE}
        COMMAND ${WEBIDLGEN_EXECUTABLE}
                --header ${WEBIDL_HEADER}
                --source ${WEBIDL_SOURCE}
                ${WEBIDL_IDL}
        DEPENDS ${WEBIDL_IDL} webidlgen
        COMMENT "Generating WebIDL bindings: ${target_name}"
        VERBATIM
    )

    add_custom_target(${target_name} DEPENDS ${WEBIDL_HEADER} ${WEBIDL_SOURCE})
endfunction()
