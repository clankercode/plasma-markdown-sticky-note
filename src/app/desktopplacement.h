// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlParserStatus>
#include <QRect>
#include <QtQml/qqmlregistration.h>

class Note;
class QQuickWindow;
class QScreen;
class QWindow;

// Places a popped-out note window on the desktop and lets the user drag it around, across screens.
//
// Usage (in DesktopNoteWindow.qml):
//     DesktopPlacement { id: placement; window: root; note: root.note }
//     // header MouseArea:
//     onPressed: (m) => placement.beginDrag(Qt.point(m.x, m.y))   // window-local coordinates
//     onPositionChanged: (m) => placement.dragTo(Qt.point(m.x, m.y))
//     onReleased: placement.endDrag()
//
// Must be configured BEFORE the window is first shown (QML: set properties, keep window `visible: false`
// until `placement.ready` is true, then show). On componentComplete it:
//   * On Wayland with LayerShellQt available (layerShell == true): turns the window into a layer surface
//     (scope "sticky-note"), anchored Top|Left, exclusive zone 0, keyboard interactivity OnDemand,
//     layer Bottom (or Top when note.pinned), screen = note.screen (fallback primary), margins = note x/y.
//   * Otherwise: frameless tool window; flags include Qt::WindowStaysOnBottomHint (unpinned) or
//     Qt::WindowStaysOnTopHint (pinned); position = screen.geometry().topLeft() + (x, y).
//   * Unplaced notes (x/y == Note::NoPosition) get a cascaded default position on the primary screen.
//   * Positions are clamped so at least 48 px of the note remains visible on its screen.
//
// Dragging:
//   * layer-shell: the surface moves under the pointer, so window-local press point is constant;
//     dragTo(p) shifts margins by (p - pressPoint). When the note's centre leaves the current screen and
//     lies inside another screen's geometry (QScreen::geometry() is in the global logical layout), the
//     surface is moved to that screen (LayerShellQt::Window::setScreen + hide/show if required) and the
//     margins are converted so the note stays under the pointer.
//   * other platforms: beginDrag calls QWindow::startSystemMove(); position is read back from the window
//     on endDrag / window x/y changes.
//   * endDrag() (and resize end) writes screen name + x/y (relative to that screen's top-left) + size to
//     the note via Note::setGeometry, which persists it to front matter (debounced).
// Size: window width/height are initialised from note width/height; commitSize() persists current size.
// Pinned changes are applied live (layer switch, or flag switch on X11).
class DesktopPlacement : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_MOC_INCLUDE("note.h")
    Q_MOC_INCLUDE(<QQuickWindow>)
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(Note *note READ note WRITE setNote NOTIFY noteChanged)
    Q_PROPERTY(bool layerShell READ layerShell CONSTANT)   // true when using wlr-layer-shell
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)  // window configured; safe to show
    Q_PROPERTY(bool dragging READ dragging NOTIFY draggingChanged)
    Q_PROPERTY(QStringList screenNames READ screenNames NOTIFY screensChanged)

public:
    explicit DesktopPlacement(QObject *parent = nullptr);
    ~DesktopPlacement() override;

    QQuickWindow *window() const;
    void setWindow(QQuickWindow *w);
    Note *note() const;
    void setNote(Note *n);
    static bool layerShellAvailable(); // compiled with LayerShellQt AND running on wayland
    bool layerShell() const { return layerShellAvailable(); }
    bool ready() const { return m_ready; }
    bool dragging() const { return m_dragging; }
    QStringList screenNames() const;

    Q_INVOKABLE void beginDrag(const QPointF &windowLocalPos);
    Q_INVOKABLE void dragTo(const QPointF &windowLocalPos);
    Q_INVOKABLE void endDrag();
    // Resize helpers for a bottom-right grip (window-local coords of the grip press/move).
    Q_INVOKABLE void beginResize(const QPointF &windowLocalPos);
    Q_INVOKABLE void resizeTo(const QPointF &windowLocalPos);
    Q_INVOKABLE void endResize();
    Q_INVOKABLE void commitSize();
    // Moves the note to the named screen keeping its relative position (clamped).
    Q_INVOKABLE void moveToScreen(const QString &screenName);
    // Raise / focus the window (best effort on Wayland: requests keyboard focus on the layer surface).
    Q_INVOKABLE void raise();

    void classBegin() override;
    void componentComplete() override;

Q_SIGNALS:
    void windowChanged();
    void noteChanged();
    void readyChanged();
    void draggingChanged();
    void screensChanged();

private:
    struct Private;
    Private *d;
    bool m_ready = false;
    bool m_dragging = false;
};
