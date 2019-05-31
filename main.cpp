#include <QApplication>
#include <QQmlApplicationEngine>
#include <QPluginLoader>
#include <QQmlExtensionPlugin>
#include <QtQml>
#include <QIcon>

#include <QtConcurrent>
#include "bonjourserviceregister.h"

using namespace qtbonjour;

static QList<BonjourServiceRegister*> _registerList;

static const quint16 servicePort = 7000;
static const quint16 airplayPort = 7100;

QList<QNetworkInterface> getNetworkInterfaces()
{
    QList<QNetworkInterface> interfaces;
    Q_FOREACH(QNetworkInterface netInterface, QNetworkInterface::allInterfaces())
    {
        // Return all non-loopback, not P2P and valid (up and running) MAC Addresses
        bool up = netInterface.flags() & QNetworkInterface::IsUp;
        bool loopBack = netInterface.flags() & QNetworkInterface::IsLoopBack;
        bool isRunning = netInterface.flags() & QNetworkInterface::IsRunning;
        bool p2p = netInterface.flags() & QNetworkInterface::IsPointToPoint;
        bool multicast = netInterface.flags() & QNetworkInterface::CanMulticast;

        if (up && multicast && !loopBack && isRunning && !p2p)
            interfaces.push_back(netInterface);
    }
    return interfaces;
}

BonjourServiceRegister* getRegisterInstance(const BonjourRecord &record, quint16 servicePort, int niIndex) {
    BonjourServiceRegister* bjSrvRegister = new BonjourServiceRegister(record, servicePort, niIndex);

    QObject::connect(bjSrvRegister, &BonjourServiceRegister::serviceRegistered,
    [&](BonjourRecord record){
        qDebug() << "Service registered" << record.serviceName << record.registeredType;
    });

    QObject::connect(bjSrvRegister, &BonjourServiceRegister::error,
            [&](DNSServiceErrorType error){
        qDebug() << "Error registering service" << QString::number(error);
    });

    _registerList.append(bjSrvRegister);

     return bjSrvRegister;
}

void registerRAOPService(const QNetworkInterface &ni) {
    QString macAddres = ni.hardwareAddress();
    QString simplifiedMacAddress = macAddres.replace(":", "");
    QString raopName = simplifiedMacAddress + "@Playmate69";
    BonjourRecord raopService( raopName,"_raop._tcp","_local");

    raopService.txtRecord["vv"] = "2";
    raopService.txtRecord["vs"] = "220.68";
    raopService.txtRecord["vn"] = "65537";
    raopService.txtRecord["tp"] = "UDP";
    raopService.txtRecord["sf"] = "0x4";
    raopService.txtRecord["md"] = "0,1,2";
    raopService.txtRecord["ft"] = "0x4A7FFFF7,0xE";
    raopService.txtRecord["et"] = "0,3,5";
    raopService.txtRecord["da"] = "true";
    raopService.txtRecord["cn"] = "0,1,2,3";
    raopService.txtRecord["am"] = "AppleTV3,2";
    raopService.txtRecord["sf"] = "0x4";
    raopService.txtRecord["pk"] = "3b6a27bcceb6a42d62a3a8d02a6f0d73653215771de243a63ac048a18b59da29";
    raopService.txtRecord["pi"] = "5e66cf9b-0a39-4e0c-081a8ce63231";

    getRegisterInstance(raopService, servicePort, ni.index())->registerService();
}

void registerAirplayService(const QNetworkInterface& ni) {
    BonjourRecord airplayService("Playmate69", "_airplay._tcp", "_local");

    airplayService.txtRecord["deviceid"] = ni.hardwareAddress();
    airplayService.txtRecord["features"] = "0x4A7FFFF7,0xE";
    airplayService.txtRecord["model"] = "AppleTV5,3";
    airplayService.txtRecord["flags"] = "0x4";
    airplayService.txtRecord["srcvers"] = "220.68";
    airplayService.txtRecord["vv"] = "2";
    airplayService.txtRecord["pk"] = "3b6a27bcceb6a42d62a3a8d02a6f0d73653215771de243a63ac048a18b59da29";
    airplayService.txtRecord["pi"] = "5e66cf9b-0a39-4e0c-081a8ce63231";

    getRegisterInstance(airplayService, airplayPort, ni.index())->registerService();
}

void registerPlaymateService(const QNetworkInterface & ni) {
    BonjourRecord playmateService("Playmate69", "_playmate._tcp", "_local");

    playmateService.txtRecord["deviceid"] = ni.hardwareAddress();
    playmateService.txtRecord["ft"] = "0x4A7FFFF7,0xE";
    playmateService.txtRecord["am"] = "MacBookPro9,2";
    playmateService.txtRecord["vs"] = "6.0.3";
    playmateService.txtRecord["os"] = "10.10.5";
    playmateService.txtRecord["pf"] = "macosx";
    playmateService.txtRecord["sf"] = "0x4";
    playmateService.txtRecord["pk"] = "3b6a27bcceb6a42d62a3a8d02a6f0d73653215771de243a63ac048a18b59da29";
    playmateService.txtRecord["pi"] ="5e66cf9b-0a39-4e0c-081a8ce63231";

    getRegisterInstance(playmateService, airplayPort, ni.index())->registerService();
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#if defined(Q_OS_WIN)
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES, true);
#endif

    QApplication app(argc, argv);

    QList<QNetworkInterface> interfaces = getNetworkInterfaces();
    for (QNetworkInterface ni : interfaces) {
        registerRAOPService(ni);
        registerAirplayService(ni);
        registerPlaymateService(ni);
    }

    // Wait to see if the device see my service
    QThread::msleep(5000);

    Q_FOREACH(BonjourServiceRegister * bonjourService, _registerList) {
        if (bonjourService == nullptr) continue;
        bonjourService->disconnect();
        bonjourService->deleteLater();
    }
    _registerList.clear();

    return app.exec();
}
