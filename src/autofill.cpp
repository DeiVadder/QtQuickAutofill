// SPDX-License-Identifier: MIT
#include "autofill.h"
#include "autofillbackend.h"

#include <QFocusEvent>
#include <QQuickWindow>

AutofillAttached *Autofill::qmlAttachedProperties(QObject *object)
{
    if (!qobject_cast<QQuickItem *>(object)) {
        qWarning("QtQuickAutofill: Autofill can only be attached to an Item");
    }
    return new AutofillAttached(object);
}

AutofillAttached::AutofillAttached(QObject *object)
    : QObject(object)
    , m_item(qobject_cast<QQuickItem *>(object))
{
    if (!m_item) {
        return;
    }
    connect(m_item, &QQuickItem::activeFocusChanged, this, &AutofillAttached::onActiveFocusChanged);
    connect(m_item, &QQuickItem::visibleChanged, this, &AutofillAttached::onVisibleChanged);
    connect(m_item, &QQuickItem::windowChanged, this, &AutofillAttached::trackWindow);
    // Focus events carry the reason and arrive before activeFocusChanged
    m_item->installEventFilter(this);

    // TextInput and TextField have textChanged() but no common C++ base we could rely on
    const int signalIndex = m_item->metaObject()->indexOfSignal("textChanged()");
    if (signalIndex >= 0) {
        QMetaObject::connect(m_item, signalIndex, this, metaObject()->indexOfSlot("onTextChanged()"));
    } else {
        qWarning("QtQuickAutofill: %s has no textChanged() signal", m_item->metaObject()->className());
    }
}

AutofillAttached::~AutofillAttached()
{
    if (m_id != 0) {
        AutofillService::instance()->unregisterField(m_id);
    }
}

void AutofillAttached::setRole(Autofill::Role role)
{
    if (role == m_role || !m_item) {
        return;
    }
    if (m_id != 0) {
        AutofillService::instance()->unregisterField(m_id);
        m_id = 0;
    }
    m_role = role;
    if (m_role != Autofill::None) {
        m_id = AutofillService::instance()->registerField(this);
    }
    // A field that already has focus would otherwise stay unknown to the platform until it is focused again
    if (m_item->hasActiveFocus()) {
        onActiveFocusChanged();
    }
    emit roleChanged();
}

QRect AutofillAttached::windowRect() const
{
    if (!m_item || !m_item->window()) {
        return {};
    }
    // Window-relative on purpose: on Android mapToGlobal() does not include the status bar offset of the window
    const QRectF scene = m_item->mapRectToScene(QRectF(0, 0, m_item->width(), m_item->height()));
    const qreal dpr = m_item->window()->devicePixelRatio();
    return QRectF(scene.topLeft() * dpr, scene.size() * dpr).toAlignedRect();
}

bool AutofillAttached::isShown() const
{
    if (!m_item || !m_item->window() || !m_item->isVisible()) {
        return false;
    }
    const QSizeF windowSize = QSizeF(m_item->window()->size()) * m_item->window()->devicePixelRatio();
    return windowRect().intersects(QRect(QPoint(0, 0), windowSize.toSize()));
}

void AutofillAttached::applyAutofill(const QString &value)
{
    // Never fill a field the user cannot see, e.g. on another page of a SwipeView
    if (!m_item || !isShown()) {
        return;
    }
    // Setting the text emits textChanged; do not echo the platform's own value back to it
    m_applying = true;
    m_item->setProperty("text", value);
    m_applying = false;
    emit filled(value);
}

bool AutofillAttached::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_item && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        m_focusReason = static_cast<QFocusEvent *>(event)->reason();
    }
    return QObject::eventFilter(watched, event);
}

void AutofillAttached::onActiveFocusChanged()
{
    const bool focused = m_item->hasActiveFocus();
    const Qt::FocusReason reason = m_focusReason;
    m_focusReason = Qt::OtherFocusReason;
    if (m_id == 0) {
        trackWindow(nullptr);
        return;
    }
    AutofillService *service = AutofillService::instance();
    if (focused) {
        service->syncFields();
    }
    // When the window only gets reactivated (back from another app or an autofill auth screen), the platform still
    // gets the focus, but no new explicit request
    const bool requestFill = focused && service->requestOnFocus() && reason != Qt::ActiveWindowFocusReason;
    m_lastRect = windowRect();
    service->backend()->focusChanged(m_id, focused, m_lastRect, requestFill);
    trackWindow(focused ? m_item->window() : nullptr);
}

void AutofillAttached::onVisibleChanged()
{
    if (m_id != 0) {
        AutofillService::instance()->backend()->visibilityChanged(m_id, isShown());
    }
}

void AutofillAttached::onTextChanged()
{
    if (m_id == 0 || m_applying) {
        return;
    }
    AutofillService::instance()->backend()->valueChanged(m_id, m_item->property("text").toString());
}

void AutofillAttached::onAfterAnimating()
{
    // Only connected while the field has focus and a role
    if (m_id == 0) {
        return;
    }
    const QRect rect = windowRect();
    if (rect != m_lastRect) {
        m_lastRect = rect;
        AutofillService::instance()->backend()->boundsChanged(m_id, rect);
    }
}

void AutofillAttached::trackWindow(QQuickWindow *window)
{
    if (m_animatingConnection) {
        disconnect(m_animatingConnection);
    }
    m_window = window;
    if (m_window && m_item && m_id != 0 && m_item->hasActiveFocus()) {
        m_animatingConnection =
            connect(m_window, &QQuickWindow::afterAnimating, this, &AutofillAttached::onAfterAnimating);
    }
}

namespace {
// Lives until the process ends; attached objects may unregister from it during shutdown
AutofillService *s_service = nullptr;
}

AutofillService::AutofillService(std::unique_ptr<AutofillBackend> backend)
    : m_backend(std::move(backend))
{
}

AutofillService::~AutofillService() = default;

AutofillService *AutofillService::instance()
{
    if (!s_service) {
        s_service = new AutofillService(AutofillBackend::createPlatformBackend());
    }
    return s_service;
}

AutofillService *AutofillService::create(QQmlEngine *, QJSEngine *)
{
    AutofillService *service = instance();
    QJSEngine::setObjectOwnership(service, QJSEngine::CppOwnership);
    return service;
}

void AutofillService::setBackend(std::unique_ptr<AutofillBackend> backend)
{
    if (!backend) {
        backend = std::make_unique<AutofillBackend>();
    }
    if (!s_service) {
        // Do not create (and install) the platform backend just to replace it
        s_service = new AutofillService(std::move(backend));
        return;
    }
    const bool wasAvailable = s_service->isAvailable();
    s_service->m_backend = std::move(backend);
    for (auto it = s_service->m_fields.cbegin(); it != s_service->m_fields.cend(); ++it) {
        s_service->m_backend->addField(it.key(), int(it.value()->role()));
    }
    if (s_service->isAvailable() != wasAvailable) {
        emit s_service->availableChanged();
    }
}

bool AutofillService::isAvailable() const
{
    return m_backend->isAvailable();
}

void AutofillService::setRequestOnFocus(bool on)
{
    if (on == m_requestOnFocus) {
        return;
    }
    m_requestOnFocus = on;
    emit requestOnFocusChanged();
}

void AutofillService::commit()
{
    m_backend->commit();
}

void AutofillService::cancel()
{
    m_backend->cancel();
}

void AutofillService::requestFill(QQuickItem *field)
{
    auto *attached = qobject_cast<AutofillAttached *>(qmlAttachedPropertiesObject<Autofill>(field, false));
    if (!attached || attached->fieldId() == 0) {
        qWarning("QtQuickAutofill: requestFill() needs an item with Autofill.role set");
        return;
    }
    syncFields();
    m_backend->requestFill(attached->fieldId(), attached->windowRect());
}

int AutofillService::registerField(AutofillAttached *field)
{
    const int id = m_nextId++;
    m_fields.insert(id, field);
    m_backend->addField(id, int(field->role()));
    return id;
}

void AutofillService::unregisterField(int id)
{
    if (m_fields.remove(id) > 0) {
        m_backend->removeField(id);
    }
}

void AutofillService::syncFields()
{
    for (auto it = m_fields.cbegin(); it != m_fields.cend(); ++it) {
        m_backend->boundsChanged(it.key(), it.value()->windowRect());
        m_backend->visibilityChanged(it.key(), it.value()->isShown());
    }
}

void AutofillService::deliverAutofill(int id, const QString &value)
{
    if (AutofillAttached *f = m_fields.value(id)) {
        f->applyAutofill(value);
    }
}
