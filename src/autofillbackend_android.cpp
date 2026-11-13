// SPDX-License-Identifier: MIT
#include "autofill.h"
#include "autofillbackend.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>

namespace {
constexpr auto kOverlayClass = "io/github/deivadder/qtquickautofill/AutofillOverlay";

// The autofill framework needs Android 8.0
constexpr int kMinSdk = 26;

// Runs on the Android UI thread; hand the value over to the Qt thread before touching the service
void jniAutofilled(JNIEnv *, jclass, jint id, jstring value)
{
    const QString text = QJniObject(value).toString();
    QMetaObject::invokeMethod(
        QCoreApplication::instance(), [id, text] { AutofillService::instance()->deliverAutofill(id, text); },
        Qt::QueuedConnection);
}

class AndroidAutofillBackend : public AutofillBackend
{
public:
    AndroidAutofillBackend()
    {
        if (QNativeInterface::QAndroidApplication::sdkVersion() < kMinSdk) {
            qWarning("QtQuickAutofill: autofill needs Android 8.0 (API 26) or newer; disabled");
            return;
        }
        QJniEnvironment env;
        const JNINativeMethod methods[] = {
            {"nativeAutofilled", "(ILjava/lang/String;)V", reinterpret_cast<void *>(jniAutofilled)},
        };
        if (!env.registerNativeMethods(kOverlayClass, methods, 1)) {
            qWarning("QtQuickAutofill: Java class not found; did you call qtquickautofill_configure_android()?");
            return;
        }
        m_installed = true;
        const QJniObject context = QNativeInterface::QAndroidApplication::context();
        m_available = QJniObject::callStaticMethod<jboolean>(kOverlayClass, "install",
                                                             "(Landroid/content/Context;)Z", context.object());
    }

    bool isAvailable() const override { return m_available; }

    void addField(int id, int role) override
    {
        if (!m_installed) {
            return;
        }
        QJniObject::callStaticMethod<void>(kOverlayClass, "addField", "(II)V", jint(id), jint(role));
    }

    void removeField(int id) override
    {
        if (!m_installed) {
            return;
        }
        QJniObject::callStaticMethod<void>(kOverlayClass, "removeField", "(I)V", jint(id));
    }

    void focusChanged(int id, bool focused, const QRect &r, bool requestFill) override
    {
        if (!m_installed) {
            return;
        }
        QJniObject::callStaticMethod<void>(kOverlayClass, "focusChanged", "(IZIIIIZ)V", jint(id), jboolean(focused),
                                           jint(r.x()), jint(r.y()), jint(r.width()), jint(r.height()),
                                           jboolean(requestFill));
    }

    void boundsChanged(int id, const QRect &r) override
    {
        if (!m_installed) {
            return;
        }
        QJniObject::callStaticMethod<void>(kOverlayClass, "boundsChanged", "(IIIII)V", jint(id), jint(r.x()),
                                           jint(r.y()), jint(r.width()), jint(r.height()));
    }

    void visibilityChanged(int id, bool visible) override
    {
        if (!m_installed) {
            return;
        }
        QJniObject::callStaticMethod<void>(kOverlayClass, "visibilityChanged", "(IZ)V", jint(id),
                                           jboolean(visible));
    }

    void valueChanged(int id, const QString &value) override
    {
        if (!m_installed) {
            return;
        }
        QJniObject::callStaticMethod<void>(kOverlayClass, "valueChanged", "(ILjava/lang/String;)V", jint(id),
                                           QJniObject::fromString(value).object<jstring>());
    }

    void commit() override
    {
        if (m_installed) {
            QJniObject::callStaticMethod<void>(kOverlayClass, "commit", "()V");
        }
    }

    void cancel() override
    {
        if (m_installed) {
            QJniObject::callStaticMethod<void>(kOverlayClass, "cancel", "()V");
        }
    }

    void requestFill(int id, const QRect &r) override
    {
        if (!m_installed) {
            return;
        }
        QJniObject::callStaticMethod<void>(kOverlayClass, "requestFill", "(IIIII)V", jint(id), jint(r.x()),
                                           jint(r.y()), jint(r.width()), jint(r.height()));
    }

private:
    bool m_installed{false};
    bool m_available{false};
};
} // namespace

std::unique_ptr<AutofillBackend> AutofillBackend::createPlatformBackend()
{
    return std::make_unique<AndroidAutofillBackend>();
}
