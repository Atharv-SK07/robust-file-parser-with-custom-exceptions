#include "CsvParser.h"

#include "CsvTokenizer.h"
#include "Exceptions.h"
#include "FileReader.h"

namespace rfp {

namespace {

// Collect the field tokens of a tokenized line.
std::vector<Token> fieldTokens(const std::vector<Token>& tokens) {
    std::vector<Token> out;
    for (const Token& t : tokens) {
        if (t.type == TokenType::Field || t.type == TokenType::QuotedField) out.push_back(t);
    }
    return out;
}

// Rules: field count, then required values, then types.
void validateRow(const std::vector<Token>& fields, const std::vector<FieldSpec>& schema,
                 std::size_t expectedCount, std::size_t lineNo, std::size_t endCol) {
    if (fields.size() < expectedCount) {
        throw ValidationError("missing field(s): expected " + std::to_string(expectedCount) +
                                  " fields but found " + std::to_string(fields.size()),
                              lineNo, endCol);
    }
    if (fields.size() > expectedCount) {
        throw ValidationError("too many fields: expected " + std::to_string(expectedCount) +
                                  " but found " + std::to_string(fields.size()),
                              lineNo, fields[expectedCount].column);
    }
    for (std::size_t i = 0; i < schema.size(); ++i) {
        const Token& f = fields[i];
        if (isBlank(f.text)) {
            if (schema[i].required) {
                throw ValidationError("field '" + schema[i].name + "' is required but empty",
                                      lineNo, f.column);
            }
            continue;  // optional and empty: fine
        }
        if (!conformsTo(f.text, schema[i].type)) {
            throw ValidationError("field '" + schema[i].name + "' expected " +
                                      toString(schema[i].type) + " but found '" + f.text + "'",
                                  lineNo, f.column);
        }
    }
}

}  // namespace

CsvDocument CsvParser::parseFile(const std::string& path) const {
    CsvDocument doc;
    FileReader reader(path);  // RAII: closed automatically, even on exceptions
    CsvTokenizer tokenizer(opt_.delimiter);

    std::vector<FieldSpec> schema = opt_.schema;
    std::size_t expectedCount = schema.size();
    bool headerPending = opt_.hasHeader;
    bool haveExpected = !schema.empty();

    std::string line;
    while (reader.readLine(line)) {
        const std::size_t lineNo = reader.lineNumber();
        if (isBlank(line)) continue;  // blank lines are ignored

        const bool isHeaderLine = headerPending;
        if (!isHeaderLine) ++doc.rowsRead;  // count before tokenizing so broken rows are counted too
        try {
            const std::vector<Token> tokens = tokenizer.tokenizeLine(line, lineNo);
            const std::vector<Token> fields = fieldTokens(tokens);

            if (isHeaderLine) {
                headerPending = false;
                for (const Token& f : fields) doc.headers.push_back(f.text);
                if (!schema.empty()) {
                    if (doc.headers.size() != schema.size()) {
                        // Logged but not fatal: the schema is trusted over the header.
                        throw ValidationError("header has " + std::to_string(doc.headers.size()) +
                                                  " columns but schema defines " +
                                                  std::to_string(schema.size()),
                                              lineNo, 1);
                    }
                } else {
                    expectedCount = doc.headers.size();
                    haveExpected = true;
                    for (const std::string& h : doc.headers) schema.push_back({h, FieldType::String, false});
                    // no schema given: only the field count is enforced
                }
                continue;
            }

            if (!haveExpected) {  // headerless + schemaless: first row defines the width
                expectedCount = fields.size();
                haveExpected = true;
                for (std::size_t i = 0; i < expectedCount; ++i) {
                    schema.push_back({"column" + std::to_string(i + 1), FieldType::String, false});
                }
            }

            validateRow(fields, schema, expectedCount, lineNo, line.size() + 1);

            CsvRecord rec;
            rec.line = lineNo;
            for (const Token& f : fields) rec.fields.push_back(f.text);
            doc.records.push_back(std::move(rec));
        } catch (const ParserException& ex) {
            if (opt_.stopOnFirstError) throw;
            if (isHeaderLine) headerPending = false;  // never treat the next line as header
            const bool skipped = !isHeaderLine;       // a bad header isn't a skipped data row
            if (skipped) ++doc.rowsSkipped;
            doc.log.add(ex, skipped);
            if (opt_.maxErrors != 0 && doc.log.size() >= opt_.maxErrors) {
                throw ParseError("too many errors (" + std::to_string(doc.log.size()) +
                                     "), giving up",
                                 lineNo, 0);
            }
        }
    }

    if (reader.lineNumber() == 0) throw FileError("file is empty: '" + path + "'");
    return doc;
}

}  // namespace rfp
