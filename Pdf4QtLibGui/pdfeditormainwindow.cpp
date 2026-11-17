// MIT License
//
// Copyright (c) 2018-2026 Jakub Melka and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pdfeditormainwindow.h"
#include "pdfannotationstyletoolbar.h"
#include "pdftoolbarmanager.h"
#include "ui_pdfeditormainwindow.h"

#include "pdfaboutdialog.h"
#include "pdfsidebarwidget.h"
#include "pdfadvancedfindwidget.h"
#include "pdfviewersettingsdialog.h"
#include "pdfdocumentpropertiesdialog.h"
#include "pdfrendertoimagesdialog.h"

#include "pdfdocumentreader.h"
#include "pdfvisitor.h"
#include "pdfstreamfilters.h"
#include "pdfdrawwidget.h"
#include "pdfdrawspacecontroller.h"
#include "pdfrenderingerrorswidget.h"
#include "pdffont.h"
#include "pdfitemmodels.h"
#include "pdfutils.h"
#include "pdfexecutionpolicy.h"
#include "pdfwidgetutils.h"
#include "pdfdocumentwriter.h"
#include "pdfsignaturehandler.h"
#include "pdfadvancedtools.h"
#include "pdfwidgetutils.h"
#include "pdfactioncombobox.h"

#include <QPainter>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QApplication>
#include <QStandardPaths>
#include <QDockWidget>
#include <QTreeView>
#include <QLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QSpinBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QLockFile>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>
#include <QtConcurrent/QtConcurrent>
#include <QToolButton>
#include <QActionGroup>
#include <QToolBar>
#include <QWidgetAction>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>

#include <algorithm>

#include "pdfdbgheap.h"

#ifdef Q_OS_WIN
#include "Windows.h"
#endif

namespace pdfviewer
{

PDFEditorMainWindow::PDFEditorMainWindow(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::PDFEditorMainWindow),
    m_actionManager(new PDFActionManager(this)),
    m_programController(new PDFProgramController(this)),
    m_sidebarWidget(nullptr),
    m_pagesMenu(nullptr),
    m_styleToolBar(nullptr),
    m_toolBarManager(nullptr),
    m_sidebarDockWidget(nullptr),
    m_advancedFindWidget(nullptr),
    m_advancedFindDockWidget(nullptr),
    m_pageNumberSpinBox(nullptr),
    m_pageNumberLabel(nullptr),
    m_sidebarPageSpinBox(nullptr),
    m_sidebarPageCountLabel(nullptr),
    m_pageZoomSpinBox(nullptr),
    m_toolBarPositionMenu(nullptr),
    m_toolBarsTopAction(nullptr),
    m_toolBarsRightAction(nullptr),
    m_isLoadingUI(false),
    m_progress(new pdf::PDFProgress(this)),
    m_progressTaskbarIndicator(new PDFWinTaskBarProgress(this)),
    m_progressBarOnStatusBar(nullptr),
    m_progressBarLeftLabelOnStatusBar(nullptr),
    m_isChangingProgressStep(false)
{
    ui->setupUi(this);

    setAcceptDrops(true);

    // Content of the toolbars, customizable in the options
    m_toolBarManager = new PDFToolBarManager(this);

    // Initialize toolbar icon size
    adjustToolbar(ui->mainToolBar);
    ui->mainToolBar->setWindowTitle(tr("Standard"));

    // Initialize status bar
    m_progressBarOnStatusBar = new QProgressBar(this);
    m_progressBarOnStatusBar->setHidden(true);
    m_progressBarLeftLabelOnStatusBar = new QLabel(this);
    m_progressBarLeftLabelOnStatusBar->setHidden(true);
    statusBar()->addPermanentWidget(m_progressBarLeftLabelOnStatusBar);
    statusBar()->addPermanentWidget(m_progressBarOnStatusBar);

    // Initialize actions
    m_actionManager->setAction(PDFActionManager::Open, ui->actionOpen);
    m_actionManager->setAction(PDFActionManager::Close, ui->actionClose);
    m_actionManager->setAction(PDFActionManager::AutomaticDocumentRefresh, ui->actionAutomaticDocumentRefresh);
    m_actionManager->setAction(PDFActionManager::Quit, ui->actionQuit);
    m_actionManager->setAction(PDFActionManager::ZoomIn, ui->actionZoom_In);
    m_actionManager->setAction(PDFActionManager::ZoomOut, ui->actionZoom_Out);
    m_actionManager->setAction(PDFActionManager::Find, ui->actionFind);
    m_actionManager->setAction(PDFActionManager::FindPrevious, ui->actionFindPrevious);
    m_actionManager->setAction(PDFActionManager::FindNext, ui->actionFindNext);
    m_actionManager->setAction(PDFActionManager::SelectTextAll, ui->actionSelectTextAll);
    m_actionManager->setAction(PDFActionManager::DeselectText, ui->actionDeselectText);
    m_actionManager->setAction(PDFActionManager::CopyText, ui->actionCopyText);
    m_actionManager->setAction(PDFActionManager::RotateRight, ui->actionRotateRight);
    m_actionManager->setAction(PDFActionManager::RotateLeft, ui->actionRotateLeft);
    m_actionManager->setAction(PDFActionManager::Print, ui->actionPrint);
    m_actionManager->setAction(PDFActionManager::Undo, ui->actionUndo);
    m_actionManager->setAction(PDFActionManager::Redo, ui->actionRedo);
    m_actionManager->setAction(PDFActionManager::Save, ui->actionSave);
    m_actionManager->setAction(PDFActionManager::SaveAs, ui->actionSave_As);
    m_actionManager->setAction(PDFActionManager::GoToDocumentStart, ui->actionGoToDocumentStart);
    m_actionManager->setAction(PDFActionManager::GoToDocumentEnd, ui->actionGoToDocumentEnd);
    m_actionManager->setAction(PDFActionManager::GoToNextPage, ui->actionGoToNextPage);
    m_actionManager->setAction(PDFActionManager::GoToPreviousPage, ui->actionGoToPreviousPage);
    m_actionManager->setAction(PDFActionManager::GoToNextLine, ui->actionGoToNextLine);
    m_actionManager->setAction(PDFActionManager::GoToPreviousLine, ui->actionGoToPreviousLine);
    m_actionManager->setAction(PDFActionManager::CreateStickyNoteComment, ui->actionStickyNoteComment);
    m_actionManager->setAction(PDFActionManager::CreateStickyNoteHelp, ui->actionStickyNoteHelp);
    m_actionManager->setAction(PDFActionManager::CreateStickyNoteNote, ui->actionStickyNoteNote);
    m_actionManager->setAction(PDFActionManager::CreateTextHighlight, ui->actionCreateTextHighlight);
    m_actionManager->setAction(PDFActionManager::CreateTextUnderline, ui->actionCreateTextUnderline);
    m_actionManager->setAction(PDFActionManager::CreateTextStrikeout, ui->actionCreateTextStrikeout);
    m_actionManager->setAction(PDFActionManager::CreateTextSquiggly, ui->actionCreateTextSquiggly);
    m_actionManager->setAction(PDFActionManager::CreateHyperlink, ui->actionCreateHyperlink);
    m_actionManager->setAction(PDFActionManager::CreateHyperlinkToThisPDFFit, ui->actionCreateHyperlinkToThisPDFFit);
    m_actionManager->setAction(PDFActionManager::CreateHyperlinkToThisPDFFitH, ui->actionCreateHyperlinkToThisPDFFitH);
    m_actionManager->setAction(PDFActionManager::CreateHyperlinkToThisPDFFitV, ui->actionCreateHyperlinkToThisPDFFitV);
    m_actionManager->setAction(PDFActionManager::CreateHyperlinkToThisPDFFitR, ui->actionCreateHyperlinkToThisPDFFitR);
    m_actionManager->setAction(PDFActionManager::CreateHyperlinkToThisPDFFitB, ui->actionCreateHyperlinkToThisPDFFitB);
    m_actionManager->setAction(PDFActionManager::CreateHyperlinkToThisPDFFitBH, ui->actionCreateHyperlinkToThisPDFFitBH);
    m_actionManager->setAction(PDFActionManager::CreateHyperlinkToThisPDFFitBV, ui->actionCreateHyperlinkToThisPDFFitBV);
    m_actionManager->setAction(PDFActionManager::CreateHyperlinkToThisPDFXYZ, ui->actionCreateHyperlinkToThisPDFXYZ);
    m_actionManager->setAction(PDFActionManager::CreateHyperlinkToThisPDFXYZInheritZoom, ui->actionCreateHyperlinkToThisPDFXYZInheritZoom);
    m_actionManager->setAction(PDFActionManager::InsertPageNumbers, ui->actionInsertPageNumbers);
    m_actionManager->setAction(PDFActionManager::CreateInlineText, ui->actionInlineText);
    m_actionManager->setAction(PDFActionManager::CreateImage, ui->actionCreateImage);
    m_actionManager->setAction(PDFActionManager::CreateStraightLine, ui->actionCreateStraightLine);
    m_actionManager->setAction(PDFActionManager::CreatePolyline, ui->actionCreatePolyline);
    m_actionManager->setAction(PDFActionManager::CreateRectangle, ui->actionCreateRectangle);
    m_actionManager->setAction(PDFActionManager::CreatePolygon, ui->actionCreatePolygon);
    m_actionManager->setAction(PDFActionManager::CreateEllipse, ui->actionCreateEllipse);
    m_actionManager->setAction(PDFActionManager::CreateFreehandCurve, ui->actionCreateFreehandCurve);
    m_actionManager->setAction(PDFActionManager::CreateHighlighter, ui->actionCreateHighlighter);
    m_actionManager->setAction(PDFActionManager::Eraser, ui->actionEraser);
    m_actionManager->setAction(PDFActionManager::StrokeEraser, ui->actionStrokeEraser);
    m_actionManager->setAction(PDFActionManager::DeleteAnnotation, ui->actionDeleteAnnotation);
    m_actionManager->setAction(PDFActionManager::RenderOptionAntialiasing, ui->actionRenderOptionAntialiasing);
    m_actionManager->setAction(PDFActionManager::RenderOptionTextAntialiasing, ui->actionRenderOptionTextAntialiasing);
    m_actionManager->setAction(PDFActionManager::RenderOptionSmoothPictures, ui->actionRenderOptionSmoothPictures);
    m_actionManager->setAction(PDFActionManager::RenderOptionIgnoreOptionalContentSettings, ui->actionRenderOptionIgnoreOptionalContentSettings);
    m_actionManager->setAction(PDFActionManager::RenderOptionDisplayRenderTimes, ui->actionRenderOptionDisplayRenderTimes);
    m_actionManager->setAction(PDFActionManager::RenderOptionDisplayAnnotations, ui->actionRenderOptionDisplayAnnotations);
    m_actionManager->setAction(PDFActionManager::RenderOptionInvertColors, ui->actionColorInvert);
    m_actionManager->setAction(PDFActionManager::RenderOptionGrayscale, ui->actionColorGrayscale);
    m_actionManager->setAction(PDFActionManager::RenderOptionHighContrast, ui->actionColorHighContrast);
    m_actionManager->setAction(PDFActionManager::RenderOptionBitonal, ui->actionColorBitonal);
    m_actionManager->setAction(PDFActionManager::RenderOptionCustomColors, ui->actionColorCustom);
    m_actionManager->setAction(PDFActionManager::RenderOptionShowTextBlocks, ui->actionShow_Text_Blocks);
    m_actionManager->setAction(PDFActionManager::RenderOptionShowTextLines, ui->actionShow_Text_Lines);
    m_actionManager->setAction(PDFActionManager::Properties, ui->actionProperties);
    m_actionManager->setAction(PDFActionManager::Options, ui->actionOptions);
    m_actionManager->setAction(PDFActionManager::ResetToFactorySettings, ui->actionResetToFactorySettings);
    m_actionManager->setAction(PDFActionManager::ClearRecentFileHistory, ui->actionClearRecentFileHistory);
    m_actionManager->setAction(PDFActionManager::CertificateManager, ui->actionCertificateManager);
    m_actionManager->setAction(PDFActionManager::GetSource, ui->actionGetSource);
    m_actionManager->setAction(PDFActionManager::About, ui->actionAbout);
    m_actionManager->setAction(PDFActionManager::RenderToImages, ui->actionRender_to_Images);
    m_actionManager->setAction(PDFActionManager::Optimize, ui->actionOptimize);
    m_actionManager->setAction(PDFActionManager::OptimizeImages, ui->actionOptimizeImages);
    m_actionManager->setAction(PDFActionManager::Sanitize, ui->actionSanitize);
    m_actionManager->setAction(PDFActionManager::RemoveExternalLinks, ui->actionRemoveExternalLinks);
    m_actionManager->setAction(PDFActionManager::PageGeometry, ui->actionPageGeometry);
    m_actionManager->setAction(PDFActionManager::CreateBitonalDocument, ui->actionCreateBitonalDocument);
    m_actionManager->setAction(PDFActionManager::RecognizeText, ui->actionRecognizeText);
    m_actionManager->setAction(PDFActionManager::ManageOCRLanguages, ui->actionManageOCRLanguages);
    m_actionManager->setAction(PDFActionManager::PrepareScannedPages, ui->actionPrepareScannedPages);
    m_actionManager->setAction(PDFActionManager::BatchRecognizeText, ui->actionBatchRecognizeText);
    m_actionManager->setAction(PDFActionManager::Encryption, ui->actionEncryption);
    m_actionManager->setAction(PDFActionManager::FitPage, ui->actionFitPage);
    m_actionManager->setAction(PDFActionManager::FitWidth, ui->actionFitWidth);
    m_actionManager->setAction(PDFActionManager::FitHeight, ui->actionFitHeight);
    m_actionManager->setAction(PDFActionManager::ShowRenderingErrors, ui->actionRendering_Errors);
    m_actionManager->setAction(PDFActionManager::PageLayoutSinglePage, ui->actionPageLayoutSinglePage);
    m_actionManager->setAction(PDFActionManager::PageLayoutContinuous, ui->actionPageLayoutContinuous);
    m_actionManager->setAction(PDFActionManager::PageLayoutTwoPages, ui->actionPageLayoutTwoPages);
    m_actionManager->setAction(PDFActionManager::PageLayoutTwoColumns, ui->actionPageLayoutTwoColumns);
    m_actionManager->setAction(PDFActionManager::PageLayoutFirstPageOnRightSide, ui->actionFirstPageOnRightSide);
    m_actionManager->setAction(PDFActionManager::FullscreenMode, ui->actionFullscreenMode);
    m_actionManager->setAction(PDFActionManager::ToolSelectText, ui->actionSelectText);
    m_actionManager->setAction(PDFActionManager::ToolSelectTable, ui->actionSelectTable);
    m_actionManager->setAction(PDFActionManager::ToolMagnifier, ui->actionMagnifier);
    m_actionManager->setAction(PDFActionManager::ToolScreenshot, ui->actionScreenshot);
    m_actionManager->setAction(PDFActionManager::ToolExtractImage, ui->actionExtractImage);
    m_actionManager->setAction(PDFActionManager::BookmarkPage, ui->actionBookmarkPage);
    m_actionManager->setAction(PDFActionManager::BookmarkGoToNext, ui->actionGotoNextBookmark);
    m_actionManager->setAction(PDFActionManager::BookmarkGoToPrevious, ui->actionGotoPreviousBookmark);
    m_actionManager->setAction(PDFActionManager::BookmarkExport, ui->actionBookmarkExport);
    m_actionManager->setAction(PDFActionManager::BookmarkImport, ui->actionBookmarkImport);
    m_actionManager->setAction(PDFActionManager::BookmarkGenerateAutomatically, ui->actionBookmarkAutoGenerate);
    m_actionManager->initActions(pdf::PDFWidgetUtils::scaleDPI(this, QSize(24, 24)), true);

    for (QAction* action : m_programController->getRecentFileManager()->getActions())
    {
        ui->menuFile->insertAction(ui->actionClearRecentFileHistory, action);
    }
    m_programController->getRecentFileManager()->setClearRecentFileHistoryAction(ui->actionClearRecentFileHistory);
    ui->menuFile->insertSeparator(ui->actionQuit);

    connect(ui->actionQuit, &QAction::triggered, this, &PDFEditorMainWindow::onActionQuitTriggered);

    m_pageNumberSpinBox = new QSpinBox(this);
    m_pageNumberSpinBox->setObjectName("pageNumberSpinBox");
    m_pageNumberLabel = new QLabel(this);
    m_pageNumberLabel->setObjectName("pageNumberLabel");
    m_pageNumberSpinBox->setFixedWidth(pdf::PDFWidgetUtils::scaleDPI_x(m_pageNumberSpinBox, 80));
    m_pageNumberSpinBox->setAlignment(Qt::AlignCenter);
    connect(m_pageNumberSpinBox, &QSpinBox::editingFinished, this, [this]() { onPageNumberSpinboxEditingFinished(m_pageNumberSpinBox); });

    for (QAction* action : m_actionManager->getActionGroup(PDFActionManager::CreateStampGroup)->actions())
    {
        ui->menuStamp->addAction(action);
    }

    // Page control (inkpage: a toolbar of its own, after the main toolbar)
    QToolBar* navigationToolBar = new QToolBar(tr("Navigation"), this);
    navigationToolBar->setObjectName("NavigationToolBar");
    adjustToolbar(navigationToolBar);
    navigationToolBar->addAction(ui->actionGoToDocumentStart);
    navigationToolBar->addAction(ui->actionGoToPreviousPage);
    navigationToolBar->addAction(m_toolBarManager->createWidgetItem("widgetPageNumber", tr("Page Number"), QIcon(), m_pageNumberSpinBox));
    navigationToolBar->addAction(m_toolBarManager->createWidgetItem("widgetPageCount", tr("Page Count"), QIcon(), m_pageNumberLabel));
    navigationToolBar->addAction(ui->actionGoToNextPage);
    navigationToolBar->addAction(ui->actionGoToDocumentEnd);
    addToolBar(Qt::TopToolBarArea, navigationToolBar);

    ui->mainToolBar->addSeparator();

    // Zoom (inkpage: at the bottom right, in the status bar)
    m_pageZoomSpinBox = new QDoubleSpinBox(this);
    m_pageZoomSpinBox->setObjectName("pageZoomSpinBox");
    m_pageZoomSpinBox->setMinimum(pdf::PDFDrawWidgetProxy::getMinZoom() * 100);
    m_pageZoomSpinBox->setMaximum(pdf::PDFDrawWidgetProxy::getMaxZoom() * 100);
    m_pageZoomSpinBox->setDecimals(2);
    m_pageZoomSpinBox->setSuffix(tr("%"));
    m_pageZoomSpinBox->setFixedWidth(pdf::PDFWidgetUtils::scaleDPI_x(m_pageNumberSpinBox, 80));
    m_pageZoomSpinBox->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    connect(m_pageZoomSpinBox, &QDoubleSpinBox::editingFinished, this, &PDFEditorMainWindow::onPageZoomSpinboxEditingFinished);
    initializeStatusBarZoom();

    // Tools
    ui->mainToolBar->addAction(ui->actionSelectText);
    ui->mainToolBar->addAction(ui->actionSelectTable);
    ui->mainToolBar->addAction(ui->actionCreateTextHighlight);
    ui->mainToolBar->addAction(ui->actionCreateTextUnderline);
    ui->mainToolBar->addAction(ui->actionCreateTextStrikeout);
    ui->mainToolBar->addAction(ui->actionCreateTextSquiggly);
    ui->mainToolBar->addAction(ui->actionMagnifier);
    ui->mainToolBar->addAction(ui->actionScreenshot);
    ui->mainToolBar->addAction(ui->actionExtractImage);
    ui->mainToolBar->addSeparator();

    initializePageActions();

    m_programController->initialize(PDFProgramController::AllFeatures, this, this, m_actionManager, m_progress);
    setCentralWidget(m_programController->getPdfWidget());
    setFocusProxy(m_programController->getPdfWidget());

    m_styleToolBar->setAnnotationManager(m_programController->getPdfWidget()->getDrawWidgetProxy()->getAnnotationManager());

    // The toolbars are complete (their content is the default content), the content
    // chosen by the user is applied after the plugins are loaded, so their actions
    // can be put in the toolbars. The Style toolbar is not customizable (own logic).
    m_toolBarManager->setActionProvider([this]() { return m_actionManager->getActions(); });
    for (const char* toolBarName : { "mainToolBar", "NavigationToolBar", "PagesToolBar", "InsertToolBar" })
    {
        QToolBar* toolBar = findChild<QToolBar*>(toolBarName, Qt::FindDirectChildrenOnly);
        Q_ASSERT(toolBar);
        m_toolBarManager->registerToolBar(toolBar);
    }
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
        m_toolBarManager->readSettings(settings);
    }

    m_sidebarWidget = new PDFSidebarWidget(m_programController->getPdfWidget()->getDrawWidgetProxy(), m_programController->getCertificateStore(), m_programController->getBookmarkManager(), m_programController->getSettings(), true, this);
    m_sidebarDockWidget = new QDockWidget(tr("&Sidebar"), this);
    m_sidebarDockWidget->setObjectName("SidebarDockWidget");
    m_sidebarDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_sidebarDockWidget->setWidget(m_sidebarWidget);
    addDockWidget(Qt::LeftDockWidgetArea, m_sidebarDockWidget);
    m_sidebarDockWidget->hide();
    initializeSidebarTitleBar();
    connect(m_sidebarWidget, &PDFSidebarWidget::actionTriggered, m_programController, &PDFProgramController::onActionTriggered);
    connect(m_sidebarWidget, &PDFSidebarWidget::documentModified, m_programController, &PDFProgramController::onDocumentModified);
    connect(m_sidebarWidget, &PDFSidebarWidget::sidebarVisibilityRequested, this, &PDFEditorMainWindow::onSidebarVisibilityRequested);
    connect(m_sidebarWidget, &PDFSidebarWidget::thumbnailContextMenuRequested, this, &PDFEditorMainWindow::onThumbnailContextMenuRequested);
    for (QAction* action : m_sidebarWidget->getOutlineActions())
    {
        m_actionManager->addAdditionalAction(action);
    }

    // Outline items are created directly from the document being read, so the action
    // must be reachable from the menu (and by its shortcut) without opening the sidebar
    if (QAction* outlineNewItemAction = m_sidebarWidget->getOutlineNewItemAction())
    {
        ui->menuInsert->addSeparator();
        ui->menuInsert->addAction(outlineNewItemAction);
    }

    m_advancedFindWidget = new PDFAdvancedFindWidget(m_programController->getPdfWidget()->getDrawWidgetProxy(), this);
    m_advancedFindDockWidget = new QDockWidget(tr("Advanced find"), this);
    m_advancedFindDockWidget->setObjectName("AdvancedFind");
    m_advancedFindDockWidget->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    m_advancedFindDockWidget->setWidget(m_advancedFindWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_advancedFindDockWidget);
    m_advancedFindDockWidget->hide();
    QAction* toggleAdvancedFindAction = m_advancedFindDockWidget->toggleViewAction();
    toggleAdvancedFindAction->setObjectName("actionAdvancedFind");
    toggleAdvancedFindAction->setText(tr("Ad&vanced Find..."));
    toggleAdvancedFindAction->setShortcut(QKeySequence("Ctrl+Shift+F"));
    toggleAdvancedFindAction->setIcon(QIcon(":/resources/find-advanced.svg"));
    // inkpage: the advanced find is placed right after Find Next
    const QList<QAction*> editActions = ui->menuEdit->actions();
    const qsizetype findNextIndex = editActions.indexOf(ui->actionFindNext);
    QAction* actionAfterFindNext = (findNextIndex >= 0 && findNextIndex + 1 < editActions.size()) ? editActions[findNextIndex + 1] : nullptr;
    ui->menuEdit->insertAction(actionAfterFindNext, toggleAdvancedFindAction);
    m_actionManager->addAdditionalAction(m_advancedFindDockWidget->toggleViewAction());

    ui->menuView->addSeparator();
    ui->menuView->addAction(m_sidebarDockWidget->toggleViewAction());
    m_sidebarDockWidget->toggleViewAction()->setObjectName("actionSidebar");
    m_actionManager->addAdditionalAction(m_sidebarDockWidget->toggleViewAction());

    initializeStatusBarAction();
    initializeToolBarPositionActions();
    initializeHiddenActions();

    connect(m_progress, &pdf::PDFProgress::progressStarted, this, &PDFEditorMainWindow::onProgressStarted);
    connect(m_progress, &pdf::PDFProgress::progressStep, this, &PDFEditorMainWindow::onProgressStep);
    connect(m_progress, &pdf::PDFProgress::progressFinished, this, &PDFEditorMainWindow::onProgressFinished);

    PDFActionComboBox* actionComboBox = new PDFActionComboBox(this);
    menuBar()->setCornerWidget(actionComboBox);

    m_programController->finishInitialization();
    updateDeveloperMenu();

    if (pdf::PDFToolManager* toolManager = m_programController->getToolManager())
    {
        connect(toolManager, &pdf::PDFToolManager::messageDisplayRequest, statusBar(), &QStatusBar::showMessage);
    }

    m_programController->initActionComboBox(actionComboBox);

#ifndef NDEBUG
    pdf::PDFWidgetUtils::checkMenuAccessibility(this);
#endif
}

PDFEditorMainWindow::~PDFEditorMainWindow()
{
    delete m_programController;
    m_programController = nullptr;

    delete m_actionManager;
    m_actionManager = nullptr;

    delete ui;
}

void PDFEditorMainWindow::onSidebarVisibilityRequested()
{
    if (!m_sidebarDockWidget->isVisible())
    {
        m_sidebarDockWidget->show();
    }

    m_sidebarDockWidget->raise();
}

void PDFEditorMainWindow::onActionQuitTriggered()
{
    close();
}

void PDFEditorMainWindow::onPageNumberSpinboxEditingFinished(QSpinBox* spinBox)
{
    if (m_isLoadingUI)
    {
        return;
    }

    if (spinBox->hasFocus())
    {
        m_programController->getPdfWidget()->setFocus();
    }

    m_programController->getPdfWidget()->getDrawWidgetProxy()->goToPage(spinBox->value() - 1);
}

void PDFEditorMainWindow::onPageZoomSpinboxEditingFinished()
{
    if (m_isLoadingUI)
    {
        return;
    }

    if (m_pageZoomSpinBox->hasFocus())
    {
        m_programController->getPdfWidget()->setFocus();
    }

    m_programController->getPdfWidget()->getDrawWidgetProxy()->zoom(m_pageZoomSpinBox->value() / 100.0);
}

void PDFEditorMainWindow::onProgressStarted(pdf::ProgressStartupInfo info)
{
    m_progressBarLeftLabelOnStatusBar->setText(info.text);
    m_progressBarLeftLabelOnStatusBar->setVisible(!info.text.isEmpty());

    m_progressBarOnStatusBar->setRange(0, 100);
    m_progressBarOnStatusBar->reset();
    m_progressBarOnStatusBar->show();

    m_progressTaskbarIndicator->setRange(0, 100);
    m_progressTaskbarIndicator->reset();
    m_progressTaskbarIndicator->show();

    m_programController->setIsBusy(true);
    m_programController->updateActionsAvailability();
}

void PDFEditorMainWindow::onProgressStep(int percentage)
{
    if (m_isChangingProgressStep)
    {
        return;
    }

    pdf::PDFTemporaryValueChange guard(&m_isChangingProgressStep, true);
    m_progressBarOnStatusBar->setValue(percentage);
    m_progressTaskbarIndicator->setValue(percentage);
}

void PDFEditorMainWindow::onProgressFinished()
{
    m_progressBarLeftLabelOnStatusBar->hide();
    m_progressBarOnStatusBar->hide();
    m_progressTaskbarIndicator->hide();

    m_programController->setIsBusy(false);
    m_programController->updateActionsAvailability();
}

void PDFEditorMainWindow::updateDeveloperMenu()
{
    bool isDeveloperMode = m_programController->getSettings()->getSettings().m_allowDeveloperMode;
    ui->menuDeveloper->menuAction()->setVisible(isDeveloperMode);
}

void PDFEditorMainWindow::updateUI(bool fullUpdate)
{
    pdf::PDFTemporaryValueChange guard(&m_isLoadingUI, true);

    if (fullUpdate)
    {
        if (pdf::PDFDocument* document = m_programController->getDocument())
        {
            size_t pageCount = document->getCatalog()->getPageCount();
            m_pageNumberSpinBox->setMinimum(1);
            m_pageNumberSpinBox->setMaximum(static_cast<int>(pageCount));
            m_pageNumberSpinBox->setEnabled(true);
            m_pageNumberLabel->setText(tr(" / %1").arg(pageCount));
        }
        else
        {
            m_pageNumberSpinBox->setEnabled(false);
            m_pageNumberLabel->setText(QString());
        }
    }
    else
    {
        std::vector<pdf::PDFInteger> currentPages = m_programController->getPdfWidget()->getDrawWidget()->getCurrentPages();
        if (!currentPages.empty())
        {
            m_pageNumberSpinBox->setValue(currentPages.front() + 1);

            // Prefetch pages, if it is enabled
            if (m_programController->getSettings()->isPagePrefetchingEnabled())
            {
                m_programController->getPdfWidget()->getDrawWidgetProxy()->prefetchPages(currentPages.back());
            }
        }

        m_sidebarWidget->setCurrentPages(currentPages);
    }

    m_pageZoomSpinBox->setValue(m_programController->getPdfWidget()->getDrawWidgetProxy()->getZoom() * 100);
    updateSidebarPageNumber();
}

QMenu* PDFEditorMainWindow::addToolMenu(QString name, pdf::PDFPlugin::PluginMenuLocation location)
{
    QMenu* parentMenu = (location == pdf::PDFPlugin::PluginMenuLocation::Edit) ? ui->menuEdit : ui->menuTools;
    return parentMenu->addMenu(name);
}

void PDFEditorMainWindow::setStatusBarMessage(QString message, int time)
{
    statusBar()->showMessage(message, time);
}

void PDFEditorMainWindow::setDocument(const pdf::PDFModifiedDocument& document)
{
    if (m_sidebarWidget)
    {
        m_sidebarWidget->setDocument(document, *m_programController->getSignatures());
    }

    if (m_advancedFindWidget)
    {
        m_advancedFindWidget->setDocument(document);
    }

    if (m_sidebarWidget)
    {
        if (!document || m_sidebarWidget->isEmpty())
        {
            m_sidebarDockWidget->hide();
        }
        else if (document.hasReset() && !document.hasPreserveUndoRedo())
        {
            const bool showSidebar = m_programController->getSettings()->getSettings().m_showSidebarOnDocumentOpen;
            m_sidebarDockWidget->setVisible(showSidebar);
        }
    }

    if (!document && m_advancedFindDockWidget)
    {
        m_advancedFindDockWidget->hide();
    }
}

void PDFEditorMainWindow::adjustToolbar(QToolBar* toolbar)
{
    QSize iconSize = pdf::PDFWidgetUtils::scaleDPI(this, QSize(24, 24));
    toolbar->setIconSize(iconSize);
}

void PDFEditorMainWindow::onThumbnailContextMenuRequested(pdf::PDFInteger pageIndex, QPoint globalPosition)
{
    if (!m_pagesMenu)
    {
        return;
    }

    // The Pages menu is reused: while it is open, its actions operate on the page of the thumbnail
    m_programController->setPageOperationTarget(pageIndex);
    m_pagesMenu->exec(globalPosition);
    m_programController->setPageOperationTarget(std::nullopt);
}

void PDFEditorMainWindow::initializePageActions()
{
    auto createAction = [this](PDFActionManager::Action type, QString text, QString toolTip, QString iconName, QString objectName)
    {
        QAction* action = new QAction(QIcon(QString(":/resources/%1.svg").arg(iconName)), text, this);
        action->setToolTip(toolTip);
        action->setStatusTip(toolTip);
        action->setObjectName(objectName);
        m_actionManager->setAction(type, action);
        return action;
    };

    QAction* addAction = createAction(PDFActionManager::PageAddBlank, tr("&Add Blank Page After"), tr("Add a blank page after the current page, with the same size and background"), "page-add", "actionPageAddBlank");
    QAction* insertAction = createAction(PDFActionManager::PageInsertBlank, tr("&Insert Blank Page Before"), tr("Insert a blank page before the current page, with the same size and background"), "page-insert", "actionPageInsertBlank");
    QAction* duplicateAction = createAction(PDFActionManager::PageDuplicate, tr("&Duplicate Page"), tr("Insert a copy of the current page after it, including its annotations"), "page-duplicate", "actionPageDuplicate");
    QAction* removeAction = createAction(PDFActionManager::PageRemove, tr("D&elete Page"), tr("Delete the current page"), "page-remove", "actionPageRemove");
    QAction* moveUpAction = createAction(PDFActionManager::PageMoveUp, tr("Move Page &Up"), tr("Move the current page before the previous page"), "page-move-up", "actionPageMoveUp");
    QAction* moveDownAction = createAction(PDFActionManager::PageMoveDown, tr("Move Page Do&wn"), tr("Move the current page after the next page"), "page-move-down", "actionPageMoveDown");

    // Backgrounds of the current page (the background of the page is checked) and of all
    // pages. Plain and Ruled keep their actions of the action manager (the shortcuts are
    // stored by the object names); the backgrounds of Xournal++ follow them.
    QMenu* backgroundMenu = new QMenu(tr("&Background"), this);
    backgroundMenu->setObjectName("menuPageBackground");
    QMenu* allPagesBackgroundMenu = new QMenu(tr("&All Pages"), this);
    allPagesBackgroundMenu->setObjectName("menuPageBackgroundAllPages");
    QActionGroup* backgroundGroup = new QActionGroup(this);
    backgroundGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::Exclusive);
    QActionGroup* allPagesBackgroundGroup = new QActionGroup(this);
    allPagesBackgroundGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);

    for (pdf::PDFPageOrganizer::Background background : pdf::PDFPageOrganizer::getBackgrounds())
    {
        const QString name = QString::fromLatin1(pdf::PDFPageOrganizer::getBackgroundName(background));
        const QString text = getBackgroundText(background);
        QAction* action = nullptr;
        QAction* allPagesAction = nullptr;

        switch (background)
        {
            case pdf::PDFPageOrganizer::Background::Plain:
                action = createAction(PDFActionManager::PageBackgroundPlain, text, tr("Remove the background of the current page"), "page", "actionPageBackgroundPlain");
                allPagesAction = createAction(PDFActionManager::PageBackgroundPlainAllPages, text, tr("Remove the background of all pages"), "page", "actionPageBackgroundPlainAllPages");
                break;

            case pdf::PDFPageOrganizer::Background::Ruled:
                action = createAction(PDFActionManager::PageBackgroundRuled, text, tr("Set a ruled background on the current page"), "page-background", "actionPageBackgroundRuled");
                allPagesAction = createAction(PDFActionManager::PageBackgroundRuledAllPages, text, tr("Set a ruled background on all pages"), "page-background", "actionPageBackgroundRuledAllPages");
                break;

            default:
                action = new QAction(text, this);
                action->setObjectName(QString("actionPageBackground%1").arg(name));
                action->setStatusTip(tr("Set the background \"%1\" on the current page").arg(QString(text).remove('&')));
                m_actionManager->addAdditionalAction(action);
                allPagesAction = new QAction(text, this);
                allPagesAction->setObjectName(QString("actionPageBackground%1AllPages").arg(name));
                allPagesAction->setStatusTip(tr("Set the background \"%1\" on all pages").arg(QString(text).remove('&')));
                m_actionManager->addAdditionalAction(allPagesAction);
                break;
        }

        action->setCheckable(true);
        action->setData(int(background));
        allPagesAction->setData(int(background));
        backgroundGroup->addAction(action);
        allPagesBackgroundGroup->addAction(allPagesAction);
        backgroundMenu->addAction(action);
        allPagesBackgroundMenu->addAction(allPagesAction);

        if (background == pdf::PDFPageOrganizer::Background::Ruled)
        {
            backgroundMenu->addSection(tr("Xournal++"));
            allPagesBackgroundMenu->addSection(tr("Xournal++"));
        }
    }
    backgroundMenu->addSeparator();
    backgroundMenu->addMenu(allPagesBackgroundMenu);
    m_actionManager->setActionGroup(PDFActionManager::PageBackgroundGroup, backgroundGroup);
    m_actionManager->setActionGroup(PDFActionManager::PageBackgroundAllPagesGroup, allPagesBackgroundGroup);

    // The background of the page, on which the menu operates, is checked
    connect(backgroundMenu, &QMenu::aboutToShow, this, [this, backgroundGroup]()
    {
        const pdf::PDFInteger pageIndex = m_programController->getPageOperationIndex();
        const pdf::PDFDocument* document = m_programController->getDocument();
        const pdf::PDFPageOrganizer::Background background = (document && pageIndex >= 0) ? pdf::PDFPageOrganizer::getBackground(document, pageIndex) : pdf::PDFPageOrganizer::Background::Plain;
        for (QAction* action : backgroundGroup->actions())
        {
            action->setChecked(action->data().toInt() == int(background));
        }
    });

    QAction* backgroundAction = backgroundMenu->menuAction();
    backgroundAction->setIcon(QIcon(":/resources/page-background.svg"));
    backgroundAction->setToolTip(tr("Background of the page"));
    backgroundAction->setObjectName("actionPageBackground");
    m_actionManager->addAdditionalAction(backgroundAction);

    QMenu* pagesMenu = new QMenu(tr("&Pages"), this);
    pagesMenu->setObjectName("menuPages");
    pagesMenu->addAction(addAction);
    pagesMenu->addAction(insertAction);
    pagesMenu->addAction(duplicateAction);
    pagesMenu->addAction(removeAction);
    pagesMenu->addSeparator();
    pagesMenu->addAction(moveUpAction);
    pagesMenu->addAction(moveDownAction);
    pagesMenu->addSeparator();
    pagesMenu->addMenu(backgroundMenu);
    menuBar()->insertMenu(ui->menuTools->menuAction(), pagesMenu);
    m_pagesMenu = pagesMenu;

    // The toolbar starts the second row, the toolbars of the plugins follow it
    QToolBar* pagesToolBar = new QToolBar(tr("Pages"), this);
    pagesToolBar->setObjectName("PagesToolBar");
    adjustToolbar(pagesToolBar);
    pagesToolBar->addAction(addAction);
    pagesToolBar->addAction(insertAction);
    pagesToolBar->addAction(duplicateAction);
    pagesToolBar->addAction(removeAction);
    pagesToolBar->addSeparator();
    pagesToolBar->addAction(moveUpAction);
    pagesToolBar->addAction(moveDownAction);
    pagesToolBar->addSeparator();
    pagesToolBar->addAction(backgroundAction);
    if (QToolButton* backgroundButton = qobject_cast<QToolButton*>(pagesToolBar->widgetForAction(backgroundAction)))
    {
        backgroundButton->setPopupMode(QToolButton::InstantPopup);
    }

    addToolBarBreak(Qt::TopToolBarArea);
    addToolBar(Qt::TopToolBarArea, pagesToolBar);
    addToolBar(Qt::TopToolBarArea, createInsertToolBar());

    // Style of the annotations (colors, fill, pen width), it follows the Insert toolbar
    m_styleToolBar = new PDFAnnotationStyleToolBar(this);
    adjustToolbar(m_styleToolBar);
    addToolBar(Qt::TopToolBarArea, m_styleToolBar);
}

QToolBar* PDFEditorMainWindow::createInsertToolBar()
{
    // The annotation tools of the Insert menu (like the tool palette of Xournal++),
    // the Style toolbar placed after it sets their color and pen width
    QToolBar* insertToolBar = new QToolBar(tr("Insert"), this);
    insertToolBar->setObjectName("InsertToolBar");
    adjustToolbar(insertToolBar);
    insertToolBar->addAction(ui->actionInlineText);
    insertToolBar->addAction(ui->actionCreateImage);
    if (QToolButton* insertStickyNoteButton = m_actionManager->createToolButtonForActionGroup(PDFActionManager::CreateStickyNoteGroup, insertToolBar))
    {
        insertStickyNoteButton->setObjectName("insertStickyNoteButton");
        const QIcon icon = insertStickyNoteButton->defaultAction() ? insertStickyNoteButton->defaultAction()->icon() : QIcon();
        insertToolBar->addAction(m_toolBarManager->createWidgetItem("widgetStickyNotes", tr("Sticky Notes"), icon, insertStickyNoteButton));
    }
    insertToolBar->addSeparator();
    insertToolBar->addAction(ui->actionCreateStraightLine);
    insertToolBar->addAction(ui->actionCreatePolyline);
    insertToolBar->addAction(ui->actionCreateRectangle);
    insertToolBar->addAction(ui->actionCreatePolygon);
    insertToolBar->addAction(ui->actionCreateEllipse);
    insertToolBar->addSeparator();
    insertToolBar->addAction(ui->actionCreateFreehandCurve);
    insertToolBar->addAction(ui->actionCreateHighlighter);
    insertToolBar->addAction(ui->actionEraser);
    insertToolBar->addAction(ui->actionStrokeEraser);
    return insertToolBar;
}

QString PDFEditorMainWindow::getBackgroundText(pdf::PDFPageOrganizer::Background background)
{
    switch (background)
    {
        case pdf::PDFPageOrganizer::Background::Plain:
            return tr("&Plain");
        case pdf::PDFPageOrganizer::Background::Ruled:
            return tr("&Ruled");
        case pdf::PDFPageOrganizer::Background::Lines:
            return tr("&Lines");
        case pdf::PDFPageOrganizer::Background::LinesMargin:
            return tr("Lines with &Margin");
        case pdf::PDFPageOrganizer::Background::LinesRightMargin:
            return tr("Lines with Margin on the Righ&t");
        case pdf::PDFPageOrganizer::Background::Graph:
            return tr("&Graph");
        case pdf::PDFPageOrganizer::Background::GraphBorder:
            return tr("Graph with &Border");
        case pdf::PDFPageOrganizer::Background::GraphBorderBold:
            return tr("Graph with Border, Every 5th Line B&old");
        case pdf::PDFPageOrganizer::Background::Dotted:
            return tr("&Dotted");
        case pdf::PDFPageOrganizer::Background::IsometricDotted:
            return tr("&Isometric Dotted");
        case pdf::PDFPageOrganizer::Background::IsometricGraph:
            return tr("Isometric Grap&h");
        case pdf::PDFPageOrganizer::Background::Staves:
            return tr("&Staves");
    }

    Q_ASSERT(false);
    return QString();
}

void PDFEditorMainWindow::initializeHiddenActions()
{
    QList<QAction*> hiddenActions = {
        ui->actionScreenshot,
        ui->menuStamp->menuAction(),
    };

    // The stamp actions are hidden too, so they are not offered by the quick search
    if (QActionGroup* stampGroup = m_actionManager->getActionGroup(PDFActionManager::CreateStampGroup))
    {
        hiddenActions.append(stampGroup->actions());
    }

    for (QAction* action : hiddenActions)
    {
        action->setVisible(false);
    }
}

void PDFEditorMainWindow::initializeStatusBarZoom()
{
    // Zoom out, zoom, zoom in, fit page / width / height - like the zoom of a
    // document viewer, at the right side of the status bar (after the progress bar)
    QWidget* zoomWidget = new QWidget(this);
    zoomWidget->setObjectName("statusBarZoomWidget");
    QHBoxLayout* layout = new QHBoxLayout(zoomWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(pdf::PDFWidgetUtils::scaleDPI_x(zoomWidget, 2));

    const QSize iconSize = pdf::PDFWidgetUtils::scaleDPI(this, QSize(16, 16));
    auto addButton = [zoomWidget, layout, iconSize](QAction* action)
    {
        QToolButton* button = new QToolButton(zoomWidget);
        button->setDefaultAction(action);
        button->setAutoRaise(true);
        button->setIconSize(iconSize);
        layout->addWidget(button);
    };

    addButton(ui->actionZoom_Out);
    layout->addWidget(m_pageZoomSpinBox);
    addButton(ui->actionZoom_In);
    layout->addSpacing(pdf::PDFWidgetUtils::scaleDPI_x(zoomWidget, 6));
    addButton(ui->actionFitPage);
    addButton(ui->actionFitWidth);
    addButton(ui->actionFitHeight);

    statusBar()->addPermanentWidget(zoomWidget);
}

void PDFEditorMainWindow::initializeSidebarTitleBar()
{
    QToolBar* titleBar = new QToolBar(m_sidebarDockWidget);
    titleBar->setObjectName("SidebarTitleBar");
    titleBar->setMovable(false);
    titleBar->setFloatable(false);
    titleBar->setIconSize(pdf::PDFWidgetUtils::scaleDPI(this, QSize(16, 16)));
    titleBar->addAction(ui->actionGoToDocumentStart);
    titleBar->addAction(ui->actionGoToPreviousPage);
    m_sidebarPageSpinBox = new QSpinBox(titleBar);
    m_sidebarPageSpinBox->setObjectName("sidebarPageSpinBox");
    m_sidebarPageSpinBox->setFixedWidth(pdf::PDFWidgetUtils::scaleDPI_x(m_sidebarPageSpinBox, 64));
    m_sidebarPageSpinBox->setAlignment(Qt::AlignCenter);
    connect(m_sidebarPageSpinBox, &QSpinBox::editingFinished, this, [this]() { onPageNumberSpinboxEditingFinished(m_sidebarPageSpinBox); });
    titleBar->addWidget(m_sidebarPageSpinBox);
    m_sidebarPageCountLabel = new QLabel(titleBar);
    m_sidebarPageCountLabel->setObjectName("sidebarPageCountLabel");
    titleBar->addWidget(m_sidebarPageCountLabel);
    titleBar->addAction(ui->actionGoToNextPage);
    titleBar->addAction(ui->actionGoToDocumentEnd);

    // The empty area keeps the dock widget movable: the mouse events not used
    // by the buttons are passed to the dock widget, which then starts dragging
    QWidget* spacer = new QWidget(titleBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    titleBar->addWidget(spacer);

    QAction* floatAction = titleBar->addAction(style()->standardIcon(QStyle::SP_TitleBarNormalButton), tr("Detach the sidebar"));
    floatAction->setObjectName("actionSidebarFloat");
    connect(floatAction, &QAction::triggered, m_sidebarDockWidget, [this]() { m_sidebarDockWidget->setFloating(!m_sidebarDockWidget->isFloating()); });
    connect(m_sidebarDockWidget, &QDockWidget::topLevelChanged, floatAction, [floatAction](bool floating) { floatAction->setText(floating ? tr("Attach the sidebar") : tr("Detach the sidebar")); });

    QAction* closeAction = titleBar->addAction(style()->standardIcon(QStyle::SP_TitleBarCloseButton), tr("Close the sidebar"));
    closeAction->setObjectName("actionSidebarClose");
    connect(closeAction, &QAction::triggered, m_sidebarDockWidget, &QDockWidget::close);

    m_sidebarDockWidget->setTitleBarWidget(titleBar);
    updateSidebarPageNumber();
}

void PDFEditorMainWindow::updateSidebarPageNumber()
{
    if (!m_sidebarPageSpinBox)
    {
        return;
    }

    pdf::PDFTemporaryValueChange guard(&m_isLoadingUI, true);
    m_sidebarPageSpinBox->setRange(m_pageNumberSpinBox->minimum(), m_pageNumberSpinBox->maximum());
    m_sidebarPageSpinBox->setValue(m_pageNumberSpinBox->value());
    m_sidebarPageSpinBox->setEnabled(m_pageNumberSpinBox->isEnabled());
    m_sidebarPageCountLabel->setText(m_pageNumberLabel->text());
}

void PDFEditorMainWindow::initializeStatusBarAction()
{
    // The visibility of the status bar is not a part of the window state
    // (QMainWindow::saveState), it is stored separately
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const bool isVisible = settings.value(STATUS_BAR_VISIBLE_KEY, true).toBool();
    statusBar()->setVisible(isVisible);

    QAction* action = new QAction(tr("Status &Bar"), this);
    action->setObjectName("actionStatusBar");
    action->setCheckable(true);
    action->setChecked(isVisible);
    action->setToolTip(tr("Show or hide the status bar"));
    connect(action, &QAction::toggled, this, [this](bool checked)
    {
        statusBar()->setVisible(checked);
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
        settings.setValue(STATUS_BAR_VISIBLE_KEY, checked);
    });

    ui->menuView->addAction(action);
    m_actionManager->addAdditionalAction(action);
}

void PDFEditorMainWindow::initializeToolBarPositionActions()
{
    m_toolBarPositionMenu = new QMenu(tr("&Toolbar Position"), this);

    m_toolBarsTopAction = m_toolBarPositionMenu->addAction(tr("&Top"));
    m_toolBarsTopAction->setObjectName("actionToolBarsTop");
    m_toolBarsTopAction->setCheckable(true);

    m_toolBarsRightAction = m_toolBarPositionMenu->addAction(tr("&Right"));
    m_toolBarsRightAction->setObjectName("actionToolBarsRight");
    m_toolBarsRightAction->setCheckable(true);

    // Toolbars can be dragged also into another area (or left floating),
    // then none of the actions is checked.
    QActionGroup* actionGroup = new QActionGroup(this);
    actionGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    actionGroup->addAction(m_toolBarsTopAction);
    actionGroup->addAction(m_toolBarsRightAction);

    connect(m_toolBarsTopAction, &QAction::triggered, this, [this]() { setToolBarsArea(Qt::TopToolBarArea); });
    connect(m_toolBarsRightAction, &QAction::triggered, this, [this]() { setToolBarsArea(Qt::RightToolBarArea); });
    connect(m_toolBarPositionMenu, &QMenu::aboutToShow, this, &PDFEditorMainWindow::updateToolBarPositionActions);

    m_actionManager->addAdditionalAction(m_toolBarsTopAction);
    m_actionManager->addAdditionalAction(m_toolBarsRightAction);

    ui->menuView->addMenu(m_toolBarPositionMenu);
}

void PDFEditorMainWindow::updateToolBarPositionActions()
{
    const Qt::ToolBarArea area = ui->mainToolBar->isFloating() ? Qt::NoToolBarArea : toolBarArea(ui->mainToolBar);
    m_toolBarsTopAction->setChecked(area == Qt::TopToolBarArea);
    m_toolBarsRightAction->setChecked(area == Qt::RightToolBarArea);
}

void PDFEditorMainWindow::setToolBarsArea(Qt::ToolBarArea area)
{
    // The standard toolbar is created first, the toolbars of the plugins follow
    // in the order in which the plugins were loaded
    const QList<QToolBar*> toolBars = findChildren<QToolBar*>(Qt::FindDirectChildrenOnly);

    // Toolbars already placed in the area keep the arrangement made by the user
    const bool isAlreadyInArea = std::all_of(toolBars.cbegin(), toolBars.cend(), [this, area](QToolBar* toolBar) { return !toolBar->isFloating() && toolBarArea(toolBar) == area; });
    if (!isAlreadyInArea)
    {
        pdf::PDFWidgetUtils::moveToolBars(this, toolBars, area);
    }

    updateToolBarPositionActions();
}

QMenu* PDFEditorMainWindow::createPopupMenu()
{
    // The context menu of the toolbars and of the dock widgets
    QMenu* menu = QMainWindow::createPopupMenu();

    // inkpage: the advanced find is opened by Edit > Advanced Find, it is not listed here
    if (menu && m_advancedFindDockWidget)
    {
        menu->removeAction(m_advancedFindDockWidget->toggleViewAction());
    }

    if (menu && m_toolBarPositionMenu)
    {
        updateToolBarPositionActions();
        menu->addSeparator();
        menu->addAction(m_toolBarsTopAction);
        menu->addAction(m_toolBarsRightAction);
    }

    return menu;
}

pdf::PDFTextSelection PDFEditorMainWindow::getSelectedText() const
{
    if (!m_advancedFindWidget)
    {
        return pdf::PDFTextSelection();
    }

    return m_advancedFindWidget->getSelectedText();
}

void PDFEditorMainWindow::closeEvent(QCloseEvent* event)
{
    if (!m_programController->canClose())
    {
        // Jakub Melka: Do not allow to close the application, if document
        // reading is running.
        event->ignore();
    }
    else
    {
        if (!m_programController->askForSaveDocumentBeforeClose())
        {
            // User cancelled close operation
            event->ignore();
            return;
        }

        if (!m_programController->isFactorySettingsBeingRestored())
        {
            m_programController->writeSettings();
        }

        m_programController->closeDocument();
        event->accept();
    }
}

void PDFEditorMainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    m_progressTaskbarIndicator->setWindow(windowHandle());
}

void PDFEditorMainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        event->setDropAction(Qt::LinkAction);
        event->accept();
    }
}

void PDFEditorMainWindow::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        event->setDropAction(Qt::LinkAction);
        event->accept();
    }
}

void PDFEditorMainWindow::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        QList<QUrl> urls = event->mimeData()->urls();
        if (urls.size() == 1)
        {
            m_programController->openDocument(urls.front().toLocalFile());
            event->acceptProposedAction();
        }
    }
}

}   // namespace pdfviewer
