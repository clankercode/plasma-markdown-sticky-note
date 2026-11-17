// SPDX-License-Identifier: MIT
#include "frontmatter.h"
#include <QRegularExpression>

namespace {
bool commentBoundary(const QString &text, qsizetype i)
{
    return text.at(i) == '#' && (i == 0 || text.at(i - 1).isSpace());
}
}

namespace FrontMatter {
QString Document::value(const QString &key, const QString &fallback) const
{
    for (const auto &line : lines)
        if (line.key == key)
            return line.value;
    return fallback;
}
bool Document::contains(const QString &key) const
{
    for (const auto &line : lines)
        if (line.key == key)
            return true;
    return false;
}
void Document::setValue(const QString &key, const QString &value)
{
    bool found = false;
    for (auto &line : lines) {
        if (line.key != key)
            continue;
        found = true;
        if (line.value != value) {
            line.value = value;
            line.raw.clear();
        }
    }
    if (!found)
        lines.append({key, value, {}});
}
void Document::remove(const QString &key)
{
    lines.removeIf([&](const Line &line) { return line.key == key; });
}

QString decodeScalar(const QString &raw)
{
    const QString text = raw.trimmed();
    if (text.isEmpty())
        return {};
    if (text.front() == '\'' || text.front() == '"') {
        const QChar quote = text.front();
        QString result;
        for (qsizetype i = 1; i < text.size(); ++i) {
            QChar c = text.at(i);
            if (c == quote) {
                if (quote == '\'' && i + 1 < text.size() && text.at(i + 1) == '\'') {
                    result += '\'';
                    ++i;
                    continue;
                }
                return result;
            }
            if (quote == '"' && c == '\\' && i + 1 < text.size()) {
                c = text.at(++i);
                if (c == 'n') result += '\n';
                else if (c == 't') result += '\t';
                else if (c == 'r') result += '\r';
                else if (c == '"' || c == '\\') result += c;
                else { result += '\\'; result += c; }
            } else {
                result += c;
            }
        }
        // An unterminated quote is not a valid scalar: retain the text rather than truncate it.
        return text;
    }
    for (qsizetype i = 0; i < text.size(); ++i)
        if (commentBoundary(text, i))
            return text.left(i).trimmed();
    return text;
}

QString encodeScalar(const QString &value)
{
    static const QRegularExpression typed(QStringLiteral(
        R"(^(?:true|false|yes|no|on|off|null|~|[-+]?(?:[0-9][0-9_.]*(?:[eE][-+]?[0-9]+)?|\.[0-9]+(?:[eE][-+]?[0-9]+)?|0[xob][0-9a-f]+|\.inf|\.nan))$)"),
        QRegularExpression::CaseInsensitiveOption);
    bool quote = value.isEmpty() || value != value.trimmed() || value.contains(": ")
        || value.contains(" #") || value.contains('\n') || value.contains('\r') || value.contains('\t')
        || typed.match(value).hasMatch();
    for (qsizetype i = 0; !quote && i < value.size(); ++i)
        quote = commentBoundary(value, i);
    if (!value.isEmpty())
        quote = quote || QStringLiteral("#&*!|>'\"%@`-?:[]{},").contains(value.front());
    if (!quote)
        return value;
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("\n", "\\n");
    escaped.replace("\r", "\\r");
    escaped.replace("\t", "\\t");
    return '"' + escaped + '"';
}

Document parse(const QString &input)
{
    Document doc;
    QString text = input;
    text.replace("\r\n", "\n");
    doc.body = text;
    const bool bom = text.startsWith(QChar(0xfeff));
    const qsizetype start = bom ? 1 : 0;
    const auto firstEnd = text.indexOf('\n', start);
    const auto first = text.mid(start, firstEnd < 0 ? -1 : firstEnd - start);
    auto delimiter = [](const QString &line) {
        return line.startsWith("---") && line.mid(3).trimmed().isEmpty();
    };
    if (!delimiter(first))
        return doc;
    doc.malformed = true;
    if (firstEnd < 0)
        return doc;
    QList<Line> lines;
    qsizetype pos = firstEnd + 1;
    while (pos < text.size()) {
        const auto end = text.indexOf('\n', pos);
        const auto line = text.mid(pos, end < 0 ? -1 : end - pos);
        if (delimiter(line)) {
            doc.hadFrontMatter = true;
            doc.malformed = false;
            doc.byteOrderMark = bom;
            doc.openingDelimiter = first;
            doc.closingDelimiter = line;
            doc.closingNewline = end >= 0;
            doc.lines = lines;
            doc.body = end < 0 ? QString() : text.mid(end + 1);
            return doc;
        }
        Line entry{{}, {}, line};
        const auto colon = line.indexOf(':');
        if (colon > 0 && !line.startsWith('#') && !line.front().isSpace()) {
            const auto key = line.left(colon);
            bool valid = true;
            for (const auto c : key)
                if (!c.isLetterOrNumber() && c != '_' && c != '-') { valid = false; break; }
            if (valid) {
                entry.key = key;
                entry.value = decodeScalar(line.mid(colon + 1));
            }
        }
        lines.append(entry);
        if (end < 0)
            break;
        pos = end + 1;
    }
    return doc;
}

QString serialize(const Document &doc)
{
    if (!doc.hadFrontMatter && doc.lines.isEmpty())
        return doc.body;
    QString result;
    if (doc.byteOrderMark)
        result += QChar(0xfeff);
    result += doc.openingDelimiter + '\n';
    for (const auto &line : doc.lines) {
        if (line.key.isEmpty() || !line.raw.isEmpty())
            result += line.raw;
        else
            result += line.key + ": " + encodeScalar(line.value);
        result += '\n';
    }
    result += doc.closingDelimiter;
    if (doc.closingNewline || !doc.body.isEmpty())
        result += '\n';
    result += doc.body;
    return result;
}
} // namespace FrontMatter
