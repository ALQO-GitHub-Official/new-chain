// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/pivxgui.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "qt/guiutil.h"
#include "clientmodel.h"
#include "optionsmodel.h"
#include "networkstyle.h"
#include "notificator.h"
#include "guiinterface.h"
#include "qt/qtutils.h"
#include "qt/settings/settingsfaqwidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QApplication>
#include <QColor>
#include <QShortcut>
#include <QKeySequence>
#include <QWindowStateChangeEvent>
#include "util.h"

#define BASE_WINDOW_WIDTH 1200
#define BASE_WINDOW_HEIGHT 800


const QString ALQOGUI::DEFAULT_WALLET = "~Default";

ALQOGUI::ALQOGUI(const NetworkStyle* networkStyle, QWidget* parent) :
        QMainWindow(parent),
        clientModel(0){

    /* Open CSS when configured */
    this->setStyleSheet(GUIUtil::loadStyleSheet());
    this->setMinimumSize(BASE_WINDOW_WIDTH, BASE_WINDOW_HEIGHT);
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

	QFont font("Gotham Book");
	//font.setStyleHint(QFont::Monospace);
	QApplication::setFont(font);


 //   setWindowFlags(Qt::FramelessWindowHint);

    GUIUtil::restoreWindowGeometry("nWindow", QSize(BASE_WINDOW_WIDTH, BASE_WINDOW_HEIGHT), this);

#ifdef ENABLE_WALLET
    /* if compiled with wallet support, -disablewallet can still disable the wallet */
    enableWallet = !GetBoolArg("-disablewallet", false);
#else
    enableWallet = false;
#endif // ENABLE_WALLET

    QString windowTitle = tr("ALQO") + " - ";
    windowTitle += ((enableWallet) ? tr("Wallet") : tr("Node"));
    windowTitle += " " + networkStyle->getTitleAddText();
    setWindowTitle(windowTitle);

#ifndef Q_OS_MAC
    QApplication::setWindowIcon(networkStyle->getAppIcon());
    setWindowIcon(networkStyle->getAppIcon());
#else
    MacDockIconHandler::instance()->setIcon(networkStyle->getAppIcon());
#endif




#ifdef ENABLE_WALLET
    // Create wallet frame
    if(enableWallet){

        QFrame* centralWidget = new QFrame(this);
        this->setMinimumWidth(BASE_WINDOW_WIDTH);
        this->setMinimumHeight(BASE_WINDOW_HEIGHT);
        QHBoxLayout* centralWidgetLayouot = new QHBoxLayout();
        centralWidget->setLayout(centralWidgetLayouot);
        centralWidgetLayouot->setContentsMargins(0,0,0,0);
        centralWidgetLayouot->setSpacing(0);

        centralWidget->setProperty("cssClass", "container");
        centralWidget->setStyleSheet("padding:0px; border:none; margin:0px;");

        // First the nav
        navMenu = new NavMenuWidget(this);
        //centralWidgetLayouot->addWidget(navMenu);

        this->setCentralWidget(centralWidget);
        this->setContentsMargins(0,0,0,0);

        QFrame *container = new QFrame(centralWidget);
        container->setContentsMargins(0,0,0,0);
        centralWidgetLayouot->addWidget(container);

        // Then topbar + the stackedWidget
        QVBoxLayout *baseScreensContainer = new QVBoxLayout(this);
        baseScreensContainer->setMargin(0);
        baseScreensContainer->setSpacing(0);
        baseScreensContainer->setContentsMargins(0,0,0,0);
        container->setLayout(baseScreensContainer);

        // Insert the topbar
//        topBar = new TopBar(this);
//        topBar->setContentsMargins(0,0,0,0);
        baseScreensContainer->addWidget(navMenu);
        //baseScreensContainer->addWidget(topBar);
        

        // Now stacked widget
        stackedContainer = new QSlideStackedWidget(this);
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        stackedContainer->setSizePolicy(sizePolicy);
        stackedContainer->setContentsMargins(0,200,0,0);
        baseScreensContainer->addWidget(stackedContainer);
        stackedContainer->setSpeed(300);

        // Init
        dashboard = new DashboardWidget(this);
        chartsWidget = new ChartsWidget(this);
        sendWidget = new SendWidget(this);
        receiveWidget = new ReceiveWidget(this);
        addressesWidget = new AddressesWidget(this);
        masterNodesWidget = new MasterNodesWidget(this);
        settingsWidget = new SettingsWidget(this);
        historyWidget = new HistoryWidget(this);

        // Add to parent
        stackedContainer->addWidget(dashboard);
        stackedContainer->addWidget(sendWidget);
        stackedContainer->addWidget(receiveWidget);
        stackedContainer->addWidget(historyWidget);
        stackedContainer->addWidget(chartsWidget);
        stackedContainer->addWidget(addressesWidget);
        stackedContainer->addWidget(masterNodesWidget);
        stackedContainer->addWidget(settingsWidget);
        stackedContainer->setCurrentWidget(dashboard);

    } else
#endif
    {
        // When compiled without wallet or -disablewallet is provided,
        // the central widget is the rpc console.
        rpcConsole = new RPCConsole(enableWallet ? this : 0);
        setCentralWidget(rpcConsole);
    }

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions(networkStyle);

    // Create system tray icon and notification
    createTrayIcon(networkStyle);

    // Connect events
    connectActions();

    // TODO: Add event filter??
    // // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    //    this->installEventFilter(this);

    // Subscribe to notifications from core
    subscribeToCoreSignals();

}

void ALQOGUI::createActions(const NetworkStyle* networkStyle){
    toggleHideAction = new QAction(networkStyle->getAppIcon(), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);

    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
}

/**
 * Here add every event connection
 */
void ALQOGUI::connectActions() {
    QShortcut *consoleShort = new QShortcut(this);
    consoleShort->setKey(QKeySequence(SHORT_KEY + Qt::Key_C));
    connect(consoleShort, &QShortcut::activated, [this](){
        navMenu->selectSettings();
        settingsWidget->showDebugConsole();
        goToSettings();
    });
//    connect(topBar, &TopBar::showHide, this, &ALQOGUI::showHide);
//    connect(topBar, &TopBar::themeChanged, this, &ALQOGUI::changeTheme);
    connect(settingsWidget, &SettingsWidget::showHide, this, &ALQOGUI::showHide);
    connect(sendWidget, &SendWidget::showHide, this, &ALQOGUI::showHide);
    connect(chartsWidget, &ChartsWidget::showHide, this, &ALQOGUI::showHide);
    connect(receiveWidget, &ReceiveWidget::showHide, this, &ALQOGUI::showHide);
    connect(historyWidget, &HistoryWidget::showHide, this, &ALQOGUI::showHide);
    connect(addressesWidget, &AddressesWidget::showHide, this, &ALQOGUI::showHide);
    connect(masterNodesWidget, &MasterNodesWidget::showHide, this, &ALQOGUI::showHide);
    connect(masterNodesWidget, &MasterNodesWidget::execDialog, this, &ALQOGUI::execDialog);
    connect(settingsWidget, &SettingsWidget::execDialog, this, &ALQOGUI::execDialog);
}


void ALQOGUI::createTrayIcon(const NetworkStyle* networkStyle) {
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("ALQO") + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getAppIcon());
    trayIcon->hide();
#endif
    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

//
ALQOGUI::~ALQOGUI() {
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    GUIUtil::saveWindowGeometry("nWindow", this);
    if (trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    MacDockIconHandler::cleanup();
#endif
}


/** Get restart command-line parameters and request restart */
void ALQOGUI::handleRestart(QStringList args){
    if (!ShutdownRequested())
        emit requestedRestart(args);
}


void ALQOGUI::setClientModel(ClientModel* clientModel) {
    this->clientModel = clientModel;
    if(this->clientModel) {

        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        navMenu->setClientModel(clientModel);
        dashboard->setClientModel(clientModel);
        sendWidget->setClientModel(clientModel);
        settingsWidget->setClientModel(clientModel);

        // Receive and report messages from client model
        connect(clientModel, SIGNAL(message(QString, QString, unsigned int)), this, SLOT(message(QString, QString, unsigned int)));
        connect(dashboard, SIGNAL(infomessage(QString, QString, unsigned int)), this, SLOT(message(QString, QString, unsigned int)));
        connect(dashboard, SIGNAL(walletSynced(bool)), this, SLOT(walletSynced(bool)));

        // Get restart command-line parameters and handle restart
        connect(settingsWidget, &SettingsWidget::handleRestart, [this](QStringList arg){handleRestart(arg);});

        if (rpcConsole) {
            rpcConsole->setClientModel(clientModel);
        }

        if (trayIcon) {
            trayIcon->show();
        }
    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if (trayIconMenu) {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
    }
}

void ALQOGUI::createTrayIconMenu() {
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler* dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow*)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();

#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void ALQOGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#endif

void ALQOGUI::changeEvent(QEvent* e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if (e->type() == QEvent::WindowStateChange) {
        if (clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray()) {
            QWindowStateChangeEvent* wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if (!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized()) {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void ALQOGUI::closeEvent(QCloseEvent* event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if (clientModel && clientModel->getOptionsModel()) {
        if (!clientModel->getOptionsModel()->getMinimizeOnClose()) {
            QApplication::quit();
        }
    }
#endif
    QMainWindow::closeEvent(event);
}


void ALQOGUI::messageInfo(const QString& text){
    if(!this->snackBar) this->snackBar = new SnackBar(this, this);
    this->snackBar->setText(text);
    this->snackBar->resize(this->width(), snackBar->height());
    openDialog(this->snackBar, this);
}


void ALQOGUI::message(const QString& title, const QString& message, unsigned int style, bool* ret) {
    QString strTitle =  tr("ALQO"); // default title
    // Default to information icon
    int nNotifyIcon = Notificator::Information;

    QString msgType;

    // Prefer supplied title over style based title
    if (!title.isEmpty()) {
        msgType = title;
    } else {
        switch (style) {
            case CClientUIInterface::MSG_ERROR:
                msgType = tr("Error");
                break;
            case CClientUIInterface::MSG_WARNING:
                msgType = tr("Warning");
                break;
            case CClientUIInterface::MSG_INFORMATION:
                msgType = tr("Information");
                break;
            default:
                msgType = tr("System Message");
                break;
        }
    }

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nNotifyIcon = Notificator::Critical;
    } else if (style & CClientUIInterface::ICON_WARNING) {
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        int r = 0;
        showNormalIfMinimized();
        if(style & CClientUIInterface::BTN_MASK){
            r = openStandardDialog(
                    (title.isEmpty() ? strTitle : title), message, "OK", "CANCEL"
                );
        }else{
            r = openStandardDialog((title.isEmpty() ? strTitle : title), message, "OK");
        }
        if (ret != NULL)
            *ret = r;
    } else if(style & CClientUIInterface::MSG_INFORMATION_SNACK){
        messageInfo(message);
    }else {
        // Append title to "ALQO - "
        if (!msgType.isEmpty())
            strTitle += " - " + msgType;
        notificator->notify((Notificator::Class) nNotifyIcon, strTitle, message);
    }
}

bool ALQOGUI::openStandardDialog(QString title, QString body, QString okBtn, QString cancelBtn){
    DefaultDialog *dialog;
    showHide(true);
    if (isVisible()) {
        dialog = new DefaultDialog(this);
        dialog->setText(title, body, okBtn, cancelBtn);
        dialog->adjustSize();
        openDialogWithOpaqueBackground(dialog, this);
    } else {
        dialog = new DefaultDialog();
        dialog->setText(title, body, okBtn);
        dialog->setStyleForFirstMessage();
        dialog->setWindowTitle(tr("ALQO"));
        dialog->adjustSize();
        dialog->raise();
        dialog->exec();
    }
    bool ret = dialog->isOk;
    dialog->deleteLater();
    return ret;
}


void ALQOGUI::showNormalIfMinimized(bool fToggleHidden) {
    if (!clientModel)
        return;
    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden()) {
        show();
        activateWindow();
    } else if (isMinimized()) {
        showNormal();
        activateWindow();
    } else if (GUIUtil::isObscured(this)) {
        raise();
        activateWindow();
    } else if (fToggleHidden)
        hide();
}

void ALQOGUI::toggleHidden() {
    showNormalIfMinimized(true);
}

void ALQOGUI::detectShutdown() {
    if (ShutdownRequested()) {
        if (rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

void ALQOGUI::goToDashboard(){
    if(stackedContainer->currentWidget() != dashboard){
        stackedContainer->slideInWgt(dashboard, QSlideStackedWidget::LEFT2RIGHT);
    }
}

void ALQOGUI::goToHistory()
{
    showTop(historyWidget);
}

void ALQOGUI::goToSend(){
    showTop(sendWidget);
}

void ALQOGUI::goToAddresses(){
    addressesWidget->updateListView();
    showTop(addressesWidget);
}

void ALQOGUI::goToMasterNodes(){
    showTop(masterNodesWidget);
}

void ALQOGUI::goToSettings(){
    showTop(settingsWidget);
}

void ALQOGUI::goToReceive(){
    showTop(receiveWidget);
}

void ALQOGUI::goToCharts(){
    showTop(chartsWidget);
}

void ALQOGUI::showTop(QWidget* view){
    int newIndex = stackedContainer->indexOf(view);
    int oldIndex = stackedContainer->currentIndex();
    if(newIndex != oldIndex){
        stackedContainer->slideInIdx(newIndex, (newIndex > oldIndex ? QSlideStackedWidget::RIGHT2LEFT : QSlideStackedWidget::LEFT2RIGHT));
       // topBar->showTop();
    }
}

void ALQOGUI::changeTheme(bool isLightTheme){

    QString css = GUIUtil::loadStyleSheet();
    this->setStyleSheet(css);

    // Notify
    emit themeChanged(isLightTheme, css);

    // Update style
    updateStyle(this);
}

void ALQOGUI::resizeEvent(QResizeEvent* event){
    // Parent..
    QMainWindow::resizeEvent(event);
    // background
    showHide(opEnabled);
    // Notify
//    if (dialog) {
//        dialog->move(QPoint((this->width() - dialog->width()) / 2, (this->height()  - dialog->height()) / 2));
//    }
    emit windowResizeEvent(event);
}

bool ALQOGUI::execDialog(QDialog *dialog, int xDiv, int yDiv){
    return openDialogWithOpaqueBackgroundY(dialog, this);
}

void ALQOGUI::showHide(bool show){
    if(!op) op = new QLabel(this);
    if(!show){
        op->setVisible(false);
        opEnabled = false;
    }else{
        QColor bg("#0f0f1f");
        bg.setAlpha(200);
        if(!isLightTheme()){
            bg = QColor(15, 15, 31, 0);
            bg.setAlpha(100);
        }

        QPalette palette;
        palette.setColor(QPalette::Window, bg);
        op->setAutoFillBackground(true);
        op->setPalette(palette);
        op->setWindowFlags(Qt::CustomizeWindowHint);
        op->move(0,0);
        op->show();
        op->activateWindow();
        op->resize(width(), height());
        op->setVisible(true);
        opEnabled = true;
    }
}

int ALQOGUI::getNavWidth(){
    return this->navMenu->width();
}

void ALQOGUI::openFAQ(int section){
}


#ifdef ENABLE_WALLET
bool ALQOGUI::addWallet(const QString& name, WalletModel* walletModel)
{
    // Single wallet supported for now..
    if(!stackedContainer || !clientModel || !walletModel)
        return false;

    // set the model for every view
    dashboard->setWalletModel(walletModel);
    navMenu->setWalletModel(walletModel);
    receiveWidget->setWalletModel(walletModel);
    sendWidget->setWalletModel(walletModel);
    addressesWidget->setWalletModel(walletModel);
    masterNodesWidget->setWalletModel(walletModel);
    settingsWidget->setWalletModel(walletModel);
    historyWidget->setWalletModel(walletModel);
    chartsWidget->setWalletModel(walletModel);

    // Connect actions..
    connect(masterNodesWidget, &MasterNodesWidget::message, this, &ALQOGUI::message);
    //connect(navMenu, &NavMenuWidget::message, this, &ALQOGUI::message);
    connect(sendWidget, &SendWidget::message,this, &ALQOGUI::message);
    connect(receiveWidget, &ReceiveWidget::message,this, &ALQOGUI::message);
    connect(addressesWidget, &AddressesWidget::message,this, &ALQOGUI::message);
    connect(settingsWidget, &SettingsWidget::message, this, &ALQOGUI::message);
    connect(historyWidget, &HistoryWidget::message,this, &ALQOGUI::message);
    connect(chartsWidget, &ChartsWidget::message,this, &ALQOGUI::message);

    // Pass through transaction notifications
    connect(dashboard, SIGNAL(incomingTransaction(QString, int, CAmount, QString, QString)), this, SLOT(incomingTransaction(QString, int, CAmount, QString, QString)));
    connect(historyWidget, SIGNAL(incomingTransaction(QString, int, CAmount, QString, QString)), this, SLOT(incomingTransaction(QString, int, CAmount, QString, QString)));
    connect(chartsWidget, SIGNAL(incomingTransaction(QString, int, CAmount, QString, QString)), this, SLOT(incomingTransaction(QString, int, CAmount, QString, QString)));

    return true;
}

bool ALQOGUI::setCurrentWallet(const QString& name) {
    // Single wallet supported.
    return true;
}

void ALQOGUI::removeAllWallets() {
    // Single wallet supported.
}

void ALQOGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address) {
    // Only send notifications when not disabled
    if(!bdisableSystemnotifications){
        // On new transaction, make an info balloon
        message((amount) < 0 ? (pwalletMain->fMultiSendNotify == true ? tr("Sent MultiSend transaction") : tr("Sent transaction")) : tr("Incoming transaction"),
            tr("Date: %1\n"
               "Amount: %2\n"
               "Type: %3\n"
               "Address: %4\n")
                .arg(date)
                .arg(BitcoinUnits::formatWithUnit(unit, amount, true))
                .arg(type)
                .arg(address),
            CClientUIInterface::MSG_INFORMATION);

        pwalletMain->fMultiSendNotify = false;
    }
}

#endif // ENABLE_WALLET


static bool ThreadSafeMessageBox(ALQOGUI* gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    std::cout << "thread safe box: " << message << std::endl;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
              modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
              Q_ARG(QString, QString::fromStdString(caption)),
              Q_ARG(QString, QString::fromStdString(message)),
              Q_ARG(unsigned int, style),
              Q_ARG(bool*, &ret));
    return ret;
}


void ALQOGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}

void ALQOGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ThreadSafeMessageBox.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}
