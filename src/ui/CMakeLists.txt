set(SOURCES_UI
    main.cpp
    QtNDSLocUtils.cpp
    QtNDSLocUtils.h
    QtNDSLocUtils.qrc
    QtNDSLocUtils.ui
    Worker.cpp
    Worker.h
)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
)

add_executable(${PROJECT_NAME_GUI} ${SOURCES_UI})

set_target_properties(${PROJECT_NAME_GUI} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

target_link_libraries(${PROJECT_NAME_GUI} PRIVATE
    ${PROJECT_NAME}Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "src" FILES ${SOURCES_UI})

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

    # Resources/ lives at the root of the repository, not next to this file.
    file(GLOB BIN_RESOURCES "${PROJECT_SOURCE_DIR}/Resources/*.*")
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

install(TARGETS ${PROJECT_NAME_GUI}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
