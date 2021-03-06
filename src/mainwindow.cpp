/* -*- mode: c++; c-basic-offset:4 -*-
    mainwindow.cpp

    This file is part of Kleopatra, the KDE keymanager
    Copyright (c) 2007 Klarälvdalens Datakonsult AB

    Kleopatra is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kleopatra is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    In addition, as a special exception, the copyright holders give
    permission to link the code of this program with any edition of
    the Qt library by Trolltech AS, Norway (or with modified versions
    of Qt that use the same license as Qt), and distribute linked
    combinations including the two.  You must obey the GNU General
    Public License in all respects for all of the code used other than
    Qt.  If you modify this file, you may extend this exception to
    your version of the file, but you are not obligated to do so.  If
    you do not wish to do so, delete this exception statement from
    your version.
*/

#include <config-kleopatra.h>

#include "mainwindow.h"
#include "aboutdata.h"

#include "view/padwidget.h"
#include "view/searchbar.h"
#include "view/tabwidget.h"
#include "view/keylistcontroller.h"
#include "view/keycacheoverlay.h"
#include "view/smartcardwidget.h"
#include "view/welcomewidget.h"

#include "commands/selftestcommand.h"
#include "commands/importcrlcommand.h"
#include "commands/importcertificatefromfilecommand.h"
#include "commands/decryptverifyfilescommand.h"
#include "commands/signencryptfilescommand.h"

#include "utils/detail_p.h"
#include "utils/gnupg-helper.h"
#include "utils/action_data.h"
#include "utils/filedialog.h"
#include "utils/clipboardmenu.h"

#include "dialogs/updatenotification.h"

#include <KXMLGUIFactory>
#include <QApplication>
#include <QSize>
#include <QLineEdit>
#include <KActionMenu>
#include <KActionCollection>
#include <KLocalizedString>
#include <KStandardAction>
#include <QAction>
#include <KAboutData>
#include <KMessageBox>
#include <KStandardGuiItem>
#include <KShortcutsDialog>
#include <KEditToolBar>
#include "kleopatra_debug.h"
#include <KConfigGroup>

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QMenu>
#include <QTimer>
#include <QProcess>
#include <QVBoxLayout>
#include <QMimeData>
#include <QDesktopServices>
#include <QDir>
#include <QStackedWidget>
#include <QStatusBar>
#include <QLabel>

#include <Libkleo/Formatting>
#include <Libkleo/KeyListModel>
#include <Libkleo/KeyListSortFilterProxyModel>
#include <Libkleo/CryptoConfigDialog>
#include <Libkleo/Stl_Util>
#include <Libkleo/Classify>
#include <Libkleo/KeyCache>
#include <QGpgME/CryptoConfig>
#include <QGpgME/Protocol>

#include <vector>
#include <KSharedConfig>

using namespace Kleo;
using namespace Kleo::Commands;
using namespace GpgME;

static KGuiItem KStandardGuiItem_quit()
{
    static const QString app = KAboutData::applicationData().componentName();
    KGuiItem item = KStandardGuiItem::quit();
    item.setText(i18nc("Quit [ApplicationName]", "&Quit %1", app));
    return item;
}

static KGuiItem KStandardGuiItem_close()
{
    KGuiItem item = KStandardGuiItem::close();
    item.setText(i18n("Only &Close Window"));
    return item;
}

static bool isQuitting = false;

class MainWindow::Private
{
    friend class ::MainWindow;
    MainWindow *const q;

public:
    explicit Private(MainWindow *qq);
    ~Private();

    template <typename T>
    void createAndStart()
    {
        (new T(this->currentView(), &this->controller))->start();
    }
    template <typename T>
    void createAndStart(QAbstractItemView *view)
    {
        (new T(view, &this->controller))->start();
    }
    template <typename T>
    void createAndStart(const QStringList &a)
    {
        (new T(a, this->currentView(), &this->controller))->start();
    }
    template <typename T>
    void createAndStart(const QStringList &a, QAbstractItemView *view)
    {
        (new T(a, view, &this->controller))->start();
    }

    void closeAndQuit()
    {
        const QString app = KAboutData::applicationData().componentName();
        const int rc = KMessageBox::questionYesNoCancel(q,
                       i18n("%1 may be used by other applications as a service.\n"
                            "You may instead want to close this window without exiting %1.", app),
                       i18n("Really Quit?"), KStandardGuiItem_close(), KStandardGuiItem_quit(), KStandardGuiItem::cancel(),
                       QLatin1String("really-quit-") + app.toLower());
        if (rc == KMessageBox::Cancel) {
            return;
        }
        isQuitting = true;
        if (!q->close()) {
            return;
        }
        // WARNING: 'this' might be deleted at this point!
        if (rc == KMessageBox::No) {
            qApp->quit();
        }
    }
    void configureToolbars()
    {
        KEditToolBar dlg(q->factory());
        dlg.exec();
    }
    void editKeybindings()
    {
        KShortcutsDialog::configure(q->actionCollection(), KShortcutsEditor::LetterShortcutsAllowed);
        updateSearchBarClickMessage();
    }

    void updateSearchBarClickMessage()
    {
        const QString shortcutStr = focusToClickSearchAction->shortcut().toString();
        ui.searchBar->updateClickMessage(shortcutStr);
    }

    void updateStatusBar()
    {
        const auto complianceMode = Formatting::complianceMode();
        if (!complianceMode.isEmpty()) {
            auto statusBar = new QStatusBar;
            q->setStatusBar(statusBar);
            auto statusLbl = new QLabel(i18nc("Compliance means that GnuPG is running in a more restricted mode e.g. to handle restricted documents.",
                                        // The germans want some extra sausage
                                        "Compliance: %1", complianceMode == QLatin1String("de-vs") ? QStringLiteral ("VS-NfD") : complianceMode));
            statusBar->insertPermanentWidget(0, statusLbl);

        } else {
            q->setStatusBar(nullptr);
        }
    }

    void selfTest()
    {
        createAndStart<SelfTestCommand>();
    }
    void configureBackend();

    void showHandbook();

    void gnupgLogViewer()
    {
        if (!QProcess::startDetached(QStringLiteral("kwatchgnupg"), QStringList()))
            KMessageBox::error(q, i18n("Could not start the GnuPG Log Viewer (kwatchgnupg). "
                                       "Please check your installation."),
                               i18n("Error Starting KWatchGnuPG"));
    }

    void forceUpdateCheck()
    {
        UpdateNotification::forceUpdateCheck(q);
    }

    void openCompendium()
    {
        QDir datadir(QCoreApplication::applicationDirPath() + QStringLiteral("/../share/gpg4win"));
        const auto path = datadir.filePath(i18nc("The Gpg4win compendium is only available"
                                                 "at this point (24.7.2017) in german and english."
                                                 "Please check with Gpg4win before translating this filename.",
                                                 "gpg4win-compendium-en.pdf"));
        qCDebug(KLEOPATRA_LOG) << "Opening Compendium at:" << path;
        // The compendium is always installed. So this should work. Otherwise
        // we have debug output.
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }

    void slotConfigCommitted();
    void slotContextMenuRequested(QAbstractItemView *, const QPoint &p)
    {
        if (QMenu *const menu = qobject_cast<QMenu *>(q->factory()->container(QStringLiteral("listview_popup"), q))) {
            menu->exec(p);
        } else {
            qCDebug(KLEOPATRA_LOG) << "no \"listview_popup\" <Menu> in kleopatra's ui.rc file";
        }
    }

    void slotFocusQuickSearch()
    {
        ui.searchBar->lineEdit()->setFocus();
    }

    void toggleSmartcardView()
    {
        const auto coll = q->actionCollection();
        if (coll) {
            auto act = coll->action(QStringLiteral("pad_view"));
            if (act) {
                act->setChecked(false);
            }
        }
        if (ui.stackWidget->currentWidget() == ui.scWidget) {
            ui.stackWidget->setCurrentWidget(ui.searchTab);
            checkWelcomePage();
            return;
        }
        ui.stackWidget->setCurrentWidget(ui.scWidget);
    }

    void togglePadView()
    {
        const auto coll = q->actionCollection();
        if (coll) {
            auto act = coll->action(QStringLiteral("manage_smartcard"));
            if (act) {
                act->setChecked(false);
            }
        }
        if (ui.stackWidget->currentWidget() == ui.padWidget) {
            ui.stackWidget->setCurrentWidget(ui.searchTab);
            checkWelcomePage();
            return;
        }
        if (!ui.padWidget) {
            ui.padWidget = new PadWidget;
            ui.stackWidget->addWidget(ui.padWidget);
        }
        ui.stackWidget->setCurrentWidget(ui.padWidget);
        ui.stackWidget->resize(ui.padWidget->sizeHint());
    }

private:
    void setupActions();

    QAbstractItemView *currentView() const
    {
        return ui.tabWidget.currentView();
    }

    void checkWelcomePage()
    {
        const auto curWidget = ui.stackWidget->currentWidget();
        if (curWidget == ui.scWidget || curWidget == ui.padWidget) {
           return;
        }
        if (KeyCache::instance()->keys().empty()) {
            ui.stackWidget->setCurrentWidget(ui.welcomeWidget);
        } else {
            ui.stackWidget->setCurrentWidget(ui.searchTab);
        }
    }

private:
    Kleo::KeyListController controller;
    bool firstShow : 1;
    struct UI {
        QWidget *searchTab;
        TabWidget tabWidget;
        SearchBar *searchBar;
        PadWidget *padWidget;
        SmartCardWidget *scWidget;
        WelcomeWidget *welcomeWidget;
        QStackedWidget *stackWidget;
        explicit UI(MainWindow *q);
    } ui;
    QAction *focusToClickSearchAction;
    ClipboardMenu *clipboadMenu;
};

MainWindow::Private::UI::UI(MainWindow *q)
    : tabWidget(q), padWidget(nullptr)
{
    KDAB_SET_OBJECT_NAME(tabWidget);

    searchTab = new QWidget;
    QVBoxLayout *vbox = new QVBoxLayout(searchTab);
    vbox->setSpacing(0);
    searchBar = new SearchBar;
    vbox->addWidget(searchBar);
    tabWidget.connectSearchBar(searchBar);
    vbox->addWidget(&tabWidget);

    QWidget *mainWidget = new QWidget;
    auto mainLayout = new QVBoxLayout(mainWidget);
    stackWidget = new QStackedWidget;

    mainLayout->addWidget(stackWidget);

    stackWidget->addWidget(searchTab);

    new KeyCacheOverlay(mainWidget, q);

    scWidget = new SmartCardWidget();
    stackWidget->addWidget(scWidget);

    welcomeWidget = new WelcomeWidget();
    stackWidget->addWidget(welcomeWidget);

    q->setCentralWidget(mainWidget);
}

MainWindow::Private::Private(MainWindow *qq)
    : q(qq),
      controller(q),
      firstShow(true),
      ui(q)
{
    KDAB_SET_OBJECT_NAME(controller);

    AbstractKeyListModel *flatModel = AbstractKeyListModel::createFlatKeyListModel(q);
    AbstractKeyListModel *hierarchicalModel = AbstractKeyListModel::createHierarchicalKeyListModel(q);

    KDAB_SET_OBJECT_NAME(flatModel);
    KDAB_SET_OBJECT_NAME(hierarchicalModel);

    controller.setFlatModel(flatModel);
    controller.setHierarchicalModel(hierarchicalModel);
    controller.setTabWidget(&ui.tabWidget);

    ui.tabWidget.setFlatModel(flatModel);
    ui.tabWidget.setHierarchicalModel(hierarchicalModel);

    ui.stackWidget->setCurrentWidget(ui.searchTab);

    setupActions();

    connect(&controller, SIGNAL(contextMenuRequested(QAbstractItemView*,QPoint)), q, SLOT(slotContextMenuRequested(QAbstractItemView*,QPoint)));
    connect(ui.scWidget, &SmartCardWidget::backRequested, q, [this]() {
            auto action = q->actionCollection()->action(QStringLiteral("manage_smartcard"));
            Q_ASSERT(action);
            action->setChecked(false);
        });
    connect(KeyCache::instance().get(), &KeyCache::keyListingDone, q, [this] () {checkWelcomePage();});

    q->createGUI(QStringLiteral("kleopatra.rc"));

    q->setAcceptDrops(true);

    // set default window size
    q->resize(QSize(1024, 500));
    q->setAutoSaveSettings();

    updateSearchBarClickMessage();
    updateStatusBar();
}

MainWindow::Private::~Private() {}

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
    : KXmlGuiWindow(parent, flags), d(new Private(this))
{}

MainWindow::~MainWindow() {}

void MainWindow::Private::setupActions()
{

    KActionCollection *const coll = q->actionCollection();

    const action_data action_data[] = {
        // most have been MOVED TO keylistcontroller.cpp
        // Tools menu
#ifndef Q_OS_WIN
        {
            "tools_start_kwatchgnupg", i18n("GnuPG Log Viewer"), QString(),
            "kwatchgnupg", q, SLOT(gnupgLogViewer()), QString(), false, true
        },
#endif
#ifdef Q_OS_WIN
        {
            "help_check_updates", i18n("Check for updates"), QString(),
            "gpg4win-compact", q, SLOT(forceUpdateCheck()), QString(), false, true
        },
        {
            "help_show_compendium", i18n("Gpg4win Compendium"), QString(),
            "gpg4win-compact", q, SLOT(openCompendium()), QString(), false, true
        },
#endif
        {
            "pad_view", i18nc("Title for a generic data input / output area supporting text actions.", "Notepad"),
            i18n("Switch to Pad view."), "edittext", q, SLOT(togglePadView()), QString(), true, true
        },
        // most have been MOVED TO keylistcontroller.cpp
#if 0
        {
            "configure_backend", i18n("Configure GnuPG Backend..."), QString(),
            0, q, SLOT(configureBackend()), QString(), false, true
        },
#endif
        // Settings menu
        {
            "settings_self_test", i18n("Perform Self-Test"), QString(),
            nullptr, q, SLOT(selfTest()), QString(), false, true
        },
        {
            "manage_smartcard", i18n("Manage Smartcards"),
            i18n("Edit or initialize a crypto hardware token."), "secure-card", q, SLOT(toggleSmartcardView()), QString(), true, true
        }

        // most have been MOVED TO keylistcontroller.cpp
    };

    make_actions_from_data(action_data, /*sizeof action_data / sizeof *action_data,*/ coll);

    if (QAction *action = coll->action(QStringLiteral("configure_backend"))) {
        action->setMenuRole(QAction::NoRole);    //prevent Qt OS X heuristics for config* actions
    }

    KStandardAction::close(q, SLOT(close()), coll);
    KStandardAction::quit(q, SLOT(closeAndQuit()), coll);
    KStandardAction::configureToolbars(q, SLOT(configureToolbars()), coll);
    KStandardAction::keyBindings(q, SLOT(editKeybindings()), coll);
    KStandardAction::preferences(qApp, SLOT(openOrRaiseConfigDialog()), coll);

    focusToClickSearchAction = new QAction(i18n("Set Focus to Quick Search"), q);
    coll->addAction(QStringLiteral("focus_to_quickseach"), focusToClickSearchAction);
    coll->setDefaultShortcut(focusToClickSearchAction, QKeySequence(Qt::ALT + Qt::Key_Q));
    connect(focusToClickSearchAction, SIGNAL(triggered(bool)), q, SLOT(slotFocusQuickSearch()));
    clipboadMenu = new ClipboardMenu(q);
    clipboadMenu->setMainWindow(q);
    clipboadMenu->clipboardMenu()->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
    clipboadMenu->clipboardMenu()->setDelayed(false);
    coll->addAction(QStringLiteral("clipboard_menu"), clipboadMenu->clipboardMenu());

    q->setStandardToolBarMenuEnabled(true);

    controller.createActions(coll);

    ui.tabWidget.createActions(coll);
}

void MainWindow::Private::configureBackend()
{
    QGpgME::CryptoConfig *const config = QGpgME::cryptoConfig();
    if (!config) {
        KMessageBox::error(q, i18n("Could not configure the cryptography backend (gpgconf tool not found)"), i18n("Configuration Error"));
        return;
    }

    Kleo::CryptoConfigDialog dlg(config);

    const int result = dlg.exec();

    // Forget all data parsed from gpgconf, so that we show updated information
    // when reopening the configuration dialog.
    config->clear();

    if (result == QDialog::Accepted) {
#if 0
        // Tell other apps (e.g. kmail) that the gpgconf data might have changed
        QDBusMessage message =
            QDBusMessage::createSignal(QString(), "org.kde.kleo.CryptoConfig", "changed");
        QDBusConnection::sessionBus().send(message);
#endif
    }
}

void MainWindow::Private::slotConfigCommitted()
{
    controller.updateConfig();
    updateStatusBar();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    // KMainWindow::closeEvent() insists on quitting the application,
    // so do not let it touch the event...
    qCDebug(KLEOPATRA_LOG);
    if (d->controller.hasRunningCommands()) {
        if (d->controller.shutdownWarningRequired()) {
            const int ret = KMessageBox::warningContinueCancel(this, i18n("There are still some background operations ongoing. "
                            "These will be terminated when closing the window. "
                            "Proceed?"),
                            i18n("Ongoing Background Tasks"));
            if (ret != KMessageBox::Continue) {
                e->ignore();
                return;
            }
        }
        d->controller.cancelCommands();
        if (d->controller.hasRunningCommands()) {
            // wait for them to be finished:
            setEnabled(false);
            QEventLoop ev;
            QTimer::singleShot(100, &ev, &QEventLoop::quit);
            connect(&d->controller, &KeyListController::commandsExecuting, &ev, &QEventLoop::quit);
            ev.exec();
            if (d->controller.hasRunningCommands())
                qCWarning(KLEOPATRA_LOG)
                        << "controller still has commands running, this may crash now...";
            setEnabled(true);
        }
    }
    if (isQuitting || qApp->isSavingSession()) {
        d->ui.tabWidget.saveViews(KSharedConfig::openConfig().data());
        KConfigGroup grp(KConfigGroup(KSharedConfig::openConfig(), autoSaveGroup()));
        saveMainWindowSettings(grp);
        e->accept();
    } else {
        e->ignore();
        hide();
    }
}

void MainWindow::showEvent(QShowEvent *e)
{
    KXmlGuiWindow::showEvent(e);
    if (d->firstShow) {
        d->ui.tabWidget.loadViews(KSharedConfig::openConfig().data());
        d->firstShow = false;
    }

    if (!savedGeometry.isEmpty()) {
        restoreGeometry(savedGeometry);
    }

}

void MainWindow::hideEvent(QHideEvent *e)
{
    savedGeometry = saveGeometry();
    KXmlGuiWindow::hideEvent(e);
}

void MainWindow::importCertificatesFromFile(const QStringList &files)
{
    if (!files.empty()) {
        d->createAndStart<ImportCertificateFromFileCommand>(files);
    }
}

static QStringList extract_local_files(const QMimeData *data)
{
    const QList<QUrl> urls = data->urls();
    // begin workaround KDE/Qt misinterpretation of text/uri-list
    QList<QUrl>::const_iterator end = urls.end();
    if (urls.size() > 1 && !urls.back().isValid()) {
        --end;
    }
    // end workaround
    QStringList result;
    std::transform(urls.begin(), end,
                   std::back_inserter(result),
                   std::mem_fn(&QUrl::toLocalFile));
    result.erase(std::remove_if(result.begin(), result.end(),
                                std::mem_fn(&QString::isEmpty)), result.end());
    return result;
}

static bool can_decode_local_files(const QMimeData *data)
{
    if (!data) {
        return false;
    }
    return !extract_local_files(data).empty();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    qCDebug(KLEOPATRA_LOG);

    if (can_decode_local_files(e->mimeData())) {
        e->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *e)
{
    qCDebug(KLEOPATRA_LOG);

    if (!can_decode_local_files(e->mimeData())) {
        return;
    }

    e->setDropAction(Qt::CopyAction);

    const QStringList files = extract_local_files(e->mimeData());

    const unsigned int classification = classify(files);

    QMenu menu;

    QAction *const signEncrypt = menu.addAction(i18n("Sign/Encrypt..."));
    QAction *const decryptVerify = mayBeAnyMessageType(classification) ? menu.addAction(i18n("Decrypt/Verify...")) : nullptr;
    if (signEncrypt || decryptVerify) {
        menu.addSeparator();
    }

    QAction *const importCerts = mayBeAnyCertStoreType(classification) ? menu.addAction(i18n("Import Certificates")) : nullptr;
    QAction *const importCRLs  = mayBeCertificateRevocationList(classification) ? menu.addAction(i18n("Import CRLs")) : nullptr;
    if (importCerts || importCRLs) {
        menu.addSeparator();
    }

    if (!signEncrypt && !decryptVerify && !importCerts && !importCRLs) {
        return;
    }

    menu.addAction(i18n("Cancel"));

    const QAction *const chosen = menu.exec(mapToGlobal(e->pos()));

    if (!chosen) {
        return;
    }

    if (chosen == signEncrypt) {
        d->createAndStart<SignEncryptFilesCommand>(files);
    } else if (chosen == decryptVerify) {
        d->createAndStart<DecryptVerifyFilesCommand>(files);
    } else if (chosen == importCerts) {
        d->createAndStart<ImportCertificateFromFileCommand>(files);
    } else if (chosen == importCRLs) {
        d->createAndStart<ImportCrlCommand>(files);
    }

    e->accept();
}

void MainWindow::readProperties(const KConfigGroup &cg)
{
    qCDebug(KLEOPATRA_LOG);
    KXmlGuiWindow::readProperties(cg);
    setHidden(cg.readEntry("hidden", false));
}

void MainWindow::saveProperties(KConfigGroup &cg)
{
    qCDebug(KLEOPATRA_LOG);
    KXmlGuiWindow::saveProperties(cg);
    cg.writeEntry("hidden", isHidden());
}

#include "moc_mainwindow.cpp"
