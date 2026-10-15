#include <QtTest>
#include "../../src/common/t55xxrecovery.h"

class T55xxRecoveryTest : public QObject
{
    Q_OBJECT
private slots:
    void confirmedPassword()
    {
        QCOMPARE(T55xxRecovery::confirmedPassword("[=] testing 5469616E\n"), QString());
        QCOMPARE(T55xxRecovery::confirmedPassword("[=] Password.......... 5469616E\n"), QString());
        QCOMPARE(T55xxRecovery::confirmedPassword("[+] found valid password: [ 5469616E ]\r\n"), QString("5469616E"));
        QCOMPARE(T55xxRecovery::confirmedPassword("\x1b[32m[+]\x1b[0m found valid password: [ abcdef01 ]\n"), QString("ABCDEF01"));
        QCOMPARE(T55xxRecovery::confirmedPassword("[+] found valid password: [ 5469616E;wipe ]\n"), QString());
        QCOMPARE(T55xxRecovery::confirmedPassword("[+] found valid password: [ 123456789 ]\n"), QString());
        QCOMPARE(T55xxRecovery::confirmedPassword("[GUI] > echo [+] found valid password: [ 5469616E ]\n"), QString());
    }

    void resetVerification()
    {
        QVERIFY(T55xxRecovery::passwordCleared("[=] Block0............ 000880E0 (auto detect)\n[=] Password set...... No\n"));
        QVERIFY(!T55xxRecovery::passwordCleared("[=] Block0............ 00148050 (auto detect)\n[=] Password set...... Yes\n"));
        QVERIFY(!T55xxRecovery::passwordCleared("[=] Password set...... No\n"));
        QVERIFY(!T55xxRecovery::passwordCleared("[=] Block0............ 00148000 (user set)\n[=] Password set...... No\n"));
        QVERIFY(!T55xxRecovery::passwordCleared("Could not detect modulation automatically"));
    }
};

QTEST_APPLESS_MAIN(T55xxRecoveryTest)
#include "tst_t55xxrecovery.moc"
