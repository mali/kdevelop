add_library(kdevclangduchain STATIC
    parsesession.cpp
    clangdiagnosticevaluator.cpp
    clangducontext.cpp
    clangindex.cpp
    clangparsingenvironmentfile.cpp
    clangparsingenvironment.cpp
    clangproblem.cpp
    debugvisitor.cpp
    duchainutils.cpp
    builder.cpp
    clangpch.cpp
    clanghelpers.cpp
    unknowndeclarationproblem.cpp
    macrodefinition.cpp
    missingincludepathproblem.cpp
    macronavigationcontext.cpp
    navigationwidget.cpp
    todoextractor.cpp
    types/classspecializationtype.cpp
    unsavedfile.cpp
    documentfinderhelpers.cpp
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
)

generate_export_header(kdevclangduchain EXPORT_FILE_NAME clangduchainexport.h)
target_link_libraries(kdevclangduchain
LINK_PRIVATE
    Qt5::Core
    kdevclangcodegen
    kdevclangcodecompletion
    kdevclangutil
    settingsmanager
LINK_PUBLIC
    KDev::Language
    KDev::Project
    KDev::Util
    ${CLANG_LIBCLANG_LIB}
)

install(FILES gcc_compat.h DESTINATION ${DATA_INSTALL_DIR}/kdevclangsupport PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)
install(DIRECTORY wrappedQtHeaders DESTINATION ${DATA_INSTALL_DIR}/kdevclangsupport
        DIRECTORY_PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_WRITE GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
        FILE_PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)
