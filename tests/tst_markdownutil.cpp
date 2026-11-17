// SPDX-License-Identifier: MIT
#include "markdownutil.h"
#include <QtTest>

class MarkdownTest : public QObject {
    Q_OBJECT
private slots:
    void titles() {
        for (const QString &prefix : {QString("# "), QString("> "), QString("- "), QString("* "), QString("+ "), QString("12. "), QString("- [ ] "), QString("- [X] ")})
            QCOMPARE(MarkdownUtil::deriveTitle("\n  " + prefix + "**hello** _world_ `code`\nsecond"), "hello world code");
        QCOMPARE(MarkdownUtil::deriveTitle("\n \n"), "");
        QCOMPARE(MarkdownUtil::deriveTitle("abcdef", 4), "abc…");
        QCOMPARE(MarkdownUtil::deriveTitle("abcdef", 1), "…");
        QCOMPARE(MarkdownUtil::deriveTitle("abc", 0), "");
        QCOMPARE(MarkdownUtil::deriveTitle("**abc**    __", 3), "abc");
        QCOMPARE(MarkdownUtil::deriveTitle("abc   d", 4), "abc…");
        QCOMPARE(MarkdownUtil::deriveTitle("*_*     **abc**", 3), "abc");
        QCOMPARE(MarkdownUtil::deriveTitle("```\nignored\n```\n# Actual"), "Actual");
    }
    void tasks() {
        const QString body = "- [ ] milk\n  * [X] eggs\n+ [x] bread\n1. [ ] tea\n```cpp\n- [ ] ignored\n````\n~~~\n- [x] ignored\n~~~\nnot [ ] task\n- [ ]\n";
        QCOMPARE(MarkdownUtil::taskCount(body), 4);
        QCOMPARE(MarkdownUtil::toggleTask(body, -1), body);
        QCOMPARE(MarkdownUtil::toggleTask(body, 4), body);
        auto toggled = MarkdownUtil::toggleTask(body, 1);
        QVERIFY(toggled.contains("  * [ ] eggs"));
        QCOMPARE(MarkdownUtil::setTaskChecked(body, 1, true), body);
        QCOMPARE(MarkdownUtil::setTaskChecked(body, 0, false), body);
        const QString rendered = MarkdownUtil::renderable(body);
        QVERIFY(rendered.startsWith("- [☐](task:0) milk\n  * [☑](task:1) eggs\n+ [☑](task:2) bread\n1. [☐](task:3) tea"));
        QVERIFY(rendered.contains("```cpp\n- [ ] ignored\n````"));
    }
    void fences() {
        QCOMPARE(MarkdownUtil::taskCount("````\n```\n- [ ] hidden\n````\n- [ ] shown\n"), 1);
        QCOMPARE(MarkdownUtil::taskCount("~~~\n```\n- [ ] hidden\n~~~\n- [ ] shown\n"), 1);
        QCOMPARE(MarkdownUtil::taskCount("- [ ] a\r\n2. [X] b\r\n"), 2);
        QCOMPARE(MarkdownUtil::renderable("- [ ] a\r\n"), "- [☐](task:0) a\r\n");
    }
};
QTEST_GUILESS_MAIN(MarkdownTest)
#include "tst_markdownutil.moc"
