#include "bonjourrecord.h"

namespace {
static const struct RegisterMetaTypes
{
   inline RegisterMetaTypes()
   {
       qRegisterMetaType<BonjourRecord>("BonjourRecord");
   }
} _registerMetaTypes;
}

BonjourRecord::BonjourRecord(QObject * parent) :
    QObject(parent)
{
}

BonjourRecord::BonjourRecord(const QString &name, const QString &regType, const QString &domain, QObject * parent) :
    QObject(parent)
  , serviceName(name)
  , registeredType(regType)
  , replyDomain(domain)
{
}

BonjourRecord::BonjourRecord(const char *name, const char *regType, const char *domain, QObject * parent) :
    QObject(parent)
  , serviceName(QString::fromUtf8(name))
  , registeredType(QString::fromUtf8(regType))
  , replyDomain(QString::fromUtf8(domain))
{
}

BonjourRecord::BonjourRecord(const BonjourRecord & bonjourRecord) :
    QObject(bonjourRecord.parent())
  , serviceName(bonjourRecord.serviceName)
  , registeredType(bonjourRecord.registeredType)
  , replyDomain(bonjourRecord.replyDomain)
{
}

BonjourRecord::~BonjourRecord()
{
    txtRecord.clear();
}

bool BonjourRecord::operator==(const BonjourRecord &other) const
{
    return serviceName == other.serviceName
            && registeredType == other.registeredType
            && replyDomain == other.replyDomain;
}

BonjourRecord BonjourRecord::operator=(const BonjourRecord &other)
{
    serviceName    = other.serviceName;
    registeredType = other.registeredType;
    replyDomain    = other.replyDomain;

    return *this;
}
