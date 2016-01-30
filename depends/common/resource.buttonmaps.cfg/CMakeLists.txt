project(resource.buttonmaps.cfg)
cmake_minimum_required(VERSION 2.8)

set(APP_NAME_LC kodi)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/
        DESTINATION share/${APP_NAME_LC}/addons/${PROJECT_NAME}/resources
        FILES_MATCHING PATTERN "*.cfg"
)

