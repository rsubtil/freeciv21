set(CMAKE_AUTOMOC ON)

add_executable(
  lunar_gambit-ruledit ${GUI_TYPE}
  conversion_log.cpp
  edit_utype.cpp
  effect_edit.cpp
  req_edit.cpp
  req_vec_fix.cpp
  requirers_dlg.cpp
  ruledit_qt.cpp
  ruledit.cpp
  tab_building.cpp
  tab_enablers.cpp
  tab_extras.cpp
  tab_good.cpp
  tab_gov.cpp
  tab_misc.cpp
  tab_multiplier.cpp
  tab_nation.cpp
  tab_tech.cpp
  tab_terrains.cpp
  tab_unit.cpp
  univ_value.cpp
  validity.cpp
)

target_link_libraries(lunar_gambit-ruledit server)
target_link_libraries(lunar_gambit-ruledit tools_ruleutil)
target_link_libraries(lunar_gambit-ruledit tools_shared)
target_link_libraries(lunar_gambit-ruledit Qt5::Widgets)
add_dependencies(lunar_gambit-ruledit freeciv_translations)

install(TARGETS lunar_gambit-ruledit
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT tool_ruledit)
