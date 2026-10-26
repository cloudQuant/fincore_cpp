cmake_minimum_required(VERSION 3.25)
project(fincore_optional_component_missing LANGUAGES CXX)

find_package(fincore CONFIG REQUIRED OPTIONAL_COMPONENTS implot)

if(NOT TARGET fincore::fincore)
    message(FATAL_ERROR "the headless fincore target was not exported")
endif()
if(fincore_implot_FOUND OR TARGET fincore::implot)
    message(FATAL_ERROR "an absent optional implot component was reported as available")
endif()
