# Robust File Parser with Custom Exceptions

A C++17 parser for **CSV** and **JSON-like** files that reports problems with their exact
**line and column**, groups them into a **custom exception hierarchy**, and **recovers**
by skipping bad records instead of crashing.

The project includes an interactive demonstration menu, input previews, and error-context
display so that invalid data and the parser's response can be inspected during a live demo.

**Domain:** Data Engineering  
**Language:** C++17 (`fstream`, `vector`, `string`, exceptions, RAII)  
**Compiler used:** MinGW-w64 G++  
**External libraries:** None

---

## 1. Project structure

```text
robust-parser/
├── src/
│   ├── Exceptions.h        Module 4  exception hierarchy
│   ├── FileReader.h/.cpp   Module 1  RAII file reader
│   ├── Token.h             Module 2  token type (+ line/column)
│   ├── CsvTokenizer.h/.cpp Module 2  CSV tokenizer
│   ├── JsonTokenizer.h/.cpp Module 2  JSON-like tokenizer
│   ├── Schema.h/.cpp       Module 3  field types + validators
│   ├── CsvParser.h/.cpp    Module 3+4 CSV parser, validation, recovery
│   ├── JsonValue.h         Module 3  parsed JSON value
│   ├── JsonParser.h/.cpp   Module 3+4 JSON parser, validation, recovery
│   ├── ErrorLog.h/.cpp     Module 5  error report
│   └── main.cpp            interactive menu + command-line demo
├── tests/
│   └── test_main.cpp       Module 5  74 automated checks
├── testdata/               valid and corrupt input files
├── logs/                   generated error reports
└── Makefile                optional (g++ users)
```

---

## 2. Build and run

### MinGW-w64 G++ on Windows (PowerShell)

The project was compiled using MinGW-w64 G++ with the C++17 standard. Open PowerShell
in the project root directory.

**Build the interactive demo:**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc src\CsvParser.cpp src\CsvTokenizer.cpp src\ErrorLog.cpp src\FileReader.cpp src\JsonParser.cpp src\JsonTokenizer.cpp src\Schema.cpp src\main.cpp -o rfp_demo.exe
```

**Run the interactive demo:**

```powershell
.\rfp_demo.exe
```

**Build the automated tests:**

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -Isrc src\CsvParser.cpp src\CsvTokenizer.cpp src\ErrorLog.cpp src\FileReader.cpp src\JsonParser.cpp src\JsonTokenizer.cpp src\Schema.cpp tests\test_main.cpp -o rfp_tests.exe
```

**Run the tests:**

```powershell
.\rfp_tests.exe
```

### Interactive demonstration menu

When the program is started without command-line arguments, it displays this menu:

```text
========== ROBUST FILE PARSER ==========
1. Parse Valid CSV File
2. Parse Corrupted CSV File
3. Parse Valid JSON File
4. Parse Corrupted JSON File
5. Parse Custom File
6. Demonstrate Strict Mode
7. Exit
```

| Option | Demonstration |
|---|---|
| 1 | Parses `testdata/valid_products.csv` |
| 2 | Parses `testdata/corrupt_products.csv` and writes `logs/corrupt_products.log` |
| 3 | Parses `testdata/valid_users.json` |
| 4 | Parses `testdata/corrupt_users.json` and writes `logs/corrupt_users.log` |
| 5 | Asks for a custom file path and schema; format is inferred from the file extension |
| 6 | Demonstrates strict mode using the corrupted CSV input |
| 7 | Exits the application |

After a demonstration finishes, press Enter to return to the menu.

### Input preview and error context

Before parsing, the interactive demo displays the selected input file with line numbers.
This makes it easy to see the original data, including malformed rows or JSON records.

When errors are reported, the demo also displays the relevant source line and a caret (`^`)
to indicate the reported column, followed by the error category, line, column, and message.
This helps the person watching the demonstration connect a parser error to the original input.

The normal error report is also written to the corresponding file in `logs/` for the
corrupted CSV and JSON demonstrations.

### Command-line mode

The original command-line interface is retained. If command-line arguments are supplied,
the program uses command-line mode instead of displaying the interactive menu.

```powershell
.\rfp_demo.exe testdata\corrupt_products.csv --schema "id:int,name:string,price:double,in_stock:bool"
```

### Command-line options

```text
rfp_demo <file> [--format csv|json] [--schema "id:int,name:string,note:string?"]
                [--delim ;] [--no-header] [--strict] [--max-errors N] [--log report.txt]
```

- Format is guessed from the extension (`.json` = JSON, everything else = CSV).
- Schema types: `string`, `int`, `double`, `bool`. A trailing `?` marks an optional field.
- `--strict` stops at the first error. The default mode attempts to recover and continue.
- `--max-errors N` gives up after N errors (default 1000, 0 = unlimited).
- `--log report.txt` writes the error report to the specified file.
- Exit code: `0` clean, `1` errors found (and recovered), `2` fatal (file problem or bad arguments).

### Visual Studio / Code::Blocks

The source code is standard C++17 and can also be built in an IDE configured for C++17.

For Visual Studio, create an Empty C++ Project, add the source files from `src/`, and set
the language standard to **ISO C++17**. For the automated tests, use a separate test
project containing the source files except `main.cpp`, together with `tests/test_main.cpp`.

For Code::Blocks, create a C++ Console Application, add the source files, and enable the
C++17 language standard in the compiler settings. Use a separate test target for
`tests/test_main.cpp`, excluding `src/main.cpp`.

---

## 3. How each module works

### Module 1: RAII file reader (`FileReader`)

Opens the file in its **constructor** (throws `FileError` on failure) and closes it in its
**destructor**. Copying is disabled. It also normalises CRLF line endings, strips a UTF-8
BOM, and counts lines. A static `openCount()` lets the tests prove that no file stays open
after an exception (`file_reader_releases_file_when_exception_thrown`).

### Module 2: Tokenizer

Every `Token` carries `type`, `text`, `line`, and `column` (1-based).

- **CSV:** `Field`, `QuotedField`, `Delimiter`, `EndOfLine`. Handles `""` escapes,
  delimiters inside quotes, and trailing delimiters.
- **JSON-like:** `{ } [ ] : ,`, strings (with escapes and `\uXXXX`), numbers (strict JSON
  grammar), `true/false/null`.
- Lexical errors become `Invalid` tokens (message inside) so the tokenizer does not stop
  midway through the file.

### Module 3: Parser and validation

- **CSV** builds records from tokens and checks, in this order: field count (missing or
  too many), required-but-empty fields, and the type of each field.
- **JSON** is a recursive-descent parser (depth limit 64). Each top-level object is a
  *record*, checked against the schema for required keys and value types (`int` rejects
  `17.5`). It also rejects duplicate keys, trailing commas, and leading-zero numbers
  such as `01`.

### Module 4: Exception hierarchy and recovery

```text
std::exception
  └── ParserException       category, message, line, column, raise()
        ├── FileError        cannot open / read / empty file          (fatal)
        ├── ParseError       syntax: unclosed quote, bad token, bad nesting
        └── ValidationError  well-formed but breaks a rule: count, type, required
```

`raise()` is a virtual re-throw that keeps the **derived** type (a plain `throw ex;`
through a base reference would slice it).

**Recovery strategy** (errors are caught at the *record* level):

| Situation | Behaviour |
|---|---|
| Bad CSV row (parse or validation) | Log it, skip that row, continue with the next line |
| Bad CSV header | Log it, continue (schema or first row defines the width) |
| Bad JSON record | Log it, skip to the next record boundary (`, {`) and continue |
| Unterminated JSON string | The record is skipped; the next `{` on a later line starts a new record |
| Missing comma between JSON records | Logged, both records are kept |
| Missing closing `]` | Logged, everything parsed before it is kept |
| `FileError` (missing/empty file) | Not recoverable; propagates to the caller |
| More than `maxErrors` errors | Aborts with a `ParseError` ("too many errors") |

### Module 5: Error report and testing

`ErrorLog` collects every error (`category, line, column, message, skipped?`) and prints
a summary. The interactive demo supplements this report with the original source line
and a caret marker to make the error location easier to understand during a presentation.

`tests/test_main.cpp` is a small self-written test runner; no testing framework needs to
be installed.

---

## 4. Design decisions and known limits

These are useful points to explain during a project viva:

- **Quoted CSV fields cannot span lines.** An unclosed quote therefore damages one row,
  not the rest of the file. (RFC 4180 allows multi-line fields; this project trades that
  for recoverability.)
- **JSON strings cannot span lines**, for the same reason.
- JSON resynchronisation is a heuristic (comma followed by `{`, or a `{` on a new line
  after a bad token). Deeply malformed input can lose a neighbouring record, but this is
  reported as an error and does not cause the parser to intentionally crash.
- CSV values are kept as strings after validation; the schema only *checks* types.
- Blank CSV lines are ignored.
- In strict mode, parsing stops at the first error. The normal recovery-mode error-context
  listing depends on the completed error log, so strict mode demonstrates the first
  exception rather than a full list of all error contexts.
- The caret display uses the reported column as a character position; it is a simple
  terminal indicator and may not align perfectly for tabs or wide Unicode characters.

---

## 5. Test data (`testdata/`)

| File | Purpose |
|---|---|
| `valid_products.csv` | Quoted commas, `""` escapes, blank line, `yes/1/0` booleans |
| `corrupt_products.csv` | Unclosed quote, bad type, missing field, extra field, text after quote, stray quote, bad boolean, empty required field |
| `empty.csv` | Empty file → `FileError` |
| `valid_users.json` | Nested array, null, escaped quotes |
| `corrupt_users.json` | Unterminated/missing quote, wrong type, float for int, missing key, bad character, bad literal, duplicate key, leading zero, trailing comma |
| `unclosed_array.json` | Missing `]` |

---

## 6. Sample error logs

### Corrupted CSV

Command-line example:

```powershell
.\rfp_demo.exe testdata\corrupt_products.csv --schema "id:int,name:string,price:double,in_stock:bool" --log logs\corrupt_products.log
```

Expected report from the sample test data:

```text
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

Two valid rows (lines 2 and 11) are still returned.

### Corrupted JSON

Command-line example:

```powershell
.\rfp_demo.exe testdata\corrupt_users.json --schema "id:int,name:string,age:int,active:bool,email:string?"
```

Expected report from the sample test data:

```text
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

Three valid records (IDs 1, 8, and 11) are still returned.

### Strict mode

The interactive menu's strict-mode demonstration stops at the first error. For the sample
corrupted CSV file, the first reported error is:

```text
[ParseError] line 3, col 3: unclosed quote (field opened here never ends on this line)
```

---

## 7. Using it as a library

```cpp
#include "CsvParser.h"
using namespace rfp;

CsvOptions opt;
opt.schema = parseSchemaSpec("id:int,name:string,price:double");

try {
    CsvDocument doc = CsvParser(opt).parseFile("data.csv");
    for (const CsvRecord& r : doc.records) {
        /* r.line, r.fields */
    }
    doc.log.print(std::cout, "data.csv");  // every error, with line and column
} catch (const FileError& e) {
    std::cerr << e.what() << "\n";
}
```

---

## 8. Test results

The automated test suite was rebuilt and run after the interactive-menu and error-context
changes.

```text
74/74 checks passed
0 failures
```

The project uses standard C++17 features and does not require third-party libraries.
