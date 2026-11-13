// SPDX-License-Identifier: MIT
//
// Qt's iOS input object QIOSTextInputResponder (qtbase/src/plugins/platforms/ios/qiostextresponder.mm) implements
// UITextInputTraits but never sets textContentType, so iOS Password AutoFill cannot classify Qt Quick fields.
// This backend adds a textContentType getter to that class at runtime. It reads Autofill.role of the item that has
// focus. QIOSTextInputResponder is a private Qt class: if a Qt version renames it, the hook logs a warning and does
// nothing.
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#include "autofill.h"
#include "autofillbackend.h"

#include <QGuiApplication>

namespace {

id textContentTypeForFocus(id, SEL)
{
    QObject *focus = QGuiApplication::focusObject();
    if (!focus) {
        return nil;
    }
    auto *attached = qobject_cast<AutofillAttached *>(qmlAttachedPropertiesObject<Autofill>(focus, false));
    if (!attached) {
        return nil;
    }
    switch (attached->role()) {
    case Autofill::Username: return UITextContentTypeUsername;
    case Autofill::Password: return UITextContentTypePassword;
    case Autofill::NewPassword: return UITextContentTypeNewPassword;
    case Autofill::Email: return UITextContentTypeEmailAddress;
    case Autofill::OneTimeCode: return UITextContentTypeOneTimeCode;
    case Autofill::None: break;
    }
    return nil;
}

class IosAutofillBackend : public AutofillBackend
{
public:
    IosAutofillBackend()
    {
        Class cls = NSClassFromString(@"QIOSTextInputResponder");
        if (!cls) {
            qWarning("QtQuickAutofill: QIOSTextInputResponder not found; iOS autofill hints disabled");
            return;
        }
        const SEL selector = @selector(textContentType);
        const IMP imp = reinterpret_cast<IMP>(textContentTypeForFocus);
        if (!class_addMethod(cls, selector, imp, "@@:")) {
            method_setImplementation(class_getInstanceMethod(cls, selector), imp);
        }
        m_available = true;
    }

    bool isAvailable() const override { return m_available; }
    // iOS needs nothing else: UIKit asks the responder when a field gets focus. Saving after sign-in is decided by
    // iOS and requires Associated Domains (webcredentials:), see README.

private:
    bool m_available{false};
};

} // namespace

std::unique_ptr<AutofillBackend> AutofillBackend::createPlatformBackend()
{
    return std::make_unique<IosAutofillBackend>();
}
