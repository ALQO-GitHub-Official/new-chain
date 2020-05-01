// Copyright (c) 2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/dashboardwidget.h"
#include "qt/forms/ui_dashboardwidget.h"
#include "qt/sendconfirmdialog.h"
#include "qt/guitransactionsutils.h"

#include <qt/receivedialog.h>
#include "qt/txrow.h"
#include "qt/qtutils.h"
#include "guiutil.h"
#include "walletmodel.h"
#include "clientmodel.h"
#include "optionsmodel.h"
#include "utiltime.h"
#include <QPainter>
#include <QModelIndex>
#include <QList>
#include <QGraphicsLayout>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrl>
#include <QUrlQuery>
#include <QTextDocumentFragment>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <boost/lexical_cast.hpp>
#include <boost/xpressive/xpressive.hpp>

#define DECORATION_SIZE 58
#define NUM_ITEMS 3
#define SHOW_EMPTY_CHART_VIEW_THRESHOLD 4000
#define REQUEST_LOAD_TASK 1
#define CHART_LOAD_MIN_TIME_INTERVAL 15

DashboardWidget::DashboardWidget(ALQOGUI* parent) :
    PWidget(parent),
    ui(new Ui::DashboardWidget)
{
    ui->setupUi(this);
    timerid = 0;
    lastrefresh = 0;
    curbal =0;

    txHolder = new TxViewHolder(isLightTheme(), true);
    txViewDelegate = new FurAbstractListItemDelegate(
        DECORATION_SIZE,DECORATION_SIZE,
        txHolder,
        this
    );

    this->setStyleSheet(parent->styleSheet());
    this->setContentsMargins(0,0,0,0);
    ui->dashLayout->setContentsMargins(0,0,0,0);

    // Balance
    //setCssProperty(ui->labelAmountPiv, "dash-balance");
    QString totalPiv = GUIUtil::formatBalance(0, nDisplayUnit);

    QFont fontBalance = ui->labelAmountPiv->font();
    fontBalance.setWeight(QFont::Bold);
	fontBalance.setPointSize(48);
	ui->labelAmountPiv->setFont(fontBalance);
    ui->labelAmountPiv->setText("<font color='white'>"+totalPiv+"</font>");

    //Frames
    // Send Receive Frame
    //Send
    setCssProperty(ui->frameSendReceive, "dash-frame");
    setCssProperty(ui->sendpagebtn, "dash-label");
    setCssProperty(ui->receivepagebtn, "dash-label");
    setCssProperty(ui->pushButtonSend, "dash-btn-send");

    ui->lineEditAmount->setPlaceholderText(QString("  ") + " 0.00 XLQ ");
    GUIUtil::setupAmountWidget(ui->lineEditAmount, this);
    setCssProperty(ui->lineEditAmount, "edit-primary-dash");

//    btnContact = ui->lineEditContact->addAction(QIcon("://ic-contact-arrow-down"), QLineEdit::TrailingPosition);
//    ui->lineEditContact->setPlaceholderText(QString("  ") + " Select Contact");
//    setCssProperty(ui->lineEditContact, "edit-primary-multi-book-sm");

    ui->labelSendAddress->setVisible(false);
    setCssProperty(ui->labelSendAddress, "dash-label-sm");
    ui->sendpagebtn->setChecked(true);

    //Receive
    setCssProperty(ui->pushButtonQR, "dash-btn-send");
    setCssProperty(ui->labelAddress, "dash-label-sm");

    // QR image
    QPixmap pixmap("://img-qr-test");
    ui->btnQr->setIcon(QIcon(pixmap.scaled(ui->btnQr->width(), ui->btnQr->height(), Qt::KeepAspectRatio)));

    // Transactions
    setCssProperty(ui->frameTransactions, "dash-frame");
    setCssProperty(ui->listTransactions, "listTransactions");
    setCssProperty(ui->lblTransactions, "dash-label");
    ui->listTransactions->setItemDelegate(txViewDelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * DECORATION_SIZE);
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->listTransactions->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Market Info
    setCssProperty(ui->frameStaking, "dash-frame");
    setCssProperty(ui->lblStaking, "dash-label");

    setCssProperty(ui->labelbtcusd, "dash-label-md");
    setCssProperty(ui->labelxlqbtc, "dash-label-md");
    setCssProperty(ui->labelusdxlq, "dash-label-md");
    setCssProperty(ui->lblwalletvalue, "dash-label-md");
    setCssProperty(ui->labeltime, "dash-label-md");


    //set labels to richtext to use css.
    ui->labelbtcusd->setTextFormat(Qt::RichText);
    ui->labelxlqbtc->setTextFormat(Qt::RichText);
    ui->labelusdxlq->setTextFormat(Qt::RichText);
    ui->lblwalletvalue->setTextFormat(Qt::RichText);
    ui->labeltime->setTextFormat(Qt::RichText);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    connect(ui->pushButtonSend, SIGNAL(clicked()), this, SLOT(onSendClicked()));

    connect(ui->btnQr, SIGNAL(clicked()), this, SLOT(onBtnReceiveClicked()));
    connect(ui->pushButtonQR, SIGNAL(clicked()), this, SLOT(onBtnReceiveClicked()));

    connect(ui->sendpagebtn, SIGNAL(clicked()), this, SLOT(showSend()));
    connect(ui->receivepagebtn, SIGNAL(clicked()), this, SLOT(showReceive()));

//    connect(ui->lineEditContact, SIGNAL(clicked()), this, SLOT(onContactsClicked()));
//    connect(btnContact, &QAction::triggered, [this](){emit onContactsClicked();});

 //   loadWalletModel();

//    SetExchangeInfoTextLabels();

	//Timer is not set,lets create one.
	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(SetExchangeInfoTextLabels()));
	timer->start(60000);
	timerid = timer->timerId();

}

void DashboardWidget::onSendClicked(){

    if (!walletModel || !walletModel->getOptionsModel())
        return;

    SendCoinsRecipient recipient;

    QString address = ui->labelSendAddress->text();
    bool isValid = false;
    CAmount value = GUIUtil::parseValue(ui->lineEditAmount->text(), nDisplayUnit, &isValid);

    recipient.amount = value;
//    recipient.address = address;

    bool retval = true;

    // Sending a zero amount or dust is invalid
    if (value <= 0 || GUIUtil::isDust(address, value)) {
        setCssEditLine(ui->lineEditAmount, false, true);
        retval = false;
    }

    // request unlock only if was locked or unlocked for mixing:
    // this way we let users unlock by walletpassphrase or by menu
    // and make many transactions while unlocking through this dialog
    // will call relock
    if(!GUIUtil::requestUnlock(walletModel, AskPassphraseDialog::Context::Send_PIV, true)){
        // Unlock wallet was cancelled
        inform(tr("Cannot send, wallet locked"));
        return;
    }

    QList<SendCoinsRecipient> recipients;
    recipients.append(recipient);

    if(send(recipients)) {
		ui->lineEditAmount->clear();
    }
}

bool DashboardWidget::send(QList<SendCoinsRecipient> recipients){
    // prepare transaction for getting txFee earlier
    WalletModelTransaction currentTransaction(recipients);
    WalletModel::SendCoinsReturn prepareStatus;

    prepareStatus = walletModel->prepareTransaction(currentTransaction, CoinControlDialog::coinControl);

    // process prepareStatus and on error generate message shown to user
    GuiTransactionsUtils::ProcessSendCoinsReturn(
            this,
            prepareStatus,
            walletModel,
            BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(),
                                         currentTransaction.getTransactionFee()),
            true
    );

    if (prepareStatus.status != WalletModel::OK) {
        inform(tr("Cannot create transaction."));
        return false;
    }

    showHideOp(true);
    QString warningStr = QString();
//    if (currentTransaction.getTransaction()->fStakeDelegationVoided)
//        warningStr = tr("WARNING:\nTransaction spends a cold-stake delegation, voiding it.\n"
//                     "These coins will no longer be cold-staked.");
    TxDetailDialog* dialog = new TxDetailDialog(window, true, warningStr);
    dialog->setDisplayUnit(walletModel->getOptionsModel()->getDisplayUnit());
    dialog->setData(walletModel, currentTransaction);
    dialog->adjustSize();
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 5);

    if(dialog->isConfirm()){
        // now send the prepared transaction
        WalletModel::SendCoinsReturn sendStatus = dialog->getStatus();
        // process sendStatus and on error generate message shown to user
        GuiTransactionsUtils::ProcessSendCoinsReturn(
                this,
                sendStatus,
                walletModel
        );

        if (sendStatus.status == WalletModel::OK) {
            inform(tr("Transaction sent"));
            dialog->deleteLater();
            return true;
        }
    }

    dialog->deleteLater();
    return false;
}


void DashboardWidget::onBtnReceiveClicked(){
    if(walletModel) {
        showHideOp(true);
        ReceiveDialog *receiveDialog = new ReceiveDialog(window);

        receiveDialog->updateQr(walletModel->getAddressTableModel()->getLastUnusedAddress());

		receiveDialog->setWindowFlags(Qt::CustomizeWindowHint);
		receiveDialog->setAttribute(Qt::WA_TranslucentBackground, true);

		receiveDialog->resize(window->width(),window->height());

		QPropertyAnimation* animation = new QPropertyAnimation(receiveDialog, "pos");
		animation->setDuration(300);
		int xPos = 0;
		animation->setStartValue(QPoint(xPos, window->height()));
		animation->setEndValue(QPoint(xPos, 0));
		animation->setEasingCurve(QEasingCurve::OutQuad);
		animation->start(QAbstractAnimation::DeleteWhenStopped);

		bool res = receiveDialog->exec();

        if (res) {
            inform(tr("Address Copied"));
        }

        receiveDialog->deleteLater();
    }
}

void DashboardWidget::handleTransactionClicked(const QModelIndex &index){

    ui->listTransactions->setCurrentIndex(index);
    QModelIndex rIndex = filter->mapToSource(index);

    window->showHide(true);
    TxDetailDialog *dialog = new TxDetailDialog(window, false);
    dialog->setData(walletModel, rIndex);
    openDialogWithOpaqueBackgroundY(dialog, window, 3, 17);

    // Back to regular status
    ui->listTransactions->scrollTo(index);
    ui->listTransactions->clearSelection();
    ui->listTransactions->setFocus();
    dialog->deleteLater();
}

void DashboardWidget::loadWalletModel(){
    if (walletModel && walletModel->getOptionsModel()) {
        txModel = walletModel->getTransactionTableModel();
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setDynamicSortFilter(true);
        filter->setSortCaseSensitivity(Qt::CaseInsensitive);
        filter->setFilterCaseSensitivity(Qt::CaseInsensitive);
        filter->setSortRole(Qt::EditRole);
        filter->setSourceModel(txModel);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);
        txHolder->setFilter(filter);
        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        if(txModel->size() == 0){
            ui->listTransactions->setVisible(false);
        }

        //connect(txModel, &TransactionTableModel::txArrived, this, &DashboardWidget::onTxArrived);

        // Notification pop-up for new transaction
        connect(txModel, SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(processNewTransaction(QModelIndex, int, int)));

        QString qraddress = walletModel->getAddressTableModel()->getLastUnusedAddress();
        ui->labelAddress->setText(qraddress);

		info = new SendCoinsRecipient();
		info->address = qraddress;
		QString uri = GUIUtil::formatBitcoinURI(*info);
		QString error;
		QPixmap pixmap = encodeToQr(uri, error);
		if(!pixmap.isNull()){
			ui->btnQr->setIcon(QIcon(pixmap.scaled(ui->btnQr->width(), ui->btnQr->height(), Qt::KeepAspectRatio)));
		}else{
			ui->btnQr->setText(!error.isEmpty() ? error : "Error encoding address");
		}
    }

    connect(walletModel, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this,
            SLOT(updateBalances(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
    // update the display unit, to not use the default ("ALQO")
    updateDisplayUnit();
    	loadContacts();
}

void DashboardWidget::onTxArrived(const QString& hash, const bool& isCoinStake, const bool& isCSAnyType) {
    showList();
}

void DashboardWidget::showList(){
    if (filter->rowCount() == 0){
  //      ui->listTransactions->setVisible(false);
    } else {
  //      ui->listTransactions->setVisible(true);
    }
}

void DashboardWidget::updateBalances(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance,
                            const CAmount& zerocoinBalance, const CAmount& unconfirmedZerocoinBalance, const CAmount& immatureZerocoinBalance,
                            const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance){

    CAmount nLockedBalance = 0;
    if (!walletModel) {
        nLockedBalance = walletModel->getLockedBalance();
    }

    // ALQO Balance
    //CAmount nTotalBalance = balance + unconfirmedBalance;
    CAmount pivAvailableBalance = balance - immatureBalance - nLockedBalance;
    CAmount pivimmatureBalance = immatureBalance;
    CAmount pivlockedBalance = nLockedBalance;
    curbal = pivAvailableBalance;
    // Set
    //QString totalPiv = QString::number(pivAvailableBalance);
    QString totalPiv = BitcoinUnits::simplestFormat(nDisplayUnit, pivAvailableBalance, 2, false, BitcoinUnits::separatorAlways);
    QString immaturePiv = BitcoinUnits::simplestFormat(nDisplayUnit, pivimmatureBalance, 2, false, BitcoinUnits::separatorAlways);

    QFont fontBalance = ui->labelAmountPiv->font();
    fontBalance.setWeight(QFont::Bold);
	fontBalance.setPointSize(48);
	ui->labelAmountPiv->setFont(fontBalance);
    ui->labelAmountPiv->setText("<font color='white'>"+totalPiv+"</font>");
    SetExchangeInfoTextLabels();
}

void DashboardWidget::updateDisplayUnit() {
    if (walletModel && walletModel->getOptionsModel()) {
        int displayUnitPrev = nDisplayUnit;
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        txHolder->setDisplayUnit(nDisplayUnit);
        ui->listTransactions->update();

        if (displayUnitPrev != nDisplayUnit)
            updateBalances(walletModel->getBalance(), walletModel->getUnconfirmedBalance(), walletModel->getImmatureBalance(),
                           walletModel->getZerocoinBalance(), walletModel->getUnconfirmedZerocoinBalance(), walletModel->getImmatureZerocoinBalance(),
                           walletModel->getWatchBalance(), 0, 0);

    }
}

void DashboardWidget::onSortChanged(const QString& value){
    if (!filter) return;
    int columnIndex = 0;
    Qt::SortOrder order = Qt::DescendingOrder;
    if(!value.isNull()) {
    }
    filter->sort(columnIndex, order);
    ui->listTransactions->update();
}

void DashboardWidget::onSortTypeChanged(const QString& value){
    if (!filter) return;

}

void DashboardWidget::walletSynced(bool sync){
    if (this->isSync != sync) {
        this->isSync = sync;
    }
}

void DashboardWidget::changeTheme(bool isLightTheme, QString& theme){
    static_cast<TxViewHolder*>(this->txViewDelegate->getRowFactory())->isLightTheme = isLightTheme;
}

void DashboardWidget::run(int type) {
}
void DashboardWidget::onError(QString error, int type) {
    inform(tr("Error loading chart: %1").arg(error));
}

void DashboardWidget::processNewTransaction(const QModelIndex& parent, int start, int /*end*/) {
    // Prevent notifications-spam when initial block download is in progress
    if (!walletModel || !clientModel || clientModel->inInitialBlockDownload())
        return;

    if (!txModel || txModel->processingQueuedTransactions())
        return;

    QString date = txModel->index(start, TransactionTableModel::Date, parent).data().toString();
    qint64 amount = txModel->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    QString type = txModel->index(start, TransactionTableModel::Type, parent).data().toString();
    QString address = txModel->index(start, TransactionTableModel::ToAddress, parent).data().toString();

    emit incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amount, type, address);
}

void DashboardWidget::showSend(){
    if(ui->stackedWidget->currentIndex() != 0){
		ui->receivepagebtn->setChecked(false);
		ui->sendpagebtn->setChecked(true);
        ui->stackedWidget->setCurrentIndex(0);
    }
}

void DashboardWidget::showReceive(){
    if(ui->stackedWidget->currentIndex() != 1){
		ui->sendpagebtn->setChecked(false);
		ui->receivepagebtn->setChecked(true);
        ui->stackedWidget->setCurrentIndex(1);
    }
}

void DashboardWidget::onContactsClicked(){
	QPushButton *button = (QPushButton *)sender();
	QString address = button->property("name").toString();
	ui->labelSendAddress->setText(address);
	ui->labelSendAddress->setVisible(true);
}

void DashboardWidget::loadContacts(){

    int height = 120;
    int width = 200;

    if(!menuContacts){
        menuContacts = new ContactsDropdown(
                    width,
                    height,
                    this,
                    true
        );
        menuContacts->setWalletModel(walletModel, AddressTableModel::Send);
       /* connect(menuContacts, &ContactsDropdown::contactSelected, [this](QString address, QString label){
                  ui->lineEditContact->setText(label);
                  ui->labelSendAddress->setText(address);
                  ui->labelSendAddress->setVisible(true);
        });*/

    }

    if(menuContacts->isVisible()){
        menuContacts->hide();
        return;
    }
    QMap<QString, QString> strings = menuContacts->getMini();
    QMap<QString, QString>::const_iterator i;
    int j =0;
	for(i = strings.begin(); i != strings.end(); i++)
	{
		buttons[j] = new QPushButton();
		buttons[j]->setProperty("name", i.value());
        buttons[j]->setText(i.key());
        setCssProperty(buttons[j], "dash-btn-contact");
		ui->horizontalLayoutcontacts->addWidget(buttons[j]);
		connect(buttons[j], SIGNAL(clicked()), this, SLOT(onContactsClicked()));
	}

    menuContacts->hide();
}

const QString marketdetails = "https://explorer.alqo.app/api/getmarketinfo";

QString dequote(QString s)
{
    std::string str(s.toStdString());
    boost::xpressive::sregex nums = boost::xpressive::sregex::compile(":\\\"(-?\\d*(\\.\\d+))\\\"");
    std::string nm(":$1");
    str = regex_replace(str, nums, nm);
    boost::xpressive::sregex tru = boost::xpressive::sregex::compile("\\\"true\\\"");
    std::string tr("true");
    str = regex_replace(str, tru, tr);
    boost::xpressive::sregex fal = boost::xpressive::sregex::compile("\\\"false\\\"");
    std::string fl("false");
    str = regex_replace(str, fal, fl);
    QString res = str.c_str();
    return res;
}

QString DashboardWidget::sendRequest(QString url)
{
    QString Response = "";
    // create custom temporary event loop on stack
    QEventLoop eventLoop;

    // "quit()" the event-loop, when the network request "finished()"
    QNetworkAccessManager mgr;
    QObject::connect(&mgr, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));

    // the HTTP request
    QNetworkRequest req = QNetworkRequest(QUrl(url));

    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    req.setRawHeader("User-Agent", "AlQO Wallet"); //set header
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);


    QNetworkReply* reply = mgr.get(req);
    eventLoop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        Response = reply->readAll();
       // qDebug() << "Success" <<Response;
       //  QMessageBox::information(this,"Success",Response);

        delete reply;
    } else {
	//	qDebug() << "Failure" <<reply->errorString();
        Response = "Error";
    //    QMessageBox::information(this,"Error",reply->errorString());

        delete reply;
    }

    return Response;
}

void DashboardWidget::SetExchangeInfoTextLabels()
{
    // Get the current exchange information
    QString str = "";
    QString response = dequote(sendRequest(marketdetails));

    // parse the json result to get values.
    QJsonDocument jsonResponse = QJsonDocument::fromJson(response.toUtf8());       //get json from str.
    QJsonObject obj = jsonResponse.object();

    QJsonObject objmetrics = obj["metrics"].toArray().first().toObject();
    QJsonObject prices = obj["base_prices"].toObject();

    QJsonDocument doc(prices);
    QString strJson(doc.toJson(QJsonDocument::Compact));

    //QJsonObject ResultObject = ResponseObject.value(QString("metrics")).toObject(); //get result object
    //QJsonArray jsonArraya = jsonObjecta["result"].toArray();
    //QMessageBox::information(this,"Success",strJson);


    // get current values (so we can see color label text baes on increase/decrease)

//    double currentbtc = QTextDocumentFragment::fromHtml(ui->labelbtcusd->text()).toPlainText().toDouble();
//    double currentusd = QTextDocumentFragment::fromHtml(ui->labelusdxlq->text()).toPlainText().toDouble();
//    double currentxlq = QTextDocumentFragment::fromHtml(ui->labelxlqbtc->text()).toPlainText().toDouble();
//    double currentwalletvalue = QTextDocumentFragment::fromHtml(ui->lblwalletvalue->text()).toPlainText().toDouble();
//    int currenttime = QTextDocumentFragment::fromHtml(ui->labeltime->text()).toPlainText().toDouble();

    double bal = QTextDocumentFragment::fromHtml(ui->labelAmountPiv->text()).toPlainText().toDouble();

    double newbtc = objmetrics["latest"].toDouble();
    double newusd = prices["USD"].toDouble();
    double newxlq = newusd * newbtc;
    double newwalletvalue = bal * newxlq;

    //QMessageBox::information(this,"Success",str.number(bal, 'i', 8));

    ui->labelbtcusd->setText("$ " + str.number(newusd, 'i', 2));
    ui->labelxlqbtc->setText(str.number(newbtc, 'i', 8) + " BTC");
    ui->labelusdxlq->setText("$ " + str.number(newxlq, 'i', 2));
    ui->lblwalletvalue->setText("$ " + str.number(newwalletvalue, 'i', 2));

    obj.empty();
    objmetrics.empty();
    prices.empty();

}

DashboardWidget::~DashboardWidget(){
    delete ui;
}