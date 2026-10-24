include_guard(GLOBAL)

function(fincore_enable_sanitizers target)
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "INTERFACE_LIBRARY")
        set(compile_scope INTERFACE)
        set(link_scope INTERFACE)
    elseif(target_type STREQUAL "STATIC_LIBRARY")
        set(compile_scope PRIVATE)
        # A static archive has no link step. Its instrumented objects require
        # the final consumer to link the sanitizer runtimes.
        set(link_scope INTERFACE)
    else()
        set(compile_scope PRIVATE)
        set(link_scope PRIVATE)
    endif()

    if(FINCORE_ENABLE_ASAN)
        if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            target_compile_options(${target} ${compile_scope} /fsanitize=address)
            target_link_options(${target} ${link_scope} /fsanitize=address)
        else()
            target_compile_options(${target} ${compile_scope} -fsanitize=address)
            target_link_options(${target} ${link_scope} -fsanitize=address)
        endif()
    endif()

    if(FINCORE_ENABLE_UBSAN)
        if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            message(FATAL_ERROR
                "FINCORE_ENABLE_UBSAN is not supported by MSVC; use a "
                "Clang or GCC toolchain for undefined-behaviour checks")
        endif()
        target_compile_options(${target} ${compile_scope} -fsanitize=undefined)
        target_link_options(${target} ${link_scope} -fsanitize=undefined)
    endif()
endfunction()
