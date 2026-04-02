cmake_minimum_required(VERSION 3.16)

project(HMI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

find_package(PkgConfig REQUIRED)
# wpa_supplicant often provides an interface via libwpa_client
# pkg_check_modules looks for .pc file when find_package looks for .cmake file
# which is not shipped by wpa_supplicant
# pkg_check_modules(WPA_CTRL REQUIRED libwpa_client) # it must return WPA_CTRL_INCLUDE_DIRS but .pc is not shipped as well

# find libwpa_client.so in sysroot 
find_path(WPA_CTRL_INC wpa_ctrl.h)
find_library(WPA_CLIENT_LIB wpa_client)

qt_add_executable(appHMI
    main.cpp
    wpaController.cpp
)

qt_add_qml_module(appHMI
    URI HMI
    VERSION 1.0
    QML_FILES
        Main.qml
        RESOURCES res.qrc
        QML_FILES
        QML_FILES
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appHMI PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appHMI
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appHMI
    PRIVATE Qt6::Quick
)

# wpa_suplicant depandancy
target_include_directories(appHMI PRIVATE ${WPA_CTRL_INC})
target_link_libraries(appHMI PRIVATE ${WPA_CLIENT_LIB})

include(GNUInstallDirs)
install(TARGETS appHMI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
