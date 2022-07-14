project (repm3gw)

file(GLOB_RECURSE _HDRFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB_RECURSE _SRCFILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE _SRCCONFIG ${CMAKE_CURRENT_SOURCE_DIR}/configuration/*)
file(GLOB_RECURSE _SRCJS ${CMAKE_CURRENT_SOURCE_DIR}/javaScript/*)

source_group("Header Files" FILES ${_HDRFILES})
source_group("Source Files" FILES ${_SRCFILES})
source_group("Configuration" FILES ${_SRCCONFIG})
source_group("Java Script" FILES ${_SRCJS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_BINARY_DIR})

add_executable(${PROJECT_NAME} ${_HDRFILES} ${_SRCFILES}  ${_SRCCONFIG} ${_SRCJS})

target_link_libraries(${PROJECT_NAME} 
  launcher
)

if(UNIX)
  target_link_libraries(${PROJECT_NAME} 
    dl
    pthread
  )
endif()

DeployShapeComponent(${PROJECT_NAME})

# not runable from deploy - prepare for install repm3gw daemon
DeployShapeConfiguration(repm3gwLinDaemon
	${CMAKE_CURRENT_SOURCE_DIR}/configuration	configuration
	${CMAKE_CURRENT_SOURCE_DIR}/backup	configuration/backup
	${CMAKE_CURRENT_SOURCE_DIR}/configurationLinDaemon/configuration	configuration
	${CMAKE_CURRENT_SOURCE_DIR}/configurationLinDaemon/config.json config.json
	${CMAKE_CURRENT_SOURCE_DIR}/configurationLinDaemon/repm3gw.service repm3gw.service
	${CMAKE_CURRENT_SOURCE_DIR}/configurationLinDaemon/create-install.sh create-install.sh
	${CMAKE_CURRENT_SOURCE_DIR}/configurationLinDaemon/update-install.sh update-install.sh
	${CMAKE_CURRENT_SOURCE_DIR}/configurationLinDaemon/purge.sh purge.sh
	${CMAKE_CURRENT_SOURCE_DIR}/configurationLinDaemon/install.sh install.sh
	#${CMAKE_SOURCE_DIR}/../oegateway/DataBase/sql/init	DataBase/sql/init
	${CMAKE_CURRENT_SOURCE_DIR}/configurationLinDaemon/cert cert
)

DeployShapeConfiguration(repm3gwWin
	${CMAKE_CURRENT_SOURCE_DIR}/configuration	configuration
	${CMAKE_CURRENT_SOURCE_DIR}/backup	configuration/backup
	${CMAKE_CURRENT_SOURCE_DIR}/configurationWin/configuration	configuration
	${CMAKE_CURRENT_SOURCE_DIR}/configurationWin/config.json configuration/config.json
	#${CMAKE_SOURCE_DIR}/../oegateway/DataBase/sql/init	configuration/DataBase/sql/init
)
DeployShapeConfigurationStartUp("${PROJECT_INSTALL_PREFIX}/bin/${PROJECT_NAME}" repm3gwWin)

ConfigureMsvcProject("${PROJECT_INSTALL_PREFIX}/bin/${PROJECT_NAME}" repm3gwWin)
