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
};

class RecordingBackend : public AutofillBackend
{
public:
    explicit RecordingBackend(QList<Call> *calls) : m_calls(calls) {}
    bool isAvailable() const override { return true; }
    void addField(int id, int role) override { m_calls->append({"add", id, role}); }
    void removeField(int id) override { m_calls->append({"remove", id}); }
    void focusChanged(int id, bool focused, const QRect &, bool requestFill) override
    {
        m_calls->append({focused ? "focusIn" : "focusOut", id, 0, requestFill});
    }
    void valueChanged(int id, const QString &value) override { m_calls->append({"value", id, 0, false, value}); }
    void commit() override { m_calls->append({"commit"}); }
    void cancel() override { m_calls->append({"cancel"}); }
    void requestFill(int id, const QRect &) override { m_calls->append({"request", id}); }

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

private:
    QQuickItem *field(const char *name) const { return m_root->property(name).value<QQuickItem *>(); }
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

QTEST_MAIN(tst_Autofill)
#include "tst_autofill.moc"
