// SPDX-License-Identifier: MIT
//
// Platform-independent part: field registration, focus/value forwarding and applying autofilled values.
// A recording backend stands in for Android/iOS.
#include "autofill.h"
#include "autofillbackend.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(QtQuickAutofillPlugin)

namespace {

struct Call
{
    QString what;
    int id{0};
    int role{0};
    bool flag{false};
    QString value;
    QRect rect;
};

class RecordingBackend : public AutofillBackend
{
public:
    explicit RecordingBackend(QList<Call> *calls) : m_calls(calls) {}
    bool isAvailable() const override { return true; }
    void addField(int id, int role) override { m_calls->append({"add", id, role}); }
    void removeField(int id) override { m_calls->append({"remove", id}); }
    void focusChanged(int id, bool focused, const QRect &rect, bool requestFill) override
    {
        m_calls->append({focused ? "focusIn" : "focusOut", id, 0, requestFill, {}, rect});
    }
    void boundsChanged(int id, const QRect &rect) override { m_calls->append({"bounds", id, 0, false, {}, rect}); }
    void visibilityChanged(int id, bool visible) override { m_calls->append({"visibility", id, 0, visible}); }
    void valueChanged(int id, const QString &value) override { m_calls->append({"value", id, 0, false, value}); }
    void commit() override { m_calls->append({"commit"}); }
    void cancel() override { m_calls->append({"cancel"}); }
    void requestFill(int id, const QRect &rect) override { m_calls->append({"request", id, 0, false, {}, rect}); }

private:
    QList<Call> *m_calls;
};

const QByteArray kQml = R"(
import QtQuick
import QtQuickAutofill
Item {
    width: 200; height: 200
    property alias user: user
    property alias pass: pass
    TextInput { id: user; width: 100; height: 20; Autofill.role: Autofill.Username }
    TextInput { id: pass; y: 30; width: 100; height: 20; echoMode: TextInput.Password; Autofill.role: Autofill.Password }
    property alias plain: plain
    TextInput { id: plain; y: 60; width: 100; height: 20 }
}
)";

} // namespace

class tst_Autofill : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void registersFieldsWithTheirRole();
    void forwardsFocusWithRequestOnFocus();
    void doesNotRequestWhenRequestOnFocusIsOff();
    void forwardsUserTextChanges();
    void appliesAutofilledValueWithoutEchoingIt();
    void ignoresAutofillForUnknownField();
    void unregistersWhenItemIsDestroyed();
    void roleNoneUnregisters();
    void commitAndCancelReachBackend();
    void requestFillUsesFieldId();
    void reportsWindowRectWithScale();
    void settingRoleOnFocusedFieldReportsFocus();
    void changingRoleOnFocusedFieldReportsNewId();
    void windowReactivationDoesNotRequestAgain();
    void focusSyncsBoundsAndVisibilityOfAllFields();
    void hiddenFieldReportsVisibilityAndIsNotFilled();
    void tracksBoundsOfFocusedField();
    void roleNoneStopsBoundsTracking();
    void setBackendAnnouncesRegisteredFields();

private:
    QQuickItem *field(const char *name) const { return m_root->property(name).value<QQuickItem *>(); }
    AutofillAttached *attachedOf(const char *name, bool create = false) const
    {
        return qobject_cast<AutofillAttached *>(qmlAttachedPropertiesObject<Autofill>(field(name), create));
    }
    int idOf(const char *name) const;
    QList<Call> callsOf(const QString &what) const;

    QList<Call> m_calls;
    QQmlEngine *m_engine{nullptr};
    QQuickWindow *m_window{nullptr};
    QQuickItem *m_root{nullptr};
};

void tst_Autofill::init()
{
    m_calls.clear();
    AutofillService::setBackend(std::make_unique<RecordingBackend>(&m_calls));
    AutofillService::instance()->setRequestOnFocus(true);

    m_engine = new QQmlEngine;
    QQmlComponent component(m_engine);
    component.setData(kQml, QUrl());
    QObject *object = component.create();
    QVERIFY2(object, qPrintable(component.errorString()));
    m_root = qobject_cast<QQuickItem *>(object);
    m_window = new QQuickWindow;
    m_root->setParentItem(m_window->contentItem());
    m_window->resize(200, 200);
    m_window->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_window));
}

void tst_Autofill::cleanup()
{
    delete m_root;
    m_root = nullptr;
    delete m_window;
    m_window = nullptr;
    delete m_engine;
    m_engine = nullptr;
    AutofillService::setBackend(nullptr);
}

int tst_Autofill::idOf(const char *name) const
{
    auto *attached = qobject_cast<AutofillAttached *>(qmlAttachedPropertiesObject<Autofill>(field(name), false));
    return attached ? attached->fieldId() : 0;
}

QList<Call> tst_Autofill::callsOf(const QString &what) const
{
    QList<Call> result;
    for (const Call &c : m_calls) {
        if (c.what == what) {
            result.append(c);
        }
    }
    return result;
}

void tst_Autofill::registersFieldsWithTheirRole()
{
    const QList<Call> adds = callsOf("add");
    QCOMPARE(adds.size(), 2);
    QVERIFY(idOf("user") != 0);
    QVERIFY(idOf("pass") != 0);
    QVERIFY(idOf("user") != idOf("pass"));
    for (const Call &c : adds) {
        if (c.id == idOf("user")) {
            QCOMPARE(c.role, int(Autofill::Username));
        } else {
            QCOMPARE(c.id, idOf("pass"));
            QCOMPARE(c.role, int(Autofill::Password));
        }
    }
}

void tst_Autofill::forwardsFocusWithRequestOnFocus()
{
    field("user")->forceActiveFocus();
    field("pass")->forceActiveFocus();

    const QList<Call> in = callsOf("focusIn");
    QCOMPARE(in.size(), 2);
    QCOMPARE(in.at(0).id, idOf("user"));
    QVERIFY(in.at(0).flag);
    QCOMPARE(in.at(1).id, idOf("pass"));
    const QList<Call> out = callsOf("focusOut");
    QCOMPARE(out.size(), 1);
    QCOMPARE(out.at(0).id, idOf("user"));
    QVERIFY(!out.at(0).flag); // never request on focus loss
}

void tst_Autofill::doesNotRequestWhenRequestOnFocusIsOff()
{
    AutofillService::instance()->setRequestOnFocus(false);
    field("user")->forceActiveFocus();
    const QList<Call> in = callsOf("focusIn");
    QCOMPARE(in.size(), 1);
    QVERIFY(!in.at(0).flag);
}

void tst_Autofill::forwardsUserTextChanges()
{
    field("user")->setProperty("text", QStringLiteral("alice"));
    const QList<Call> values = callsOf("value");
    QCOMPARE(values.size(), 1);
    QCOMPARE(values.at(0).id, idOf("user"));
    QCOMPARE(values.at(0).value, QStringLiteral("alice"));
}

void tst_Autofill::appliesAutofilledValueWithoutEchoingIt()
{
    auto *attached = qobject_cast<AutofillAttached *>(qmlAttachedPropertiesObject<Autofill>(field("pass"), false));
    QSignalSpy filled(attached, &AutofillAttached::filled);

    AutofillService::instance()->deliverAutofill(idOf("pass"), QStringLiteral("s3cret"));

    QCOMPARE(field("pass")->property("text").toString(), QStringLiteral("s3cret"));
    QCOMPARE(filled.size(), 1);
    QCOMPARE(filled.at(0).at(0).toString(), QStringLiteral("s3cret"));
    QVERIFY(callsOf("value").isEmpty());
    QCOMPARE(field("user")->property("text").toString(), QString()); // other field untouched
}

void tst_Autofill::ignoresAutofillForUnknownField()
{
    AutofillService::instance()->deliverAutofill(9999, QStringLiteral("x"));
    QCOMPARE(field("user")->property("text").toString(), QString());
    QCOMPARE(field("pass")->property("text").toString(), QString());
}

void tst_Autofill::unregistersWhenItemIsDestroyed()
{
    const int id = idOf("user");
    delete field("user");
    const QList<Call> removes = callsOf("remove");
    QCOMPARE(removes.size(), 1);
    QCOMPARE(removes.at(0).id, id);
    // A late value for the destroyed field must not crash or reach the other field
    AutofillService::instance()->deliverAutofill(id, QStringLiteral("late"));
    QCOMPARE(field("pass")->property("text").toString(), QString());
}

void tst_Autofill::roleNoneUnregisters()
{
    const int id = idOf("pass");
    auto *attached = qobject_cast<AutofillAttached *>(qmlAttachedPropertiesObject<Autofill>(field("pass"), false));
    attached->setRole(Autofill::None);
    QCOMPARE(attached->fieldId(), 0);
    QCOMPARE(callsOf("remove").size(), 1);
    QCOMPARE(callsOf("remove").at(0).id, id);
    // Without a role, user input is not forwarded
    field("pass")->setProperty("text", QStringLiteral("typed"));
    QVERIFY(callsOf("value").isEmpty());
}

void tst_Autofill::commitAndCancelReachBackend()
{
    AutofillService::instance()->commit();
    AutofillService::instance()->cancel();
    QCOMPARE(callsOf("commit").size(), 1);
    QCOMPARE(callsOf("cancel").size(), 1);
}

void tst_Autofill::requestFillUsesFieldId()
{
    AutofillService::instance()->requestFill(field("pass"));
    QCOMPARE(callsOf("request").size(), 1);
    QCOMPARE(callsOf("request").at(0).id, idOf("pass"));

    QTest::ignoreMessage(QtWarningMsg, "QtQuickAutofill: requestFill() needs an item with Autofill.role set");
    AutofillService::instance()->requestFill(m_root);
    QCOMPARE(callsOf("request").size(), 1);
}

void tst_Autofill::reportsWindowRectWithScale()
{
    QQuickItem *user = field("user");
    user->setTransformOrigin(QQuickItem::TopLeft);
    user->setScale(2);
    user->setX(10);
    user->forceActiveFocus();

    const qreal dpr = m_window->devicePixelRatio();
    const QRect expected = QRectF(QPointF(10, 0) * dpr, QSizeF(200, 40) * dpr).toAlignedRect();
    const QList<Call> in = callsOf("focusIn");
    QCOMPARE(in.size(), 1);
    QCOMPARE(in.at(0).rect, expected);
}

void tst_Autofill::settingRoleOnFocusedFieldReportsFocus()
{
    field("plain")->forceActiveFocus();
    QVERIFY(callsOf("focusIn").isEmpty());

    attachedOf("plain", true)->setRole(Autofill::Email);

    const int id = idOf("plain");
    QVERIFY(id != 0);
    const QList<Call> in = callsOf("focusIn");
    QCOMPARE(in.size(), 1);
    QCOMPARE(in.at(0).id, id);
    QVERIFY(in.at(0).flag);
}

void tst_Autofill::changingRoleOnFocusedFieldReportsNewId()
{
    field("user")->forceActiveFocus();
    const int oldId = idOf("user");
    m_calls.clear();

    attachedOf("user")->setRole(Autofill::Email);

    const int newId = idOf("user");
    QVERIFY(newId != 0 && newId != oldId);
    QCOMPARE(callsOf("remove").size(), 1);
    QCOMPARE(callsOf("remove").at(0).id, oldId);
    QCOMPARE(callsOf("add").size(), 1);
    QCOMPARE(callsOf("add").at(0).role, int(Autofill::Email));
    QCOMPARE(callsOf("focusIn").size(), 1);
    QCOMPARE(callsOf("focusIn").at(0).id, newId);
}

void tst_Autofill::windowReactivationDoesNotRequestAgain()
{
    field("user")->forceActiveFocus();
    QCOMPARE(callsOf("focusIn").size(), 1);
    QVERIFY(callsOf("focusIn").at(0).flag);

    QFocusEvent out(QEvent::FocusOut, Qt::ActiveWindowFocusReason);
    QCoreApplication::sendEvent(m_window, &out);
    QFocusEvent in(QEvent::FocusIn, Qt::ActiveWindowFocusReason);
    QCoreApplication::sendEvent(m_window, &in);

    const QList<Call> ins = callsOf("focusIn");
    QCOMPARE(callsOf("focusOut").size(), 1);
    QCOMPARE(ins.size(), 2);
    QCOMPARE(ins.at(1).id, idOf("user"));
    QVERIFY(!ins.at(1).flag); // focus is reported, but no second explicit request
}

void tst_Autofill::focusSyncsBoundsAndVisibilityOfAllFields()
{
    field("user")->forceActiveFocus();

    const qreal dpr = m_window->devicePixelRatio();
    bool passBounds = false;
    for (const Call &c : callsOf("bounds")) {
        if (c.id == idOf("pass")) {
            QCOMPARE(c.rect, QRectF(QPointF(0, 30) * dpr, QSizeF(100, 20) * dpr).toAlignedRect());
            passBounds = true;
        }
    }
    QVERIFY(passBounds); // the unfocused field gets real bounds too
    QCOMPARE(callsOf("visibility").size(), 2);
    for (const Call &c : callsOf("visibility")) {
        QVERIFY(c.flag);
    }
}

void tst_Autofill::hiddenFieldReportsVisibilityAndIsNotFilled()
{
    field("pass")->setVisible(false);
    QCOMPARE(callsOf("visibility").size(), 1);
    QCOMPARE(callsOf("visibility").at(0).id, idOf("pass"));
    QVERIFY(!callsOf("visibility").at(0).flag);

    QSignalSpy filled(attachedOf("pass"), &AutofillAttached::filled);
    AutofillService::instance()->deliverAutofill(idOf("pass"), QStringLiteral("s3cret"));
    QCOMPARE(field("pass")->property("text").toString(), QString());
    QCOMPARE(filled.size(), 0);

    // Outside the window counts as hidden as well
    field("pass")->setVisible(true);
    field("pass")->setY(500);
    QVERIFY(!attachedOf("pass")->isShown());
}

void tst_Autofill::tracksBoundsOfFocusedField()
{
    field("user")->forceActiveFocus();
    m_calls.clear();

    field("user")->setY(100);
    m_window->update();

    const qreal dpr = m_window->devicePixelRatio();
    const QRect expected = QRectF(QPointF(0, 100) * dpr, QSizeF(100, 20) * dpr).toAlignedRect();
    QTRY_VERIFY(!callsOf("bounds").isEmpty());
    QCOMPARE(callsOf("bounds").last().id, idOf("user"));
    QCOMPARE(callsOf("bounds").last().rect, expected);
}

void tst_Autofill::roleNoneStopsBoundsTracking()
{
    field("user")->forceActiveFocus();
    attachedOf("user")->setRole(Autofill::None);
    m_calls.clear();

    field("user")->setY(100);
    m_window->update();
    // Give the render loop a few frames
    QTest::qWait(100);
    QVERIFY(callsOf("bounds").isEmpty());
}

void tst_Autofill::setBackendAnnouncesRegisteredFields()
{
    QList<Call> fresh;
    AutofillService::setBackend(std::make_unique<RecordingBackend>(&fresh));
    int adds = 0;
    for (const Call &c : fresh) {
        if (c.what == QLatin1String("add")) {
            ++adds;
            QVERIFY(c.id == idOf("user") || c.id == idOf("pass"));
        }
    }
    QCOMPARE(adds, 2);
    // fresh goes out of scope; cleanup() must not reach it
    AutofillService::setBackend(std::make_unique<RecordingBackend>(&m_calls));
}

QTEST_MAIN(tst_Autofill)
#include "tst_autofill.moc"
