// SPDX-License-Identifier: MIT
#include "desktopplacement.h"
#include "note.h"
#include "placementmath.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <algorithm>

#ifdef HAVE_LAYERSHELL
#include <LayerShellQt/window.h>
#endif
#ifdef HAVE_WAYLAND_REGISTRY
#include <QtGui/qguiapplication_platform.h>
#include <wayland-client.h>
#include <cstring>
#endif

// Enable with QT_LOGGING_RULES="sticky.placement.debug=true".
Q_LOGGING_CATEGORY(lcPlacement, "sticky.placement", QtWarningMsg)

namespace {
QScreen *screenNamed(const QString &name)
{
    for (auto *screen : QGuiApplication::screens()) {
        if (screen->name() == name)
            return screen;
    }
    return QGuiApplication::primaryScreen();
}

QList<QRect> outputGeometries()
{
    QList<QRect> rects;
    const auto screens = QGuiApplication::screens();
    rects.reserve(screens.size());
    for (auto *screen : screens)
        rects.append(screen->geometry());
    return rects;
}

#ifdef HAVE_WAYLAND_REGISTRY
bool compositorHasLayerShell()
{
    auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!native || !native->display())
        return false;
    auto *display = native->display();
    auto *queue = wl_display_create_queue(display);
    auto *registry = wl_display_get_registry(display);
    wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(registry), queue);
    bool supported = false;
    static const wl_registry_listener listener = {
        [](void *data, wl_registry *, uint32_t, const char *interface, uint32_t) {
            if (std::strcmp(interface, "zwlr_layer_shell_v1") == 0)
                *static_cast<bool *>(data) = true;
        },
        [](void *, wl_registry *, uint32_t) {}
    };
    wl_registry_add_listener(registry, &listener, &supported);
    const bool ok = wl_display_roundtrip_queue(display, queue) >= 0;
    wl_registry_destroy(registry);
    wl_event_queue_destroy(queue);
    return ok && supported;
}
#endif
}

struct DesktopPlacement::Private
{
    explicit Private(DesktopPlacement *owner) : q(owner)
    {
        marginTimer.setSingleShot(true);
        marginTimer.setInterval(16);
        settleTimer.setSingleShot(true);
        settleTimer.setInterval(300);
        QObject::connect(&marginTimer, &QTimer::timeout, q, [this] { applyPending(); });
        QObject::connect(&settleTimer, &QTimer::timeout, q, [this] { persist(); });
    }

    DesktopPlacement *q;
    QPointer<QQuickWindow> window;
    QPointer<Note> note;
    QPointer<QScreen> screen;
    QList<QMetaObject::Connection> windowConnections;
    QList<QMetaObject::Connection> noteConnections;
    QTimer marginTimer;
    QTimer settleTimer;
    bool complete = false;
    bool resizing = false;
    bool committing = false;
    bool configuring = false;
    QPoint position;        // requested position (margins we last set), relative to `screen`
    QPoint pendingPosition;
    // Layer-shell drag: pointer events are reported relative to the surface position the compositor
    // has APPLIED, which lags `position` until a frame carrying the new margins is committed.
    QPoint appliedPosition;
    QPoint requestedSinceCommit; // last margins handed to LayerShellQt
    QPoint inFlightFrame;        // margins carried by the frame currently being rendered
    bool commitPending = false;  // margins requested but not yet in a rendered frame
    bool frameInFlight = false;
    QPointF pressPoint;
    QPointF resizePress;
    QSize resizeStart;
#ifdef HAVE_LAYERSHELL
    QPointer<LayerShellQt::Window> layer;
#endif

    QPoint clamped(QPoint p) const
    {
        if (!screen || !window)
            return p;
        return PlacementMath::clamped(p, screen->geometry().size(), window->size());
    }

    void switchScreen(QScreen *target)
    {
        if (!window || !target || screen == target)
            return;
        screen = target;
        position = clamped(position);
#ifdef HAVE_LAYERSHELL
        if (layer) {
            const bool visible = window->isVisible();
            // An output is immutable on a mapped layer surface. A remap creates a new one.
            if (visible)
                window->hide();
            window->setScreen(target);
            layer->setScreen(target);
            applyMargins();
            if (visible)
                window->show();
            return;
        }
#endif
        window->setScreen(target);
    }

    void applyPosition()
    {
        if (!window || !screen)
            return;
        position = clamped(position);
        applyMargins();
    }

    void applyMargins()
    {
        if (!window || !screen)
            return;
#ifdef HAVE_LAYERSHELL
        if (layer) {
            layer->setMargins(QMargins(position.x(), position.y(), 0, 0));
            // set_margin is double-buffered: it only takes effect on the next wl_surface.commit,
            // which happens when Qt renders a frame. Nothing repaints during a drag, so request one.
            requestedSinceCommit = position;
            commitPending = true;
            window->update();
            return;
        }
#endif
        window->setPosition(screen->geometry().topLeft() + position);
    }

    // Layer-shell drag step: move the surface, staying on its current output. Margins may run past
    // the output's edge while dragging; the hand-off to another output happens once, on release
    // (switchToScreenUnderNote), because remapping a layer surface mid-drag destroys it and with it
    // the pointer grab.
    void applyPending()
    {
        if (!window || !screen)
            return;
        qCDebug(lcPlacement) << "applyPending: screen" << screen->name() << "from" << position << "to" << pendingPosition;
        position = pendingPosition;
        applyMargins();
    }

    // After a drag: if the note's centre now lies on another output (or in a gap nearest to one),
    // move it there at the same global position, clamped so it stays reachable.
    void switchToScreenUnderNote()
    {
        if (!window || !screen)
            return;
        const auto screens = QGuiApplication::screens();
        const auto settled = PlacementMath::settle(outputGeometries(), screens.indexOf(screen), position, window->size());
        if (settled.output < 0)
            return applyPosition();
        QScreen *target = screens.at(settled.output);
        position = settled.position;
        if (target != screen) {
            qCDebug(lcPlacement) << "switchScreen ->" << target->name() << "position" << position;
            switchScreen(target);
        }
        applyPosition();
    }

    void pinnedChanged()
    {
        if (!window || !note || !q->m_ready)
            return;
#ifdef HAVE_LAYERSHELL
        if (layer) {
            layer->setLayer(note->pinned() ? LayerShellQt::Window::LayerTop : LayerShellQt::Window::LayerBottom);
            return;
        }
#endif
        const bool visible = window->isVisible();
        window->setFlags(Qt::Tool | Qt::FramelessWindowHint
                         | (note->pinned() ? Qt::WindowStaysOnTopHint : Qt::WindowStaysOnBottomHint));
        if (visible)
            window->show();
    }

    void restoreGeometry()
    {
        if (!window || !note || configuring)
            return;
        configuring = true;
        QScreen *target = screenNamed(note->screen());
        window->resize(std::max(160, note->width()), std::max(120, note->height()));
#ifdef HAVE_LAYERSHELL
        if (layer)
            layer->setDesiredSize(window->size());
#endif
        if (note->x() == Note::NoPosition || note->y() == Note::NoPosition) {
            static unsigned cascade = 0;
            const int offset = 32 * static_cast<int>(cascade++ % 10);
            position = QPoint(60 + offset, 60 + offset);
            target = QGuiApplication::primaryScreen();
        } else {
            position = QPoint(note->x(), note->y());
        }
        switchScreen(target);
        applyPosition();
        configuring = false;
    }

    void configure()
    {
        if (!complete || !window || !note)
            return;
        configuring = true;
        window->setMinimumSize(QSize(160, 120));
        window->setFlags(Qt::Tool | Qt::FramelessWindowHint
                         | (note->pinned() ? Qt::WindowStaysOnTopHint : Qt::WindowStaysOnBottomHint));
#ifdef HAVE_LAYERSHELL
        if (q->layerShell()) {
            // This MUST precede first show/create: Qt selects the shell integration here.
            layer = LayerShellQt::Window::get(window);
            layer->setScope(QStringLiteral("sticky-note"));
            layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)
                              | LayerShellQt::Window::AnchorLeft);
            layer->setExclusiveZone(0);
            layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
            layer->setLayer(note->pinned() ? LayerShellQt::Window::LayerTop : LayerShellQt::Window::LayerBottom);
            layer->setWantsToBeOnActiveScreen(false);
            layer->setCloseOnDismissed(false);
            layer->setActivateOnShow(false);
        }
#endif
        configuring = false;
        restoreGeometry();
        q->m_ready = true;
        Q_EMIT q->readyChanged();
        persist();
    }

    void persist()
    {
        if (!window || !note || !q->m_ready || configuring || committing)
            return;
#ifdef HAVE_LAYERSHELL
        const bool isLayer = bool(layer);
#else
        const bool isLayer = false;
#endif
        if (!isLayer) {
            QScreen *target = window->screen();
            if (target)
                screen = target;
            if (screen)
                position = window->position() - screen->geometry().topLeft();
        }
        if (!screen)
            return;
        committing = true;
        note->setGeometry(screen->name(), position.x(), position.y(), window->width(), window->height());
        committing = false;
    }

    void screensChanged()
    {
        if (!q->m_ready || !window)
            return;
        if (!screen || !QGuiApplication::screens().contains(screen)) {
            switchScreen(QGuiApplication::primaryScreen());
        }
        applyPosition();
        persist();
    }
};

DesktopPlacement::DesktopPlacement(QObject *parent) : QObject(parent), d(new Private(this))
{
    const auto watchScreen = [this](QScreen *screen) {
        connect(screen, &QScreen::geometryChanged, this, [this] { d->screensChanged(); });
    };
    for (auto *screen : QGuiApplication::screens())
        watchScreen(screen);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this, watchScreen](QScreen *screen) {
        watchScreen(screen);
        Q_EMIT screensChanged();
        d->screensChanged();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
        Q_EMIT screensChanged();
        d->screensChanged();
    });
}

DesktopPlacement::~DesktopPlacement()
{
    delete d;
}

QQuickWindow *DesktopPlacement::window() const { return d->window; }
Note *DesktopPlacement::note() const { return d->note; }

void DesktopPlacement::setWindow(QQuickWindow *w)
{
    if (w == d->window)
        return;
    for (const auto &connection : std::as_const(d->windowConnections))
        disconnect(connection);
    d->windowConnections.clear();
    d->marginTimer.stop();
    d->settleTimer.stop();
    d->window = w;
    d->screen = nullptr;
#ifdef HAVE_LAYERSHELL
    d->layer = nullptr;
#endif
    if (m_ready) {
        m_ready = false;
        Q_EMIT readyChanged();
    }
    if (w) {
        const auto settled = [this] {
            if (m_ready && !d->configuring)
                d->settleTimer.start();
        };
        d->windowConnections << connect(w, &QWindow::xChanged, this, settled)
                             << connect(w, &QWindow::yChanged, this, settled)
                             << connect(w, &QWindow::widthChanged, this, settled)
                             << connect(w, &QWindow::heightChanged, this, settled)
                             << connect(w, &QWindow::screenChanged, this, settled)
                             // afterAnimating runs on the GUI thread right before a frame is synced and
                             // rendered: whatever margins are set by then go out with that frame's commit.
                             << connect(w, &QQuickWindow::afterAnimating, this, [this] {
                                    if (d->commitPending) {
                                        d->inFlightFrame = d->requestedSinceCommit;
                                        d->frameInFlight = true;
                                        d->commitPending = false;
                                    }
                                })
                             // frameSwapped may come from the render thread; queue it to the GUI thread.
                             << connect(w, &QQuickWindow::frameSwapped, this, [this] {
                                    if (d->frameInFlight) {
                                        d->appliedPosition = d->inFlightFrame;
                                        d->frameInFlight = false;
                                    }
                                }, Qt::QueuedConnection);
    }
    Q_EMIT windowChanged();
    d->configure();
}

void DesktopPlacement::setNote(Note *n)
{
    if (n == d->note)
        return;
    for (const auto &connection : std::as_const(d->noteConnections))
        disconnect(connection);
    d->noteConnections.clear();
    d->note = n;
    if (n) {
        d->noteConnections << connect(n, &Note::pinnedChanged, this, [this] { d->pinnedChanged(); })
                           << connect(n, &Note::geometryChanged, this, [this] {
            if (m_ready && !d->committing && !m_dragging && !d->resizing)
                d->restoreGeometry();
        });
    }
    Q_EMIT noteChanged();
    d->configure();
}

bool DesktopPlacement::layerShellAvailable()
{
#ifdef HAVE_LAYERSHELL
    if (QGuiApplication::platformName() != QStringLiteral("wayland"))
        return false;
#ifdef HAVE_WAYLAND_REGISTRY
    static const bool supported = compositorHasLayerShell();
    return supported;
#else
    return true;
#endif
#else
    return false;
#endif
}

QStringList DesktopPlacement::screenNames() const
{
    QStringList names;
    for (auto *screen : QGuiApplication::screens())
        names.append(screen->name());
    return names;
}

void DesktopPlacement::classBegin() {}
void DesktopPlacement::componentComplete()
{
    d->complete = true;
    d->configure();
}

void DesktopPlacement::beginDrag(const QPointF &windowLocalPos)
{
    if (!m_ready || !d->window || m_dragging)
        return;
    d->pressPoint = windowLocalPos;
    d->appliedPosition = d->position;
    d->commitPending = false;
    d->frameInFlight = false;
    qCDebug(lcPlacement) << "beginDrag press" << windowLocalPos << "position" << d->position
                         << "screen" << (d->screen ? d->screen->name() : QString());
    m_dragging = true;
    Q_EMIT draggingChanged();
    if (!layerShell())
        d->window->startSystemMove();
}

void DesktopPlacement::dragTo(const QPointF &windowLocalPos)
{
    if (!m_dragging || !layerShell())
        return;
    // Pointer coordinates are relative to where the compositor has the surface NOW, which is the
    // last position committed in a frame (appliedPosition), not the last one we asked for.
    // Using the requested position here made every not-yet-applied move count twice, so the note
    // ran away from the pointer. The target is absolute, so a late commit can't compound.
    d->pendingPosition = PlacementMath::dragTarget(d->appliedPosition, windowLocalPos, d->pressPoint);
    qCDebug(lcPlacement) << "dragTo applied" << d->appliedPosition << "local" << windowLocalPos << "press" << d->pressPoint
                         << "position" << d->position << "pending" << d->pendingPosition;
    if (!d->marginTimer.isActive())
        d->marginTimer.start();
}

void DesktopPlacement::endDrag()
{
    if (!m_dragging)
        return;
    if (d->marginTimer.isActive()) {
        d->marginTimer.stop();
        d->applyPending();
    }
    if (layerShell())
        d->switchToScreenUnderNote();
    m_dragging = false;
    Q_EMIT draggingChanged();
    d->persist();
    d->settleTimer.start();
}

void DesktopPlacement::beginResize(const QPointF &windowLocalPos)
{
    if (!m_ready || !d->window)
        return;
    d->resizing = true;
    d->resizePress = windowLocalPos;
    d->resizeStart = d->window->size();
    if (!layerShell())
        d->window->startSystemResize(Qt::BottomEdge | Qt::RightEdge);
}

void DesktopPlacement::resizeTo(const QPointF &windowLocalPos)
{
    if (!d->resizing || !d->window || !layerShell())
        return;
    const QPoint delta = (windowLocalPos - d->resizePress).toPoint();
    const QSize size(std::max(160, d->resizeStart.width() + delta.x()),
                     std::max(120, d->resizeStart.height() + delta.y()));
#ifdef HAVE_LAYERSHELL
    if (d->layer)
        d->layer->setDesiredSize(size);
#endif
    d->window->resize(size);
}

void DesktopPlacement::endResize()
{
    d->resizing = false;
    commitSize();
}

void DesktopPlacement::commitSize()
{
    d->persist();
    d->settleTimer.start();
}

void DesktopPlacement::moveToScreen(const QString &screenName)
{
    if (!m_ready || !d->window)
        return;
    d->switchScreen(screenNamed(screenName));
    d->applyPosition();
    d->persist();
}

void DesktopPlacement::raise()
{
    if (!d->window)
        return;
    d->window->show();
    d->window->raise();
    d->window->requestActivate();
}
