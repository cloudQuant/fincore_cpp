cmake_minimum_required(VERSION 3.25)
project(fincore_required_component_missing LANGUAGES CXX)

find_package(fincore CONFIG REQUIRED COMPONENTS implot)
