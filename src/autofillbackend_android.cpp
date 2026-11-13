// SPDX-License-Identifier: MIT
#include "autofill.h"
#include "autofillbackend.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>

namespace {
constexpr auto kOverlayClass = "io/github/deivadder/qtquickautofill/AutofillOverlay";

void jniAutofilled(JNIEnv *, jclass, jint id, jstring value)
{
    const QString text = QJniObject(value).toString();
    QMetaObject::invokeMethod(AutofillService::instance(),
                              [id, text] { AutofillService::instance()->deliverAutofill(id, text); },
                              Qt::QueuedConnection);
}

class AndroidAutofillBackend : public AutofillBackend
{
public:
    AndroidAutofillBackend()
    {
        QJniEnvironment env;
        const JNINativeMethod methods[] = {
            {"nativeAutofilled", "(ILjava/lang/String;)V", reinterpret_cast<void *>(jniAutofilled)},
        };
        if (!env.registerNativeMethods(kOverlayClass, methods, 1)) {
            qWarning("QtQuickAutofill: Java class not found; did you call qtquickautofill_configure_android()?");
            return;
        }
        const QJniObject context = QNativeInterface::QAndroidApplication::context();
        m_available = QJniObject::callStaticMethod<jboolean>(kOverlayClass, "install",
                                                             "(Landroid/content/Context;)Z", context.object());
    }

    bool isAvailable() const override { return m_available; }

    void addField(int id, int role) override
    {
        QJniObject::callStaticMethod<void>(kOverlayClass, "addField", "(II)V", jint(id), jint(role));
    }

    void removeField(int id) override
    {
        QJniObject::callStaticMethod<void>(kOverlayClass, "removeField", "(I)V", jint(id));
    }

    void focusChanged(int id, bool focused, const QRect &r, bool requestFill) override
    {
        QJniObject::callStaticMethod<void>(kOverlayClass, "focusChanged", "(IZIIIIZ)V", jint(id), jboolean(focused),
                                           jint(r.x()), jint(r.y()), jint(r.width()), jint(r.height()),
                                           jboolean(requestFill));
    }

    void boundsChanged(int id, const QRect &r) override
    {
        QJniObject::callStaticMethod<void>(kOverlayClass, "boundsChanged", "(IIIII)V", jint(id), jint(r.x()),
                                           jint(r.y()), jint(r.width()), jint(r.height()));
    }

    void valueChanged(int id, const QString &value) override
    {
        QJniObject::callStaticMethod<void>(kOverlayClass, "valueChanged", "(ILjava/lang/String;)V", jint(id),
                                           QJniObject::fromString(value).object<jstring>());
    }

    void commit() override { QJniObject::callStaticMethod<void>(kOverlayClass, "commit", "()V"); }
    void cancel() override { QJniObject::callStaticMethod<void>(kOverlayClass, "cancel", "()V"); }

    void requestFill(int id, const QRect &r) override
    {
        QJniObject::callStaticMethod<void>(kOverlayClass, "requestFill", "(IIIII)V", jint(id), jint(r.x()),
                                           jint(r.y()), jint(r.width()), jint(r.height()));
    }

private:
    bool m_available{false};
};
} // namespace

std::unique_ptr<AutofillBackend> AutofillBackend::createPlatformBackend()
{
    return std::make_unique<AndroidAutofillBackend>();
}
