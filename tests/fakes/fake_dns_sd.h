#ifndef QTBONJOUR_TESTS_FAKE_DNS_SD_H
#define QTBONJOUR_TESTS_FAKE_DNS_SD_H

// Test double for the Bonjour SDK. fake_dns_sd.cpp defines the dns_sd.h
// functions qtbonjour calls, so the test executable links it instead of
// dnssd64 and never talks to the Bonjour service. Each call is recorded here,
// including the reply callback and its context, so a test can play the part of
// the daemon by calling the callback itself.

#include <dns_sd.h>

#include <initializer_list>
#include <string>

namespace fakedns {

struct BrowseCall
{
    int count = 0;
    DNSServiceRef ref = nullptr;
    std::string regtype;
    DNSServiceBrowseReply callback = nullptr;
    void * context = nullptr;
};

struct ResolveCall
{
    int count = 0;
    DNSServiceRef ref = nullptr;
    std::string name;
    std::string regtype;
    std::string domain;
    DNSServiceResolveReply callback = nullptr;
    void * context = nullptr;
};

struct RegisterCall
{
    int count = 0;
    uint32_t interfaceIndex = 0;
    std::string name;
    std::string regtype;
    uint16_t port = 0;            // as passed, i.e. meant to be in network byte order
    std::string txt;              // the TXT record bytes as passed
    bool txtRecordWasCreated = false; // false: the TXTRecordRef never went through TXTRecordCreate
};

// What DNSServiceProcessResult() does for a register ref: the daemon's answer.
struct RegisterReply
{
    DNSServiceErrorType errorCode = kDNSServiceErr_NoError;
    std::string name;
    std::string regtype;
    std::string domain;
};

// Clears everything below and every knob. Call it from each test's SetUp.
void reset();

// Knobs.
void setBrowseResult(DNSServiceErrorType result);
void setResolveResult(DNSServiceErrorType result);
void setRegisterResult(DNSServiceErrorType result);
void setProcessResult(DNSServiceErrorType result);
void setSockFdFails(bool fails);                     // DNSServiceRefSockFD() returns -1
void setTxtItemResult(DNSServiceErrorType result);   // TXTRecordGetItemAtIndex() fails with this
void setRegisterReply(const RegisterReply & reply);  // answered from DNSServiceProcessResult()

// Observations. The register runs on its own thread, so these copy under a lock.
BrowseCall lastBrowse();
ResolveCall lastResolve();
RegisterCall lastRegister();
int processResultCount();
int liveRefCount();                                  // refs handed out and not deallocated
int sockFdOf(DNSServiceRef ref);

// Builds TXT record bytes the way the daemon sends them: one length-prefixed
// "key=value" (or bare "key") string per entry.
std::string txtRecord(std::initializer_list<std::string> entries);

} // namespace fakedns

#endif // QTBONJOUR_TESTS_FAKE_DNS_SD_H
