#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QSocketNotifier>

#include "bonjourservicebrowser.h"
#include "fake_dns_sd.h"

using qtbonjour::BonjourRecord;
using qtbonjour::BonjourServiceBrowser;

namespace {

class BonjourServiceBrowserTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        fakedns::reset();
        QObject::connect(&m_browser, &BonjourServiceBrowser::currentBonjourRecordsChanged,
                         [this](const QList<BonjourRecord> & list) {
                             m_published.append(names(list));
                         });
    }

    // Plays the daemon: one browse reply, as DNSServiceProcessResult would deliver it.
    void reply(DNSServiceFlags flags, const char * name,
               DNSServiceErrorType errorCode = kDNSServiceErr_NoError)
    {
        const fakedns::BrowseCall browse = fakedns::lastBrowse();
        ASSERT_NE(browse.callback, nullptr);
        browse.callback(browse.ref, flags, 0, errorCode, name, "_airplay._tcp.", "local.",
                        browse.context);
    }

    static QStringList names(const QList<BonjourRecord> & list)
    {
        QStringList result;
        for (const BonjourRecord & record : list)
            result << record.serviceName;
        return result;
    }

    BonjourServiceBrowser m_browser;
    QList<QStringList> m_published;   // one entry per currentBonjourRecordsChanged
};

TEST_F(BonjourServiceBrowserTest, BrowsesForTheRequestedTypeAndWatchesTheDaemonSocket)
{
    QSignalSpy errors(&m_browser, &BonjourServiceBrowser::error);

    m_browser.browseForServiceType(QStringLiteral("_airplay._tcp"));

    const fakedns::BrowseCall browse = fakedns::lastBrowse();
    EXPECT_EQ(browse.count, 1);
    EXPECT_EQ(browse.regtype, "_airplay._tcp");
    EXPECT_EQ(browse.context, &m_browser);
    EXPECT_EQ(errors.count(), 0);
    const auto notifiers = m_browser.findChildren<QSocketNotifier *>();
    ASSERT_EQ(notifiers.size(), 1);
    EXPECT_EQ(notifiers.first()->socket(), fakedns::sockFdOf(browse.ref));
}

TEST_F(BonjourServiceBrowserTest, ReportsABrowseThatTheDaemonRejects)
{
    fakedns::setBrowseResult(kDNSServiceErr_ServiceNotRunning);
    QSignalSpy errors(&m_browser, &BonjourServiceBrowser::error);

    m_browser.browseForServiceType(QStringLiteral("_airplay._tcp"));

    ASSERT_EQ(errors.count(), 1);
    EXPECT_EQ(errors.at(0).at(0).toInt(), kDNSServiceErr_ServiceNotRunning);
    EXPECT_TRUE(m_browser.findChildren<QSocketNotifier *>().isEmpty());
}

TEST_F(BonjourServiceBrowserTest, ReportsInvalidWhenTheDaemonSocketIsMissing)
{
    fakedns::setSockFdFails(true);
    QSignalSpy errors(&m_browser, &BonjourServiceBrowser::error);

    m_browser.browseForServiceType(QStringLiteral("_airplay._tcp"));

    ASSERT_EQ(errors.count(), 1);
    EXPECT_EQ(errors.at(0).at(0).toInt(), kDNSServiceErr_Invalid);
    EXPECT_TRUE(m_browser.findChildren<QSocketNotifier *>().isEmpty());
}

TEST_F(BonjourServiceBrowserTest, PublishesEachServiceThatAppears)
{
    m_browser.browseForServiceType(QStringLiteral("_airplay._tcp"));

    reply(kDNSServiceFlagsAdd, "Room 1");
    reply(kDNSServiceFlagsAdd, "Room 2");

    ASSERT_EQ(m_published.size(), 2);
    EXPECT_EQ(m_published.at(0), QStringList({"Room 1"}));
    EXPECT_EQ(m_published.at(1), QStringList({"Room 1", "Room 2"}));
    EXPECT_EQ(names(m_browser.currentRecords()), QStringList({"Room 1", "Room 2"}));
}

TEST_F(BonjourServiceBrowserTest, KeepsOneRecordForAServiceAnnouncedTwice)
{
    m_browser.browseForServiceType(QStringLiteral("_airplay._tcp"));

    reply(kDNSServiceFlagsAdd, "Room 1");
    reply(kDNSServiceFlagsAdd, "Room 1");

    // Still published on the duplicate, with the same list.
    ASSERT_EQ(m_published.size(), 2);
    EXPECT_EQ(m_published.at(1), QStringList({"Room 1"}));
}

TEST_F(BonjourServiceBrowserTest, DropsAServiceThatGoesAway)
{
    m_browser.browseForServiceType(QStringLiteral("_airplay._tcp"));
    reply(kDNSServiceFlagsAdd, "Room 1");
    reply(kDNSServiceFlagsAdd, "Room 2");

    reply(0, "Room 1");

    EXPECT_EQ(m_published.last(), QStringList({"Room 2"}));
    EXPECT_EQ(names(m_browser.currentRecords()), QStringList({"Room 2"}));
}

TEST_F(BonjourServiceBrowserTest, IgnoresTheRemovalOfAnUnknownServiceButStillPublishes)
{
    m_browser.browseForServiceType(QStringLiteral("_airplay._tcp"));
    reply(kDNSServiceFlagsAdd, "Room 1");

    reply(0, "Room 9");

    ASSERT_EQ(m_published.size(), 2);
    EXPECT_EQ(m_published.last(), QStringList({"Room 1"}));
}

TEST_F(BonjourServiceBrowserTest, WaitsForTheLastReplyOfABatchBeforePublishing)
{
    m_browser.browseForServiceType(QStringLiteral("_airplay._tcp"));

    reply(kDNSServiceFlagsAdd | kDNSServiceFlagsMoreComing, "Room 1");
    reply(kDNSServiceFlagsAdd | kDNSServiceFlagsMoreComing, "Room 2");
    EXPECT_TRUE(m_published.isEmpty());
    reply(kDNSServiceFlagsAdd, "Room 3");

    ASSERT_EQ(m_published.size(), 1);
    EXPECT_EQ(m_published.at(0), QStringList({"Room 1", "Room 2", "Room 3"}));
}

TEST_F(BonjourServiceBrowserTest, ReportsAnErrorReplyAndKeepsTheRecords)
{
    m_browser.browseForServiceType(QStringLiteral("_airplay._tcp"));
    reply(kDNSServiceFlagsAdd, "Room 1");
    QSignalSpy errors(&m_browser, &BonjourServiceBrowser::error);

    reply(kDNSServiceFlagsAdd, "Room 2", kDNSServiceErr_Unknown);

    ASSERT_EQ(errors.count(), 1);
    EXPECT_EQ(errors.at(0).at(0).toInt(), kDNSServiceErr_Unknown);
    EXPECT_EQ(m_published.size(), 1);
    EXPECT_EQ(names(m_browser.currentRecords()), QStringList({"Room 1"}));
}

TEST_F(BonjourServiceBrowserTest, ProcessesTheDaemonReplyWhenTheSocketIsReadable)
{
    m_browser.browseForServiceType(QStringLiteral("_airplay._tcp"));
    QSignalSpy errors(&m_browser, &BonjourServiceBrowser::error);

    ASSERT_TRUE(QMetaObject::invokeMethod(&m_browser, "bonjourSocketReadyRead", Qt::DirectConnection));
    fakedns::setProcessResult(kDNSServiceErr_ServiceNotRunning);
    ASSERT_TRUE(QMetaObject::invokeMethod(&m_browser, "bonjourSocketReadyRead", Qt::DirectConnection));

    EXPECT_EQ(fakedns::processResultCount(), 2);
    ASSERT_EQ(errors.count(), 1);
    EXPECT_EQ(errors.at(0).at(0).toInt(), kDNSServiceErr_ServiceNotRunning);
}

TEST(BonjourServiceBrowserLifetimeTest, ReleasesTheBrowseOnDestruction)
{
    fakedns::reset();
    {
        BonjourServiceBrowser browser;
        browser.browseForServiceType(QStringLiteral("_airplay._tcp"));
        EXPECT_EQ(fakedns::liveRefCount(), 1);
    }
    EXPECT_EQ(fakedns::liveRefCount(), 0);
}

} // namespace
