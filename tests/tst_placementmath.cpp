// SPDX-License-Identifier: MIT
#include "placementmath.h"

#include <QtTest>
#include <deque>

using namespace PlacementMath;

namespace {
// A real 4-monitor layout with vertical offsets and gaps (from a user report):
//   DP-1 0,1234 2560x1440 | DP-2 2560,0 2560x1440 (above DP-3) | DP-3 2560,1440 3440x1440 | HDMI-A-1 6000,1440
const QList<QRect> kUserLayout{
    QRect(0, 1234, 2560, 1440),   // 0: DP-1
    QRect(2560, 0, 2560, 1440),   // 1: DP-2
    QRect(2560, 1440, 3440, 1440), // 2: DP-3
    QRect(6000, 1440, 2560, 1440), // 3: HDMI-A-1
};
const QSize kNote(300, 280);

// Simulates a layer-shell drag the way the compositor really behaves: the margins we request are
// applied only `lag` pointer events later (set_margin is double-buffered until the next frame
// commit), and pointer events are reported relative to the APPLIED surface position.
// `useApplied` = the fixed algorithm; false = the 1.0.x algorithm (measured against the requested
// position), which made the note run away from the pointer.
QPoint simulateDrag(QPoint start, QPointF press, const QList<QPoint> &pointerGlobalPath, int lag, bool useApplied)
{
    QPoint requested = start;
    QPoint applied = start;
    std::deque<QPoint> inFlight; // requested positions not yet applied by the compositor
    for (const QPoint &pointer : pointerGlobalPath) {
        // The compositor reports the pointer relative to where the surface currently is.
        const QPointF local = QPointF(pointer - applied);
        const QPoint base = useApplied ? applied : requested;
        requested = dragTarget(base, local, press);
        inFlight.push_back(requested);
        if (int(inFlight.size()) > lag) { // a frame commit lands: oldest request becomes applied
            applied = inFlight.front();
            inFlight.pop_front();
        }
    }
    // Release: everything still in flight gets committed.
    return requested;
}
} // namespace

class PlacementMathTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void outputAtFindsContainingOutput()
    {
        QCOMPARE(outputAt(kUserLayout, QPoint(100, 2000)), 0);   // DP-1
        QCOMPARE(outputAt(kUserLayout, QPoint(3000, 100)), 1);   // DP-2
        QCOMPARE(outputAt(kUserLayout, QPoint(3000, 2000)), 2);  // DP-3
        QCOMPARE(outputAt(kUserLayout, QPoint(7000, 2000)), 3);  // HDMI-A-1
    }

    void outputAtSnapsGapsToNearestOutput()
    {
        // Above DP-1 (y < 1234), left of DP-2: nothing there. Nearest is DP-1 (just below).
        QCOMPARE(outputAt(kUserLayout, QPoint(1000, 1200)), 0);
        // Right of DP-2, above HDMI-A-1: nearest is DP-2 (x just past its right edge).
        QCOMPARE(outputAt(kUserLayout, QPoint(5200, 300)), 1);
        // Far below everything, under HDMI-A-1.
        QCOMPARE(outputAt(kUserLayout, QPoint(7000, 5000)), 3);
        QCOMPARE(outputAt({}, QPoint(0, 0)), -1);
    }

    void clampKeepsGrabbableStrip()
    {
        const QSize area(2560, 1440);
        QCOMPARE(clamped(QPoint(100, 100), area, kNote), QPoint(100, 100));
        QCOMPARE(clamped(QPoint(5000, 5000), area, kNote), QPoint(2560 - MinVisible, 1440 - MinVisible));
        QCOMPARE(clamped(QPoint(-5000, -5000), area, kNote), QPoint(MinVisible - 300, MinVisible - 280));
    }

    // Regression for the 1.0.x teleport bug: with the compositor applying margins a couple of events
    // late, the old algorithm ran away from the pointer; the fixed one ends exactly under it.
    void dragFollowsPointerDespiteCommitLag_data()
    {
        QTest::addColumn<int>("lag");
        QTest::newRow("instant") << 0;
        QTest::newRow("1 event late") << 1;
        QTest::newRow("2 events late") << 2;
        QTest::newRow("5 events late") << 5;
    }
    void dragFollowsPointerDespiteCommitLag()
    {
        QFETCH(int, lag);
        const QPoint start(100, 100);
        const QPointF press(130, 24);
        QList<QPoint> path;
        for (int i = 1; i <= 50; ++i)
            path.append(start + press.toPoint() + QPoint(i * 10, i * 2)); // pointer moves +500, +100
        const QPoint expected = start + QPoint(500, 100);

        QCOMPARE(simulateDrag(start, press, path, lag, /*useApplied*/ true), expected);
        // Sanity-check the simulation itself: with >= 2 events of commit lag, the 1.0.x formula
        // (measured against the requested position) diverges wildly — 21,000+ px at lag 2.
        if (lag >= 2) {
            const QPoint old = simulateDrag(start, press, path, lag, /*useApplied*/ false);
            QVERIFY2((old - expected).manhattanLength() > 10000,
                     "the old algorithm is expected to run away; if this fails the simulation is wrong");
        }
    }

    // Dropping a note across a monitor boundary moves it to that monitor at the same global spot.
    void settleHandsOffAcrossOutputs()
    {
        // Dragged from DP-3 up past its top edge until its centre is on DP-2 above it.
        // DP-3 top-left (2560,1440). position (400,-500) => global (2960,940); centre (3110,1080) on DP-2.
        const Placement p = settle(kUserLayout, 2, QPoint(400, -500), kNote);
        QCOMPARE(p.output, 1);
        QCOMPARE(p.position, QPoint(400, 940)); // DP-2 top-left is (2560,0)

        // DP-3 -> HDMI-A-1 (right): global x 6100 => HDMI-local 100.
        const Placement r = settle(kUserLayout, 2, QPoint(3540, 200), kNote);
        QCOMPARE(r.output, 3);
        QCOMPARE(r.position, QPoint(100, 200));

        // DP-3 -> DP-1 (left; DP-1 is offset 206px higher). global (2000,1600) => DP-1 local (2000,366).
        const Placement l = settle(kUserLayout, 2, QPoint(-560, 160), kNote);
        QCOMPARE(l.output, 0);
        QCOMPARE(l.position, QPoint(2000, 366));
    }

    void settleStaysWhenCentreIsOnSameOutput()
    {
        const Placement p = settle(kUserLayout, 0, QPoint(500, 500), kNote);
        QCOMPARE(p.output, 0);
        QCOMPARE(p.position, QPoint(500, 500));
    }

    // Dropped in a dead zone of the layout (no monitor there): it must land fully reachable on the
    // nearest monitor, never stranded off-screen.
    void settleSnapsDeadZoneToNearestOutput()
    {
        // From DP-1, dragged up above its top (y<1234) where nothing exists.
        const Placement p = settle(kUserLayout, 0, QPoint(1000, -900), kNote);
        QCOMPARE(p.output, 0);
        QCOMPARE(p.position, QPoint(1000, MinVisible - kNote.height()));
        QVERIFY(QRect(QPoint(0, 0), QSize(2560, 1440)).intersects(QRect(p.position, kNote)));
    }

    void settleRejectsBadInput()
    {
        QCOMPARE(settle(kUserLayout, -1, QPoint(0, 0), kNote).output, -1);
        QCOMPARE(settle(kUserLayout, 9, QPoint(0, 0), kNote).output, -1);
    }
};

QTEST_GUILESS_MAIN(PlacementMathTest)
#include "tst_placementmath.moc"
