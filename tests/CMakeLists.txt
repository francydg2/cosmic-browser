# Tests (all run offscreen, no display needed)

set(NEBULA_TEST_RES_FILES
    ${CMAKE_SOURCE_DIR}/resources/styles/cosmic-dark.qss
    ${CMAKE_SOURCE_DIR}/resources/icons/saturn.svg
    ${CMAKE_SOURCE_DIR}/resources/blocklists/default.txt
)

# 1) Engine smoke test: real HTTPS navigation with Qt WebEngine.
add_executable(cosmic_smoke_webengine smoke_webengine.cpp)
target_link_libraries(cosmic_smoke_webengine PRIVATE
    Qt6::Widgets Qt6::WebEngineCore Qt6::WebEngineWidgets
)
add_test(NAME smoke_https COMMAND cosmic_smoke_webengine)
set_tests_properties(smoke_https PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 90
)

# 2) Unit tests: URL/search detection in the address bar logic.
add_executable(cosmic_test_urlutils
    test_urlutils.cpp
    ${CMAKE_SOURCE_DIR}/src/core/UrlUtils.cpp
)
target_include_directories(cosmic_test_urlutils PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cosmic_test_urlutils PRIVATE Qt6::Core Qt6::Test)
add_test(NAME url_utils COMMAND cosmic_test_urlutils)

# 3) Settings persistence (Phase 2).
add_executable(cosmic_test_settings test_settings.cpp)
target_include_directories(cosmic_test_settings PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cosmic_test_settings PRIVATE cosmic_core Qt6::Test)
add_test(NAME settings COMMAND cosmic_test_settings)

# 4) Search engine registry + configured-engine resolution (Phase 2).
add_executable(cosmic_test_searchengines test_searchengines.cpp)
target_include_directories(cosmic_test_searchengines PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cosmic_test_searchengines PRIVATE cosmic_core Qt6::Test)
add_test(NAME search_engines COMMAND cosmic_test_searchengines)

# 5) Request-blocking rules (Phase 2).
add_executable(cosmic_test_blocklist test_blocklist.cpp)
target_include_directories(cosmic_test_blocklist PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cosmic_test_blocklist PRIVATE cosmic_core Qt6::Test)
qt_add_resources(cosmic_test_blocklist "nebula_res_blocklist"
    PREFIX "/"
    BASE ${CMAKE_SOURCE_DIR}/resources
    FILES ${NEBULA_TEST_RES_FILES}
)
add_test(NAME blocklist COMMAND cosmic_test_blocklist)

# 6) Session serialization (Phase 2).
add_executable(cosmic_test_session test_session.cpp)
target_include_directories(cosmic_test_session PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cosmic_test_session PRIVATE cosmic_core Qt6::Test)
add_test(NAME session COMMAND cosmic_test_session)

# 6b) User catalog.
add_executable(cosmic_test_users test_users.cpp)
target_include_directories(cosmic_test_users PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cosmic_test_users PRIVATE cosmic_core Qt6::Test)
add_test(NAME users COMMAND cosmic_test_users)

# 6c) Bookmarks model.
add_executable(cosmic_test_bookmarks test_bookmarks.cpp)
target_include_directories(cosmic_test_bookmarks PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cosmic_test_bookmarks PRIVATE cosmic_core Qt6::Test)
add_test(NAME bookmarks COMMAND cosmic_test_bookmarks)

# 6d) Encrypted password vault (OpenSSL AES-256-GCM + PBKDF2).
add_executable(cosmic_test_vault test_vault.cpp)
target_include_directories(cosmic_test_vault PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cosmic_test_vault PRIVATE cosmic_core Qt6::Test)
add_test(NAME vault COMMAND cosmic_test_vault)

# 6e) UI layout model + layout editor (customizer data layer, no widgets).
add_executable(cosmic_test_uilayout test_uilayout.cpp)
target_include_directories(cosmic_test_uilayout PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cosmic_test_uilayout PRIVATE cosmic_core Qt6::Test)
add_test(NAME uilayout COMMAND cosmic_test_uilayout)

# 7) Integration tests: BrowserWindow, tabs, createWindow, crash handling,
#    settings dialog, session roundtrip through real windows.
add_executable(cosmic_test_window test_browserwindow.cpp)
target_include_directories(cosmic_test_window PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(cosmic_test_window PRIVATE cosmic_core Qt6::Test)
qt_add_resources(cosmic_test_window "nebula_res_test"
    PREFIX "/"
    BASE ${CMAKE_SOURCE_DIR}/resources
    FILES ${NEBULA_TEST_RES_FILES}
)
add_test(NAME browser_window COMMAND cosmic_test_window)
set_tests_properties(browser_window PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 180
)

# 8) Desktop entry validation (requires desktop-file-utils; skipped if absent).
# Validates the generated entry (absolute Exec path for the install prefix).
find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
if(DESKTOP_FILE_VALIDATE)
    add_test(NAME desktop_entry
        COMMAND ${DESKTOP_FILE_VALIDATE} ${CMAKE_BINARY_DIR}/resources/cosmic.desktop
    )
endif()
