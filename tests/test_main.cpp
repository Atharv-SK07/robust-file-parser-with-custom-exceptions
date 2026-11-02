// test_main.cpp - self-contained test runner (no external framework needed).
// Run it from the project root so "testdata/..." paths resolve.
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "CsvParser.h"
#include "CsvTokenizer.h"
#include "Exceptions.h"
#include "FileReader.h"
#include "JsonParser.h"
#include "Schema.h"

using namespace rfp;

static int g_run = 0, g_failed = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++g_run;                                                                      \
        if (!(cond)) {                                                                \
            ++g_failed;                                                               \
            std::cout << "    FAIL  " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                             \
    } while (0)

#define TEST(name) static void name(); static Registrar reg_##name(#name, name); static void name()

typedef void (*TestFn)();
struct Registrar {
    Registrar(const char* n, TestFn f);
};
struct Entry { const char* name; TestFn fn; };
static Entry g_tests[64];
static int g_count = 0;
Registrar::Registrar(const char* n, TestFn f) { g_tests[g_count++] = {n, f}; }

// Writes a throwaway file for edge-case tests and deletes it afterwards.
struct TempFile {
    std::string path;
    TempFile(const std::string& name, const std::string& content) : path(name) {
        std::ofstream out(path, std::ios::binary);
        out << content;
    }
    ~TempFile() { std::remove(path.c_str()); }
};

static const char* kProductSchema = "id:int,name:string,price:double,in_stock:bool";
static const char* kUserSchema = "id:int,name:string,age:int,active:bool,email:string?";

static CsvOptions csvOpts(const char* schema = nullptr) {
    CsvOptions o;
    if (schema) o.schema = parseSchemaSpec(schema);
    return o;
}

// ---------------------------------------------------------------- Module 1
TEST(file_reader_missing_file_throws_FileError) {
    bool caught = false;
    try { FileReader r("testdata/does_not_exist.csv"); }
    catch (const FileError& e) { caught = true; CHECK(e.category() == "FileError"); }
    CHECK(caught);
    CHECK(FileReader::openCount() == 0);
}

TEST(file_reader_releases_file_when_exception_thrown) {
    CHECK(FileReader::openCount() == 0);
    try {
        FileReader r("testdata/valid_products.csv");
        CHECK(FileReader::openCount() == 1);
        throw ParseError("simulated failure", 1, 1);
    } catch (const ParseError&) {
    }
    CHECK(FileReader::openCount() == 0);  // destructor ran during unwinding
}

TEST(file_reader_strips_crlf_and_bom) {
    TempFile f("tmp_bom.csv", "\xEF\xBB\xBF" "a,b\r\n1,2\r\n");
    FileReader r(f.path);
    std::string line;
    CHECK(r.readLine(line) && line == "a,b");
    CHECK(r.readLine(line) && line == "1,2");
    CHECK(!r.readLine(line));
}

// ---------------------------------------------------------------- Module 2
TEST(csv_tokenizer_tracks_line_and_column) {
    CsvTokenizer tk;
    auto toks = tk.tokenizeLine("ab,\"c,d\",e", 7);
    // Field ab | Delim | Quoted "c,d" | Delim | Field e | EOL
    CHECK(toks.size() == 6);
    CHECK(toks[0].line == 7 && toks[0].column == 1 && toks[0].text == "ab");
    CHECK(toks[2].type == TokenType::QuotedField && toks[2].column == 4 && toks[2].text == "c,d");
    CHECK(toks[4].column == 10 && toks[4].text == "e");
}

TEST(csv_tokenizer_unclosed_quote_has_exact_position) {
    CsvTokenizer tk;
    try {
        tk.tokenizeLine("1,\"oops,3", 12);
        CHECK(false);
    } catch (const ParseError& e) {
        CHECK(e.line() == 12 && e.column() == 3);
    }
}

TEST(csv_tokenizer_escaped_quotes_and_trailing_delimiter) {
    CsvTokenizer tk;
    auto toks = tk.tokenizeLine("\"say \"\"hi\"\"\",", 1);
    CHECK(toks[0].text == "say \"hi\"");
    // trailing delimiter => one extra empty field
    int fields = 0;
    for (auto& t : toks) if (t.type == TokenType::Field || t.type == TokenType::QuotedField) ++fields;
    CHECK(fields == 2);
}

// ---------------------------------------------------------------- Module 3 + 4 (CSV)
TEST(csv_valid_file_parses_cleanly) {
    CsvDocument d = CsvParser(csvOpts(kProductSchema)).parseFile("testdata/valid_products.csv");
    CHECK(d.records.size() == 5);
    CHECK(d.log.empty());
    CHECK(d.headers.size() == 4);
    CHECK(d.records[1].fields[1] == "Pen, blue");
    CHECK(d.records[2].fields[1] == "Ruler \"30cm\"");
}

TEST(csv_corrupt_file_recovers_and_reports_every_error) {
    CsvDocument d = CsvParser(csvOpts(kProductSchema)).parseFile("testdata/corrupt_products.csv");
    CHECK(d.rowsRead == 10);
    CHECK(d.records.size() == 2);   // lines 2 and 11 are good
    CHECK(d.rowsSkipped == 8);
    CHECK(d.log.count("ParseError") == 3);
    CHECK(d.log.count("ValidationError") == 5);
    CHECK(d.records[0].line == 2 && d.records[1].line == 11);
    // exact locations of a few errors
    CHECK(d.log.entries()[0].line == 3 && d.log.entries()[0].column == 3);   // unclosed quote
    CHECK(d.log.entries()[1].line == 4 && d.log.entries()[1].column == 9);   // 'abc' price
    CHECK(d.log.entries()[2].line == 5);                                     // missing field
}

TEST(csv_without_schema_enforces_header_field_count_only) {
    TempFile f("tmp_nos.csv", "a,b,c\n1,2,3\n4,5\n6,7,8,9\n");
    CsvDocument d = CsvParser().parseFile(f.path);
    CHECK(d.records.size() == 1);
    CHECK(d.log.size() == 2);
}

TEST(csv_no_header_first_row_defines_width) {
    CsvOptions o;
    o.hasHeader = false;
    TempFile f("tmp_nohdr.csv", "1,2\n3,4\n5\n");
    CsvDocument d = CsvParser(o).parseFile(f.path);
    CHECK(d.records.size() == 2);
    CHECK(d.log.size() == 1);
}

TEST(csv_custom_delimiter_and_crlf) {
    CsvOptions o;
    o.delimiter = ';';
    TempFile f("tmp_semi.csv", "x;y\r\n1;\"a;b\"\r\n");
    CsvDocument d = CsvParser(o).parseFile(f.path);
    CHECK(d.records.size() == 1 && d.records[0].fields[1] == "a;b");
}

TEST(csv_optional_field_may_be_empty) {
    CsvOptions o;
    o.schema = parseSchemaSpec("id:int,note:string?");
    TempFile f("tmp_opt.csv", "id,note\n1,\n2,hello\n");
    CsvDocument d = CsvParser(o).parseFile(f.path);
    CHECK(d.records.size() == 2 && d.log.empty());
}

TEST(csv_empty_and_missing_files_throw_FileError) {
    bool emptyCaught = false, missingCaught = false;
    try { CsvParser().parseFile("testdata/empty.csv"); } catch (const FileError&) { emptyCaught = true; }
    try { CsvParser().parseFile("testdata/nope.csv"); } catch (const FileError&) { missingCaught = true; }
    CHECK(emptyCaught && missingCaught);
    CHECK(FileReader::openCount() == 0);
}

TEST(csv_strict_mode_rethrows_derived_type) {
    CsvOptions o = csvOpts(kProductSchema);
    o.stopOnFirstError = true;
    bool parseErr = false;
    try { CsvParser(o).parseFile("testdata/corrupt_products.csv"); }
    catch (const ParseError& e) { parseErr = (e.line() == 3); }  // first bad row is a ParseError
    CHECK(parseErr);
    CHECK(FileReader::openCount() == 0);  // file closed despite the exception
}

TEST(csv_max_errors_aborts) {
    CsvOptions o = csvOpts(kProductSchema);
    o.maxErrors = 3;
    bool aborted = false;
    try { CsvParser(o).parseFile("testdata/corrupt_products.csv"); }
    catch (const ParseError& e) { aborted = std::string(e.message()).find("too many errors") != std::string::npos; }
    CHECK(aborted);
    CHECK(FileReader::openCount() == 0);
}

// ---------------------------------------------------------------- Module 3 + 4 (JSON)
TEST(json_valid_file_parses_cleanly) {
    JsonOptions o;
    o.schema = parseSchemaSpec(kUserSchema);
    JsonDocument d = JsonParser(o).parseFile("testdata/valid_users.json");
    CHECK(d.records.size() == 3 && d.log.empty());
    CHECK(d.records[1].find("name")->text == "Rohan \"R\" Patil");
    CHECK(d.records[2].find("tags")->items.size() == 2);
}

TEST(json_corrupt_file_recovers_and_reports_every_error) {
    JsonOptions o;
    o.schema = parseSchemaSpec(kUserSchema);
    JsonDocument d = JsonParser(o).parseFile("testdata/corrupt_users.json");
    CHECK(d.recordsRead == 11);          // ids 1..11, none silently lost
    CHECK(d.records.size() == 3);        // ids 1, 8, 11
    CHECK(d.recordsSkipped == 8);
    CHECK(d.log.count("ValidationError") == 3);  // "three", 17.5, missing name
    CHECK(d.log.count("ParseError") == 6);       // 5 skipped records + the trailing comma
    CHECK(d.log.entries()[1].line == 4);         // the record after a broken one is still checked
    CHECK(d.records[0].find("id")->numberText == "1");
    CHECK(d.records[1].find("id")->numberText == "8");
    CHECK(d.records[2].find("id")->numberText == "11");
}

TEST(json_missing_closing_bracket_is_reported_not_crashed) {
    JsonDocument d = JsonParser().parseFile("testdata/unclosed_array.json");
    CHECK(d.records.size() == 2);
    CHECK(d.log.size() == 1);
}

TEST(json_unterminated_string_skips_only_that_record) {
    TempFile f("tmp_unterm.json",
               "[\n {\"a\": 1},\n {\"a\": \"oops},\n {\"a\": 3}\n]\n");
    JsonDocument d = JsonParser().parseFile(f.path);
    CHECK(d.records.size() == 2);
    CHECK(d.log.size() >= 1);
    CHECK(d.log.entries()[0].line == 3);
}

TEST(json_missing_comma_between_records_keeps_both) {
    TempFile f("tmp_nocomma.json", "[ {\"a\": 1} {\"a\": 2} ]");
    JsonDocument d = JsonParser().parseFile(f.path);
    CHECK(d.records.size() == 2);
    CHECK(d.log.size() == 1);
}

TEST(json_single_object_and_nested_values) {
    TempFile f("tmp_single.json", "{\"a\": {\"b\": [1, 2, {\"c\": null}]}, \"d\": -1.5e3}");
    JsonDocument d = JsonParser().parseFile(f.path);
    CHECK(d.records.size() == 1 && d.log.empty());
    CHECK(d.records[0].find("d")->number == -1500.0);
}

TEST(json_deep_nesting_is_rejected_not_stack_overflow) {
    std::string deep(200, '[');
    deep += std::string(200, ']');
    TempFile f("tmp_deep.json", "[" + deep + "]");
    JsonDocument d = JsonParser().parseFile(f.path);  // must not crash
    CHECK(d.log.size() >= 1);
}

TEST(json_non_object_record_is_validation_error) {
    TempFile f("tmp_scalar.json", "[1, {\"a\": 2}]");
    JsonDocument d = JsonParser().parseFile(f.path);
    CHECK(d.records.size() == 1);
    CHECK(d.log.count("ValidationError") == 1);
}

TEST(json_empty_and_blank_files_throw_FileError) {
    TempFile blank("tmp_blank.json", "   \n\n");
    TempFile empty("tmp_empty.json", "");
    bool a = false, b = false;
    try { JsonParser().parseFile(blank.path); } catch (const FileError&) { a = true; }
    try { JsonParser().parseFile(empty.path); } catch (const FileError&) { b = true; }
    CHECK(a && b);
    CHECK(FileReader::openCount() == 0);
}

TEST(json_strict_mode_rethrows_derived_type) {
    JsonOptions o;
    o.schema = parseSchemaSpec(kUserSchema);
    o.stopOnFirstError = true;
    bool validation = false;
    // first bad record in corrupt_users.json is a ParseError (line 3)
    try { JsonParser(o).parseFile("testdata/corrupt_users.json"); }
    catch (const ParseError& e) { validation = (e.line() == 3); }
    CHECK(validation);
}

// ---------------------------------------------------------------- Schema helpers
TEST(schema_validators) {
    CHECK(isValidInteger("42") && isValidInteger(" -7 ") && !isValidInteger("4.2") && !isValidInteger("abc"));
    CHECK(!isValidInteger("99999999999999999999"));
    CHECK(isValidDouble("3.14") && isValidDouble("1e3") && !isValidDouble("nan") && !isValidDouble("0x10"));
    CHECK(isValidBoolean("TRUE") && isValidBoolean("no") && !isValidBoolean("maybe"));
    bool threw = false;
    try { parseSchemaSpec("id:integer,x:wat"); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

int main() {
    for (int i = 0; i < g_count; ++i) {
        const int before = g_failed;
        try {
            g_tests[i].fn();
        } catch (const std::exception& e) {
            ++g_failed;
            std::cout << "    FAIL  unexpected exception: " << e.what() << "\n";
        }
        std::cout << (g_failed == before ? "[ PASS ] " : "[ FAIL ] ") << g_tests[i].name << "\n";
    }
    std::cout << "\n" << (g_run - g_failed) << "/" << g_run << " checks passed\n";
    return g_failed == 0 ? 0 : 1;
}
