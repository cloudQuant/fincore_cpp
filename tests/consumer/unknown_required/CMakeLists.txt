cmake_minimum_required(VERSION 3.25)
project(fincore_unknown_required_component LANGUAGES CXX)

find_package(fincore CONFIG REQUIRED COMPONENTS unsupported_component)
