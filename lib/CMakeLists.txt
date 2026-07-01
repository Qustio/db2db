if(ENABLE_COVERAGE)
	add_library(db2db STATIC "src/source.cpp")
else()
	add_library(db2db SHARED "src/source.cpp")
endif()

target_compile_features(db2db PUBLIC cxx_std_23)

find_package(ODBC REQUIRED)
find_package(nanodbc CONFIG)

if(NOT nanodbc_FOUND)
	FetchContent_Declare(
		nanodbc
		GIT_REPOSITORY https://github.com/nanodbc/nanodbc
		GIT_TAG fd9b4f551b0f03780168c4b2ba880dcb5777aad4
	)
	FetchContent_MakeAvailable(nanodbc)
endif()

target_include_directories(
	db2db
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(db2db PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_link_libraries(db2db PUBLIC nanodbc)

add_custom_command(
	TARGET db2db
	POST_BUILD
	COMMAND
	${CMAKE_COMMAND} -E copy_if_different
	$<TARGET_FILE:nanodbc>
	$<TARGET_FILE_DIR:db2db>
)

if(DB2DB_BUILD_TESTS)
	include(GoogleTest)

	find_package(GTest CONFIG)

	if(NOT GTest_FOUND)
		FetchContent_Declare(
			GTest
			GIT_REPOSITORY https://github.com/google/googletest
			GIT_TAG v1.17.0
		)
		FetchContent_MakeAvailable(GTest)
	endif()

	add_executable(db2db_test "tests/sqlite_test.cpp")
	target_link_libraries(db2db_test PRIVATE GTest::gtest_main db2db)
	gtest_discover_tests(db2db_test)

	if(ENABLE_COVERAGE)
		target_compile_options(
			db2db
			PRIVATE -fprofile-instr-generate -fcoverage-mapping
		)
		target_link_options(db2db PRIVATE -fprofile-instr-generate -fcoverage-mapping)
		target_compile_definitions(db2db_test PRIVATE ENABLE_COVERAGE)
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

		# file(MAKE_DIRECTORY ${PROFRAW_DIR})
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
endif()
