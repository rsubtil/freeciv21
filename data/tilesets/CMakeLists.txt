# Install Tileset sprites
install(
  DIRECTORY buildings
  DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/tilesets"
  COMPONENT lunar_gambit
  FILES_MATCHING PATTERN *.png PATTERN *.license)
install(
  DIRECTORY flags
  DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/tilesets"
  COMPONENT lunar_gambit
  FILES_MATCHING PATTERN *.png)
install(
  DIRECTORY units
  DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/tilesets"
  COMPONENT lunar_gambit
  FILES_MATCHING PATTERN *.png PATTERN *.license)
install(
  DIRECTORY wonders
  DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/tilesets"
  COMPONENT lunar_gambit
  FILES_MATCHING PATTERN *.png PATTERN *.license)
