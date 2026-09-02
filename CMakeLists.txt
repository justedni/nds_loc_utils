cmake_minimum_required(VERSION 3.16)

project(NDSLocUtils
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(SOURCES_CORE
    src/core/cakp.cpp
    src/core/cakp.h
    src/core/lang.cpp
    src/core/lang.h
    src/core/lzss.cpp
    src/core/lzss.h
    src/core/nds.cpp
    src/core/nds.h
    src/core/p2.cpp
    src/core/p2.h
    src/core/strings.cpp
    src/core/strings.h
    src/core/stringtable.cpp
    src/core/stringtable.h
    src/core/utils.cpp
    src/core/utils.h
)

set(SOURCES_UI
    src/ui/main.cpp
    src/ui/QtNDSLocUtils.cpp
    src/ui/QtNDSLocUtils.h
    src/ui/QtNDSLocUtils.qrc
    src/ui/QtNDSLocUtils.ui
    src/ui/Worker.cpp
    src/ui/Worker.h
)

set(PROJECT_NAME_GUI ${PROJECT_NAME}Gui)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
)

add_executable(${PROJECT_NAME_GUI} ${SOURCES_CORE} ${SOURCES_UI})

target_link_libraries(${PROJECT_NAME_GUI} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_include_directories(${PROJECT_NAME_GUI} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

source_group("src" FILES ${SOURCES_CORE} ${SOURCES_GUI})


if(WIN32)
    set_target_properties(${PROJECT_NAME_GUI} PROPERTIES
        WIN32_EXECUTABLE ON
    )
    
    get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    get_filename_component(QT_INSTALL_DIR "${QT_BIN_DIR}" DIRECTORY)
    
    set(QT_DLLS
        Qt6Core$<$<CONFIG:Debug>:d>.dll
        Qt6Gui$<$<CONFIG:Debug>:d>.dll
        Qt6Widgets$<$<CONFIG:Debug>:d>.dll
    )
    
    foreach(QT_DLL ${QT_DLLS})
        add_custom_command(TARGET ${PROJECT_NAME_GUI} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_BIN_DIR}/${QT_DLL}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME_GUI}>/"
            COMMENT "Copying ${QT_DLL}..."
        )
    endforeach()
    
    add_custom_command(TARGET ${PROJECT_NAME_GUI} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory 
            "$<TARGET_FILE_DIR:${PROJECT_NAME_GUI}>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_INSTALL_DIR}/plugins/platforms/qwindows$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME_GUI}>/platforms/"
        COMMENT "Copying platform plugin..."
    )
    
    add_custom_command(TARGET ${PROJECT_NAME_GUI} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory 
            "$<TARGET_FILE_DIR:${PROJECT_NAME_GUI}>/styles"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_INSTALL_DIR}/plugins/styles/qmodernwindowsstyle$<$<CONFIG:Debug>:d>.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME_GUI}>/styles/"
        COMMENT "Copying styles plugin..."
    )

    file(GLOB BIN_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.*")
    if(BIN_RESOURCES)
        add_custom_command(TARGET ${PROJECT_NAME_GUI} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME_GUI}>/Data"
            COMMENT "Creating Data directory..."
        )
        
        foreach(BIN_FILE ${BIN_RESOURCES})
            add_custom_command(TARGET ${PROJECT_NAME_GUI} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${BIN_FILE}"
                    "$<TARGET_FILE_DIR:${PROJECT_NAME_GUI}>/Data/"
                COMMENT "Copying ${BIN_FILE} to Data folder..."
            )
        endforeach()
    endif()
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME_GUI}
)

install(TARGETS ${PROJECT_NAME_GUI}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
