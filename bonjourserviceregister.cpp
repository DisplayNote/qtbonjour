/*
Copyright (c) 2007, Trenton Schulz
Copyright (c) 2009-2011, Stefan Hacker

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 3. The name of the author may not be used to endorse or promote products
    derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Own includes
#include "bonjourserviceregister.h"

// Qt includes
#include <QtCore/QSocketNotifier>
#include <QtCore/QtEndian>

BonjourServiceRegister::BonjourServiceRegister(const BonjourRecord &record, quint16 servicePort, int niIndex, QObject *parent) :
    QObject(parent)
  , dnssref(nullptr)
  , m_record(record)
  , m_servicePort(servicePort)
  , m_niIndex(niIndex)
  , m_thread(nullptr)
{
    m_thread = new QThread();
    this->moveToThread(m_thread);

    connect(m_thread, &QThread::started, this, &BonjourServiceRegister::onStarted);
    connect(m_thread, &QThread::finished, this, &BonjourServiceRegister::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QThread::deleteLater);
}

BonjourServiceRegister::~BonjourServiceRegister()
{
    if (dnssref != nullptr) {
        DNSServiceRefDeallocate(dnssref);
    }
    dnssref = nullptr;
}

void BonjourServiceRegister::registerService()
{
    if (m_thread == nullptr) return;
    m_thread->start();
}

void BonjourServiceRegister::unregisterService()
{
    if (m_thread == nullptr) return;
    m_thread->exit();
}

BonjourRecord BonjourServiceRegister::registeredRecord() const {
    return finalRecord;
}

void BonjourServiceRegister::onStarted()
{
    if (dnssref != nullptr)
    {
        qWarning("Warning: Already registered a service for this object, aborting new register");
        return;
    }

    quint16 bigEndianPort = m_servicePort;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    {
        bigEndianPort =  0 | ((m_servicePort & 0x00ff) << 8) | ((m_servicePort & 0xff00) >> 8);
    }
#endif

    //Get TXTRecords from BonjourRecord
    TXTRecordRef txtRecord;
    char * txt = nullptr;
    if (m_record.txtRecord.count() > 0) {
        txt = new char[256];
        TXTRecordCreate(&txtRecord, 256, txt);
        QMap<QString, QString>::const_iterator txtIt = m_record.txtRecord.begin();
        DNSServiceErrorType err = kDNSServiceErr_NoError;
        while(txtIt != m_record.txtRecord.end()) {
            err = TXTRecordSetValue(&txtRecord,
                                    txtIt.key().toUtf8().constData(),
                                    static_cast<uint8_t>(txtIt.value().length()),
                                    txtIt.value().toUtf8().constData());

            if (err != kDNSServiceErr_NoError) {
                TXTRecordDeallocate(&txtRecord);
                if (txt != nullptr) delete[] txt;

                Q_EMIT error(err);

                return;
            }
            txtIt++;
        }
    }

    DNSServiceErrorType err = DNSServiceRegister(&dnssref, 0, static_cast<uint32_t>(m_niIndex),
                                                 m_record.serviceName.toUtf8().constData(),
                                                 m_record.registeredType.toUtf8().constData(),
                                                 nullptr,
                                                 nullptr,
                                                 bigEndianPort,
                                                 TXTRecordGetLength(&txtRecord), TXTRecordGetBytesPtr(&txtRecord),
                                                 &bonjourRegisterService, this);

    if (txt != nullptr) {
        TXTRecordDeallocate(&txtRecord);
        delete[] txt;
    }

    if (err != kDNSServiceErr_NoError) {
        Q_EMIT error(err);
    } else {
        DNSServiceErrorType err = DNSServiceProcessResult(dnssref);
        if (err != kDNSServiceErr_NoError) {
            Q_EMIT error(err);
        }
    }
}

void DNSSD_API BonjourServiceRegister::bonjourRegisterService(DNSServiceRef, DNSServiceFlags,
                                                              DNSServiceErrorType errorCode, const char *name,
                                                              const char *regtype, const char *domain,
                                                              void *context)
{
    BonjourServiceRegister *serviceRegister = static_cast<BonjourServiceRegister *>(context);
    if (serviceRegister == nullptr) return;

    if (errorCode != kDNSServiceErr_NoError) {
        Q_EMIT serviceRegister->error(errorCode);
    } else {
        serviceRegister->finalRecord = BonjourRecord(name, regtype, domain);
        Q_EMIT serviceRegister->serviceRegistered(serviceRegister->finalRecord);
    }
}
