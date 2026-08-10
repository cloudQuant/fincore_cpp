if (NOT FINCORE_BUILD_IMPLOT)
    return()
endif()

set(_fincore_imgui_sources
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp)
set(_fincore_imgui_required_headers
    imgui.h
    imgui_internal.h
    imstb_rectpack.h
    imstb_textedit.h
    imstb_truetype.h)
set(_fincore_implot_sources
    implot.cpp
    implot_items.cpp)

if (NOT IS_DIRECTORY "${FINCORE_IMGUI_SOURCE_DIR}")
    message(FATAL_ERROR
        "FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all "
        "required files; "
        "received '${FINCORE_IMGUI_SOURCE_DIR}'")
endif()

foreach (_fincore_source IN LISTS _fincore_imgui_sources)
    if (NOT EXISTS "${FINCORE_IMGUI_SOURCE_DIR}/${_fincore_source}")
        message(FATAL_ERROR
            "FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all "
            "required files; "
            "missing '${FINCORE_IMGUI_SOURCE_DIR}/${_fincore_source}'")
    endif()
endforeach()
foreach (_fincore_header IN LISTS _fincore_imgui_required_headers)
    if (NOT EXISTS "${FINCORE_IMGUI_SOURCE_DIR}/${_fincore_header}")
        message(FATAL_ERROR
            "FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all "
            "required files; "
            "missing '${FINCORE_IMGUI_SOURCE_DIR}/${_fincore_header}'")
    endif()
endforeach()

if (NOT IS_DIRECTORY "${FINCORE_IMPLOT_SOURCE_DIR}")
    message(FATAL_ERROR
        "FINCORE_IMPLOT_SOURCE_DIR must name a local ImPlot checkout containing "
        "implot.h, implot.cpp, and implot_items.cpp; received "
        "'${FINCORE_IMPLOT_SOURCE_DIR}'")
endif()
foreach (_fincore_source IN LISTS _fincore_implot_sources)
    if (NOT EXISTS "${FINCORE_IMPLOT_SOURCE_DIR}/${_fincore_source}")
        message(FATAL_ERROR
            "FINCORE_IMPLOT_SOURCE_DIR must name a local ImPlot checkout containing "
            "implot.h, implot.cpp, and implot_items.cpp; missing "
            "'${FINCORE_IMPLOT_SOURCE_DIR}/${_fincore_source}'")
    endif()
endforeach()
if (NOT EXISTS "${FINCORE_IMPLOT_SOURCE_DIR}/implot.h")
    message(FATAL_ERROR
        "FINCORE_IMPLOT_SOURCE_DIR must name a local ImPlot checkout containing "
        "implot.h; missing '${FINCORE_IMPLOT_SOURCE_DIR}/implot.h'")
endif()

if (TARGET imgui)
    message(FATAL_ERROR
        "fincore builds its imgui target from FINCORE_IMGUI_SOURCE_DIR, but an "
        "imgui target already exists")
endif()
add_library(imgui STATIC)
target_sources(imgui PRIVATE
    ${FINCORE_IMGUI_SOURCE_DIR}/imgui.cpp
    ${FINCORE_IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${FINCORE_IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${FINCORE_IMGUI_SOURCE_DIR}/imgui_widgets.cpp)
target_compile_features(imgui PUBLIC cxx_std_23)
target_include_directories(imgui PUBLIC "${FINCORE_IMGUI_SOURCE_DIR}")

if (TARGET implot)
    message(FATAL_ERROR
        "fincore builds its implot target from FINCORE_IMPLOT_SOURCE_DIR, but an "
        "implot target already exists")
endif()
add_library(implot STATIC)
target_sources(implot PRIVATE
    ${FINCORE_IMPLOT_SOURCE_DIR}/implot.cpp
    ${FINCORE_IMPLOT_SOURCE_DIR}/implot_items.cpp)
target_compile_features(implot PUBLIC cxx_std_23)
target_include_directories(implot PUBLIC "${FINCORE_IMPLOT_SOURCE_DIR}")
target_link_libraries(implot PUBLIC imgui)

add_library(fincore_implot src/viz/implot_backend.cpp)
add_library(fincore::implot ALIAS fincore_implot)
target_compile_features(fincore_implot PUBLIC cxx_std_23)
target_include_directories(fincore_implot PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(fincore_implot
    PUBLIC fincore::fincore
    PRIVATE implot)

fincore_enable_sanitizers(fincore_implot)

if (FINCORE_BUILD_IMPLOT_DEMO)
    set(_fincore_imgui_backend_sources
        backends/imgui_impl_glfw.cpp
        backends/imgui_impl_opengl3.cpp)
    foreach (_fincore_source IN LISTS _fincore_imgui_backend_sources)
        if (NOT EXISTS "${FINCORE_IMGUI_SOURCE_DIR}/${_fincore_source}")
            message(FATAL_ERROR
                "FINCORE_BUILD_IMPLOT_DEMO requires Dear ImGui backend source "
                "'${FINCORE_IMGUI_SOURCE_DIR}/${_fincore_source}'")
        endif()
    endforeach()

    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)

    if (TARGET glfw)
        set(_fincore_glfw_target glfw)
    elseif (TARGET glfw3::glfw)
        set(_fincore_glfw_target glfw3::glfw)
    elseif (TARGET glfw::glfw)
        set(_fincore_glfw_target glfw::glfw)
    else()
        message(FATAL_ERROR
            "FINCORE_BUILD_IMPLOT_DEMO could not find a GLFW CMake target after "
            "find_package(glfw3)")
    endif()

    if (TARGET OpenGL::GL)
        set(_fincore_opengl_target OpenGL::GL)
    elseif (TARGET OpenGL::OpenGL)
        set(_fincore_opengl_target OpenGL::OpenGL)
    else()
        message(FATAL_ERROR
            "FINCORE_BUILD_IMPLOT_DEMO could not find an OpenGL CMake target after "
            "find_package(OpenGL)")
    endif()

    add_executable(fincore_implot_demo examples/implot_dashboard.cpp)
    target_sources(fincore_implot_demo PRIVATE
        ${FINCORE_IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${FINCORE_IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
    target_include_directories(fincore_implot_demo PRIVATE
        "${FINCORE_IMGUI_SOURCE_DIR}/backends")
    target_link_libraries(fincore_implot_demo PRIVATE
        fincore::implot
        implot
        ${_fincore_glfw_target}
        ${_fincore_opengl_target})
endif()
