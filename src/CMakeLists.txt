project(novus-world VERSION 1.0.0 DESCRIPTION "Novus World Server")

file(GLOB_RECURSE FILES "*.cpp" "*.h")

set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/appicon.rc")
add_executable(${PROJECT_NAME} ${FILES} ${APP_ICON_RESOURCE_WINDOWS})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ROOT_FOLDER})

find_assign_files(${FILES})
add_compile_definitions(NOMINMAX _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

target_link_libraries(${PROJECT_NAME} PRIVATE
	common::common
	gameplay::gameplay
	network::network
	Entt::Entt
	taskflow::taskflow
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)