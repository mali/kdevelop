project(gdb)
add_definitions(-DTRANSLATION_DOMAIN=\"kdevgdb\")

function(add_debuggable_executable target)
    cmake_parse_arguments(add_debuggable_executable "" "" "SRCS" ${ARGN})
    add_executable(${target} ${add_debuggable_executable_SRCS})
    # force debug symbols for our debuggees, disable optimizations
    if (WIN32)
        set(_flags "/0d")
    else()
        set(_flags "-O0")
    endif()
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS_DEBUG} ${_flags}")
endfunction()

find_package(KF5SysGuard)
if(KF5SysGuard_FOUND)
    add_definitions(
        -DKDEV_ENABLE_GDB_ATTACH_DIALOG
    )
endif()

if (CMAKE_VERSION VERSION_GREATER "2.9" OR NOT CMAKE_GENERATOR MATCHES "Ninja")
    set(HAVE_PATH_WITH_SPACES_TEST TRUE)
else()
    message(WARNING "Disabling 'path with spaces' test, this CMake version would create a faulty build.ninja file. Upgrade to at least CMake v3.0")
endif()

add_subdirectory(unittests)
add_subdirectory(printers)

include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

#add_subdirectory(tests)

########### next target ###############

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  ${KDE4_ENABLE_EXCEPTIONS}")

set(kdevgdb_SRCS
    debuggerplugin.cpp
    gdb.cpp
    gdbcommandqueue.cpp
    gdbcommand.cpp
    stty.cpp
    disassemblewidget.cpp
    gdboutputwidget.cpp
#    debuggertracingdialog.cpp
    breakpointcontroller.cpp
    mi/gdbmi.cpp
    mi/milexer.cpp
    mi/miparser.cpp
    stringhelpers.cpp
    debugsession.cpp
    gdbconfigpage.cpp
    debugjob.cpp
    selectcoredialog.cpp
    variablecontroller.cpp
    gdbframestackmodel.cpp
    gdbvariable.cpp
    registers/registersview.cpp
    registers/registercontroller.cpp
    registers/registersmanager.cpp
    registers/registercontroller_x86.cpp
    registers/registercontroller_arm.cpp
    registers/modelsmanager.cpp
    registers/converters.cpp
)

if(OKTETA_FOUND)
    add_definitions(-DWITH_OKTETA=1)
    list(APPEND kdevgdb_SRCS
        memviewdlg.cpp)
endif()


if(KF5SysGuard_FOUND)
    set(kdevgdb_SRCS
        ${kdevgdb_SRCS}
        processselection.cpp
    )
endif()

set(kdevgdb_UI
    debuggertracingdialog.ui
    gdbconfigpage.ui
    selectcoredialog.ui
    selectaddressdialog.ui
    registers/registersview.ui
)

kde_enable_exceptions()
ki18n_wrap_ui(kdevgdb_SRCS ${kdevgdb_UI})
qt5_add_resources(kdevgdb_SRCS kdevgdb.qrc)
kdevplatform_add_plugin(kdevgdb JSON kdevgdb.json SOURCES ${kdevgdb_SRCS})
target_link_libraries(kdevgdb
    KDev::Sublime
    KDev::Interfaces
    KDev::Language
    KDev::Debugger
    KDev::OutputView
    KDev::Project
    KDev::Util
    KF5::TextEditor
)
if(KF5SysGuard_FOUND)
    target_link_libraries(kdevgdb KF5::ProcessUi)
endif()

set(test_gdb_SRCS unittests/test_gdb.cpp ${kdevgdb_SRCS})
ecm_add_test(${test_gdb_SRCS}
    TEST_NAME test_gdb
    LINK_LIBRARIES
        Qt5::Test
        KDev::Shell
        KDev::Interfaces
        KDev::Project
        KDev::Debugger
        KDev::Tests
        KDev::Util
        KF5::KIOWidgets
        KF5::TextEditor
        KF5::Parts
)
if(KF5SysGuard_FOUND)
    target_link_libraries(test_gdb KF5::ProcessUi)
endif()

if (HAVE_PATH_WITH_SPACES_TEST)
    set_target_properties(test_gdb PROPERTIES COMPILE_FLAGS "-DHAVE_PATH_WITH_SPACES_TEST")
endif()
