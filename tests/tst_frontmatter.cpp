// SPDX-License-Identifier: MIT
#include "frontmatter.h"
#include <QtTest>

class FrontMatterTest : public QObject {
    Q_OBJECT
private slots:
    void roundTrip() {
        const QString text = "---\n# comment\nid: abc\ntitle: 'A # title' # keep\nunknown: [one, two]\n\nnot yaml\n---\n\nbody\n";
        auto doc = FrontMatter::parse(text);
        QVERIFY(doc.hadFrontMatter);
        QCOMPARE(doc.value("title"), "A # title");
        QCOMPARE(doc.body, "\nbody\n");
        QCOMPARE(FrontMatter::serialize(doc), text);
        doc.setValue("title", "A # title");
        QCOMPARE(FrontMatter::serialize(doc), text);
        doc.setValue("title", "different: title");
        const auto updated = FrontMatter::serialize(doc);
        QVERIFY(updated.contains("title: \"different: title\"\n"));
        QVERIFY(updated.contains("unknown: [one, two]\n\nnot yaml\n"));
        doc.remove("id");
        QVERIFY(!doc.contains("id"));
    }
    void delimiters() {
        for (const QString &text : QStringList{QString("---\na: b\n---"), QString("---  \na: b\n---  \nbody"), QString(QChar(0xfeff)) + "---\na: b\n---\nbody"})
            QCOMPARE(FrontMatter::serialize(FrontMatter::parse(text)), text);
        auto doc = FrontMatter::parse("---\r\na: 'b'\r\n--- \r\n\r\nbody\r\n");
        QCOMPARE(doc.body, "\nbody\n");
        QCOMPARE(FrontMatter::serialize(doc), "---\na: 'b'\n--- \n\nbody\n");
    }
    void malformed() {
        for (const QString &text : {QString("ordinary\nbody"), QString("---\nmissing end\nbody"), QString("---"), QString()}) {
            auto doc = FrontMatter::parse(text);
            QCOMPARE(doc.body, text);
            QCOMPARE(FrontMatter::serialize(doc), text);
            QCOMPARE(doc.malformed, text.startsWith("---"));
            QVERIFY(!doc.hadFrontMatter);
            doc.setValue("title", "Recovered");
            QCOMPARE(FrontMatter::parse(FrontMatter::serialize(doc)).body, text);
        }
    }
    void scalars_data() {
        QTest::addColumn<QString>("value");
        const QStringList values = {"", "plain", " leading", "trailing ", "#tag", "&x", "*x", "!x", "|x", ">x", "'x", "\"x", "%x", "@x", "`x", "-x", "?x", ":x", "[x]", "]x", "{x}", "}x", "a: b", "a # b", "true", "FALSE", "yes", "no", "null", "~", "123", "-1", "1.5", "1e3", ".inf", "line\nbreak", "tab\there", "slash\\quote\""};
        for (int i = 0; i < values.size(); ++i)
            QTest::newRow(qPrintable(QString::number(i))) << values.at(i);
    }
    void scalars() {
        QFETCH(QString, value);
        const auto encoded = FrontMatter::encodeScalar(value);
        QCOMPARE(FrontMatter::decodeScalar(encoded), value);
        if (value != "plain" && value != "slash\\quote\"")
            QVERIFY2(encoded.startsWith('"'), qPrintable(encoded));
    }
    void unicodeCommentBoundaries() {
        for (const auto space : {QChar(0x00a0), QChar(0x2003), QChar(0x202f)}) {
            const QString value = QStringLiteral("words") + space + "#tag";
            QCOMPARE(FrontMatter::decodeScalar(FrontMatter::encodeScalar(value)), value);
        }
    }
    void decode() {
        QCOMPARE(FrontMatter::decodeScalar(" 'it''s # fine' # comment"), "it's # fine");
        QCOMPARE(FrontMatter::decodeScalar("\"a\\n\\t\\r\\\"\\\\b\" # comment"), "a\n\t\r\"\\b");
        QCOMPARE(FrontMatter::decodeScalar(" a#b # comment"), "a#b");
        QCOMPARE(FrontMatter::decodeScalar(" # comment"), "");
        auto doc = FrontMatter::parse("---\nkey: one\nkey: two\n---\n");
        doc.setValue("key", "three");
        QCOMPARE(FrontMatter::parse(FrontMatter::serialize(doc)).value("key"), "three");
    }
};
QTEST_GUILESS_MAIN(FrontMatterTest)
#include "tst_frontmatter.moc"
