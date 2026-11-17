// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QString>

// Minimal, tolerant YAML front-matter reader/writer for flat `key: value` scalars.
//
// Round-trip contract:
//  * Lines are kept in order. Unknown keys, comment lines and blank lines inside the
//    front matter are preserved verbatim when the document is re-serialised.
//  * A field whose value has not been changed is written back using its original raw line.
//  * Changed / new fields are written as `key: <encoded value>` (quoted only when needed).
//  * A document without front matter parses with hadFrontMatter=false and body = whole text.
//  * Malformed front matter (opening `---` but no closing `---`) => malformed=true,
//    no fields, body = whole text (content is never lost).
//  * Accepts LF and CRLF input; output always uses LF.
namespace FrontMatter {

struct Line {
    QString key;      // empty for comment / blank / unparsable lines
    QString value;    // decoded scalar value (quotes removed, escapes resolved, trailing comment stripped)
    QString raw;      // original line text (without newline); empty for newly-added fields
};

struct Document {
    QList<Line> lines;
    QString body;
    bool hadFrontMatter = false;
    bool malformed = false;
    // Source framing, retained for lossless LF round-trips (including BOM and EOF delimiter).
    QString openingDelimiter = QStringLiteral("---");
    QString closingDelimiter = QStringLiteral("---");
    bool closingNewline = true;
    bool byteOrderMark = false;

    // Returns the decoded value for key, or defaultValue if absent.
    QString value(const QString &key, const QString &defaultValue = {}) const;
    bool contains(const QString &key) const;
    // Sets key's value. If the key exists and the decoded value differs, its raw text is cleared so
    // it will be re-encoded; if equal, nothing changes. New keys are appended at the end.
    void setValue(const QString &key, const QString &value);
    void remove(const QString &key);
};

Document parse(const QString &text);
QString serialize(const Document &doc);

// Encodes a scalar for YAML: bare when safe, otherwise double-quoted with escapes (\" \\ \n \t).
QString encodeScalar(const QString &value);
// Decodes a raw YAML scalar (after the `key:`), handling single/double quotes and `#` comments.
QString decodeScalar(const QString &raw);

} // namespace FrontMatter
