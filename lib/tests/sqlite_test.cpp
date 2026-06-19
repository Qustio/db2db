#include <gtest/gtest.h>
#include <source.h>
#include <nanodbc/nanodbc.h>
#include <print>
#include <variant>

#ifdef ENABLE_COVERAGE
extern "C" {
    int __llvm_profile_write_file(void);
    void __llvm_profile_set_filename(const char *name);
    void __llvm_profile_reset_counters(void);
}

class CoverageListener : public ::testing::EmptyTestEventListener  {
    void OnTestStart(const ::testing::TestInfo&) override {
        __llvm_profile_reset_counters();
    }
    void OnTestEnd(const ::testing::TestInfo& info) override {
        std::filesystem::create_directories("profraw");
        std::string path = std::string("profraw/")
                         + info.test_suite_name() + "."
                         + info.name() + ".profraw";
        __llvm_profile_set_filename(path.c_str());
        __llvm_profile_write_file();
    }
};

namespace {
    const int register_listener = [] {
        ::testing::UnitTest::GetInstance()->listeners()
            .Append(new CoverageListener);
        return 0;
    }();
}
#endif // ENABLE_COVERAGE

class test_source : public source {
public:
    nanodbc::connection& connection() {
        return _connection;
    }
};

class SqliteFixture : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef _WIN32
        conn_str = "Driver=SQLite3 ODBC Driver;Database=:memory:;";
        conn_str2 = "Driver=SQLite3 ODBC Driver;Database=:memory:;";
#else
        conn_str = "Driver=SQLite3;Database=:memory:;";
        conn_str2 = "Driver=SQLite3;Database=:memory:;";
#endif
        src = std::make_unique<test_source>(conn_str);
        dest = std::make_unique<test_source>(conn_str2);
    }

    inline void exec(const nanodbc::string& sql) {
        exec_src(sql);
    }

    void exec_src(const nanodbc::string& sql) {
        nanodbc::statement s(src->connection());
        s.prepare(sql);
        s.execute();
    }

    void exec_dest(const nanodbc::string& sql) {
        nanodbc::statement s(dest->connection());
        s.prepare(sql);
        s.execute();
    }

    std::string conn_str;
    std::string conn_str2;
    std::unique_ptr<test_source> src;
    std::unique_ptr<test_source> dest;
};

class SelectTest : public SqliteFixture {
protected:
    void SetUp() override {
        SqliteFixture::SetUp();
        exec("CREATE TABLE users (id INTEGER, name TEXT, score REAL)");
    }
};

class SyncTest : public SqliteFixture {
protected:
    void SetUp() override {
        SqliteFixture::SetUp();
        exec_src("CREATE TABLE users (id INTEGER, name TEXT, score REAL)");
        exec_src("INSERT INTO users VALUES(1, 'alice', 9.5)");
        exec_dest("CREATE TABLE users (id INTEGER, name TEXT, score REAL)");
    }
};

class db_type_test : public SqliteFixture {};

TEST(setupTest, includeTest) {
	int b = 5;
	ASSERT_EQ(b, 5);
}

TEST_F(SelectTest, EmptyTableReturnsEmptyData) {
    auto result = src->select("SELECT * FROM users");
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result["id"].size(), 0);
}


TEST_F(SelectTest, SelectParams) {
    auto result = src->select("SELECT * FROM users where id < ?", {3});
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result["id"].size(), 0);
}

TEST_F(SelectTest, SelectParamsNull) {
    auto result = src->select("SELECT * FROM users where id in (?, ?)", {3, std::monostate{}});
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result["id"].size(), 0);
}

TEST_F(SelectTest, ReturnsCorrectRows) {
    exec("INSERT INTO users VALUES(1, 'alice', 9.5)");
    exec("INSERT INTO users VALUES(2, 'bob', 7.77)");
    auto result = src->select("SELECT * FROM users");
    auto id_rows = std::vector{ db_value{1LL}, db_value{2LL} };
    auto name_rows = std::vector{ db_value{"alice"}, db_value{"bob"} };
    auto score_rows = std::vector{ db_value{9.5}, db_value{7.77} };
    EXPECT_EQ(result["id"], id_rows);
    EXPECT_EQ(result["name"], name_rows);
    EXPECT_EQ(result["score"], score_rows);
}

TEST_F(SelectTest, SelectOneNull) {
    exec("INSERT INTO users VALUES(NULL, NULL, NULL)");
    auto result = src->select_one("SELECT name FROM users");
    EXPECT_FALSE(std::holds_alternative<std::monostate>(result));
    EXPECT_EQ(std::get<std::string>(result), "");
}
TEST_F(SelectTest, SelectOneParams) {
    exec("INSERT INTO users VALUES(1, 'alice', 9.5)");
    auto result = src->select_one("SELECT name FROM users where id < ?", { 5 });
    EXPECT_EQ(result, db_value{"alice"});
}

TEST_F(SyncTest, SyncBasic) {
    auto data = src->select("SELECT * FROM users");
    dest->insert("INSERT INTO users(id, name, score) VALUES(?, ?, ?)", data, { "id", "name", "score" });
    auto result = dest->select("SELECT * FROM users");
    EXPECT_EQ(result, data);
}

TEST_F(SyncTest, ColumnNames) {
    auto data = src->select("SELECT * FROM users");
    dest->insert("INSERT INTO users(id, name, score) VALUES(?, ?, ?)", data, { "ID", "name", "scOrE" });
    auto result = dest->select("SELECT * FROM users");
    EXPECT_EQ(result, data);
}

TEST_F(SyncTest, SyncWithNulls) {
    exec_src("INSERT INTO users VALUES(NULL, NULL, NULL)");
    auto data = src->select("SELECT * FROM users");
    dest->insert("INSERT INTO users(id, name, score) VALUES(?, ?, ?)", data, { "id", "name", "score" });
    auto result = dest->select("SELECT * FROM users");
    auto id_rows = std::vector{ db_value{1LL}, db_value{} };

    EXPECT_EQ(result, data);
    EXPECT_EQ(data["id"], id_rows);
}

TEST_F(SyncTest, SyncIncremental) {
    exec_dest("INSERT INTO users VALUES(1, 'alice', 9.5)");
    exec_src("INSERT INTO users VALUES(2, 'bob', 3.5)");
    exec_src("INSERT INTO users VALUES(3, 'ivan', 1.1)");
    auto last_id = dest->select_one("select max(id) from users");
    EXPECT_EQ(last_id, db_value{1ll});
    auto data = src->select("SELECT * FROM users where id > ?", { last_id });
    dest->insert("INSERT INTO users(id, name, score) VALUES(?, ?, ?)", data, { "id", "name", "score" });
    auto result_src = src->select("SELECT * FROM users");
    auto result_dest = dest->select("SELECT * FROM users");
    EXPECT_EQ(result_src, result_dest);
}


TEST(filter, filter_basic) {
    db_data in{
        {"id", {1, 2, 3, 4, 5}},
        {"name", {"1", "2", "3", "4", "5"}}
    };
    std::println("ok");
    filter(in, "id", [](db_value& value) {
        int id = std::get<int>(value);
        return id <= 3 ? true : false;
    });
    std::println("2");
    std::vector<db_value> out_id = { 1, 2, 3 };
    std::println("3");
    std::vector<db_value> out_name = { "1", "2", "3" };
    std::println("4");
    EXPECT_EQ(in["id"], out_id);
    EXPECT_EQ(in["name"], out_name);
}

TEST(filter, filter_empty) {
    db_data in{};
    db_data out{};
    std::println("ok");
    filter(in, "id", [](db_value& value) {
        int id = std::get<int>(value);
        return id <= 3 ? true : false;
    });
    EXPECT_EQ(in, out);
}

TEST_F(db_type_test, mappings) {
    auto r = src->select_one("SELECT CAST(123 as INTEGER)");
    EXPECT_EQ(r, db_value{123ll});
    r = src->select_one("SELECT CAST(\"123\" as VARCHAR)");
    EXPECT_EQ(r, db_value{"123"});
    r = src->select_one("SELECT CAST(\"123\" as NVARCHAR)");
    EXPECT_EQ(r, db_value{"123"});
}
