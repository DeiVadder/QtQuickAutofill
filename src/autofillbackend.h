// SPDX-License-Identifier: MIT
#ifndef QTQUICKAUTOFILL_AUTOFILLBACKEND_H
#define QTQUICKAUTOFILL_AUTOFILLBACKEND_H

#include <QRect>
#include <QString>

#include <memory>

/// Platform side of QtQuickAutofill. The default implementation does nothing (desktop).
class AutofillBackend
{
public:
    virtual ~AutofillBackend();

    virtual bool isAvailable() const { return false; }

    /// A field with \a role (an Autofill::Role value) and the unique \a id appeared.
    virtual void addField(int id, int role) { (void)id; (void)role; }
    virtual void removeField(int id) { (void)id; }
    /// Focus entered or left field \a id. Rectangles are in physical pixels relative to the window.
    virtual void focusChanged(int id, bool focused, const QRect &windowRect, bool requestFill)
    {
        (void)id; (void)focused; (void)windowRect; (void)requestFill;
    }
    /// The focused field moved, e.g. while scrolling.
    virtual void boundsChanged(int id, const QRect &windowRect) { (void)id; (void)windowRect; }
    /// The user changed the text of field \a id.
    virtual void valueChanged(int id, const QString &value) { (void)id; (void)value; }
    virtual void commit() {}
    virtual void cancel() {}
    virtual void requestFill(int id, const QRect &windowRect) { (void)id; (void)windowRect; }

    /// Backend for the platform this library was built for.
    static std::unique_ptr<AutofillBackend> createPlatformBackend();
};

#endif // QTQUICKAUTOFILL_AUTOFILLBACKEND_H
