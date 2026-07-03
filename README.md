# db2db

A small C++ ODBC abstaction for typed data transfer between databases 

# Building

# TODO

- [ ] add LICENSE
- [ ] write build instructions
- [x] add namespace
- [ ] move header to include/db2db/source.h
- [ ] remove Windows.h/sqlext.h from public header
- [ ] add install/export rules + db2db::db2db alias
- [ ] fix operator== ignoring length (zip_transform truncates)
- [ ] fix filter() — marks nulls instead of removing rows
- [ ] fix row_count() crash on empty data
- [ ] fix select() row misalignment when a cell throws
- [ ] fix from_file() non-ASCII narrowing + wrong error message
- [ ] fix or delete failing db_data_size tests
- [ ] fix tolower UB in case_insensitive_equal
- [ ] rename nanodbc_optional.cpp to .hpp
- [ ] delete dead scope_guard.cpp
- [ ] make source ctor explicit
- [ ] remove dead code (unused locals, commented-out lines)
- [ ] link ODBC::ODBC explicitly
- [ ] add project VERSION + LANGUAGES CXX
- [ ] use PROJECT_IS_TOP_LEVEL for options
- [ ] guard coverage flags for clang only
- [ ] finish python module (rename mymodule, real bindings)
- [ ] replace WINDOWS_EXPORT_ALL_SYMBOLS with export macro