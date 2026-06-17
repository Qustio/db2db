include(GoogleTest)

enable_testing()

if(ENABLE_COVERAGE)
	add_library(db2db STATIC "src/source.cpp")
else()
	add_library(db2db SHARED "src/source.cpp")
endif()

target_include_directories(
	db2db
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(db2db PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
message(STATUS "Boost version: ${Boost_VERSION}")
get_target_property(BOOST_INC Boost::headers INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost::headers includes: ${BOOST_INC}")
target_include_directories(db2db PRIVATE ${Boost_INCLUDE_DIRS})

target_link_libraries(db2db PUBLIC nanodbc spdlog::spdlog Boost::container)
if(ENABLE_COVERAGE)
	target_compile_options(
		db2db
		PRIVATE -fprofile-instr-generate -fcoverage-mapping
	)
	target_link_options(db2db PRIVATE -fprofile-instr-generate -fcoverage-mapping)
endif()

add_custom_command(
	TARGET db2db
	POST_BUILD
	COMMAND
		${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:spdlog::spdlog>
		$<TARGET_FILE:nanodbc>
		$<TARGET_FILE_DIR:db2db>
)

add_executable(db2db_test "tests/sqlite_test.cpp")
target_link_libraries(db2db_test PRIVATE GTest::gtest_main db2db)

gtest_discover_tests(db2db_test)

if(ENABLE_COVERAGE)
	target_compile_options(
		db2db_test
		PRIVATE -fprofile-instr-generate -fcoverage-mapping
	)
	target_link_options(
		db2db_test
		PRIVATE -fprofile-instr-generate -fcoverage-mapping
	)
	if(NOT LLVM_COV)
		unset(LLVM_COV CACHE)
		find_program(LLVM_COV llvm-cov REQUIRED)
	endif()
	if(NOT LLVM_PROFDATA)
		unset(LLVM_PROFDATA CACHE)
		find_program(LLVM_PROFDATA llvm-profdata REQUIRED)
	endif()

	set(PROFRAW_DIR $<TARGET_FILE_DIR:db2db_test>/profraw)

	#file(MAKE_DIRECTORY ${PROFRAW_DIR})

	add_custom_target(
		remove-coverage
		COMMAND ${CMAKE_COMMAND} -E rm -rf ${PROFRAW_DIR}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${PROFRAW_DIR}
	)
	add_custom_target(
		coverage
		COMMAND
			${LLVM_PROFDATA} merge -sparse ${PROFRAW_DIR}/*.profraw -o
			$<TARGET_FILE_DIR:db2db_test>/coverage.profdata
		COMMAND
			${LLVM_COV} export $<TARGET_FILE:db2db_test>
			-instr-profile=$<TARGET_FILE_DIR:db2db_test>/coverage.profdata -format=lcov
			--Xdemangler=llvm-cxxfilt
			-ignore-filename-regex=".*deps.*|.*Program Files.*|.*tests.*" >
			${CMAKE_SOURCE_DIR}/report.info
		DEPENDS db2db_test
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		COMMENT "Generating coverage.info"
	)
endif()

add_custom_command(
	TARGET db2db_test
	POST_BUILD
	COMMAND
		${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:spdlog::spdlog>
		$<TARGET_FILE_DIR:db2db_test>
)
if(NOT ENABLE_COVERAGE)
	add_custom_command(
		TARGET db2db_test
		POST_BUILD
		COMMAND
			${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:db2db>
			$<TARGET_FILE_DIR:db2db_test>
	)
endif()
add_custom_command(
	TARGET db2db_test
	POST_BUILD
	COMMAND
		${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:GTest::gtest>
		$<TARGET_FILE:GTest::gtest_main> $<TARGET_FILE_DIR:db2db_test>
)
