// SPDX-License-Identifier: MIT
#include "note.h"
#include "markdownutil.h"

namespace {
QDateTime nowWithOffset()
{
    const auto now = QDateTime::currentDateTime();
    return now.toOffsetFromUtc(now.offsetFromUtc());
}
bool boolean(const QString &value)
{
    return value.compare("true", Qt::CaseInsensitive) == 0
        || value.compare("yes", Qt::CaseInsensitive) == 0 || value == "1"
        || value.compare("on", Qt::CaseInsensitive) == 0;
}
QDateTime date(const QString &value, const QDateTime &fallback)
{
    const auto parsed = QDateTime::fromString(value, Qt::ISODateWithMs);
    return parsed.isValid() ? parsed : fallback;
}
int integer(const FrontMatter::Document &doc, const QString &key, int fallback)
{
    bool ok = false;
    const int n = doc.value(key).toInt(&ok);
    return ok ? n : fallback;
}
} // namespace

Note::Note(const QString &id, const QString &filePath, QObject *parent)
    : QObject(parent), m_id(id), m_filePath(filePath), m_created(nowWithOffset()), m_modified(m_created)
{
}
QStringList Note::colorNames()
{
    return {"yellow", "pink", "green", "blue", "purple", "orange", "gray"};
}
QString Note::displayTitle() const
{
    if (!m_displayValid) {
        m_displayCache = m_title.isEmpty() ? MarkdownUtil::deriveTitle(m_body) : m_title;
        if (m_displayCache.isEmpty()) m_displayCache = QStringLiteral("Untitled");
        m_displayValid = true;
    }
    return m_displayCache;
}
QString Note::renderedBody() const
{
    if (!m_renderedValid) {
        m_renderedCache = MarkdownUtil::renderable(m_body);
        m_renderedValid = true;
    }
    return m_renderedCache;
}
void Note::markDirty(bool contentChange)
{
    if (contentChange) {
        m_modified = nowWithOffset();
        Q_EMIT modifiedChanged();
    }
    m_dirty = true;
    Q_EMIT dirtied();
}
void Note::setTitle(const QString &value)
{
    if (m_title == value) return;
    const auto oldDisplay = displayTitle();
    m_title = value;
    m_displayValid = false;
    Q_EMIT titleChanged();
    if (oldDisplay != displayTitle()) Q_EMIT displayTitleChanged();
    markDirty(true);
}
void Note::setBody(const QString &value)
{
    if (m_body == value) return;
    const auto oldDisplay = displayTitle();
    m_body = value;
    m_displayValid = false;
    m_renderedValid = false;
    Q_EMIT bodyChanged();
    if (oldDisplay != displayTitle()) Q_EMIT displayTitleChanged();
    markDirty(true);
}
void Note::setColor(const QString &value)
{
    if (m_color == value || !colorNames().contains(value)) return;
    m_color = value;
    Q_EMIT colorChanged();
    markDirty(true);
}
// Archive, ordering and window state are metadata, not content: they do not bump modified.
#define METADATA_SETTER(Name, Type, Member, Signal) \
    void Note::Name(Type value) { \
        if (Member == value) return; \
        Member = value; \
        Q_EMIT Signal(); \
        markDirty(false); \
    }
METADATA_SETTER(setArchived, bool, m_archived, archivedChanged)
METADATA_SETTER(setOrder, int, m_order, orderChanged)
METADATA_SETTER(setPoppedOut, bool, m_poppedOut, poppedOutChanged)
METADATA_SETTER(setPinned, bool, m_pinned, pinnedChanged)
METADATA_SETTER(setScreen, const QString &, m_screen, geometryChanged)
METADATA_SETTER(setX, int, m_x, geometryChanged)
METADATA_SETTER(setY, int, m_y, geometryChanged)
METADATA_SETTER(setWidth, int, m_width, geometryChanged)
METADATA_SETTER(setHeight, int, m_height, geometryChanged)
#undef METADATA_SETTER

void Note::setGeometry(const QString &screen, int x, int y, int width, int height)
{
    if (m_screen == screen && m_x == x && m_y == y && m_width == width && m_height == height) return;
    m_screen = screen;
    m_x = x;
    m_y = y;
    m_width = width;
    m_height = height;
    Q_EMIT geometryChanged();
    markDirty(false);
}
void Note::toggleTask(int index)
{
    setBody(MarkdownUtil::toggleTask(m_body, index));
}

void Note::loadFromText(const QString &text, const QDateTime &fileModified)
{
    auto doc = FrontMatter::parse(text);
    const auto fallback = fileModified.isValid() ? fileModified : nowWithOffset();
    const auto oldDisplay = displayTitle();
    const auto oldScreen = m_screen;
    const int oldX = m_x, oldY = m_y, oldWidth = m_width, oldHeight = m_height;
    // Assign directly: a reload must never dirty the note or alter the incoming timestamp.
#define LOAD(Member, Value, Signal) \
    do { const auto next = (Value); if (Member != next) { Member = next; Q_EMIT Signal(); } } while (false)
    m_displayValid = false;
    LOAD(m_title, doc.value("title"), titleChanged);
    if (m_body != doc.body) {
        m_body = doc.body;
        m_displayValid = false;
        m_renderedValid = false;
        Q_EMIT bodyChanged();
    }
    auto color = doc.value("color", "yellow");
    if (!colorNames().contains(color)) color = "yellow";
    LOAD(m_color, color, colorChanged);
    m_created = date(doc.value("created"), fallback);
    LOAD(m_modified, date(doc.value("modified"), fallback), modifiedChanged);
    LOAD(m_archived, boolean(doc.value("archived")), archivedChanged);
    LOAD(m_order, integer(doc, "order", 0), orderChanged);
    LOAD(m_poppedOut, boolean(doc.value("popped_out")), poppedOutChanged);
    LOAD(m_pinned, boolean(doc.value("pinned")), pinnedChanged);
#undef LOAD
    m_screen = doc.value("screen");
    m_x = integer(doc, "x", NoPosition);
    m_y = integer(doc, "y", NoPosition);
    m_width = integer(doc, "width", DefaultWidth);
    m_height = integer(doc, "height", DefaultHeight);
    if (m_width <= 0) m_width = DefaultWidth;
    if (m_height <= 0) m_height = DefaultHeight;
    if (oldScreen != m_screen || oldX != m_x || oldY != m_y || oldWidth != m_width || oldHeight != m_height)
        Q_EMIT geometryChanged();
    if (oldDisplay != displayTitle()) Q_EMIT displayTitleChanged();
    m_doc = std::move(doc);
    m_dirty = false;
}

QString Note::toText() const
{
    auto doc = m_doc;
    doc.body = m_body;
    // The generic scalar encoder protects string types. Known bool/int fields are deliberately
    // emitted as YAML bools/ints instead, without disturbing unchanged original representations.
    auto setTyped = [&](const QString &key, const QString &value) {
        doc.setValue(key, value);
        for (auto &line : doc.lines)
            if (line.key == key && line.raw.isEmpty()) line.raw = key + ": " + value;
    };
    auto setBool = [&](const QString &key, bool value) {
        if (!doc.contains(key) || boolean(doc.value(key)) != value)
            setTyped(key, value ? QStringLiteral("true") : QStringLiteral("false"));
    };
    auto setInt = [&](const QString &key, int value) {
        bool ok = false;
        const auto previous = doc.value(key).toInt(&ok);
        if (!ok || previous != value) setTyped(key, QString::number(value));
    };
    auto setDate = [&](const QString &key, const QDateTime &value) {
        if (!doc.contains(key) || QDateTime::fromString(doc.value(key), Qt::ISODateWithMs) != value)
            doc.setValue(key, value.toOffsetFromUtc(value.offsetFromUtc()).toString(Qt::ISODate));
    };
    // Stable order for new documents; existing keys and unknown lines retain their position.
    doc.setValue("id", m_id);
    doc.setValue("title", m_title);
    doc.setValue("color", m_color);
    setDate("created", m_created);
    setDate("modified", m_modified);
    setBool("archived", m_archived);
    setInt("order", m_order);
    setBool("popped_out", m_poppedOut);
    setBool("pinned", m_pinned);
    doc.setValue("screen", m_screen);
    setInt("x", m_x);
    setInt("y", m_y);
    setInt("width", m_width);
    setInt("height", m_height);
    return FrontMatter::serialize(doc);
}
