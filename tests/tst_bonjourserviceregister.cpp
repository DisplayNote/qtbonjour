#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QTest>
#include <QtEndian>

#include <atomic>
#include <memory>
#include <mutex>

#include "bonjourserviceregister.h"
#include "fake_dns_sd.h"

using qtbonjour::BonjourRecord;
using qtbonjour::BonjourServiceRegister;

namespace {

// What the register reported. Its signals come from its own worker thread, so
// everything is recorded under a lock.
struct Observed
{
    std::mutex mutex;
    QList<DNSServiceErrorType> errors;
    QList<QStringList> registered;   // name, type, domain
    std::atomic<bool> destroyed{false};

    int events()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return errors.size() + registered.size();
    }
};

// Drives one BonjourServiceRegister through the life the library gives it:
// created with new, registerService() starts its thread, unregisterService()
// stops the thread and the object deletes itself.
class RegisterRun
{
public:
    explicit RegisterRun(const BonjourRecord & record, quint16 port = 7000, int interfaceIndex = 3)
        : m_observed(std::make_shared<Observed>())
        , m_register(new BonjourServiceRegister(record, port, interfaceIndex))
    {
        auto observed = m_observed;
        QObject::connect(m_register, &BonjourServiceRegister::error,
                         [observed](DNSServiceErrorType error) {
                             std::lock_guard<std::mutex> lock(observed->mutex);
                             observed->errors.append(error);
                         });
        QObject::connect(m_register, &BonjourServiceRegister::serviceRegistered,
                         [observed](const BonjourRecord & r) {
                             std::lock_guard<std::mutex> lock(observed->mutex);
                             observed->registered.append({r.serviceName, r.registeredType, r.replyDomain});
                         });
        QObject::connect(m_register, &QObject::destroyed,
                         [observed] { observed->destroyed = true; });
    }

    ~RegisterRun() { stop(); }

    // Starts the worker thread and waits until the register reports something.
    bool start()
    {
        m_register->registerService();
        return QTest::qWaitFor([this] { return m_observed->events() > 0; }, 5000);
    }

    // Starts the worker thread and waits until it has handed its call to the daemon.
    bool startAndWaitForTheDaemon()
    {
        m_register->registerService();
        return QTest::qWaitFor([] { return fakedns::processResultCount() > 0; }, 5000);
    }

    bool stop()
    {
        if (m_observed->destroyed)
            return true;
        m_register->unregisterService();
        const bool destroyed = QTest::qWaitFor([this] { return m_observed->destroyed.load(); }, 5000);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);   // the QThread
        return destroyed;
    }

    BonjourServiceRegister * get() const { return m_register; }

    QList<DNSServiceErrorType> errors()
    {
        std::lock_guard<std::mutex> lock(m_observed->mutex);
        return m_observed->errors;
    }

    QList<QStringList> registered()
    {
        std::lock_guard<std::mutex> lock(m_observed->mutex);
        return m_observed->registered;
    }

private:
    std::shared_ptr<Observed> m_observed;
    BonjourServiceRegister * m_register;
};

class BonjourServiceRegisterTest : public ::testing::Test
{
protected:
    void SetUp() override { fakedns::reset(); }

    static BonjourRecord room1()
    {
        return BonjourRecord(QStringLiteral("Room 1"), QStringLiteral("_airplay._tcp"),
                             QStringLiteral("local."));
    }

    static void daemonAccepts()
    {
        fakedns::setRegisterReply({kDNSServiceErr_NoError, "Room 1 (2)", "_airplay._tcp.", "local."});
    }
};

TEST_F(BonjourServiceRegisterTest, RegistersOnTheGivenInterfaceWithThePortInNetworkByteOrder)
{
    daemonAccepts();
    RegisterRun run(room1(), 7000, 3);

    ASSERT_TRUE(run.start());

    const fakedns::RegisterCall call = fakedns::lastRegister();
    EXPECT_EQ(call.count, 1);
    EXPECT_EQ(call.name, "Room 1");
    EXPECT_EQ(call.regtype, "_airplay._tcp");
    EXPECT_EQ(call.interfaceIndex, 3u);
    EXPECT_EQ(call.port, qToBigEndian<quint16>(7000));
    EXPECT_TRUE(run.errors().isEmpty());
}

TEST_F(BonjourServiceRegisterTest, ReportsTheNameTheDaemonRegistered)
{
    daemonAccepts();
    RegisterRun run(room1());

    ASSERT_TRUE(run.start());

    ASSERT_EQ(run.registered().size(), 1);
    EXPECT_EQ(run.registered().at(0), QStringList({"Room 1 (2)", "_airplay._tcp.", "local."}));
    const BonjourRecord record = run.get()->registeredRecord();
    EXPECT_EQ(record.serviceName, "Room 1 (2)");
    EXPECT_EQ(record.registeredType, "_airplay._tcp.");
    EXPECT_EQ(record.replyDomain, "local.");
}

TEST_F(BonjourServiceRegisterTest, EncodesTheTxtRecordAsLengthPrefixedPairs)
{
    daemonAccepts();
    BonjourRecord record = room1();
    record.txtRecord.insert(QStringLiteral("vv"), QStringLiteral("2"));
    record.txtRecord.insert(QStringLiteral("am"), QStringLiteral("AppleTV3,2"));
    record.txtRecord.insert(QStringLiteral("pw"), QString());
    RegisterRun run(record);

    ASSERT_TRUE(run.start());

    const fakedns::RegisterCall call = fakedns::lastRegister();
    EXPECT_TRUE(call.txtRecordWasCreated);
    // QMap order, i.e. sorted by key.
    EXPECT_EQ(call.txt, fakedns::txtRecord({"am=AppleTV3,2", "pw=", "vv=2"}));
}

TEST_F(BonjourServiceRegisterTest, CutsNonAsciiTxtValuesToTheirCharacterCount)
{
    daemonAccepts();
    BonjourRecord record = room1();
    record.txtRecord.insert(QStringLiteral("name"), QString::fromUtf8("Caf\xC3\xA9"));
    RegisterRun run(record);

    ASSERT_TRUE(run.start());

    // Current behaviour: the size passed is QString::length() (4), not the
    // UTF-8 byte count (5), so the last byte of "é" is lost.
    EXPECT_EQ(fakedns::lastRegister().txt, fakedns::txtRecord({"name=Caf\xC3"}));
}

TEST_F(BonjourServiceRegisterTest, RegistersWithoutTxtThroughATxtRecordThatWasNeverCreated)
{
    daemonAccepts();
    RegisterRun run(room1());

    ASSERT_TRUE(run.start());

    // Current behaviour: with no TXT entries, TXTRecordGetLength() and
    // TXTRecordGetBytesPtr() still read the uninitialised TXTRecordRef.
    const fakedns::RegisterCall call = fakedns::lastRegister();
    EXPECT_FALSE(call.txtRecordWasCreated);
    EXPECT_TRUE(call.txt.empty());
}

TEST_F(BonjourServiceRegisterTest, ReportsAnInvalidTxtKeyWithoutRegistering)
{
    BonjourRecord record = room1();
    record.txtRecord.insert(QStringLiteral("a=b"), QStringLiteral("1"));
    RegisterRun run(record);

    ASSERT_TRUE(run.start());

    EXPECT_EQ(run.errors(), QList<DNSServiceErrorType>({kDNSServiceErr_Invalid}));
    EXPECT_EQ(fakedns::lastRegister().count, 0);
}

TEST_F(BonjourServiceRegisterTest, ReportsARegistrationTheDaemonRejects)
{
    fakedns::setRegisterResult(kDNSServiceErr_BadParam);
    RegisterRun run(room1());

    ASSERT_TRUE(run.start());

    EXPECT_EQ(run.errors(), QList<DNSServiceErrorType>({kDNSServiceErr_BadParam}));
    EXPECT_EQ(fakedns::processResultCount(), 0);
}

TEST_F(BonjourServiceRegisterTest, ReportsAFailureToReadTheDaemonReply)
{
    fakedns::setProcessResult(kDNSServiceErr_ServiceNotRunning);
    RegisterRun run(room1());

    ASSERT_TRUE(run.start());

    EXPECT_EQ(run.errors(), QList<DNSServiceErrorType>({kDNSServiceErr_ServiceNotRunning}));
}

TEST_F(BonjourServiceRegisterTest, ReportsAnErrorReplyAndKeepsNoRegisteredRecord)
{
    fakedns::setRegisterReply({kDNSServiceErr_NameConflict, "", "", ""});
    RegisterRun run(room1());

    ASSERT_TRUE(run.start());

    EXPECT_EQ(run.errors(), QList<DNSServiceErrorType>({kDNSServiceErr_NameConflict}));
    EXPECT_TRUE(run.registered().isEmpty());
    EXPECT_TRUE(run.get()->registeredRecord().serviceName.isEmpty());
}

TEST_F(BonjourServiceRegisterTest, DeletesItselfAndReleasesTheRegistrationWhenUnregistered)
{
    RegisterRun run(room1());
    ASSERT_TRUE(run.startAndWaitForTheDaemon());
    EXPECT_EQ(fakedns::liveRefCount(), 1);

    EXPECT_TRUE(run.stop());

    EXPECT_EQ(fakedns::liveRefCount(), 0);
}

} // namespace
