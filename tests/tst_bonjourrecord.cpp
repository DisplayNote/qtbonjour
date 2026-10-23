#include <gtest/gtest.h>

#include <QMetaType>

#include "bonjourrecord.h"

using qtbonjour::BonjourRecord;

namespace {

TEST(BonjourRecordTest, StartsEmpty)
{
    BonjourRecord record;

    EXPECT_TRUE(record.serviceName.isEmpty());
    EXPECT_TRUE(record.registeredType.isEmpty());
    EXPECT_TRUE(record.replyDomain.isEmpty());
    EXPECT_TRUE(record.txtRecord.isEmpty());
    EXPECT_EQ(record.parent(), nullptr);
}

TEST(BonjourRecordTest, KeepsTheNameTypeAndDomainItIsGiven)
{
    QObject owner;
    BonjourRecord record(QStringLiteral("Room 1"), QStringLiteral("_airplay._tcp"),
                         QStringLiteral("local."), &owner);

    EXPECT_EQ(record.serviceName, "Room 1");
    EXPECT_EQ(record.registeredType, "_airplay._tcp");
    EXPECT_EQ(record.replyDomain, "local.");
    EXPECT_EQ(record.parent(), &owner);
}

TEST(BonjourRecordTest, DecodesCStringsAsUtf8)
{
    BonjourRecord record("Caf\xC3\xA9", "_raop._tcp", "local.");

    EXPECT_EQ(record.serviceName, QString::fromUtf8("Caf\xC3\xA9"));
    EXPECT_EQ(record.serviceName.size(), 4);
    EXPECT_EQ(record.registeredType, "_raop._tcp");
    EXPECT_EQ(record.replyDomain, "local.");
}

TEST(BonjourRecordTest, CopyTakesEveryFieldAndTheSameParent)
{
    QObject owner;
    BonjourRecord original(QStringLiteral("Room 1"), QStringLiteral("_airplay._tcp"),
                           QStringLiteral("local."), &owner);
    original.txtRecord.insert(QStringLiteral("model"), QStringLiteral("AppleTV3,2"));

    // Current behaviour: the copy becomes another child of the original's parent.
    auto * copy = new BonjourRecord(original);

    EXPECT_EQ(copy->serviceName, "Room 1");
    EXPECT_EQ(copy->registeredType, "_airplay._tcp");
    EXPECT_EQ(copy->replyDomain, "local.");
    EXPECT_EQ(copy->txtRecord.value(QStringLiteral("model")), "AppleTV3,2");
    EXPECT_EQ(copy->parent(), &owner);
    delete copy;
}

TEST(BonjourRecordTest, EqualityIgnoresTheTxtRecord)
{
    BonjourRecord a(QStringLiteral("Room 1"), QStringLiteral("_airplay._tcp"), QStringLiteral("local."));
    BonjourRecord b(QStringLiteral("Room 1"), QStringLiteral("_airplay._tcp"), QStringLiteral("local."));
    a.txtRecord.insert(QStringLiteral("pw"), QStringLiteral("true"));

    EXPECT_TRUE(a == b);
}

TEST(BonjourRecordTest, DiffersWhenNameTypeOrDomainDiffers)
{
    const BonjourRecord base(QStringLiteral("Room 1"), QStringLiteral("_airplay._tcp"), QStringLiteral("local."));

    EXPECT_FALSE(base == BonjourRecord(QStringLiteral("Room 2"), QStringLiteral("_airplay._tcp"), QStringLiteral("local.")));
    EXPECT_FALSE(base == BonjourRecord(QStringLiteral("Room 1"), QStringLiteral("_raop._tcp"), QStringLiteral("local.")));
    EXPECT_FALSE(base == BonjourRecord(QStringLiteral("Room 1"), QStringLiteral("_airplay._tcp"), QStringLiteral("example.")));
}

TEST(BonjourRecordTest, AssignmentCopiesEveryFieldButKeepsItsOwnParent)
{
    QObject owner;
    QObject otherOwner;
    BonjourRecord source(QStringLiteral("Room 1"), QStringLiteral("_airplay._tcp"),
                         QStringLiteral("local."), &owner);
    source.txtRecord.insert(QStringLiteral("vv"), QStringLiteral("2"));
    BonjourRecord target(&otherOwner);

    target = source;

    EXPECT_EQ(target.serviceName, "Room 1");
    EXPECT_EQ(target.registeredType, "_airplay._tcp");
    EXPECT_EQ(target.replyDomain, "local.");
    EXPECT_EQ(target.txtRecord.value(QStringLiteral("vv")), "2");
    EXPECT_EQ(target.parent(), &otherOwner);
}

TEST(BonjourRecordTest, IsRegisteredAsAMetaTypeForQueuedSignals)
{
    EXPECT_TRUE(QMetaType::fromName("BonjourRecord").isValid());
}

} // namespace
