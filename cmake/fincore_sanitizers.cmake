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
        target_compile_options(${target} ${compile_scope} -fsanitize=address)
        target_link_options(${target} ${link_scope} -fsanitize=address)
    endif()

    if(FINCORE_ENABLE_UBSAN)
        target_compile_options(${target} ${compile_scope} -fsanitize=undefined)
        target_link_options(${target} ${link_scope} -fsanitize=undefined)
    endif()
endfunction()
