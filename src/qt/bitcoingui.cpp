/*
 * Qt4 rapidz GUI.
 *
 * W.J. van der Laan 2011-2012
 * The rapidz Developers 2011-2012
 */
#include "rapidzgui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "transactiondescdialog.h"
#include "addresstablemodel.h"
#include "transactionview.h"
#include "overviewpage.h"
#include "rapidzunits.h"
#include "guiconstants.h"
#include "askpassphrasedialog.h"
#include "notificator.h"
#include "guiutil.h"
#include "rpcconsole.h"
#include "wallet.h"
#include "rapidzrpc.h"
#include "cloaksend.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QDateTime>
#include <QMovie>
#include <QFileDialog>
#include <QDesktopServices>
#include <QTimer>
#include <QDragEnterEvent>
#include <QUrl>
#include <QStyle>

#include <iostream>

extern CWallet *pwalletMain;
extern int64 nLastCoinStakeSearchInterval;
extern unsigned int nStakeTargetSpacing;

rapidzGUI::rapidzGUI(QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    toolbar(0),
    encryptWalletAction(0),
    changePassphraseAction(0),
    lockWalletToggleAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0)
{
    resize(850, 550);
    setWindowTitle(tr("RapidZ") + " - " + tr("Wallet"));
#ifndef Q_OS_MAC
    qApp->setWindowIcon(QIcon(":icons/rapidz"));
    setWindowIcon(QIcon(":icons/rapidz"));
#else
    //setUnifiedTitleAndToolBarOnMac(true);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    // Accept D&D of URIs
    setAcceptDrops(true);

//    setObjectName("cloakWallet");
    //setStyleSheet("#cloakWallet { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 #eeeeee, stop:1.0 #fefefe); } QToolTip { color: #cecece; background-color: #333333;  border:0px;} ");

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create the tray icon (or setup the dock icon)
    createTrayIcon();

    // Create tabs
    overviewPage = new OverviewPage();

    transactionsPage = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout();
    transactionView = new TransactionView(this);
    vbox->addWidget(transactionView);
    transactionsPage->setLayout(vbox);

    addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);

    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);

    sendCoinsPage = new SendCoinsDialog(this);

    signVerifyMessageDialog = new SignVerifyMessageDialog(this);

    centralStackedWidget = new QStackedWidget(this);
    centralStackedWidget->addWidget(overviewPage);
    centralStackedWidget->addWidget(transactionsPage);
    centralStackedWidget->addWidget(addressBookPage);
    centralStackedWidget->addWidget(receiveCoinsPage);
    centralStackedWidget->addWidget(sendCoinsPage);

//    // Create status bar
//    statusBar();

    // Status bar notification icons
    QWidget *frameBlocks = new QWidget();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    frameBlocks->setStyleSheet("QWidget { background: none; margin-bottom: 5px; }");
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    frameBlocksLayout->setAlignment(Qt::AlignHCenter);
    labelEncryptionIcon = new QLabel();
    labelMintingIcon = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon = new QLabel();
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelEncryptionIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelMintingIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();
    toolbar->addWidget(frameBlocks);

 //toolstip for status bar
/*
    labelEncryptionIcon->setObjectName("labelEncryptionIcon");
    labelConnectionsIcon->setObjectName("labelConnectionsIcon");
    labelBlocksIcon->setObjectName("labelBlocksIcon");
    labelMintingIcon->setObjectName("labelMintingIcon");
    labelEncryptionIcon->setStyleSheet("#labelEncryptionIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
    labelConnectionsIcon->setStyleSheet("#labelConnectionsIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
    labelBlocksIcon->setStyleSheet("#labelBlocksIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");
    labelMintingIcon->setStyleSheet("#labelMintingIcon QToolTip {color:#cecece;background-color:#333333;border:0px;}");

*/
    // Set minting pixmap
    labelMintingIcon->setPixmap(QIcon(":/icons/minting").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelMintingIcon->setEnabled(false);
    // Add timer to update minting icon
    QTimer *timerMintingIcon = new QTimer(labelMintingIcon);
    timerMintingIcon->start(MODEL_UPDATE_DELAY);
    connect(timerMintingIcon, SIGNAL(timeout()), this, SLOT(updateMintingIcon()));
    // Add timer to update minting weights
    QTimer *timerMintingWeights = new QTimer(labelMintingIcon);
    timerMintingWeights->start(30 * 1000);
    connect(timerMintingWeights, SIGNAL(timeout()), this, SLOT(updateMintingWeights()));
    // Set initial values for user and network weights
    nWeight, nNetworkWeight = 0;

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new QProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = qApp->style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    QWidget *centralWidget = new QWidget();
    centralWidget->setContentsMargins(0,0,0,-12);
    QVBoxLayout *centralLayout = new QVBoxLayout(centralWidget);

    centralLayout->addWidget(centralStackedWidget);

    QWidget *progressBarWidget = new QWidget();
    QHBoxLayout *progressBarLayout = new QHBoxLayout(progressBarWidget);
    progressBarLayout->addWidget(progressBar);
    progressBarLayout->addWidget(progressBarLabel);
    progressBarLayout->addStretch();

    centralLayout->addWidget(progressBarWidget);

    setCentralWidget(centralWidget);
//status bar

//    statusBar()->setObjectName("cloakStatusBar");
//    statusBar()->setStyleSheet("#cloakStatusBar { border-top-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #4B4F52, stop:0.5 #8B8F92, stop:1.0 #8B8F92); border-top-width: 2px; border-top-style: inset; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #666666, stop:0.5 #323232, stop:1.0 #111111);  color: #ffffff; } QToolTip { color: #ffffff; background-color: #9D0000; border-width: 1px;border-color:#CA0D0D;}");

    syncIconMovie = new QMovie(":/movies/update_spinner", "mng", this);
    // this->setStyleSheet("background-color: #effbef;");

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

    // Clicking on "Verify Message" in the address book sends you to the verify message tab
    connect(addressBookPage, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString)));
    // Clicking on "Sign Message" in the receive coins page sends you to the sign message tab
    connect(receiveCoinsPage, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString)));

    gotoOverviewPage();
}

rapidzGUI::~rapidzGUI()
{
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
#endif
}

void rapidzGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setToolTip(tr("Show general overview of wallet"));
	overviewAction->setStatusTip(overviewAction->toolTip());
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_O));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send coins"), this);
    sendCoinsAction->setToolTip(tr("Send coins to a RapidZ address"));
	sendCoinsAction->setStatusTip(sendCoinsAction->toolTip());
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive coins"), this);
    receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving payments"));
	receiveCoinsAction->setStatusTip(receiveCoinsAction->toolTip());
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setToolTip(tr("Browse transaction history"));
	historyAction->setStatusTip(historyAction->toolTip());
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_T));
    tabGroup->addAction(historyAction);

    addressBookAction = new QAction(QIcon(":/icons/address-book"), tr("&Address Book"), this);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
	addressBookAction->setStatusTip(addressBookAction->toolTip());
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_A));
    tabGroup->addAction(addressBookAction);

    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setToolTip(tr("Quit application"));
	quitAction->setStatusTip(quitAction->toolTip());
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(QIcon(":/icons/rapidz"), tr("&About RapidZ"), this);
    aboutAction->setToolTip(tr("Show information about RapidZ"));
	aboutAction->setStatusTip(aboutAction->toolTip());
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
    aboutQtAction->setToolTip(tr("Show information about Qt"));
	aboutQtAction->setStatusTip(aboutQtAction->toolTip());
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setToolTip(tr("Modify configuration options for RapidZ"));
	optionsAction->setStatusTip(optionsAction->toolTip());
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(QIcon(":/icons/rapidz"), tr("&Show / Hide"), this);
    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setToolTip(tr("Encrypt or decrypt wallet"));
	encryptWalletAction->setStatusTip(encryptWalletAction->toolTip());
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setToolTip(tr("Backup wallet to another location"));
	backupWalletAction->setStatusTip(backupWalletAction->toolTip());
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setToolTip(tr("Change the passphrase used for wallet encryption"));
	changePassphraseAction->setStatusTip(changePassphraseAction->toolTip());
    lockWalletToggleAction = new QAction(this);
    signMessageAction = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);
    verifyMessageAction = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);

    exportAction = new QAction(QIcon(":/icons/export"), tr("&Export..."), this);
    exportAction->setToolTip(tr("Export the data in the current tab to a file"));
	exportAction->setStatusTip(exportAction->toolTip());
    exportAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setToolTip(tr("Open debugging and diagnostic console"));
	openRPCConsoleAction->setStatusTip(openRPCConsoleAction->toolTip());

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(lockWalletToggleAction, SIGNAL(triggered()), this, SLOT(lockWalletToggle()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
}

void rapidzGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    file->addAction(backupWalletAction);
    file->addAction(exportAction);
    file->addAction(signMessageAction);
    file->addAction(verifyMessageAction);
    file->addSeparator();
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);
    settings->addAction(lockWalletToggleAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(aboutAction);
//    help->addAction(aboutQtAction);

     QString ss("QMenuBar { background-color: transparent; } QMenuBar::item { background-color: transparent; margin: 0 5px; color: #666666 } QMenuBar::item:selected { font-weight: blod; color: black; }");
     appMenuBar->setStyleSheet(ss);
     appMenuBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

static QWidget* makeToolBarSpacer()
{
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    spacer->setStyleSheet( "QWidget { background: none; }");
    return spacer;
}

void rapidzGUI::createToolBars()
{    
    toolbar = new QToolBar(tr("Tabs toolbar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);

    QWidget *menuBarWidget = new QWidget();
    QHBoxLayout *menuBarLayout = new QHBoxLayout(menuBarWidget);
    menuBarLayout->setAlignment(Qt::AlignHCenter);
    menuBarLayout->addWidget(appMenuBar);
    toolbar->addWidget(menuBarWidget);

    QWidget* header = new QWidget();
    header->setMinimumSize(166, 110);
    header->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    header->setStyleSheet("QWidget { background-color: transparent; background-repeat: no-repeat; background-image: url(:/images/header); background-position: center center; }");
    toolbar->addWidget(header);
    toolbar->addSeparator();

    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);
    toolbar->addSeparator();
    toolbar->addAction(lockWalletToggleAction);
    toolbar->addAction(exportAction);

    toolbar->addWidget(makeToolBarSpacer());

    toolbar->setOrientation(Qt::Vertical);
    toolbar->setMovable(false);
    addToolBar(Qt::LeftToolBarArea, toolbar);

    int w = 0;
    int h = 40;
    int n =1;

    foreach(QAction *action, toolbar->actions()) {
        if(n > 1 && n!=3 && n!=9)
            w = std::max(w, toolbar->widgetForAction(action)->width());
        n++;
    }

    n = 1;

    foreach(QAction *action, toolbar->actions()) {
        toolbar->widgetForAction(action)->setFixedWidth(w);
        if(n > 1 && n < 12 && n != 2 && n!=3 && n!=9)
            toolbar->widgetForAction(action)->setFixedHeight(h);
        n++;
    }


//tool bar

//    toolbar->setObjectName("tabsToolbar");
//        toolbar2->setObjectName("actionsToolbar");
//        toolbar->setStyleSheet("QToolButton { min-height:48px;color:#ffffff;border:none;margin:0px;padding:0px;} QToolButton:hover { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #8E8F8F, stop:1.0 #7E7F81); margin:0px; padding:0px; border:none; } QToolButton:checked { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #474748, stop:1.0 #353536); margin:0px; padding:0px; border-right-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 #3b3b3b, stop:0.5 #6c6c6f, stop:1.0 #6c6c6f);border-right-width:2px;border-right-style:inset; border-left-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 #6c6c6f, stop:0.5 #6c6c6f, stop:1.0 #3b3b3b);border-left-width:2px;border-left-style:inset; } QToolButton:pressed { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #474748, stop:1.0 #353536); margin:0px; padding:0px; border:none;} QToolButton:selected { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #474748, stop:1.0 #353536); margin:0px;padding:0px;border:none; } #tabsToolbar { min-height:48px; color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #6e6e71, stop:1.0 #4e4e4f); margin:0px; padding:0px; border-top-color: rgba(160, 160, 160, 191); border-top-width: 1px; border-top-style: inset; } QToolBar::handle { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #6e6e71, stop:1.0 #4e4e4f); }");
//        toolbar2->setStyleSheet("QToolButton { min-height:48px;color:#ffffff;border:none;margin:0px;padding:0px;} QToolButton::disabled { color: #808080; } QToolButton:hover { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #7D7E7F, stop:1.0 #6D6F70); margin:0px; padding:0px; border:none; } QToolButton:checked { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #363637, stop:1.0 #242425); margin:0px; padding:0px; border-right-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 #2a2a2a, stop:0.5 #5b5b5e, stop:1.0 #5b5b5e);border-right-width:2px;border-right-style:inset; border-left-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 #5b5b5e, stop:0.5 #5b5b5e, stop:1.0 #2a2a2a);border-left-width:2px;border-left-style:inset; } QToolButton:pressed { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #363637, stop:1.0 #242425); margin:0px; padding:0px; border:none; border-left-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 #5b5b5e, stop:0.5 #5b5b5e, stop:1.0 #2a2a2a);border-left-width:2px;border-left-style:inset;} QToolButton:selected { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #363637, stop:1.0 #242425); margin:0px;padding:0px;border:none; } #actionsToolbar { color: #ffffff; background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #5D5D60, stop:1.0 #3D3D3F); margin:0px; padding:0px; border-top-color: rgba(160, 160, 160, 191); border-top-width: 1px; border-top-style: inset; } QToolBar::handle { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #5D5D60, stop:1.0 #3D3D3F); }");
}

void rapidzGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if(clientModel)
    {
        // Replace some strings and icons, when using the testnet
        if(clientModel->isTestNet())
        {
            setWindowTitle(windowTitle() + QString(" ") + tr("[testnet]"));
#ifndef Q_OS_MAC
            qApp->setWindowIcon(QIcon(":icons/rapidz_testnet"));
            setWindowIcon(QIcon(":icons/rapidz_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QIcon(":icons/rapidz_testnet"));
#endif
            if(trayIcon)
            {
                trayIcon->setToolTip(tr("RapidZ client") + QString(" ") + tr("[testnet]"));
                trayIcon->setIcon(QIcon(":/icons/rapidz_testnet"));
                toggleHideAction->setIcon(QIcon(":/icons/rapidz_testnet"));
            }

            aboutAction->setIcon(QIcon(":/icons/rapidz_testnet"));
        }

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getNumBlocksOfPeers());
        connect(clientModel, SIGNAL(numBlocksChanged(int,int)), this, SLOT(setNumBlocks(int,int)));

        // Report errors from network/worker thread
        connect(clientModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        rpcConsole->setClientModel(clientModel);
        addressBookPage->setOptionsModel(clientModel->getOptionsModel());
        receiveCoinsPage->setOptionsModel(clientModel->getOptionsModel());
    }
}

void rapidzGUI::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;

    if(walletModel)
    {
        // Report errors from wallet thread
        connect(walletModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        // Put transaction list in tabs
        transactionView->setModel(walletModel);

        overviewPage->setModel(walletModel);
        addressBookPage->setModel(walletModel->getAddressTableModel());
        receiveCoinsPage->setModel(walletModel->getAddressTableModel());
        sendCoinsPage->setModel(walletModel);
        signVerifyMessageDialog->setModel(walletModel);

        setEncryptionStatus(walletModel->getEncryptionStatus());
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(setEncryptionStatus(int)));

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(incomingTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed

        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));
    }
}

void rapidzGUI::createTrayIcon()
{
    QMenu *trayIconMenu;
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip(tr("RapidZ client"));
    trayIcon->setIcon(QIcon(":/icons/rapidz"));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
        this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    trayIconMenu = dockIconHandler->dockMenu();
    dockIconHandler->setMainWindow((QMainWindow*)this);
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif

    notificator = new Notificator(qApp->applicationName(), trayIcon);
}

#ifndef Q_OS_MAC
void rapidzGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHideAction->trigger();
    }
}
#endif

void rapidzGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;
    OptionsDialog dlg;
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void rapidzGUI::aboutClicked()
{
    AboutDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}

void rapidzGUI::setNumConnections(int count)
{
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to RapidZ network.", "", count));
}

void rapidzGUI::setNumBlocks(int count, int nTotalBlocks)
{
    // don't show / hide progress bar and its label if we have no connection to the network
    if (!clientModel || clientModel->getNumConnections() == 0)
    {
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);

        return;
    }

    QString strStatusBarWarnings = clientModel->getStatusBarWarnings();
    QString tooltip;

    if(count < nTotalBlocks)
    {
        int nRemainingBlocks = nTotalBlocks - count;
        float nPercentageDone = count / (nTotalBlocks * 0.01f);

        if (strStatusBarWarnings.isEmpty())
        {
            progressBarLabel->setText(tr("Synchronizing with network..."));
            progressBarLabel->setVisible(true);
            progressBar->setFormat(tr("~%n block(s) remaining", "", nRemainingBlocks));
            progressBar->setMaximum(nTotalBlocks);
            progressBar->setValue(count);
            progressBar->setVisible(true);
        }

        tooltip = tr("Downloaded %1 of %2 blocks of transaction history (%3% done).").arg(count).arg(nTotalBlocks).arg(nPercentageDone, 0, 'f', 2);
    }
    else
    {
        if (strStatusBarWarnings.isEmpty())
            progressBarLabel->setVisible(false);

        progressBar->setVisible(false);
        tooltip = tr("Downloaded %1 blocks of transaction history.").arg(count);
    }

    // Override progressBarLabel text and hide progress bar, when we have warnings to display
    if (!strStatusBarWarnings.isEmpty())
    {
        progressBarLabel->setText(strStatusBarWarnings);
        progressBarLabel->setVisible(true);
        progressBar->setVisible(false);
    }

    tooltip = tr("Current difficulty is %1.").arg(clientModel->GetDifficulty()) + QString("<br>") + tooltip;

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    int secs = lastBlockDate.secsTo(QDateTime::currentDateTime());
    QString text;

    // Represent time from last generated block in human readable text
    if(secs <= 0)
    {
        // Fully up to date. Leave text empty.
    }
    else if(secs < 60)
    {
        text = tr("%n second(s) ago","",secs);
    }
    else if(secs < 60*60)
    {
        text = tr("%n minute(s) ago","",secs/60);
    }
    else if(secs < 24*60*60)
    {
        text = tr("%n hour(s) ago","",secs/(60*60));
    }
    else
    {
        text = tr("%n day(s) ago","",secs/(60*60*24));
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60 && count >= nTotalBlocks)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        overviewPage->showOutOfSyncWarning(false);
    }
    else
    {
        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        labelBlocksIcon->setMovie(syncIconMovie);
        syncIconMovie->start();

        overviewPage->showOutOfSyncWarning(true);
    }

    if(!text.isEmpty())
    {
        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1.").arg(text);
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void rapidzGUI::error(const QString &title, const QString &message, bool modal)
{
    // Report errors from network/worker thread
    if(modal)
    {
        QMessageBox::critical(this, title, message, QMessageBox::Ok, QMessageBox::Ok);
    } else {
        notificator->notify(Notificator::Critical, title, message);
    }
}

void rapidzGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void rapidzGUI::closeEvent(QCloseEvent *event)
{
    if(clientModel)
    {
#ifndef Q_OS_MAC // Ignored on Mac
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
            !clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            qApp->quit();
        }
#endif
    }
    QMainWindow::closeEvent(event);
}

void rapidzGUI::askFee(qint64 nFeeRequired, bool *payFee)
{
    QString strMessage =
        tr("This transaction is over the size limit.  You can still send it for a fee of %1, "
        "which goes to the nodes that process your transaction and helps to support the network.  "
        "Do you want to pay the fee?").arg(
        rapidzUnits::formatWithUnit(rapidzUnits::BTC, nFeeRequired));
    QMessageBox::StandardButton retval = QMessageBox::question(
        this, tr("Confirm transaction fee"), strMessage,
        QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}

void rapidzGUI::incomingTransaction(const QModelIndex & parent, int start, int end)
{
    if(!walletModel || !clientModel)
        return;
    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
        .data(Qt::EditRole).toULongLong();
    if(!clientModel->inInitialBlockDownload())
    {
        // On new transaction, make an info balloon
        // Unless the initial block download is in progress, to prevent balloon-spam
        QString date = ttm->index(start, TransactionTableModel::Date, parent)
            .data().toString();
        QString type = ttm->index(start, TransactionTableModel::Type, parent)
            .data().toString();
        QString address = ttm->index(start, TransactionTableModel::ToAddress, parent)
            .data().toString();
        QIcon icon = qvariant_cast<QIcon>(ttm->index(start,
            TransactionTableModel::ToAddress, parent)
            .data(Qt::DecorationRole));

        notificator->notify(Notificator::Information,
            (amount)<0 ? tr("Sent transaction") :
            tr("Incoming transaction"),
            tr("Date: %1\n"
            "Amount: %2\n"
            "Type: %3\n"
            "Address: %4\n")
            .arg(date)
            .arg(rapidzUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amount, true))
            .arg(type)
            .arg(address), icon);
    }
}

void rapidzGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    centralStackedWidget->setCurrentWidget(overviewPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void rapidzGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    centralStackedWidget->setCurrentWidget(transactionsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), transactionView, SLOT(exportClicked()));
}

void rapidzGUI::gotoAddressBookPage()
{
    addressBookAction->setChecked(true);
    centralStackedWidget->setCurrentWidget(addressBookPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), addressBookPage, SLOT(exportClicked()));
}

void rapidzGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    centralStackedWidget->setCurrentWidget(receiveCoinsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), receiveCoinsPage, SLOT(exportClicked()));
}

void rapidzGUI::gotoSendCoinsPage()
{
    sendCoinsAction->setChecked(true);
    centralStackedWidget->setCurrentWidget(sendCoinsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void rapidzGUI::gotoSignMessageTab(QString addr)
{
    // call show() in showTab_SM()
    signVerifyMessageDialog->showTab_SM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void rapidzGUI::gotoVerifyMessageTab(QString addr)
{
    // call show() in showTab_VM()
    signVerifyMessageDialog->showTab_VM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

void rapidzGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void rapidzGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        int nValidUrisFound = 0;
        QList<QUrl> uris = event->mimeData()->urls();
        foreach(const QUrl &uri, uris)
        {
            if (sendCoinsPage->handleURI(uri.toString()))
                nValidUrisFound++;
        }

        // if valid URIs were found
        if (nValidUrisFound)
            gotoSendCoinsPage();
        else
            notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid RapidZ address or malformed URI parameters."));
    }

    event->acceptProposedAction();
}

void rapidzGUI::handleURI(QString strURI)
{
    // URI has to be valid
    if (sendCoinsPage->handleURI(strURI))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
    }
    else
        notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid RapidZ address or malformed URI parameters."));
}

void rapidzGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        encryptWalletAction->setEnabled(true);
        changePassphraseAction->setEnabled(false);
        lockWalletToggleAction->setVisible(false);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>."));
        encryptWalletAction->setChecked(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        changePassphraseAction->setEnabled(true);
        lockWalletToggleAction->setVisible(true);
        lockWalletToggleAction->setIcon(QIcon(":/icons/lock_closed"));
        lockWalletToggleAction->setText(tr("&Lock Wallet"));
        lockWalletToggleAction->setToolTip(tr("Lock wallet"));
        lockWalletToggleAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_L));
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>."));
        encryptWalletAction->setChecked(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        changePassphraseAction->setEnabled(true);
        lockWalletToggleAction->setVisible(true);
        lockWalletToggleAction->setIcon(QIcon(":/icons/lock_open"));
        lockWalletToggleAction->setText(tr("&Unlock Wallet..."));
        lockWalletToggleAction->setToolTip(tr("Unlock wallet"));
        lockWalletToggleAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_U));
        break;
    }
}

void rapidzGUI::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt:
        AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus(walletModel->getEncryptionStatus());
}

void rapidzGUI::backupWallet()
{
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
    QString filename = QFileDialog::getSaveFileName(this, tr("Backup Wallet"), saveDir, tr("Wallet Data (*.dat)"));
    if(!filename.isEmpty()) {
        if(!walletModel->backupWallet(filename)) {
            QMessageBox::warning(this, tr("Backup Failed"), tr("There was an error trying to save the wallet data to the new location."));
        }
    }
}

void rapidzGUI::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void rapidzGUI::lockWalletToggle()
{
    if(!walletModel)
        return;

    // Unlock wallet when requested by wallet model
    if(walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog::Mode mode = AskPassphraseDialog::UnlockMinting;
        AskPassphraseDialog dlg(mode, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
    else
        walletModel->setWalletLocked(true);
}

void rapidzGUI::showNormalIfMinimized(bool fToggleHidden)
{
    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

void rapidzGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void rapidzGUI::updateMintingIcon()
{
    if (pwalletMain && pwalletMain->IsLocked())
    {
        labelMintingIcon->setToolTip(tr("Not minting because wallet is locked."));
        labelMintingIcon->setEnabled(false);
    }
    else if (vNodes.empty())
    {
        labelMintingIcon->setToolTip(tr("Not minting because wallet is offline."));
        labelMintingIcon->setEnabled(false);
    }
    else if (IsInitialBlockDownload())
    {
        labelMintingIcon->setToolTip(tr("Not minting because wallet is syncing."));
        labelMintingIcon->setEnabled(false);
    }
    else if (!nWeight)
    {
        labelMintingIcon->setToolTip(tr("Not minting because you don't have mature coins."));
        labelMintingIcon->setEnabled(false);
    }
    else if (nLastCoinStakeSearchInterval)
    {
        uint64 nEstimateTime = nStakeTargetSpacing * nNetworkWeight / nWeight;

        QString text;
        if (nEstimateTime < 60)
        {
            text = tr("%n second(s)", "", nEstimateTime);
        }
        else if (nEstimateTime < 60*60)
        {
            text = tr("%n minute(s)", "", nEstimateTime/60);
        }
        else if (nEstimateTime < 24*60*60)
        {
            text = tr("%n hour(s)", "", nEstimateTime/(60*60));
        }
        else
        {
            text = tr("%n day(s)", "", nEstimateTime/(60*60*24));
        }

        labelMintingIcon->setEnabled(true);
        labelMintingIcon->setToolTip(tr("Minting.<br>Your weight is %1.<br>Network weight is %2.<br>Expected time to earn reward is %3.").arg(nWeight).arg(nNetworkWeight).arg(text));
    }
    else
    {
        labelMintingIcon->setToolTip(tr("Not minting."));
        labelMintingIcon->setEnabled(false);
    }
}

void rapidzGUI::updateMintingWeights()
{
    // Only update if we have the network's current number of blocks, or weight(s) are zero (fixes lagging GUI)
    if ((clientModel && clientModel->getNumBlocks() == clientModel->getNumBlocksOfPeers()) || !nWeight || !nNetworkWeight)
    {
        nWeight = 0;

        if (pwalletMain)
            pwalletMain->GetStakeWeight(*pwalletMain, nMinMax, nMinMax, nWeight);

        nNetworkWeight = GetPoSKernelPS();
    }
}

