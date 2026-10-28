// Fake dns_sd: the subset of the Bonjour SDK API that qtbonjour calls. See
// fake_dns_sd.h. The TXTRecord* functions follow the SDK's documented encoding,
// so what the library builds and parses is the real wire format.

#include <winsock2.h>

#include "fake_dns_sd.h"

#include <cstring>
#include <map>
#include <mutex>

struct _DNSServiceRef_t
{
    enum class Kind { Browse, Resolve, Register } kind;
    SOCKET socket = INVALID_SOCKET;
    DNSServiceRegisterReply registerCallback = nullptr;
    void * registerContext = nullptr;
};

namespace {

struct State
{
    DNSServiceErrorType browseResult = kDNSServiceErr_NoError;
    DNSServiceErrorType resolveResult = kDNSServiceErr_NoError;
    DNSServiceErrorType registerResult = kDNSServiceErr_NoError;
    DNSServiceErrorType processResult = kDNSServiceErr_NoError;
    DNSServiceErrorType txtItemResult = kDNSServiceErr_NoError;
    bool sockFdFails = false;
    bool hasRegisterReply = false;
    fakedns::RegisterReply registerReply;

    fakedns::BrowseCall browse;
    fakedns::ResolveCall resolve;
    fakedns::RegisterCall reg;
    int processResultCount = 0;
    int liveRefs = 0;
    bool lastTxtQueryWasUnknown = false;
    std::map<const TXTRecordRef *, std::string> txtRecords;
};

std::mutex & mutex()
{
    static std::mutex m;
    return m;
}

State & state()
{
    static State s;
    return s;
}

std::string orEmpty(const char * s)
{
    return s ? std::string(s) : std::string();
}

// A real socket that nothing ever writes to, so QSocketNotifier accepts it and
// never fires. The tests call the reply callbacks themselves.
SOCKET newIdleSocket()
{
    static const bool started = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return started ? ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) : INVALID_SOCKET;
}

DNSServiceRef newRef(_DNSServiceRef_t::Kind kind)
{
    auto * ref = new _DNSServiceRef_t{kind};
    ref->socket = newIdleSocket();
    ++state().liveRefs;
    return ref;
}

} // namespace

namespace fakedns {

void reset()
{
    std::lock_guard<std::mutex> lock(mutex());
    state() = State();
}

void setBrowseResult(DNSServiceErrorType result)
{
    std::lock_guard<std::mutex> lock(mutex());
    state().browseResult = result;
}

void setResolveResult(DNSServiceErrorType result)
{
    std::lock_guard<std::mutex> lock(mutex());
    state().resolveResult = result;
}

void setRegisterResult(DNSServiceErrorType result)
{
    std::lock_guard<std::mutex> lock(mutex());
    state().registerResult = result;
}

void setProcessResult(DNSServiceErrorType result)
{
    std::lock_guard<std::mutex> lock(mutex());
    state().processResult = result;
}

void setSockFdFails(bool fails)
{
    std::lock_guard<std::mutex> lock(mutex());
    state().sockFdFails = fails;
}

void setTxtItemResult(DNSServiceErrorType result)
{
    std::lock_guard<std::mutex> lock(mutex());
    state().txtItemResult = result;
}

void setRegisterReply(const RegisterReply & reply)
{
    std::lock_guard<std::mutex> lock(mutex());
    state().registerReply = reply;
    state().hasRegisterReply = true;
}

BrowseCall lastBrowse()
{
    std::lock_guard<std::mutex> lock(mutex());
    return state().browse;
}

ResolveCall lastResolve()
{
    std::lock_guard<std::mutex> lock(mutex());
    return state().resolve;
}

RegisterCall lastRegister()
{
    std::lock_guard<std::mutex> lock(mutex());
    return state().reg;
}

int processResultCount()
{
    std::lock_guard<std::mutex> lock(mutex());
    return state().processResultCount;
}

int liveRefCount()
{
    std::lock_guard<std::mutex> lock(mutex());
    return state().liveRefs;
}

int sockFdOf(DNSServiceRef ref)
{
    return static_cast<int>(ref->socket);
}

std::string txtRecord(std::initializer_list<std::string> entries)
{
    std::string bytes;
    for (const std::string & entry : entries) {
        bytes += static_cast<char>(entry.size());
        bytes += entry;
    }
    return bytes;
}

} // namespace fakedns

// --- dns_sd.h ---------------------------------------------------------------

DNSServiceErrorType DNSSD_API DNSServiceBrowse(DNSServiceRef * sdRef, DNSServiceFlags,
                                               uint32_t, const char * regtype, const char *,
                                               DNSServiceBrowseReply callBack, void * context)
{
    std::lock_guard<std::mutex> lock(mutex());
    State & s = state();
    ++s.browse.count;
    s.browse.regtype = orEmpty(regtype);
    s.browse.callback = callBack;
    s.browse.context = context;
    if (s.browseResult != kDNSServiceErr_NoError)
        return s.browseResult;
    *sdRef = newRef(_DNSServiceRef_t::Kind::Browse);
    s.browse.ref = *sdRef;
    return kDNSServiceErr_NoError;
}

DNSServiceErrorType DNSSD_API DNSServiceResolve(DNSServiceRef * sdRef, DNSServiceFlags, uint32_t,
                                                const char * name, const char * regtype,
                                                const char * domain,
                                                DNSServiceResolveReply callBack, void * context)
{
    std::lock_guard<std::mutex> lock(mutex());
    State & s = state();
    ++s.resolve.count;
    s.resolve.name = orEmpty(name);
    s.resolve.regtype = orEmpty(regtype);
    s.resolve.domain = orEmpty(domain);
    s.resolve.callback = callBack;
    s.resolve.context = context;
    if (s.resolveResult != kDNSServiceErr_NoError)
        return s.resolveResult;
    *sdRef = newRef(_DNSServiceRef_t::Kind::Resolve);
    s.resolve.ref = *sdRef;
    return kDNSServiceErr_NoError;
}

DNSServiceErrorType DNSSD_API DNSServiceRegister(DNSServiceRef * sdRef, DNSServiceFlags,
                                                 uint32_t interfaceIndex, const char * name,
                                                 const char * regtype, const char *,
                                                 const char *, uint16_t port, uint16_t txtLen,
                                                 const void * txtRecord,
                                                 DNSServiceRegisterReply callBack, void * context)
{
    std::lock_guard<std::mutex> lock(mutex());
    State & s = state();
    ++s.reg.count;
    s.reg.interfaceIndex = interfaceIndex;
    s.reg.name = orEmpty(name);
    s.reg.regtype = orEmpty(regtype);
    s.reg.port = port;
    s.reg.txtRecordWasCreated = !s.lastTxtQueryWasUnknown;
    s.reg.txt = (txtRecord && txtLen) ? std::string(static_cast<const char *>(txtRecord), txtLen)
                                      : std::string();
    if (s.registerResult != kDNSServiceErr_NoError)
        return s.registerResult;
    *sdRef = newRef(_DNSServiceRef_t::Kind::Register);
    (*sdRef)->registerCallback = callBack;
    (*sdRef)->registerContext = context;
    return kDNSServiceErr_NoError;
}

int DNSSD_API DNSServiceRefSockFD(DNSServiceRef sdRef)
{
    std::lock_guard<std::mutex> lock(mutex());
    if (state().sockFdFails || !sdRef || sdRef->socket == INVALID_SOCKET)
        return -1;
    return static_cast<int>(sdRef->socket);
}

DNSServiceErrorType DNSSD_API DNSServiceProcessResult(DNSServiceRef sdRef)
{
    DNSServiceRegisterReply callback = nullptr;
    void * context = nullptr;
    fakedns::RegisterReply reply;
    {
        std::lock_guard<std::mutex> lock(mutex());
        State & s = state();
        ++s.processResultCount;
        if (s.processResult != kDNSServiceErr_NoError)
            return s.processResult;
        if (sdRef->kind == _DNSServiceRef_t::Kind::Register && s.hasRegisterReply) {
            callback = sdRef->registerCallback;
            context = sdRef->registerContext;
            reply = s.registerReply;
        }
    }
    // Outside the lock: the callback emits signals whose handlers may query the fake.
    if (callback)
        callback(sdRef, 0, reply.errorCode, reply.name.c_str(), reply.regtype.c_str(),
                 reply.domain.c_str(), context);
    return kDNSServiceErr_NoError;
}

void DNSSD_API DNSServiceRefDeallocate(DNSServiceRef sdRef)
{
    if (!sdRef)
        return;
    if (sdRef->socket != INVALID_SOCKET)
        ::closesocket(sdRef->socket);
    delete sdRef;
    std::lock_guard<std::mutex> lock(mutex());
    --state().liveRefs;
}

void DNSSD_API TXTRecordCreate(TXTRecordRef * txtRecord, uint16_t, void *)
{
    std::lock_guard<std::mutex> lock(mutex());
    state().txtRecords[txtRecord].clear();
}

void DNSSD_API TXTRecordDeallocate(TXTRecordRef * txtRecord)
{
    std::lock_guard<std::mutex> lock(mutex());
    state().txtRecords.erase(txtRecord);
}

DNSServiceErrorType DNSSD_API TXTRecordSetValue(TXTRecordRef * txtRecord, const char * key,
                                                uint8_t valueSize, const void * value)
{
    std::lock_guard<std::mutex> lock(mutex());
    auto it = state().txtRecords.find(txtRecord);
    if (it == state().txtRecords.end())
        return kDNSServiceErr_BadParam;

    // Same rules as the SDK: printable ASCII keys without '=', items of at most 255 bytes.
    const size_t keyLen = std::strlen(key);
    for (const char * k = key; *k; ++k) {
        if (*k < 0x20 || *k > 0x7E || *k == '=')
            return kDNSServiceErr_Invalid;
    }
    if (keyLen + (value ? 1 + valueSize : 0) > 255)
        return kDNSServiceErr_Invalid;

    std::string item(key, keyLen);
    if (value) {
        item += '=';
        item.append(static_cast<const char *>(value), valueSize);
    }
    if (it->second.size() + 1 + item.size() > 0xFFFF)
        return kDNSServiceErr_NoMemory;
    it->second += static_cast<char>(item.size());
    it->second += item;
    return kDNSServiceErr_NoError;
}

uint16_t DNSSD_API TXTRecordGetLength(const TXTRecordRef * txtRecord)
{
    std::lock_guard<std::mutex> lock(mutex());
    auto it = state().txtRecords.find(txtRecord);
    state().lastTxtQueryWasUnknown = it == state().txtRecords.end();
    return state().lastTxtQueryWasUnknown ? 0 : static_cast<uint16_t>(it->second.size());
}

const void * DNSSD_API TXTRecordGetBytesPtr(const TXTRecordRef * txtRecord)
{
    std::lock_guard<std::mutex> lock(mutex());
    auto it = state().txtRecords.find(txtRecord);
    return it == state().txtRecords.end() ? nullptr : it->second.data();
}

uint16_t DNSSD_API TXTRecordGetCount(uint16_t txtLen, const void * txtRecord)
{
    const auto * p = static_cast<const unsigned char *>(txtRecord);
    const auto * e = p + txtLen;
    uint16_t count = 0;
    while (p < e) {
        p += 1 + p[0];
        ++count;
    }
    return p > e ? 0 : count;
}

DNSServiceErrorType DNSSD_API TXTRecordGetItemAtIndex(uint16_t txtLen, const void * txtRecord,
                                                      uint16_t itemIndex, uint16_t keyBufLen,
                                                      char * key, uint8_t * valueLen,
                                                      const void ** value)
{
    {
        std::lock_guard<std::mutex> lock(mutex());
        if (state().txtItemResult != kDNSServiceErr_NoError)
            return state().txtItemResult;
    }
    const auto * p = static_cast<const unsigned char *>(txtRecord);
    const auto * e = p + txtLen;
    for (uint16_t i = 0; i < itemIndex && p < e; ++i)
        p += 1 + p[0];
    if (p >= e || p + 1 + p[0] > e)
        return kDNSServiceErr_Invalid;

    const auto * item = p + 1;
    const auto * itemEnd = item + p[0];
    const auto * eq = static_cast<const unsigned char *>(std::memchr(item, '=', p[0]));
    const size_t keyLen = eq ? static_cast<size_t>(eq - item) : p[0];
    if (keyBufLen <= keyLen)
        return kDNSServiceErr_NoMemory;
    std::memcpy(key, item, keyLen);
    key[keyLen] = '\0';
    if (eq) {
        *value = eq + 1;
        *valueLen = static_cast<uint8_t>(itemEnd - (eq + 1));
    } else {
        *value = nullptr;
        *valueLen = 0;
    }
    return kDNSServiceErr_NoError;
}
