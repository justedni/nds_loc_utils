set(SOURCES_CORE
    cakp.cpp
    cakp.h
    lang.cpp
    lang.h
    lzss.cpp
    lzss.h
    nds.cpp
    nds.h
    p2.cpp
    p2.h
    rompatcher.cpp
    rompatcher.h
    strings.cpp
    strings.h
    stringtable.cpp
    stringtable.h
    utils.cpp
    utils.h
)

add_library(${PROJECT_NAME_CORE} STATIC ${SOURCES_CORE})

target_include_directories(${PROJECT_NAME_CORE} PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "src" FILES ${SOURCES_CORE})

install(TARGETS ${PROJECT_NAME_CORE}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
