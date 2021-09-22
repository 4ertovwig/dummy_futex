set(TEST_BINARY test_executable)

add_executable(${TEST_BINARY}
	mutex_inprocess_test.cpp
	condition_variable_inprocess_test.cpp
	semaphore_inprocess_test.cpp
	semaphore_interprocess_test.cpp
	#mutex_interprocess_test.cpp
	#condition_variable_interprocess_test.cpp
	main.cpp
)

target_link_libraries(${TEST_BINARY}
	pthread
	boost_system
	rt
	gtest
)
