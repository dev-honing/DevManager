#include "service/service_lifecycle.h"

#include <QtTest>

using namespace dm;

class TstServiceLifecycle : public QObject {
    Q_OBJECT
private slots:
    void runsMappedCommandAndCapturesOutput();
    void reportsMissingLifecycle();
    void startDetachedReturnsImmediately();
    void surfacesStderrReasonOnFailure();
};

static ServiceSpec fakeSpec()
{
    ServiceSpec s;
    s.id = "fake";
    s.name = "Fake";
    s.lifecycle.exe = "cmd";
    s.lifecycle.statusArgs = {"/c", "echo", "STATUS_OK"};
    s.lifecycle.stopArgs = {"/c", "echo", "STOP_OK"};
    s.lifecycle.startArgs = {"/c", "echo", "START_OK"};
    s.lifecycle.startDetached = false;
    return s;
}

void TstServiceLifecycle::runsMappedCommandAndCapturesOutput()
{
#ifndef Q_OS_WIN
    QSKIP("cmd-based fixture is Windows-only");
#endif
    const LifecycleResult st = ServiceLifecycle::run(fakeSpec(), LifecycleOp::Status);
    QVERIFY2(st.ok, st.error.toUtf8());
    QVERIFY(st.output.contains("STATUS_OK"));
    QCOMPARE(st.serviceId, QStringLiteral("fake"));

    const LifecycleResult sp = ServiceLifecycle::run(fakeSpec(), LifecycleOp::Stop);
    QVERIFY(sp.ok);
    QVERIFY(sp.output.contains("STOP_OK"));
}

void TstServiceLifecycle::reportsMissingLifecycle()
{
    ServiceSpec bare;
    bare.id = "bare";
    const LifecycleResult r = ServiceLifecycle::run(bare, LifecycleOp::Stop);
    QVERIFY(!r.ok);
    QVERIFY(!r.error.isEmpty());
}

void TstServiceLifecycle::startDetachedReturnsImmediately()
{
#ifndef Q_OS_WIN
    QSKIP("cmd-based fixture is Windows-only");
#endif
    ServiceSpec s = fakeSpec();
    s.lifecycle.startDetached = true;
    QElapsedTimer t;
    t.start();
    const LifecycleResult r = ServiceLifecycle::run(s, LifecycleOp::Start);
    QVERIFY(r.ok);
    QVERIFY(t.elapsed() < 3000);
}

void TstServiceLifecycle::surfacesStderrReasonOnFailure()
{
#ifndef Q_OS_WIN
    QSKIP("cmd-based fixture is Windows-only");
#endif
    // a command that prints a reason to stderr and exits non-zero -- the
    // reason must reach LifecycleResult.error, not just "exit 1"
    ServiceSpec s = fakeSpec();
    s.lifecycle.stopArgs = {"/c", "echo not-supported-here 1>&2 & exit /b 1"};
    const LifecycleResult r = ServiceLifecycle::run(s, LifecycleOp::Stop);
    QVERIFY(!r.ok);
    QVERIFY2(r.error.contains("not-supported-here"), r.error.toUtf8());
}

QTEST_MAIN(TstServiceLifecycle)
#include "tst_service_lifecycle.moc"
