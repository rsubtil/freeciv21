set(CMAKE_AUTOMOC ON)

add_library(
  freeciv_modpack
  STATIC
  download.cpp
  modinst.cpp
  mpcmdline.cpp
  mpdb.cpp
)

target_link_libraries(freeciv_modpack PUBLIC common)
target_link_libraries(freeciv_modpack PRIVATE cvercmp)
target_link_libraries(freeciv_modpack PRIVATE SQLite::SQLite3)

if (FREECIV_ENABLE_FCMP_CLI)
  add_executable(lunar_gambit-modpack mpcli.cpp)
  target_link_libraries(lunar_gambit-modpack PRIVATE freeciv_modpack)
  install(TARGETS lunar_gambit-modpack
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
          COMPONENT tool_fcmp_cli)
endif()

if (FREECIV_ENABLE_FCMP_QT)
  add_executable(
    lunar_gambit-modpack-qt ${GUI_TYPE}
    mpgui_qt_worker.cpp
    mpgui_qt.cpp
  )
  target_link_libraries(lunar_gambit-modpack-qt PRIVATE freeciv_modpack)
  target_link_libraries(lunar_gambit-modpack-qt PRIVATE Qt5::Widgets)
  install(TARGETS lunar_gambit-modpack-qt
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
          COMPONENT lunar_gambit)
endif()
