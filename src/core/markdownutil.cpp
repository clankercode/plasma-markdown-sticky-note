// SPDX-License-Identifier: MIT
#include "markdownutil.h"
#include <QStringView>

namespace {
struct ScanState {
    QChar fence;
    qsizetype fenceLength = 0;
    // Returns true for a fence delimiter or content inside a fence.
    bool fenced(QStringView line) {
        qsizetype p = 0;
        while (p < line.size() && (line.at(p) == ' ' || line.at(p) == '\t')) ++p;
        const auto start = p;
        if (p < line.size() && (line.at(p) == '`' || line.at(p) == '~')) {
            const QChar c = line.at(p);
            while (p < line.size() && line.at(p) == c) ++p;
            const auto length = p - start;
            if (fenceLength) {
                if (c == fence && length >= fenceLength && line.mid(p).trimmed().isEmpty())
                    fenceLength = 0;
                return true;
            }
            if (length >= 3) {
                fence = c;
                fenceLength = length;
                return true;
            }
        }
        return fenceLength != 0;
    }
};
bool space(QChar c) { return c == ' ' || c == '\t'; }
qsizetype listContent(QStringView line, qsizetype p)
{
    if (p >= line.size()) return -1;
    if (line.at(p) == '-' || line.at(p) == '*' || line.at(p) == '+') {
        ++p;
    } else if (line.at(p).isDigit()) {
        while (p < line.size() && line.at(p).isDigit()) ++p;
        if (p >= line.size() || line.at(p) != '.') return -1;
        ++p;
    } else return -1;
    if (p >= line.size() || !space(line.at(p))) return -1;
    while (p < line.size() && space(line.at(p))) ++p;
    return p;
}
qsizetype taskOffset(QStringView line)
{
    qsizetype p = 0;
    while (p < line.size() && space(line.at(p))) ++p;
    p = listContent(line, p);
    if (p < 0 || p + 3 >= line.size() || line.at(p) != '[' || line.at(p + 2) != ']' || !space(line.at(p + 3)))
        return -1;
    const auto c = line.at(p + 1);
    return c == ' ' || c == 'x' || c == 'X' ? p : -1;
}
template<typename Callback> void scanTasks(const QString &body, Callback callback)
{
    ScanState state;
    qsizetype start = 0;
    int index = 0;
    while (start < body.size()) {
        auto end = body.indexOf('\n', start);
        if (end < 0) end = body.size();
        QStringView line = QStringView(body).mid(start, end - start);
        if (!state.fenced(line)) {
            auto offset = taskOffset(line);
            if (offset >= 0 && !callback(start + offset, index++)) return;
        }
        start = end + 1;
    }
}
} // namespace

namespace MarkdownUtil {
QString deriveTitle(const QString &body, int maxLength)
{
    if (maxLength <= 0) return {};
    ScanState state;
    qsizetype start = 0;
    while (start < body.size()) {
        auto end = body.indexOf('\n', start);
        if (end < 0) end = body.size();
        auto line = QStringView(body).mid(start, end - start);
        start = end + 1;
        if (state.fenced(line)) continue;
        line = line.trimmed();
        if (line.isEmpty()) continue;
        qsizetype p = 0;
        while (p < line.size() && (line.at(p) == '#' || line.at(p) == '>' || space(line.at(p)))) ++p;
        auto content = listContent(line, p);
        if (content >= 0) p = content;
        if (p + 2 < line.size() && line.at(p) == '[' && line.at(p + 2) == ']'
            && (line.at(p + 1) == ' ' || line.at(p + 1) == 'x' || line.at(p + 1) == 'X')) p += 3;
        QString title;
        title.reserve(qMin<qsizetype>(line.size(), maxLength + 1));
        qsizetype pendingSpace = 0;
        qsizetype spaceStart = 0;
        for (; p < line.size(); ++p) {
            const auto c = line.at(p);
            if (c == '*' || c == '_' || c == '`') continue;
            if (c.isSpace()) {
                if (!title.isEmpty()) {
                    if (!pendingSpace) spaceStart = p;
                    ++pendingSpace;
                }
                continue;
            }
            if (title.size() + pendingSpace + 1 > maxLength) {
                for (qsizetype i = spaceStart; title.size() < maxLength - 1 && i < p; ++i)
                    if (line.at(i).isSpace()) title += line.at(i);
                return title.left(maxLength - 1) + QChar(0x2026);
            }
            // Preserve interior whitespace (with emphasis markers removed), but not trailing space.
            for (qsizetype i = spaceStart; pendingSpace && i < p; ++i)
                if (line.at(i).isSpace()) { title += line.at(i); --pendingSpace; }
            title += c;
        }
        return title;
    }
    return {};
}
int taskCount(const QString &body)
{
    int count = 0;
    scanTasks(body, [&](qsizetype, int) { ++count; return true; });
    return count;
}
QString setTaskChecked(const QString &body, int index, bool checked)
{
    if (index < 0) return body;
    QString result = body;
    scanTasks(body, [&](qsizetype offset, int n) {
        if (n != index) return true;
        if ((body.at(offset + 1) != ' ') != checked)
            result[offset + 1] = checked ? 'x' : ' ';
        return false;
    });
    return result;
}
QString toggleTask(const QString &body, int index)
{
    if (index < 0) return body;
    QString result = body;
    scanTasks(body, [&](qsizetype offset, int n) {
        if (n != index) return true;
        result[offset + 1] = body.at(offset + 1) == ' ' ? 'x' : ' ';
        return false;
    });
    return result;
}
QString renderable(const QString &body)
{
    QString result;
    qsizetype copied = 0;
    scanTasks(body, [&](qsizetype offset, int index) {
        result += QStringView(body).mid(copied, offset - copied);
        result += body.at(offset + 1) == ' ' ? QStringLiteral("[☐](task:") : QStringLiteral("[☑](task:");
        result += QString::number(index) + ')';
        copied = offset + 3;
        return true;
    });
    if (!copied) return body;
    result += QStringView(body).mid(copied);
    return result;
}
} // namespace MarkdownUtil
