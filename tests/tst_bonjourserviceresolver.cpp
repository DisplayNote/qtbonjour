#include <gtest/gtest.h>

#include <QtEndian>

#include <string>

#include "bonjourserviceresolver.h"
#include "fake_dns_sd.h"

using qtbonjour::BonjourRecord;
using qtbonjour::BonjourServiceResolver;

namespace {

struct Resolved
{
    BonjourRecord record;
    QString hostname;
    int port = 0;
};

struct Failed
{
    BonjourRecord record;
    DNSServiceErrorType error = kDNSServiceErr_NoError;
};

class BonjourServiceResolverTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        fakedns::reset();
        QObject::connect(&m_resolver, &BonjourServiceResolver::bonjourRecordResolved,
                         [this](const BonjourRecord & record, const QString & hostname, int port) {
                             m_resolved.append({record, hostname, port});
                         });
        QObject::connect(&m_resolver, &BonjourServiceResolver::error,
                         [this](const BonjourRecord & record, DNSServiceErrorType error) {
                             m_failed.append({record, error});
                         });
    }

    // Plays the daemon: the resolve reply, as DNSServiceProcessResult would deliver it.
    void reply(uint16_t portInNetworkOrder, const std::string & txt = {},
               DNSServiceErrorType errorCode = kDNSServiceErr_NoError)
    {
        const fakedns::ResolveCall resolve = fakedns::lastResolve();
        ASSERT_NE(resolve.callback, nullptr);
        resolve.callback(resolve.ref, 0, 0, errorCode, "Room 1._airplay._tcp.local.",
                         "room-1.local.", portInNetworkOrder, static_cast<uint16_t>(txt.size()),
                         reinterpret_cast<const unsigned char *>(txt.data()), resolve.context);
    }

    static BonjourRecord room1()
    {
        return BonjourRecord(QStringLiteral("Room 1"), QStringLiteral("_airplay._tcp"),
                             QStringLiteral("local."));
    }

    BonjourServiceResolver m_resolver;
    QList<Resolved> m_resolved;
    QList<Failed> m_failed;
};

TEST_F(BonjourServiceResolverTest, AsksTheDaemonToResolveTheRecord)
{
    m_resolver.resolveBonjourRecord(room1());

    const fakedns::ResolveCall resolve = fakedns::lastResolve();
    EXPECT_EQ(resolve.count, 1);
    EXPECT_EQ(resolve.name, "Room 1");
    EXPECT_EQ(resolve.regtype, "_airplay._tcp");
    EXPECT_EQ(resolve.domain, "local.");
    EXPECT_TRUE(m_failed.isEmpty());
    EXPECT_EQ(fakedns::liveRefCount(), 1);
}

TEST_F(BonjourServiceResolverTest, ReportsTheHostAndThePortInHostByteOrder)
{
    m_resolver.resolveBonjourRecord(room1());

    reply(qToBigEndian<quint16>(7000));

    ASSERT_EQ(m_resolved.size(), 1);
    EXPECT_EQ(m_resolved.at(0).record, room1());
    EXPECT_EQ(m_resolved.at(0).hostname, "room-1.local.");
    EXPECT_EQ(m_resolved.at(0).port, 7000);
    EXPECT_TRUE(m_resolved.at(0).record.txtRecord.isEmpty());
}

TEST_F(BonjourServiceResolverTest, ParsesTheTxtRecordIntoKeysAndValues)
{
    m_resolver.resolveBonjourRecord(room1());

    reply(qToBigEndian<quint16>(7000),
          fakedns::txtRecord({"model=AppleTV3,2", "pw=", "flag", "name=Caf\xC3\xA9"}));

    ASSERT_EQ(m_resolved.size(), 1);
    const QMap<QString, QString> & txt = m_resolved.at(0).record.txtRecord;
    EXPECT_EQ(txt.size(), 4);
    EXPECT_EQ(txt.value(QStringLiteral("model")), "AppleTV3,2");
    EXPECT_TRUE(txt.contains(QStringLiteral("pw")));
    EXPECT_EQ(txt.value(QStringLiteral("pw")), "");
    EXPECT_TRUE(txt.contains(QStringLiteral("flag")));
    EXPECT_EQ(txt.value(QStringLiteral("name")), QString::fromUtf8("Caf\xC3\xA9"));
}

TEST_F(BonjourServiceResolverTest, ReleasesTheResolveOnceAnswered)
{
    m_resolver.resolveBonjourRecord(room1());

    reply(qToBigEndian<quint16>(7000));

    EXPECT_EQ(fakedns::liveRefCount(), 0);
}

TEST_F(BonjourServiceResolverTest, ReportsAResolveThatTheDaemonRejects)
{
    fakedns::setResolveResult(kDNSServiceErr_BadParam);

    m_resolver.resolveBonjourRecord(room1());

    ASSERT_EQ(m_failed.size(), 1);
    EXPECT_EQ(m_failed.at(0).record, room1());
    EXPECT_EQ(m_failed.at(0).error, kDNSServiceErr_BadParam);
}

TEST_F(BonjourServiceResolverTest, ReportsInvalidAndReleasesWhenTheDaemonSocketIsMissing)
{
    fakedns::setSockFdFails(true);

    m_resolver.resolveBonjourRecord(room1());

    ASSERT_EQ(m_failed.size(), 1);
    EXPECT_EQ(m_failed.at(0).error, kDNSServiceErr_Invalid);
    EXPECT_EQ(fakedns::liveRefCount(), 0);
}

TEST_F(BonjourServiceResolverTest, ReportsAnErrorReplyWithoutResolving)
{
    m_resolver.resolveBonjourRecord(room1());

    reply(qToBigEndian<quint16>(7000), {}, kDNSServiceErr_NoSuchName);

    EXPECT_TRUE(m_resolved.isEmpty());
    ASSERT_EQ(m_failed.size(), 1);
    EXPECT_EQ(m_failed.at(0).error, kDNSServiceErr_NoSuchName);
    // Current behaviour: the pending resolve is forgotten but never released.
    EXPECT_EQ(fakedns::liveRefCount(), 1);
}

TEST_F(BonjourServiceResolverTest, ReportsAnUnreadableTxtRecordWithNoErrorCode)
{
    fakedns::setTxtItemResult(kDNSServiceErr_Invalid);
    m_resolver.resolveBonjourRecord(room1());

    reply(qToBigEndian<quint16>(7000), fakedns::txtRecord({"model=AppleTV3,2"}));

    EXPECT_TRUE(m_resolved.isEmpty());
    ASSERT_EQ(m_failed.size(), 1);
    // Current behaviour: the reply's errorCode (NoError) is reported, not the TXT failure.
    EXPECT_EQ(m_failed.at(0).error, kDNSServiceErr_NoError);
}

TEST_F(BonjourServiceResolverTest, ProcessesTheDaemonReplyForAKnownSocketOnly)
{
    m_resolver.resolveBonjourRecord(room1());
    const int sockfd = fakedns::sockFdOf(fakedns::lastResolve().ref);
    fakedns::setProcessResult(kDNSServiceErr_ServiceNotRunning);

    ASSERT_TRUE(QMetaObject::invokeMethod(&m_resolver, "bonjourSocketReadyRead",
                                          Qt::DirectConnection, Q_ARG(int, sockfd + 1)));
    EXPECT_EQ(fakedns::processResultCount(), 0);

    ASSERT_TRUE(QMetaObject::invokeMethod(&m_resolver, "bonjourSocketReadyRead",
                                          Qt::DirectConnection, Q_ARG(int, sockfd)));
    EXPECT_EQ(fakedns::processResultCount(), 1);
    ASSERT_EQ(m_failed.size(), 1);
    EXPECT_EQ(m_failed.at(0).error, kDNSServiceErr_ServiceNotRunning);
}

TEST(BonjourServiceResolverLifetimeTest, ReleasesPendingResolvesOnDestruction)
{
    fakedns::reset();
    {
        BonjourServiceResolver resolver;
        resolver.resolveBonjourRecord(BonjourRecord("Room 1", "_airplay._tcp", "local."));
        resolver.resolveBonjourRecord(BonjourRecord("Room 2", "_airplay._tcp", "local."));
        EXPECT_EQ(fakedns::liveRefCount(), 2);
    }
    EXPECT_EQ(fakedns::liveRefCount(), 0);
}

} // namespace
