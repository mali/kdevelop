add_library(kdevqmljsduchain STATIC
    debug.cpp
    cache.cpp
    helper.cpp
    parsesession.cpp
    debugvisitor.cpp
    contextbuilder.cpp
    declarationbuilder.cpp
    expressionvisitor.cpp
    usebuilder.cpp
    qmljsducontext.cpp
    functiondeclaration.cpp
    functiontype.cpp

    navigation/declarationnavigationcontext.cpp
    navigation/navigationwidget.cpp

    frameworks/nodejs.cpp
)

generate_export_header(kdevqmljsduchain EXPORT_MACRO_NAME KDEVQMLJSDUCHAIN_EXPORT
                                        EXPORT_FILE_NAME duchainexport.h)
target_link_libraries(kdevqmljsduchain
    LINK_PRIVATE
        Qt5::Core
        KF5::I18n
    LINK_PUBLIC
        KDev::Language
        KDev::Util
        KDev::Project
        kdevqtc-qmlsupport
)

add_subdirectory(tests)
