set(SERVER_NAME "${PROJECT_NAME}-server")

# For testing
add_library(
	${SERVER_NAME}-lib
	STATIC
	worker.c
	command.c
	)

add_executable(
	${SERVER_NAME}
	main.c
	)

target_link_libraries(
	${SERVER_NAME}
	${SERVER_NAME}-lib
	)

if(BUILD_TESTING)

	add_subdirectory(tests)

	set(TEST_EXECUTABLE "${SERVER_NAME}-test")

	add_executable(
		${TEST_EXECUTABLE}
		tests/worker.cpp
		)

	target_link_libraries(
		${TEST_EXECUTABLE}
		${SERVER_NAME}-lib
		GTest::GTest
		GTest::gtest_main
		)

	gtest_discover_tests("${SERVER_NAME}-test")

endif()
