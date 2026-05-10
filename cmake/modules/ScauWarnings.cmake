# Defines INTERFACE target scau::warnings carrying project-wide warning flags.
# Linking against scau::warnings opts into the warning set without polluting
# global flags.

if(TARGET scau::warnings)
    return()
endif()

add_library(scau_warnings INTERFACE)
add_library(scau::warnings ALIAS scau_warnings)

if(MSVC)
    target_compile_options(scau_warnings INTERFACE
        /W4
        /permissive-
        /w14242 /w14254 /w14263 /w14265 /w14287
        /we4289 /w14296 /w14311 /w14545 /w14546
        /w14547 /w14549 /w14555 /w14619 /w14640
        /w14826 /w14905 /w14906 /w14928
    )
else()
    target_compile_options(scau_warnings INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast
        -Wcast-align -Wunused -Woverloaded-virtual
        -Wconversion -Wsign-conversion
        -Wdouble-promotion -Wformat=2
    )
endif()

option(SCAU_WARNINGS_AS_ERRORS "Promote warnings to errors" OFF)
if(SCAU_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(scau_warnings INTERFACE /WX)
    else()
        target_compile_options(scau_warnings INTERFACE -Werror)
    endif()
endif()
