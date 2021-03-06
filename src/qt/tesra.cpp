





#if defined(HAVE_CONFIG_H)
#include "config/tesra-config.h"
#endif

#include "bitcoingui.h"

#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "intro.h"
#include "net.h"
#include "networkstyle.h"
#include "optionsmodel.h"
#include "splashscreen.h"
#include "utilitydialog.h"
#include "winshutdownmonitor.h"

#ifdef ENABLE_WALLET
#include "paymentserver.h"
#include "walletmodel.h"
#endif
#include "masternodeconfig.h"

#include "init.h"
#include "main.h"
#include "rpcserver.h"
#include "ui_interface.h"
#include "util.h"

#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <stdint.h>

#include <boost/filesystem/operations.hpp>
#include <boost/thread.hpp>

#include <QApplication>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QTranslator>

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if QT_VERSION < 0x050000
Q_IMPORT_PLUGIN(qcncodecs)
Q_IMPORT_PLUGIN(qjpcodecs)
Q_IMPORT_PLUGIN(qtwcodecs)
Q_IMPORT_PLUGIN(qkrcodecs)
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#else
#if QT_VERSION < 0x050400
Q_IMPORT_PLUGIN(AccessibleFactory)
#endif
#if defined(QT_QPA_PLATFORM_XCB)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_WINDOWS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_COCOA)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#endif
#endif
#endif

#if QT_VERSION < 0x050000
#include <QTextCodec>
#endif


Q_DECLARE_METATYPE(bool*)
Q_DECLARE_METATYPE(CAmount)

static void InitMessage(const std::string& message)
{
    LogPrintf("init message: %s\n", message);
}

/*
   Translate string to current locale using Qt.
 */
static std::string Translate(const char* psz)
{
    return QCoreApplication::translate("tesra-core", psz).toStdString();
}

static QString GetLangTerritory()
{
    QSettings settings;
    
    
    QString lang_territory = QLocale::system().name();
    
    QString lang_territory_qsettings = settings.value("language", "").toString();
    if (!lang_territory_qsettings.isEmpty())
        lang_territory = lang_territory_qsettings;
    
    lang_territory = QString::fromStdString(GetArg("-lang", lang_territory.toStdString()));
    return lang_territory;
}


static void initTranslations(QTranslator& qtTranslatorBase, QTranslator& qtTranslator, QTranslator& translatorBase, QTranslator& translator)
{
    
    QApplication::removeTranslator(&qtTranslatorBase);
    QApplication::removeTranslator(&qtTranslator);
    QApplication::removeTranslator(&translatorBase);
    QApplication::removeTranslator(&translator);

    
    
    QString lang_territory = GetLangTerritory();

    
    QString lang = lang_territory;
    lang.truncate(lang_territory.lastIndexOf('_'));

    
    
    

    
    if (qtTranslatorBase.load("qt_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslatorBase);

    
    if (qtTranslator.load("qt_" + lang_territory, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(&qtTranslator);

    
    if (translatorBase.load(lang, ":/translations/"))
        QApplication::installTranslator(&translatorBase);

    
    if (translator.load(lang_territory, ":/translations/"))
        QApplication::installTranslator(&translator);
}


#if QT_VERSION < 0x050000
void DebugMessageHandler(QtMsgType type, const char* msg)
{
    const char* category = (type == QtDebugMsg) ? "qt" : NULL;
    LogPrint(category, "GUI: %s\n", msg);
}
#else
void DebugMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    Q_UNUSED(context);
    const char* category = (type == QtDebugMsg) ? "qt" : "";
    string s(category);
    LogPrintf("%s  GUI: %s\n",s , msg.toStdString());
}
#endif

/** Class encapsulating TESRA Core startup and shutdown.
 * Allows running startup and shutdown in a different thread from the UI thread.
 */
class BitcoinCore : public QObject
{
    Q_OBJECT
public:
    explicit BitcoinCore();

public slots:
    void initialize();
    void shutdown();
    void restart(QStringList args);

signals:
    void initializeResult(int retval);
    void shutdownResult(int retval);
    void runawayException(const QString& message);

private:
    boost::thread_group threadGroup;

    
    bool execute_restart;

    
    void handleRunawayException(std::exception* e);
};


class BitcoinApplication : public QApplication
{
    Q_OBJECT
public:
    explicit BitcoinApplication(int& argc, char** argv);
    ~BitcoinApplication();

#ifdef ENABLE_WALLET
    
    void createPaymentServer();
#endif
    
    void createOptionsModel();
    
    void createWindow(const NetworkStyle* networkStyle);
    
    void createSplashScreen(const NetworkStyle* networkStyle);

    
    void requestInitialize();
    
    void requestShutdown();

    
    int getReturnValue() { return returnValue; }

    
    WId getMainWinId() const;

public slots:
    void initializeResult(int retval);
    void shutdownResult(int retval);
    
    void handleRunawayException(const QString& message);

signals:
    void requestedInitialize();
    void requestedRestart(QStringList args);
    void requestedShutdown();
    void stopThread();
    void splashFinished(QWidget* window);

private:
    QThread* coreThread;
    OptionsModel* optionsModel;
    ClientModel* clientModel;
    BitcoinGUI* window;
    QTimer* pollShutdownTimer;
#ifdef ENABLE_WALLET
    PaymentServer* paymentServer;
    WalletModel* walletModel;
#endif
    int returnValue;

    void startThread();
};

#include "tesra.moc"

BitcoinCore::BitcoinCore() : QObject()
{
}

void BitcoinCore::handleRunawayException(std::exception* e)
{
    PrintExceptionContinue(e, "Runaway exception");
    emit runawayException(QString::fromStdString(strMiscWarning));
}

void BitcoinCore::initialize()
{
    execute_restart = true;

    try {
        qDebug() << __func__ << ": Running AppInit2 in thread";
        int rv = AppInit2(threadGroup);
        if (rv) {
            /* Start a dummy RPC thread if no RPC thread is active yet
             * to handle timeouts.
             */
            StartDummyRPCThread();
        }
        emit initializeResult(rv);
    } catch (std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(NULL);
    }
}

void BitcoinCore::restart(QStringList args)
{
    if (execute_restart) { 
        execute_restart = false;
        try {
            qDebug() << __func__ << ": Running Restart in thread";
            threadGroup.interrupt_all();
            threadGroup.join_all();
            PrepareShutdown();
            qDebug() << __func__ << ": Shutdown finished";
            emit shutdownResult(1);
            CExplicitNetCleanup::callCleanup();
            QProcess::startDetached(QApplication::applicationFilePath(), args);
            qDebug() << __func__ << ": Restart initiated...";
            QApplication::quit();
        } catch (std::exception& e) {
            handleRunawayException(&e);
        } catch (...) {
            handleRunawayException(NULL);
        }
    }
}

void BitcoinCore::shutdown()
{
    try {
        qDebug() << __func__ << ": Running Shutdown in thread";
        threadGroup.interrupt_all();
        threadGroup.join_all();
        Shutdown();
        qDebug() << __func__ << ": Shutdown finished";
        emit shutdownResult(1);
    } catch (std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(NULL);
    }
}

BitcoinApplication::BitcoinApplication(int& argc, char** argv) : QApplication(argc, argv),
                                                                 coreThread(0),
                                                                 optionsModel(0),
                                                                 clientModel(0),
                                                                 window(0),
                                                                 pollShutdownTimer(0),
#ifdef ENABLE_WALLET
                                                                 paymentServer(0),
                                                                 walletModel(0),
#endif
                                                                 returnValue(0)
{
    setQuitOnLastWindowClosed(false);
}

BitcoinApplication::~BitcoinApplication()
{
    if (coreThread) {
        qDebug() << __func__ << ": Stopping thread";
        emit stopThread();
        coreThread->wait();
        qDebug() << __func__ << ": Stopped thread";
    }

    delete window;
    window = 0;
#ifdef ENABLE_WALLET
    delete paymentServer;
    paymentServer = 0;
#endif
    
    QSettings settings;
    if (optionsModel && optionsModel->resetSettings) {
        settings.clear();
        settings.sync();
    }
    delete optionsModel;
    optionsModel = 0;
}

#ifdef ENABLE_WALLET
void BitcoinApplication::createPaymentServer()
{
    paymentServer = new PaymentServer(this);
}
#endif

void BitcoinApplication::createOptionsModel()
{
    optionsModel = new OptionsModel();
}

void BitcoinApplication::createWindow(const NetworkStyle* networkStyle)
{
    window = new BitcoinGUI(networkStyle, 0);

    pollShutdownTimer = new QTimer(window);
    connect(pollShutdownTimer, SIGNAL(timeout()), window, SLOT(detectShutdown()));
    pollShutdownTimer->start(200);
}

void BitcoinApplication::createSplashScreen(const NetworkStyle* networkStyle)
{
    SplashScreen* splash = new SplashScreen(0, networkStyle);
    
    
    splash->setAttribute(Qt::WA_DeleteOnClose);
    splash->show();
    connect(this, SIGNAL(splashFinished(QWidget*)), splash, SLOT(slotFinish(QWidget*)));
}

void BitcoinApplication::startThread()
{
    if (coreThread)
        return;
    coreThread = new QThread(this);
    BitcoinCore* executor = new BitcoinCore();
    executor->moveToThread(coreThread);

    
    connect(executor, SIGNAL(initializeResult(int)), this, SLOT(initializeResult(int)));
    connect(executor, SIGNAL(shutdownResult(int)), this, SLOT(shutdownResult(int)));
    connect(executor, SIGNAL(runawayException(QString)), this, SLOT(handleRunawayException(QString)));
    connect(this, SIGNAL(requestedInitialize()), executor, SLOT(initialize()));
    connect(this, SIGNAL(requestedShutdown()), executor, SLOT(shutdown()));
    connect(window, SIGNAL(requestedRestart(QStringList)), executor, SLOT(restart(QStringList)));
    
    connect(this, SIGNAL(stopThread()), executor, SLOT(deleteLater()));
    connect(this, SIGNAL(stopThread()), coreThread, SLOT(quit()));

    coreThread->start();
}

void BitcoinApplication::requestInitialize()
{
    qDebug() << __func__ << ": Requesting initialize";
    startThread();
    emit requestedInitialize();
}

void BitcoinApplication::requestShutdown()
{
    qDebug() << __func__ << ": Requesting shutdown";
    startThread();
    window->hide();
    window->setClientModel(0);
    pollShutdownTimer->stop();

#ifdef ENABLE_WALLET
    window->removeAllWallets();
    delete walletModel;
    walletModel = 0;
#endif
    delete clientModel;
    clientModel = 0;

    
    ShutdownWindow::showShutdownWindow(window);

    
    emit requestedShutdown();
}

void BitcoinApplication::initializeResult(int retval)
{
    qDebug() << __func__ << ": Initialization result: " << retval;
    
    returnValue = retval ? 0 : 1;
    if (retval) {
#ifdef ENABLE_WALLET
        PaymentServer::LoadRootCAs();
        paymentServer->setOptionsModel(optionsModel);
#endif

        clientModel = new ClientModel(optionsModel);
        window->setClientModel(clientModel);

#ifdef ENABLE_WALLET
        if (pwalletMain) {
            walletModel = new WalletModel(pwalletMain, optionsModel);

            window->addWallet(BitcoinGUI::DEFAULT_WALLET, walletModel);
            window->setCurrentWallet(BitcoinGUI::DEFAULT_WALLET);

            connect(walletModel, SIGNAL(coinsSent(CWallet*, SendCoinsRecipient, QByteArray)),
                paymentServer, SLOT(fetchPaymentACK(CWallet*, const SendCoinsRecipient&, QByteArray)));
        }
#endif

        
        if (GetBoolArg("-min", false)) {
            window->showMinimized();
        } else {
            window->show();
        }
        emit splashFinished(window);

#ifdef ENABLE_WALLET
        
        
        connect(paymentServer, SIGNAL(receivedPaymentRequest(SendCoinsRecipient)),
            window, SLOT(handlePaymentRequest(SendCoinsRecipient)));
        connect(window, SIGNAL(receivedURI(QString)),
            paymentServer, SLOT(handleURIOrFile(QString)));
        connect(paymentServer, SIGNAL(message(QString, QString, unsigned int)),
            window, SLOT(message(QString, QString, unsigned int)));
        QTimer::singleShot(100, paymentServer, SLOT(uiReady()));
#endif
    } else {
        quit(); 
    }
}

void BitcoinApplication::shutdownResult(int retval)
{
    qDebug() << __func__ << ": Shutdown result: " << retval;
    quit(); 
}

void BitcoinApplication::handleRunawayException(const QString& message)
{
    QMessageBox::critical(0, "Runaway exception", BitcoinGUI::tr("A fatal error occurred. TESRA can no longer continue safely and will quit.") + QString("\n\n") + message);
    ::exit(1);
}

WId BitcoinApplication::getMainWinId() const
{
    if (!window)
        return 0;

    return window->winId();
}

#ifndef BITCOIN_QT_TEST
int main(int argc, char* argv[])
{
    SetupEnvironment();

    
    
    ParseParameters(argc, argv);




#if QT_VERSION < 0x050000
    
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForTr());
#endif

    Q_INIT_RESOURCE(tesra_locale);
    Q_INIT_RESOURCE(tesra);

    BitcoinApplication app(argc, argv);
#if QT_VERSION > 0x050100
    
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if QT_VERSION >= 0x050600
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
#ifdef Q_OS_MAC
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    
    qRegisterMetaType<bool*>();
    
    
    qRegisterMetaType<CAmount>("CAmount");

    
    
    
    QApplication::setOrganizationName(QAPP_ORG_NAME);
    QApplication::setOrganizationDomain(QAPP_ORG_DOMAIN);
    QApplication::setApplicationName(QAPP_APP_NAME_DEFAULT);
    GUIUtil::SubstituteFonts(GetLangTerritory());

    
    
    QTranslator qtTranslatorBase, qtTranslator, translatorBase, translator;
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);
    uiInterface.Translate.connect(Translate);

#ifdef Q_OS_MAC
#if __clang_major__ < 4
    QString s = QSysInfo::kernelVersion();
    std::string ver_info = s.toStdString();
    
    if (ver_info[0] == '1' && ver_info[1] == '7') {
        QMessageBox::critical(0, "Unsupported", BitcoinGUI::tr("High Sierra not supported with this build") + QString("\n\n"));
        ::exit(1);
    }
#endif
#endif
    
    
    
    if (mapArgs.count("-?") || mapArgs.count("-help") || mapArgs.count("-version")) {
        HelpMessageDialog help(NULL, mapArgs.count("-version"));
        help.showOrPrint();
        return 1;
    }

    
    
    if (!Intro::pickDataDirectory())
        return 0;

    
    
    if (!boost::filesystem::is_directory(GetDataDir(false))) {
        QMessageBox::critical(0, QObject::tr("TESRA Core"),
            QObject::tr("Error: Specified data directory \"%1\" does not exist.").arg(QString::fromStdString(mapArgs["-datadir"])));
        return 1;
    }
    try {
        ReadConfigFile(mapArgs, mapMultiArgs);
    } catch (std::exception& e) {
        QMessageBox::critical(0, QObject::tr("TESRA Core"),
            QObject::tr("Error: Cannot parse configuration file: %1. Only use key=value syntax.").arg(e.what()));
        return 0;
    }

    
    
    
    
    

    
    if (!SelectParamsFromCommandLine()) {
        QMessageBox::critical(0, QObject::tr("TESRA Core"), QObject::tr("Error: Invalid combination of -regtest and -testnet."));
        return 1;
    }
#ifdef ENABLE_WALLET
    
    PaymentServer::ipcParseCommandLine(argc, argv);
#endif

    QScopedPointer<const NetworkStyle> networkStyle(NetworkStyle::instantiate(QString::fromStdString(Params().NetworkIDString())));
    assert(!networkStyle.isNull());
    
    QApplication::setApplicationName(networkStyle->getAppName());
    
    initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);

#ifdef ENABLE_WALLET
    
    string strErr;
    if (!masternodeConfig.read(strErr)) {
        QMessageBox::critical(0, QObject::tr("TESRA Core"),
            QObject::tr("Error reading masternode configuration file: %1").arg(strErr.c_str()));
        return 0;
    }

    
    
    
    
    
    
    if (PaymentServer::ipcSendCommandLine())
        exit(0);

    
    
    app.createPaymentServer();
#endif

    
    
    app.installEventFilter(new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));
#if QT_VERSION < 0x050000
    
    qInstallMsgHandler(DebugMessageHandler);
#else
#if defined(Q_OS_WIN)
    
    qApp->installNativeEventFilter(new WinShutdownMonitor());
#endif
    
    qInstallMessageHandler(DebugMessageHandler);
#endif
    
    app.createOptionsModel();

    
    uiInterface.InitMessage.connect(InitMessage);

    if (GetBoolArg("-splash", true) && !GetBoolArg("-min", false))
        app.createSplashScreen(networkStyle.data());

    try {
        app.createWindow(networkStyle.data());
        app.requestInitialize();
#if defined(Q_OS_WIN) && QT_VERSION >= 0x050000
        WinShutdownMonitor::registerShutdownBlockReason(QObject::tr("TESRA Core didn't yet exit safely..."), (HWND)app.getMainWinId());
#endif
        app.exec();
        app.requestShutdown();
        app.exec();
    } catch (std::exception& e) {
        PrintExceptionContinue(&e, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(strMiscWarning));
    } catch (...) {
        PrintExceptionContinue(NULL, "Runaway exception");
        app.handleRunawayException(QString::fromStdString(strMiscWarning));
    }
    return app.getReturnValue();
}
#endif 
