macro (MocFolder)
    file (GLOB MOCS_TO_SOURCE_GROUP *.cxx */*.cxx)
    source_group ("CMake Moc" FILES ${MOCS_TO_SOURCE_GROUP})
endmacro (MocFolder)

#############################################################################################

# Define target name and output directory
init_target (InventoryModule OUTPUT modules/core)

# Define source files
file (GLOB CPP_FILES *.cpp)
file (GLOB H_FILES *.h)
file (GLOB XML_FILES *.xml)
file (GLOB MOC_FILES InventoryModule.h AbstractInventoryDataModel.h AbstractInventoryItem.h
    InventoryAsset.h InventoryFolder.h InventoryItemModel.h InventoryWindow.h OpenSimInventoryDataModel.h
    WebdavInventoryDataModel.h InventoryTreeView.h ItemPropertiesWindow.h UploadProgressWindow.h
    InventoryService.h)
file (GLOB RESOURCE_FILES resource/*.qrc)
set (SOURCE_FILES ${CPP_FILES} ${H_FILES})

# Qt4 Moc files to subgroup "CMake Moc"
MocFolder ()

# Qt4 Wrap
QT4_WRAP_CPP(MOC_SRCS ${MOC_FILES})
QT4_WRAP_UI(UI_SRCS ${UI_FILES})
QT4_ADD_RESOURCES(RESOURCE_SRCS ${RESOURCE_FILES})

set (FILES_TO_TRANSLATE ${FILES_TO_TRANSLATE} ${H_FILES} ${CPP_FILES} PARENT_SCOPE)

add_definitions (-DINVENTORY_MODULE_EXPORTS)

if (UISERVICE_TEST)
    add_definitions (-DUISERVICE_TEST)
endif ()

use_package (BOOST)
use_package (POCO)
use_package (CURL)
use_package (XMLRPC)
use_package (OPENJPEG)
use_package (OGRE)
use_package (QT4)
use_package (PYTHON)
use_package (PYTHON_QT)

use_modules (Core Foundation Interfaces RexCommon ProtocolUtilities HttpUtilities AssetModule)

if (NOT UISERVICE_TEST)
    use_modules (UiModule)
endif ()

build_library (${TARGET_NAME} SHARED ${SOURCE_FILES} ${MOC_SRCS} ${RESOURCE_SRCS})

link_package (BOOST)
link_package (POCO)
link_package (CURL)
link_package (XMLRPC)
link_package (OPENJPEG)
link_package (OGRE)
link_package (QT4)
link_package (PYTHON_QT)

link_modules (Core Foundation Interfaces RexCommon ProtocolUtilities HttpUtilities)

if (NOT UISERVICE_TEST)
    link_modules (UiModule)
endif ()

# MSVC -specific settings for preprocessor and PCH use
if (MSVC)
    # Label StableHeaders.cpp to create the PCH file and mark all other .cpp files to use that PCH file.
    # Add a #define DEBUG_CPP_NAME "this compilation unit name" to each compilation unit to aid in memory leak checking.
    foreach(src_file ${CPP_FILES})
            if (${src_file} MATCHES "StableHeaders.cpp$")
                set_source_files_properties(${src_file} PROPERTIES COMPILE_FLAGS "/YcStableHeaders.h")
            else()
                get_filename_component(basename ${src_file} NAME)
                set_source_files_properties(${src_file} PROPERTIES COMPILE_FLAGS "/YuStableHeaders.h -DDEBUG_CPP_NAME=\"\\\"${basename}\"\\\"")
            endif()
    endforeach(src_file ${CPP_FILES})
endif (MSVC)

# Copy XML files from source directory to target directory
foreach (XML_FILE ${XML_FILES})
    add_custom_command (TARGET ${TARGET_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${XML_FILE} ${TARGET_DIR})
endforeach (XML_FILE)

final_target ()
