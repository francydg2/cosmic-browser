cmake_minimum_required(VERSION 3.21)

project(Cosmic
    VERSION 0.1.0
    DESCRIPTION "A privacy-focused, extremely customizable Linux browser"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    WebEngineCore
    WebEngineWidgets
    Svg
    Test
)

qt_standard_project_setup()

find_package(OpenSSL REQUIRED)

# ---------------------------------------------------------------------------
# Resources (added to every target that loads them at runtime)
# ---------------------------------------------------------------------------
set(COSMIC_RESOURCE_FILES
    resources/styles/cosmic-dark.qss
    resources/icons/saturn.svg
    resources/icons/back.svg
    resources/icons/forward.svg
    resources/icons/reload.svg
    resources/icons/stop.svg
    resources/icons/gear.svg
    resources/icons/layers.svg
    resources/icons/globe.svg
    resources/icons/close.svg
    resources/icons/close-active.svg
    resources/icons/arrow-down.svg
    resources/icons/check.svg
    resources/icons/ai.svg
    resources/icons/star.svg
    resources/icons/key.svg
    resources/icons/lock.svg
    resources/icons/eye.svg
    resources/icons/ram.svg
    resources/icons/folder.svg
    resources/icons/search.svg
    resources/icons/palette.svg
    resources/icons/user.svg
    resources/icons/wrench.svg
    resources/blocklists/default.txt
)

# ---------------------------------------------------------------------------
# Static library: all browser code
# ---------------------------------------------------------------------------
add_library(cosmic_core STATIC
    src/core/Logging.h
    src/core/Logging.cpp
    src/core/UrlUtils.h
    src/core/UrlUtils.cpp
    src/core/Paths.h
    src/core/Paths.cpp
    src/core/ActionRegistry.h
    src/core/ActionRegistry.cpp
    src/core/Settings.h
    src/core/Settings.cpp
    src/core/SearchEngines.h
    src/core/SearchEngines.cpp
    src/core/Session.h
    src/core/Session.cpp
    src/core/UserCatalog.h
    src/core/UserCatalog.cpp
    src/core/Bookmarks.h
    src/core/Bookmarks.cpp
    src/core/PasswordVault.h
    src/core/PasswordVault.cpp
    src/core/UILayout.h
    src/core/UILayout.cpp
    src/core/LayoutEditor.h
    src/core/LayoutEditor.cpp
    src/web/WebPageDelegate.h
    src/web/PagePolicy.h
    src/web/WebPage.h
    src/web/WebPage.cpp
    src/web/WebView.h
    src/web/WebView.cpp
    src/web/BlocklistInterceptor.h
    src/web/BlocklistInterceptor.cpp
    src/browser/AppWindow.h
    src/browser/Browser.h
    src/browser/Browser.cpp
    src/browser/SingleInstance.h
    src/browser/SingleInstance.cpp
    src/ui/Style.h
    src/ui/Style.cpp
    src/ui/UrlBar.h
    src/ui/UrlBar.cpp
    src/ui/SaturnIllustration.h
    src/ui/SaturnIllustration.cpp
    src/ui/NewTabPage.h
    src/ui/NewTabPage.cpp
    src/ui/TabContent.h
    src/ui/TabContent.cpp
    src/ui/TabWidget.h
    src/ui/TabWidget.cpp
    src/ui/SettingsForm.h
    src/ui/SettingsForm.cpp
    src/ui/customizer/ComponentLibrary.h
    src/ui/customizer/ComponentLibrary.cpp
    src/ui/customizer/ComponentProperties.h
    src/ui/customizer/ComponentProperties.cpp
    src/ui/customizer/CustomizerPreview.h
    src/ui/customizer/CustomizerPreview.cpp
    src/ui/customizer/CustomizerWindow.h
    src/ui/customizer/CustomizerWindow.cpp
    src/ui/NameColorDialog.h
    src/ui/NameColorDialog.cpp
    src/ui/BrowserWindow.h
    src/ui/BrowserWindow.cpp
)

target_include_directories(cosmic_core PUBLIC src)

target_link_libraries(cosmic_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebEngineCore
    Qt6::WebEngineWidgets
    Qt6::Svg
    OpenSSL::Crypto
)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
qt_add_executable(cosmic
    src/main.cpp
)

target_compile_definitions(cosmic PRIVATE COSMIC_VERSION="${PROJECT_VERSION}")
target_link_libraries(cosmic PRIVATE cosmic_core)

qt_add_resources(cosmic "cosmic_res"
    PREFIX "/"
    BASE resources
    FILES ${COSMIC_RESOURCE_FILES}
)

include(GNUInstallDirs)

install(TARGETS cosmic RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

configure_file(resources/cosmic.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/resources/cosmic.desktop @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/resources/cosmic.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES resources/icons/app/cosmic.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

foreach(sz IN ITEMS 16 24 32 48 64 128 256 512)
    install(FILES resources/icons/app/cosmic-${sz}.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${sz}x${sz}/apps
        RENAME cosmic.png
    )
endforeach()

if(APPLE)
    set_target_properties(cosmic PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "app.cosmic.browser"
        MACOSX_BUNDLE_BUNDLE_NAME "Cosmic"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

# ---------------------------------------------------------------------------
# Packaging (alpha artifacts)
# ---------------------------------------------------------------------------
# Local: `cpack -G TGZ` packs the install tree, no extra tools needed.
# Windows/macOS distributables are assembled in CI (see
# .github/workflows/build.yml) with Qt's own deploy tools.
set(CPACK_PACKAGE_NAME "Cosmic")
set(CPACK_PACKAGE_VENDOR "Cosmic")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cosmic — a privacy-focused, customizable browser")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "cosmic-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/.git/;~$")
include(CPack)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
