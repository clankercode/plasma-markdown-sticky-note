// SPDX-License-Identifier: MIT
#include "desktopplacement.h"
#include "note.h"

#include <QGuiApplication>
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

namespace {
QScreen *screenNamed(const QString &name)
{
    for (auto *screen : QGuiApplication::screens()) {
        if (screen->name() == name)
            return screen;
    }
    return QGuiApplication::primaryScreen();
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
    QPoint position;
    QPoint pendingPosition;
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
        const QSize area = screen->geometry().size();
        p.setX(std::clamp(p.x(), 48 - window->width(), std::max(48 - window->width(), area.width() - 48)));
        p.setY(std::clamp(p.y(), 48 - window->height(), std::max(48 - window->height(), area.height() - 48)));
        return p;
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
            layer->setMargins(QMargins(position.x(), position.y(), 0, 0));
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
#ifdef HAVE_LAYERSHELL
        if (layer) {
            layer->setMargins(QMargins(position.x(), position.y(), 0, 0));
            return;
        }
#endif
        window->setPosition(screen->geometry().topLeft() + position);
    }

    void applyPending()
    {
        if (!window || !screen)
            return;
        position = pendingPosition;
        const QPoint global = screen->geometry().topLeft() + position;
        const QPoint center = global + QPoint(window->width() / 2, window->height() / 2);
        if (!screen->geometry().contains(center)) {
            for (auto *candidate : QGuiApplication::screens()) {
                if (candidate != screen && candidate->geometry().contains(center)) {
                    position = global - candidate->geometry().topLeft();
                    switchScreen(candidate);
                    break;
                }
            }
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
                             << connect(w, &QWindow::screenChanged, this, settled);
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
    m_dragging = true;
    Q_EMIT draggingChanged();
    if (!layerShell())
        d->window->startSystemMove();
}

void DesktopPlacement::dragTo(const QPointF &windowLocalPos)
{
    if (!m_dragging || !layerShell())
        return;
    // Local coordinates already reflect the last applied move. Coalesce absolute targets,
    // not deltas, or multiple pointer events in a frame would move the note too far.
    d->pendingPosition = d->position + (windowLocalPos - d->pressPoint).toPoint();
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
