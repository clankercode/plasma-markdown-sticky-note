// SPDX-License-Identifier: MIT
#include "markdownstyler.h"

#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QQuickTextDocument>
#include <QRectF>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>

namespace {
constexpr QChar kUnchecked(0x2610);
constexpr QChar kChecked(0x2611);


// Paper-scale typography, relative to the body font.
constexpr qreal kHeadingScale[] = {1.0, 1.32, 1.16, 1.04, 1.0, 1.0, 1.0};
constexpr qreal kCodeScale = 0.88;
constexpr qreal kIndentWidth = 22;     // per list level (Qt default: 40)
constexpr qreal kBulletInset = 8;     // where Qt draws a bullet inside the first indent level
constexpr qreal kQuoteIndent = 14;     // text inset right of the quote rule
constexpr qreal kCodeInset = 10;       // code text inset inside its background
constexpr qreal kCodePadY = 7;         // background extends this far above / below the code text

bool isTaskBlock(const QTextBlock &block)
{
    const QString text = block.text();
    const auto first = block.begin();
    return !text.isEmpty() && (text.at(0) == kUnchecked || text.at(0) == kChecked)
        && !first.atEnd() && first.fragment().charFormat().anchorHref().startsWith(QLatin1String("task:"));
}

bool isCodeBlock(const QTextBlockFormat &format)
{
    return format.hasProperty(QTextFormat::BlockCodeFence) || format.nonBreakableLines();
}
} // namespace

MarkdownStyler::MarkdownStyler(QObject *parent)
    : QObject(parent)
{
}

void MarkdownStyler::setDocument(QQuickTextDocument *document)
{
    if (m_document == document)
        return;
    m_document = document;
    Q_EMIT documentChanged();
    apply();
}

void MarkdownStyler::setMuted(const QColor &color)
{
    if (m_muted == color)
        return;
    m_muted = color;
    Q_EMIT mutedChanged();
    apply();
}

QTextDocument *MarkdownStyler::textDocument() const
{
    return m_document ? m_document->textDocument() : nullptr;
}

int MarkdownStyler::taskIndexAt(int position) const
{
    const auto *doc = textDocument();
    if (!doc || position < 0 || position >= doc->characterCount() - 1) return -1;
    const auto block = doc->findBlock(position);
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        const auto fragment = it.fragment();
        if (position < fragment.position() || position >= fragment.position() + fragment.length()) continue;
        const auto href = fragment.charFormat().anchorHref();
        if (!href.startsWith(QLatin1String("task:"))) return -1;
        bool ok = false;
        const int index = href.mid(5).toInt(&ok);
        return ok && index >= 0 ? index : -1;
    }
    return -1;
}

void MarkdownStyler::apply()
{
    QTextDocument *doc = textDocument();
    if (!doc || m_applying)
        return;
    m_applying = true;

    // Layout signals drive the decoration rects (they move whenever the text reflows).
    if (QObject *layout = doc->documentLayout(); layout != m_layout) {
        if (m_layout)
            disconnect(m_layout, nullptr, this, nullptr);
        m_layout = layout;
        connect(doc->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
                &MarkdownStyler::updateDecorations);
    }

    const QFont base = doc->defaultFont();
    const qreal basePt = base.pointSizeF() > 0 ? base.pointSizeF() : 10.0;
    const QFontMetricsF metrics(base);
    // Wrapped task text hangs under its first word, not under the box.
    const qreal boxHang = metrics.horizontalAdvance(QStringLiteral("☐⁠ "));
    QColor codeTint = m_muted.isValid() ? m_muted : QColor(Qt::black);
    codeTint.setAlphaF(0.13);

    m_codeRuns.clear();
    m_quoteRuns.clear();
    doc->setIndentWidth(kIndentWidth);

    QTextCursor cursor(doc);
    cursor.beginEditBlock();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        const int level = format.headingLevel();
        const bool code = isCodeBlock(format);
        const bool doneTask = isTaskBlock(block) && block.text().at(0) == kChecked;
        const bool quote = format.hasProperty(QTextFormat::BlockQuoteLevel) && format.intProperty(QTextFormat::BlockQuoteLevel) > 0;

        if (QTextList *list = block.textList()) {
            if (isTaskBlock(block)) {
                // The box is the marker: drop the bullet, keep the list's indent.
                // The box sits where the bullet would have been.
                const int indent = list->format().indent();
                list->remove(block);
                format = block.blockFormat();   // no longer carries the list's object index
                format.setIndent(indent - 1);
                format.setLeftMargin(kBulletInset + boxHang);
                format.setTextIndent(-boxHang);
            }
            format.setTopMargin(2);
            format.setBottomMargin(2);
        } else if (isTaskBlock(block)) {
            // Re-applied (already removed from its list on a previous pass).
            format.setLeftMargin(kBulletInset + boxHang);
            format.setTextIndent(-boxHang);
            format.setTopMargin(2);
            format.setBottomMargin(2);
        } else if (level > 0) {
            format.setTopMargin(level <= 2 ? 10 : 8);
            format.setBottomMargin(3);
        } else if (code) {
            format.setLeftMargin(kCodeInset);
            format.setRightMargin(kCodeInset);
            format.setTopMargin(0);
            format.setBottomMargin(0);
            const bool first = !block.previous().isValid() || !isCodeBlock(block.previous().blockFormat());
            const bool last = !block.next().isValid() || !isCodeBlock(block.next().blockFormat());
            if (first) {
                format.setTopMargin(kCodePadY + 6);
                m_codeRuns.append({block.blockNumber(), block.blockNumber()});
            }
            if (last)
                format.setBottomMargin(kCodePadY + 6);
            if (!m_codeRuns.isEmpty())
                m_codeRuns.last().last = block.blockNumber();
        } else if (quote) {
            format.setLeftMargin(kQuoteIndent * format.intProperty(QTextFormat::BlockQuoteLevel));
            format.setRightMargin(0);
            format.setTopMargin(6);
            format.setBottomMargin(6);
            if (!m_quoteRuns.isEmpty() && m_quoteRuns.last().last == block.blockNumber() - 1)
                m_quoteRuns.last().last = block.blockNumber();
            else
                m_quoteRuns.append({block.blockNumber(), block.blockNumber()});
        }

        // Nothing above the first block but the header strip: no leading gap.
        if (!block.previous().isValid())
            format.setTopMargin(code ? kCodePadY : 0);

        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);

        // Character formats, fragment by fragment.
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            QTextCharFormat cf = fragment.charFormat();
            bool changed = false;
            if (level > 0) {
                cf.clearProperty(QTextFormat::FontSizeAdjustment);
                cf.setFontPointSize(basePt * kHeadingScale[qMin(level, 6)]);
                cf.setFontWeight(level <= 3 ? QFont::DemiBold : QFont::Bold);
                changed = true;
            }
            if (cf.fontFixedPitch() || cf.hasProperty(QTextFormat::FontFamilies)) {
                cf.setFontPointSize(basePt * kCodeScale);
                if (!code)
                    cf.setBackground(codeTint);   // inline code
                changed = true;
            }
            if (cf.isAnchor() && cf.anchorHref().startsWith(QLatin1String("task:"))) {
                cf.clearProperty(QTextFormat::ForegroundBrush);
                cf.setFontUnderline(false);
                if (doneTask && m_muted.isValid()) cf.setForeground(m_muted);
                changed = true;
            }
            // Links: the pen-blue colour alone is too close to the ink on blue paper.
            if (cf.isAnchor() && !cf.anchorHref().startsWith(QLatin1String("task:")) && !cf.fontUnderline()) {
                cf.setFontUnderline(true);
                changed = true;
            }
            // Quotes and finished tasks recede into the muted ink (links keep their own colour).
            if ((quote || doneTask) && m_muted.isValid() && !cf.isAnchor()) {
                cf.setForeground(m_muted);
                changed = true;
            }
            if (changed) {
                cursor.setPosition(fragment.position());
                cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
                cursor.setCharFormat(cf);
            }
        }
    }
    cursor.endEditBlock();

    m_applying = false;
    updateDecorations();
}

void MarkdownStyler::updateDecorations()
{
    QTextDocument *doc = textDocument();
    QVariantList code;
    QVariantList quotes;
    if (doc && doc->documentLayout()) {
        QAbstractTextDocumentLayout *layout = doc->documentLayout();
        const qreal width = doc->textWidth() > 0 ? doc->textWidth() : doc->size().width();
        const qreal margin = doc->documentMargin();
        for (const Run &run : std::as_const(m_codeRuns)) {
            const QTextBlock first = doc->findBlockByNumber(run.first);
            const QTextBlock last = doc->findBlockByNumber(run.last);
            if (!first.isValid() || !last.isValid())
                continue;
            const QRectF a = layout->blockBoundingRect(first);
            const QRectF b = layout->blockBoundingRect(last);
            // blockBoundingRect() is the text area, without the block's top / bottom margins.
            const qreal top = a.top() - kCodePadY;
            const qreal bottom = b.bottom() + kCodePadY;
            code.append(QRectF(margin, top, width - 2 * margin, bottom - top));
        }
        for (const Run &run : std::as_const(m_quoteRuns)) {
            const QTextBlock first = doc->findBlockByNumber(run.first);
            const QTextBlock last = doc->findBlockByNumber(run.last);
            if (!first.isValid() || !last.isValid())
                continue;
            const QRectF a = layout->blockBoundingRect(first);
            const QRectF b = layout->blockBoundingRect(last);
            const qreal top = a.top();
            const qreal bottom = b.bottom();
            quotes.append(QRectF(margin, top, width - 2 * margin, bottom - top));
        }
    }
    if (code != m_codeRects || quotes != m_quoteRects) {
        m_codeRects = code;
        m_quoteRects = quotes;
        Q_EMIT decorationsChanged();
    }
}
