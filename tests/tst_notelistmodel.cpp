// SPDX-License-Identifier: MIT
#include "note.h"
#include "notelistmodel.h"
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QRandomGenerator>
#include <algorithm>
#include <QtTest>

class ModelTest : public QObject {
    Q_OBJECT
private slots:
    void incremental() {
        Note a("a", ""), b("b", ""), c("c", ""), d("d", "");
        NoteListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted), removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy moved(&model, &QAbstractItemModel::rowsMoved), reset(&model, &QAbstractItemModel::modelReset);
        QSignalSpy count(&model, &NoteListModel::countChanged);
        model.setNotes({&a, &b, &c});
        QCOMPARE(model.count(), 3);
        QCOMPARE(count.count(), 1);
        QPersistentModelIndex retained(model.index(1, 0));
        model.setNotes({&b, &c, &a});
        QCOMPARE(moved.count(), 1);
        QCOMPARE(retained.row(), 0);
        QCOMPARE(model.data(retained, NoteListModel::IdRole).toString(), "b");
        QCOMPARE(count.count(), 1);
        model.setNotes({&d, &b, &a});
        QCOMPARE(model.notes(), QList<Note *>({&d, &b, &a}));
        QCOMPARE(model.get(0), &d);
        QCOMPARE(model.indexOf(&a), 2);
        QVERIFY(!model.get(-1));
        QVERIFY(!model.get(3));
        QCOMPARE(model.indexOf(&c), -1);
        QVERIFY(!model.data({}, NoteListModel::IdRole).isValid());
        QCOMPARE(model.roleNames().value(NoteListModel::NoteRole), "note");
        QCOMPARE(model.data(model.index(0, 0), NoteListModel::NoteRole).value<Note *>(), &d);
        QVERIFY(removed.count() > 0);
        QVERIFY(inserted.count() > 0);
        QCOMPARE(reset.count(), 0);
        const int signalCount = inserted.count() + removed.count() + moved.count();
        model.setNotes(model.notes());
        QCOMPARE(inserted.count() + removed.count() + moved.count(), signalCount);
        model.setNotes({});
        QCOMPARE(model.count(), 0);
        QCOMPARE(reset.count(), 0);
    }
    void membershipAndMoves() {
        Note a("a", ""), b("b", ""), c("c", ""), d("d", "");
        const QList<Note *> all{&a, &b, &c, &d};
        NoteListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        QRandomGenerator random(1234);
        for (int run = 0; run < 300; ++run) {
            auto target = all;
            for (int i = target.size() - 1; i > 0; --i)
                target.swapItemsAt(i, random.bounded(i + 1));
            target.resize(random.bounded(5));
            QHash<Note *, QPersistentModelIndex> kept;
            for (auto *note : target)
                if (model.indexOf(note) >= 0) kept.insert(note, model.index(model.indexOf(note), 0));
            model.setNotes(target);
            QCOMPARE(model.notes(), target);
            for (auto it = kept.cbegin(); it != kept.cend(); ++it) {
                QVERIFY(it.value().isValid());
                QCOMPARE(it.value().row(), target.indexOf(it.key()));
            }
        }
        QCOMPARE(reset.count(), 0);
    }
    void permutations() {
        Note a("a", ""), b("b", ""), c("c", ""), d("d", "");
        const QList<Note *> all{&a, &b, &c, &d};
        QList<int> order{0, 1, 2, 3};
        do {
            NoteListModel model;
            QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
            model.setNotes(all);
            QList<Note *> target;
            for (int i : order) target << all.at(i);
            model.setNotes(target);
            QCOMPARE(model.notes(), target);
        } while (std::next_permutation(order.begin(), order.end()));
    }
};
QTEST_GUILESS_MAIN(ModelTest)
#include "tst_notelistmodel.moc"
