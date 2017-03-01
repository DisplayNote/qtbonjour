/*
Copyright (c) 2007, Trenton Schulz
Copyright (C) 2009-2011, Thorvald Natvig <thorvald@natvig.com>

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
#include "bonjourserviceresolver.h"

// Qt includes
#include <QtCore/QSocketNotifier>
#include <QtCore/QtEndian>

#include <QDebug>

BonjourServiceResolver::ResolveRecord::ResolveRecord(const BonjourRecord &r, BonjourServiceResolver *p) : record(r), bsr(p), dnssref(NULL), bonjourSocket(NULL), bonjourPort(-1) {
}

BonjourServiceResolver::ResolveRecord::~ResolveRecord() {
	delete bonjourSocket;
	if (dnssref)
		DNSServiceRefDeallocate(dnssref);
}

BonjourServiceResolver::BonjourServiceResolver(QObject *p) : QObject(p) {
}

BonjourServiceResolver::~BonjourServiceResolver() {
	foreach(ResolveRecord *rr, qmResolvers)
		delete rr;
}

void BonjourServiceResolver::resolveBonjourRecord(const BonjourRecord &record) {
	ResolveRecord *rr = new ResolveRecord(record, this);

	DNSServiceErrorType err = DNSServiceResolve(& rr->dnssref, 0, 0,
	                          record.serviceName.toUtf8().constData(),
	                          record.registeredType.toUtf8().constData(),
	                          record.replyDomain.toUtf8().constData(),
	                          (DNSServiceResolveReply)bonjourResolveReply, rr);

	if (err == kDNSServiceErr_NoError) {
		int sockfd = DNSServiceRefSockFD(rr->dnssref);
		if (sockfd == -1) {
			err = kDNSServiceErr_Invalid;
		} else {
            rr->bonjourSocket = new QSocketNotifier(sockfd, QSocketNotifier::Read);
			connect(rr->bonjourSocket, SIGNAL(activated(int)), this, SLOT(bonjourSocketReadyRead(int)));
			qmResolvers.insert(sockfd, rr);
			return;
		}
	}

	Q_EMIT error(record, err);
	delete rr;
}

void BonjourServiceResolver::bonjourSocketReadyRead(int sockfd) {
	ResolveRecord *rr = qmResolvers.value(sockfd);

	if (! rr)
		return;

	DNSServiceErrorType err = DNSServiceProcessResult(rr->dnssref);
	if (err != kDNSServiceErr_NoError)
		Q_EMIT error(rr->record, err);

}

void BonjourServiceResolver::bonjourResolveReply(DNSServiceRef, DNSServiceFlags ,
        quint32 , DNSServiceErrorType errorCode,
        const char * , const char *hosttarget, quint16 port,
        quint16 txtLen, const char * txtRecord, void *context) {
	ResolveRecord *rr = static_cast<ResolveRecord *>(context);
	rr->bsr->qmResolvers.remove(DNSServiceRefSockFD(rr->dnssref));

	if (errorCode != kDNSServiceErr_NoError) {
		Q_EMIT rr->bsr->error(rr->record, errorCode);
		return;
    }

	rr->bonjourPort = qFromBigEndian<quint16>(port);

    if (txtLen > 0) {
        uint16_t keyLen = 256;
        char * key = new char[keyLen];
        uint8_t valueLen;
        char * value;

        uint16_t keys = TXTRecordGetCount(txtLen, txtRecord);
        for (uint16_t index = 0; index != keys; index++) {
            keyLen = 256;
            if (TXTRecordGetItemAtIndex(txtLen, txtRecord, index, keyLen, key, &valueLen, (const void **)&value) != kDNSServiceErr_NoError) {
                delete[] key;

                Q_EMIT rr->bsr->error(rr->record, errorCode);

                return;
            }
            rr->record.txtRecord.insert(QString::fromStdString(std::string(key)),
                                        QString::fromStdString(std::string(value, valueLen)));
        }
        delete[] key;
    }

	Q_EMIT rr->bsr->bonjourRecordResolved(rr->record, QString::fromUtf8(hosttarget), rr->bonjourPort);
	delete rr;
}
