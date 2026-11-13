// SPDX-License-Identifier: MIT
#include "autofill.h"
#include "autofillbackend.h"

#include <QQuickWindow>

AutofillAttached *Autofill::qmlAttachedProperties(QObject *object)
{
    auto *item = qobject_cast<QQuickItem *>(object);
    if (!item) {
        qWarning("QtQuickAutofill: Autofill can only be attached to an Item");
    }
    return new AutofillAttached(item);
}

AutofillAttached::AutofillAttached(QQuickItem *item)
    : QObject(item)
    , m_item(item)
{
    if (!m_item) {
        return;
    }
    connect(m_item, &QQuickItem::activeFocusChanged, this, &AutofillAttached::onActiveFocusChanged);
    connect(m_item, &QQuickItem::windowChanged, this, &AutofillAttached::trackWindow);

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
    emit roleChanged();
}

QRect AutofillAttached::windowRect() const
{
    if (!m_item || !m_item->window()) {
        return {};
    }
    // Window-relative on purpose: on Android mapToGlobal() does not include the status bar offset of the window
    const QPointF topLeft = m_item->mapToScene(QPointF(0, 0));
    const qreal dpr = m_item->window()->devicePixelRatio();
    return QRect(qRound(topLeft.x() * dpr), qRound(topLeft.y() * dpr), qRound(m_item->width() * dpr),
                 qRound(m_item->height() * dpr));
}

void AutofillAttached::applyAutofill(const QString &value)
{
    if (!m_item) {
        return;
    }
    // Setting the text emits textChanged; do not echo the platform's own value back to it
    m_applying = true;
    m_item->setProperty("text", value);
    m_applying = false;
    emit filled(value);
}

void AutofillAttached::onActiveFocusChanged()
{
    if (m_id == 0) {
        return;
    }
    AutofillService *service = AutofillService::instance();
    const bool focused = m_item->hasActiveFocus();
    m_lastRect = windowRect();
    service->backend()->focusChanged(m_id, focused, m_lastRect, focused && service->requestOnFocus());
    trackWindow(focused ? m_item->window() : nullptr);
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
    // Only connected while the field has focus: keep the platform's anchor rectangle up to date
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
    if (m_window && m_item && m_item->hasActiveFocus()) {
        m_animatingConnection =
            connect(m_window, &QQuickWindow::afterAnimating, this, &AutofillAttached::onAfterAnimating);
    }
}

namespace {
AutofillService *s_service = nullptr;
}

AutofillService::AutofillService(QObject *parent)
    : QObject(parent)
    , m_backend(AutofillBackend::createPlatformBackend())
{
}

AutofillService::~AutofillService() = default;

AutofillService *AutofillService::instance()
{
    if (!s_service) {
        s_service = new AutofillService();
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
    instance()->m_backend = backend ? std::move(backend) : std::make_unique<AutofillBackend>();
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

void AutofillService::deliverAutofill(int id, const QString &value)
{
    if (AutofillAttached *f = m_fields.value(id)) {
        f->applyAutofill(value);
    }
}
