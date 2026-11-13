// SPDX-License-Identifier: MIT
#ifndef QTQUICKAUTOFILL_AUTOFILL_H
#define QTQUICKAUTOFILL_AUTOFILL_H

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QRect>

#include <memory>

class AutofillAttached;
class AutofillBackend;

/*!
    \qmltype Autofill
    \brief Attached property that announces a Qt Quick text field to the platform autofill service.

    \code
    TextField { Autofill.role: Autofill.Username }
    TextField { echoMode: TextInput.Password; Autofill.role: Autofill.Password }
    \endcode

    Attach it to the item that receives keyboard focus and has a \c text property and a \c textChanged signal
    (TextInput, TextField). Attaching it to a wrapper whose inner TextInput has the focus does not work.
*/
class Autofill : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Autofill is an attached property")
    QML_ATTACHED(AutofillAttached)

public:
    /// What the field contains. Maps to UITextContentType on iOS and to View.AUTOFILL_HINT_* on Android.
    enum Role {
        None,
        Username,
        Password,
        NewPassword,
        Email,
        OneTimeCode
    };
    Q_ENUM(Role)

    static AutofillAttached *qmlAttachedProperties(QObject *object);
};

/// Per-field state behind \c Autofill.role.
class AutofillAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Autofill::Role role READ role WRITE setRole NOTIFY roleChanged FINAL)

public:
    explicit AutofillAttached(QObject *object);
    ~AutofillAttached() override;

    Autofill::Role role() const { return m_role; }
    void setRole(Autofill::Role role);

    QQuickItem *item() const { return m_item; }
    int fieldId() const { return m_id; }

    /// Bounds of the field in physical pixels, relative to its window (includes scale and rotation).
    QRect windowRect() const;
    /// True if the field is visible and at least partly inside its window.
    bool isShown() const;

    /// Called by the service when the platform delivers a value for this field.
    void applyAutofill(const QString &value);

signals:
    void roleChanged();
    /// The platform filled this field with \a value.
    void filled(const QString &value);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onTextChanged();

private:
    void onActiveFocusChanged();
    void onVisibleChanged();
    void onAfterAnimating();
    void trackWindow(QQuickWindow *window);

    QPointer<QQuickItem> m_item;
    QPointer<QQuickWindow> m_window;
    QMetaObject::Connection m_animatingConnection;
    Autofill::Role m_role{Autofill::None};
    int m_id{0};
    QRect m_lastRect;
    bool m_applying{false};
    Qt::FocusReason m_focusReason{Qt::OtherFocusReason};
};

/*!
    \qmltype AutofillService
    \brief Singleton for the parts of an autofill session that are not tied to one field.

    Call \c commit() after a successful sign-in so that the platform can offer to save the credentials, or
    \c cancel() when the form is abandoned.
*/
class AutofillService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged FINAL)
    Q_PROPERTY(bool requestOnFocus READ requestOnFocus WRITE setRequestOnFocus NOTIFY requestOnFocusChanged FINAL)

public:
    ~AutofillService() override;

    static AutofillService *instance();
    static AutofillService *create(QQmlEngine *, QJSEngine *);

    /// Replaces the platform backend, e.g. with a test double. Takes ownership. Fields that are already registered
    /// are announced to the new backend.
    static void setBackend(std::unique_ptr<AutofillBackend> backend);

    bool isAvailable() const;

    /*!
        Android only. Gboard (Android 11+) waits for an inline-suggestion request that never comes for Qt's own input
        view, so a plain focus change does not show suggestions. When true (default), focusing a field also sends an
        explicit autofill request.
    */
    bool requestOnFocus() const { return m_requestOnFocus; }
    void setRequestOnFocus(bool on);

    Q_INVOKABLE void commit();
    Q_INVOKABLE void cancel();
    /// Explicitly asks the platform for suggestions for \a field.
    Q_INVOKABLE void requestFill(QQuickItem *field);

    // Used by AutofillAttached and the backends
    int registerField(AutofillAttached *field);
    void unregisterField(int id);
    AutofillBackend *backend() const { return m_backend.get(); }
    AutofillAttached *field(int id) const { return m_fields.value(id); }
    /// Sends the current bounds and visibility of all fields to the backend.
    void syncFields();
    /// Called by a backend (on the GUI thread) when the platform fills field \a id.
    void deliverAutofill(int id, const QString &value);

signals:
    void availableChanged();
    void requestOnFocusChanged();

private:
    explicit AutofillService(std::unique_ptr<AutofillBackend> backend);

    std::unique_ptr<AutofillBackend> m_backend;
    QHash<int, AutofillAttached *> m_fields;
    int m_nextId{1};
    bool m_requestOnFocus{true};
};

#endif // QTQUICKAUTOFILL_AUTOFILL_H
