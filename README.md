# Robust File Parser with Custom Exceptions

A C++17 parser for **CSV** and **JSON-like** files that reports every problem with its exact
**line and column**, groups problems into a **custom exception hierarchy**, and **recovers**
by skipping bad records instead of crashing.

Domain: Data Engineering. Language: C++17 (`fstream`, `vector`, `string`, exceptions, RAII).
No external libraries.

---

## 1. Project structure

```
robust-parser/
├── src/
│   ├── Exceptions.h        Module 4  exception hierarchy
│   ├── FileReader.h/.cpp   Module 1  RAII file reader
│   ├── Token.h             Module 2  token type (+ line/column)
│   ├── CsvTokenizer.h/.cpp Module 2  CSV tokenizer
│   ├── JsonTokenizer.h/.cpp Module 2 JSON-like tokenizer
│   ├── Schema.h/.cpp       Module 3  field types + validators
│   ├── CsvParser.h/.cpp    Module 3+4 CSV parser, validation, recovery
│   ├── JsonValue.h         Module 3  parsed JSON value
│   ├── JsonParser.h/.cpp   Module 3+4 JSON parser, validation, recovery
│   ├── ErrorLog.h/.cpp     Module 5  error report
│   └── main.cpp            command-line demo
├── tests/test_main.cpp     Module 5  74 automated checks
├── testdata/               valid and corrupt input files
├── logs/                   sample error logs
└── Makefile                optional (g++ users)
```

## 2. Build and run

### Visual Studio (2017 or newer)
1. *File > New > Project > Empty Project (C++)*.
2. Add all files from `src/` (*Add > Existing Item*). For the demo, `main.cpp` is included.
3. *Project > Properties > C/C++ > Language > C++ Language Standard* = **ISO C++17 (/std:c++17)**.
4. *Debugging > Command Arguments*, for example:
   `testdata\corrupt_products.csv --schema "id:int,name:string,price:double,in_stock:bool"`
   (or leave it empty. The program will ask for a file name.)
5. Set *Debugging > Working Directory* to the project root so `testdata\` is found. Press Ctrl+F5.

**To run the tests instead:** make a second project with the `src/` files **except `main.cpp`**
plus `tests/test_main.cpp`, and add `src` to *C/C++ > General > Additional Include Directories*.

### Code::Blocks
1. *File > New > Project > Console application > C++*.
2. Remove the default `main.cpp` and add the files from `src/`.
3. *Project > Build options > Compiler settings* > tick **Have g++ follow the C++17 ISO C++ language standard [-std=c++17]**.
4. *Project > Set programs' arguments* for the command-line options, and set the working directory to the project root.
   (For the tests: a second project with `src/*` minus `main.cpp`, plus `tests/test_main.cpp`; add `src` under *Search directories > Compiler*.)

### g++ / Makefile
```
make          # builds rfp_demo and rfp_tests
make test     # runs the 74 checks
./rfp_demo testdata/corrupt_products.csv --schema "id:int,name:string,price:double,in_stock:bool"
```

### Command-line options
```
rfp_demo <file> [--format csv|json]  [--schema "id:int,name:string,note:string?"]
                [--delim ;]  [--no-header]  [--strict]  [--max-errors N]  [--log report.txt]
```
- Format is guessed from the extension (`.json` = JSON, everything else = CSV).
- Schema types: `string`, `int`, `double`, `bool`. A trailing `?` marks an optional field.
- `--strict` stops at the first error (exception propagates). Default is **recover and continue**.
- `--max-errors N` gives up after N errors (default 1000, 0 = unlimited).
- Exit code: `0` clean, `1` errors found (and recovered), `2` fatal (file problem, bad arguments).

---

## 3. How each module works

### Module 1: RAII file reader (`FileReader`)
Opens the file in its **constructor** (throws `FileError` on failure) and closes it in its
**destructor**. Copying is disabled. It also normalises CRLF line endings, strips a UTF-8 BOM, and
counts lines. A static `openCount()` lets the tests prove that no file stays open after an
exception (`file_reader_releases_file_when_exception_thrown`).

### Module 2: Tokenizer
Every `Token` carries `type`, `text`, `line`, `column` (1-based).
- **CSV**: `Field`, `QuotedField`, `Delimiter`, `EndOfLine`. Handles `""` escapes, delimiters inside quotes, trailing delimiters.
- **JSON-like**: `{ } [ ] : ,`, strings (with escapes and `\uXXXX`), numbers (strict JSON grammar), `true/false/null`.
  Lexical errors become `Invalid` tokens (message inside) so the tokenizer never stops mid-file.

### Module 3: Parser and validation
- **CSV** builds records from tokens and checks, in this order: field count (missing / too many), required-but-empty, type of each field.
- **JSON** is a recursive-descent parser (depth limit 64). Each top-level object is a *record*, checked against the schema for required keys and value types (`int` rejects `17.5`). It also rejects duplicate keys, trailing commas and leading-zero numbers such as `01`.

### Module 4: Exception hierarchy and recovery
```
std::exception
  └── ParserException       category, message, line, column, raise()
        ├── FileError        cannot open / read / empty file          (fatal)
        ├── ParseError       syntax: unclosed quote, bad token, bad nesting
        └── ValidationError  well-formed but breaks a rule: count, type, required
```
`raise()` is a virtual re-throw that keeps the **derived** type (a plain `throw ex;` through a base reference would slice it).

**Recovery strategy** (errors are caught at the *record* level):
| Situation | Behaviour |
|---|---|
| Bad CSV row (parse or validation) | log it, skip that row, continue with the next line |
| Bad CSV header | log it, continue (schema or first row defines the width) |
| Bad JSON record | log it, skip to the next record boundary (`, {`) and continue |
| Unterminated JSON string | the record is skipped; the next `{` on a later line starts a new record |
| Missing comma between JSON records | logged, both records are kept |
| Missing closing `]` | logged, everything parsed before it is kept |
| `FileError` (missing/empty file) | not recoverable, propagates to the caller |
| More than `maxErrors` errors | aborts with a `ParseError` ("too many errors") |

### Module 5: Error report and testing
`ErrorLog` collects every error (`category, line, column, message, skipped?`) and prints a summary.
`tests/test_main.cpp` is a small self-written runner (no framework to install).

---

## 4. Design decisions and known limits (good to mention in the viva)
- **Quoted CSV fields cannot span lines.** An unclosed quote therefore damages one row, not the rest of the file. (RFC 4180 allows multi-line fields; we trade that for recoverability.)
- **JSON strings cannot span lines**, for the same reason.
- JSON resynchronisation is a heuristic (comma followed by `{`, or a `{` on a new line after a bad token). Deeply malformed input can lose a neighbouring record, but this is always reported as an error and never crashes.
- CSV values are kept as strings after validation; the schema only *checks* types.
- Blank CSV lines are ignored.

## 5. Test data (`testdata/`)
| File | Purpose |
|---|---|
| `valid_products.csv` | quoted commas, `""` escapes, blank line, `yes/1/0` booleans |
| `corrupt_products.csv` | unclosed quote, bad type, missing field, extra field, text after quote, stray quote, bad boolean, empty required field |
| `empty.csv` | empty file → `FileError` |
| `valid_users.json` | nested array, null, escaped quotes |
| `corrupt_users.json` | unterminated/missing quote, wrong type, float for int, missing key, bad character, bad literal, duplicate key, leading zero, trailing comma |
| `unclosed_array.json` | missing `]` |

## 6. Sample error logs

Command: `rfp_demo testdata/corrupt_products.csv --schema "id:int,name:string,price:double,in_stock:bool" --log logs/corrupt_products.log`

```
==== Error Report: testdata/corrupt_products.csv ====
[ParseError]       line 3, col 3   (record skipped) unclosed quote (field opened here never ends on this line)
[ValidationError]  line 4, col 9   (record skipped) field 'price' expected number but found 'abc'
[ValidationError]  line 5, col 14  (record skipped) missing field(s): expected 4 fields but found 3
[ValidationError]  line 6, col 14  (record skipped) too many fields: expected 4 but found 5
[ParseError]       line 7, col 9   (record skipped) unexpected character 's' after closing quote
[ParseError]       line 8, col 12  (record skipped) unexpected quote inside an unquoted field
[ValidationError]  line 9, col 14  (record skipped) field 'in_stock' expected boolean but found 'maybe'
[ValidationError]  line 10, col 1  (record skipped) field 'id' is required but empty
---- Summary ----
Total errors     : 8
  ParseError     : 3
  ValidationError: 5
  FileError      : 0
Records skipped  : 8
```
(2 valid rows, lines 2 and 11, are still returned.)

Command: `rfp_demo testdata/corrupt_users.json --schema "id:int,name:string,age:int,active:bool,email:string?"`

```
==== Error Report: testdata/corrupt_users.json ====
[ParseError]       line 3, col 30  (record skipped) unknown literal 'age'
[ValidationError]  line 4, col 10  (record skipped) field 'id' expected integer but found string 'three'
[ValidationError]  line 5, col 37  (record skipped) field 'age' expected integer but found number '17.5'
[ValidationError]  line 6, col 3   (record skipped) missing required field 'name'
[ParseError]       line 7, col 36  (record skipped) unexpected character '@'
[ParseError]       line 8, col 49  (record skipped) unknown literal 'maybe'
[ParseError]       line 10, col 56 (record skipped) duplicate key 'age'
[ParseError]       line 11, col 37 (record skipped) invalid number '01'
[ParseError]       line 13, col 1  (logged)         trailing comma before ']'
---- Summary ----
Total errors     : 9
  ParseError     : 6
  ValidationError: 3
  FileError      : 0
Records skipped  : 8
```
(3 valid records, ids 1, 8, 11, are still returned.)

Fatal errors:
```
[FileError] file is empty: 'testdata/empty.csv'
[FileError] cannot open file 'testdata/nope.csv'
```
Strict mode (`--strict`): `[ParseError] line 3, col 3: unclosed quote (field opened here never ends on this line)`

More logs are in `logs/`.

## 7. Using it as a library
```cpp
#include "CsvParser.h"
using namespace rfp;

CsvOptions opt;
opt.schema = parseSchemaSpec("id:int,name:string,price:double");
try {
    CsvDocument doc = CsvParser(opt).parseFile("data.csv");
    for (const CsvRecord& r : doc.records) { /* r.line, r.fields */ }
    doc.log.print(std::cout, "data.csv");     // every error, with line and column
} catch (const FileError& e) {
    std::cerr << e.what() << "\n";            // [FileError] cannot open file 'data.csv'
}
```

## 8. Test results
`make test` → **74/74 checks passed** (also clean under `-fsanitize=address,undefined`).
