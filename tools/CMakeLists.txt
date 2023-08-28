if (FREECIV_ENABLE_RULEDIT OR FREECIV_ENABLE_RULEUP)
  add_subdirectory(ruleutil)
endif()
if (FREECIV_ENABLE_CIVMANUAL OR FREECIV_ENABLE_RULEUP)
  add_subdirectory(shared)
endif()

if (FREECIV_ENABLE_FCMP_CLI OR FREECIV_ENABLE_FCMP_QT)
  add_subdirectory(fcmp)
endif()

if (FREECIV_ENABLE_RULEDIT)
  add_subdirectory(ruledit)
endif()

if (FREECIV_ENABLE_CIVMANUAL)
  add_executable(lunar_gambit-manual civmanual.cpp)
  target_link_libraries(lunar_gambit-manual server)
  target_link_libraries(lunar_gambit-manual tools_shared)
  install(TARGETS lunar_gambit-manual
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
          COMPONENT tool_manual)
endif()

if (FREECIV_ENABLE_RULEUP)
  add_executable(lunar_gambit-ruleup ruleup.cpp)
  target_link_libraries(lunar_gambit-ruleup server)
  target_link_libraries(lunar_gambit-ruleup tools_shared)
  target_link_libraries(lunar_gambit-ruleup tools_ruleutil)
  install(TARGETS lunar_gambit-ruleup
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
          COMPONENT tool_ruleup)
endif()

