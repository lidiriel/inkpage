// inkpage - a simple PDF editor for Linux
// Copyright (C) 2026 Ludovic Mercier
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "pdfeditormainwindow.h"
#include "pdfprogramcontroller.h"
#include "pdfdocumentbuilder.h"
#include "pdfdocumentwriter.h"
#include "pdfpageorganizer.h"
#include "pdfsidebarwidget.h"
#include "pdfdrawwidget.h"
#include "pdfdrawspacecontroller.h"
#include "pdfoptionalcontent.h"

#include <QtTest>
#include <QAction>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QDoubleSpinBox>
#include <QMenu>
#include <QLabel>
#include <QSpinBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QLineEdit>
#include <QTimer>

#include <memory>

using namespace pdfviewer;

/// Tests of the main window of inkpage: its menus, toolbars and status bar
class EditorMainWindowTest : public QObject
{
    Q_OBJECT

public:
    static void initMain()
    {
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        {
            qputenv("QT_QPA_PLATFORM", "offscreen");
        }
    }

private slots:
    void initTestCase();
    void statusBarVisibilityIsRemembered();
    void zoomIsInStatusBar();
    void navigationToolBar();
    void sidebarTitleBarHasNavigation();
    void stickyNoteIsInInsertToolBar();
    void pageBackgrounds();
    void layersPage();
    void openFromMenu();

private:
    static QSettings settings();

    QTemporaryDir m_settingsDirectory;
};

QSettings EditorMainWindowTest::settings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
}

void EditorMainWindowTest::initTestCase()
{
    // Separate settings, the settings of the user are not touched
    QVERIFY(m_settingsDirectory.isValid());
    QCoreApplication::setOrganizationName("inkpage-unittests");
    QCoreApplication::setApplicationName("UnitTestsEditorMainWindow");
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDirectory.path());

    // No plugin is loaded: a plugin is a single instance in the process, it supports
    // only one main window (the tests create several windows)
    QSettings pluginSettings = settings();
    pluginSettings.setValue("Plugins/EnabledPlugins", QStringList());
}

void EditorMainWindowTest::statusBarVisibilityIsRemembered()
{
    {
        PDFEditorMainWindow window;
        QAction* action = window.findChild<QAction*>("actionStatusBar");
        QVERIFY(action);
        QVERIFY(action->isCheckable());
        QVERIFY(action->isChecked());
        QVERIFY(!window.statusBar()->isHidden());

        action->trigger();
        QVERIFY(window.statusBar()->isHidden());
        QCOMPARE(settings().value("statusBarVisible").toBool(), false);
    }

    // A new window starts with the status bar hidden
    {
        PDFEditorMainWindow window;
        QAction* action = window.findChild<QAction*>("actionStatusBar");
        QVERIFY(!action->isChecked());
        QVERIFY(window.statusBar()->isHidden());
        action->trigger();
        QVERIFY(!window.statusBar()->isHidden());
    }
    QCOMPARE(settings().value("statusBarVisible").toBool(), true);
}

void EditorMainWindowTest::zoomIsInStatusBar()
{
    PDFEditorMainWindow window;
    QWidget* zoomWidget = window.statusBar()->findChild<QWidget*>("statusBarZoomWidget");
    QVERIFY(zoomWidget);
    QVERIFY(zoomWidget->findChild<QDoubleSpinBox*>("pageZoomSpinBox"));

    QStringList zoomActions;
    for (QToolButton* button : zoomWidget->findChildren<QToolButton*>())
    {
        zoomActions << button->defaultAction()->objectName();
    }
    QCOMPARE(zoomActions, (QStringList{ "actionZoom_Out", "actionZoom_In", "actionFitPage", "actionFitWidth", "actionFitHeight" }));

    // The zoom is not in the main toolbar any more
    QToolBar* mainToolBar = window.findChild<QToolBar*>("mainToolBar");
    QVERIFY(mainToolBar);
    for (const QString& name : zoomActions)
    {
        QAction* action = window.findChild<QAction*>(name);
        QVERIFY(action);
        QVERIFY(!mainToolBar->actions().contains(action));
    }
}

void EditorMainWindowTest::navigationToolBar()
{
    PDFEditorMainWindow window;
    QToolBar* navigationToolBar = window.findChild<QToolBar*>("NavigationToolBar");
    QVERIFY(navigationToolBar);
    QCOMPARE(window.toolBarArea(navigationToolBar), Qt::TopToolBarArea);

    QStringList names;
    for (QAction* action : navigationToolBar->actions())
    {
        if (QWidget* widget = navigationToolBar->widgetForAction(action); widget && !qobject_cast<QToolButton*>(widget))
        {
            names << widget->objectName();
        }
        else
        {
            names << action->objectName();
        }
    }
    QCOMPARE(names, (QStringList{ "actionGoToDocumentStart", "actionGoToPreviousPage", "pageNumberSpinBox", "pageNumberLabel", "actionGoToNextPage", "actionGoToDocumentEnd" }));

    // On the first row, after the main toolbar (the Pages toolbar starts the second row)
    QToolBar* mainToolBar = window.findChild<QToolBar*>("mainToolBar");
    QToolBar* pagesToolBar = window.findChild<QToolBar*>("PagesToolBar");
    QVERIFY(!window.toolBarBreak(navigationToolBar));
    QVERIFY(window.toolBarBreak(pagesToolBar));
    QVERIFY(!mainToolBar->actions().contains(window.findChild<QAction*>("actionGoToNextPage")));
}

void EditorMainWindowTest::sidebarTitleBarHasNavigation()
{
    PDFEditorMainWindow window;
    QDockWidget* sidebar = window.findChild<QDockWidget*>("SidebarDockWidget");
    QVERIFY(sidebar);
    QToolBar* titleBar = qobject_cast<QToolBar*>(sidebar->titleBarWidget());
    QVERIFY(titleBar);
    QCOMPARE(titleBar->objectName(), QString("SidebarTitleBar"));

    QStringList names;
    for (QAction* action : titleBar->actions())
    {
        if (!action->objectName().isEmpty())
        {
            names << action->objectName();
        }
    }
    QCOMPARE(names, (QStringList{ "actionGoToDocumentStart", "actionGoToPreviousPage", "actionGoToNextPage", "actionGoToDocumentEnd", "actionSidebarFloat", "actionSidebarClose" }));

    // The page number is between the previous and next page arrows, as in the navigation toolbar
    QSpinBox* pageSpinBox = titleBar->findChild<QSpinBox*>("sidebarPageSpinBox");
    QLabel* pageCountLabel = titleBar->findChild<QLabel*>("sidebarPageCountLabel");
    QVERIFY(pageSpinBox);
    QVERIFY(pageCountLabel);
    const QList<QAction*> actions = titleBar->actions();
    QCOMPARE(actions[1]->objectName(), QString("actionGoToPreviousPage"));
    QCOMPARE(titleBar->widgetForAction(actions[2]), pageSpinBox);
    QCOMPARE(titleBar->widgetForAction(actions[3]), pageCountLabel);
    QCOMPARE(actions[4]->objectName(), QString("actionGoToNextPage"));
    QVERIFY(!pageSpinBox->isEnabled());
    QVERIFY(pageCountLabel->text().isEmpty());

    // The buttons of the title bar replace the float and close buttons of the dock widget
    window.show();
    sidebar->show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    titleBar->findChild<QAction*>("actionSidebarFloat")->trigger();
    QVERIFY(sidebar->isFloating());
    titleBar->findChild<QAction*>("actionSidebarFloat")->trigger();
    QVERIFY(!sidebar->isFloating());
    titleBar->findChild<QAction*>("actionSidebarClose")->trigger();
    QVERIFY(!sidebar->isVisible());

    // Current page and page count of a document with three pages
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath("pages.pdf");
    {
        pdf::PDFDocumentBuilder builder;
        for (int i = 0; i < 3; ++i)
        {
            builder.appendPage(QRectF(0, 0, 595, 842));
        }
        const pdf::PDFDocument document = builder.build();
        pdf::PDFDocumentWriter writer(nullptr);
        QVERIFY(writer.write(fileName, &document, false));
    }
    PDFProgramController* controller = window.getProgramController();
    controller->openDocument(fileName);
    QTRY_VERIFY(controller->getDocument());
    QSpinBox* toolBarSpinBox = window.findChild<QSpinBox*>("pageNumberSpinBox");
    QTRY_VERIFY(pageSpinBox->isEnabled());
    QCOMPARE(pageSpinBox->value(), 1);
    QCOMPARE(pageSpinBox->maximum(), 3);
    QCOMPARE(pageCountLabel->text(), window.findChild<QLabel*>("pageNumberLabel")->text());
    window.findChild<QAction*>("actionGoToDocumentEnd")->trigger();
    QTRY_COMPARE(pageSpinBox->value(), 3);

    // A page entered in the sidebar is shown, and the navigation toolbar follows
    pageSpinBox->setValue(2);
    Q_EMIT pageSpinBox->editingFinished();
    QTRY_COMPARE(toolBarSpinBox->value(), 2);
    QCOMPARE(pageSpinBox->value(), 2);

    // Closed before its destruction, as in the application (the window is active)
    window.close();
}

void EditorMainWindowTest::stickyNoteIsInInsertToolBar()
{
    PDFEditorMainWindow window;
    QToolBar* insertToolBar = window.findChild<QToolBar*>("InsertToolBar");
    QVERIFY(insertToolBar);

    // Inline text, Image, then the sticky note button
    const QList<QAction*> actions = insertToolBar->actions();
    QVERIFY(actions.size() > 3);
    QCOMPARE(actions[0]->objectName(), QString("actionInlineText"));
    QCOMPARE(actions[1]->objectName(), QString("actionCreateImage"));
    QToolButton* stickyNoteButton = qobject_cast<QToolButton*>(insertToolBar->widgetForAction(actions[2]));
    QVERIFY(stickyNoteButton);
    QCOMPARE(stickyNoteButton->objectName(), QString("insertStickyNoteButton"));
    QVERIFY(stickyNoteButton->defaultAction()->objectName().startsWith("actionStickyNote"));

    QToolBar* mainToolBar = window.findChild<QToolBar*>("mainToolBar");
    QVERIFY(!mainToolBar->findChild<QToolButton*>("insertStickyNoteButton"));

    // The context menu of the toolbars lists the toolbars and the docks, not the advanced find
    std::unique_ptr<QMenu> popupMenu(window.createPopupMenu());
    QVERIFY(popupMenu);
    QStringList popupTexts;
    for (QAction* action : popupMenu->actions())
    {
        popupTexts << action->text();
    }
    QVERIFY(popupTexts.contains("Standard"));
    QAction* advancedFind = window.findChild<QAction*>("actionAdvancedFind");
    QVERIFY(advancedFind);
    QVERIFY(!popupMenu->actions().contains(advancedFind));

    // It stays in the Edit menu
    QMenu* editMenu = window.findChild<QMenu*>("menuEdit");
    QVERIFY(editMenu && editMenu->actions().contains(advancedFind));

    // Optimize Images is in the Tools menu only, not in the standard toolbar
    QAction* optimizeImages = window.findChild<QAction*>("actionOptimizeImages");
    QVERIFY(optimizeImages);
    QVERIFY(!mainToolBar->actions().contains(optimizeImages));
    QVERIFY(!optimizeImages->associatedObjects().isEmpty());
}

void EditorMainWindowTest::pageBackgrounds()
{
    // Document with one page
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath("page.pdf");
    {
        pdf::PDFDocumentBuilder builder;
        builder.appendPage(QRectF(0, 0, 595, 842));
        const pdf::PDFDocument document = builder.build();
        pdf::PDFDocumentWriter writer(nullptr);
        QVERIFY(writer.write(fileName, &document, false));
    }

    PDFEditorMainWindow window;
    QMenu* backgroundMenu = window.findChild<QMenu*>("menuPageBackground");
    QMenu* allPagesMenu = window.findChild<QMenu*>("menuPageBackgroundAllPages");
    QVERIFY(backgroundMenu);
    QVERIFY(allPagesMenu);

    auto actionNames = [](QMenu* menu)
    {
        QStringList names;
        for (QAction* action : menu->actions())
        {
            if (!action->isSeparator() && !action->menu())
            {
                names << action->objectName();
            }
        }
        return names;
    };

    // Plain and Ruled of inkpage, then the backgrounds of Xournal++
    const QStringList backgrounds = { "Plain", "Ruled", "Lines", "LinesMargin", "LinesRightMargin", "Graph", "GraphBorder",
                                      "GraphBorderBold", "Dotted", "IsometricDotted", "IsometricGraph", "Staves" };
    QStringList expectedNames;
    QStringList expectedAllPagesNames;
    for (const QString& background : backgrounds)
    {
        expectedNames << "actionPageBackground" + background;
        expectedAllPagesNames << "actionPageBackground" + background + "AllPages";
    }
    QCOMPARE(actionNames(backgroundMenu), expectedNames);
    QCOMPARE(actionNames(allPagesMenu), expectedAllPagesNames);
    QVERIFY(backgroundMenu->actions().contains(allPagesMenu->menuAction()));

    // Without a document, the backgrounds are disabled
    QAction* graph = window.findChild<QAction*>("actionPageBackgroundGraph");
    QAction* plain = window.findChild<QAction*>("actionPageBackgroundPlain");
    QAction* dottedAllPages = window.findChild<QAction*>("actionPageBackgroundDottedAllPages");
    QVERIFY(!graph->isEnabled());

    PDFProgramController* controller = window.getProgramController();
    controller->openDocument(fileName);
    QTRY_VERIFY(controller->getDocument());
    QVERIFY(graph->isEnabled());

    // The background of the page is set, and checked in the menu
    graph->trigger();
    QCOMPARE(pdf::PDFPageOrganizer::getBackground(controller->getDocument(), 0), pdf::PDFPageOrganizer::Background::Graph);
    Q_EMIT backgroundMenu->aboutToShow();
    QVERIFY(graph->isChecked());
    QVERIFY(!plain->isChecked());

    dottedAllPages->trigger();
    QCOMPARE(pdf::PDFPageOrganizer::getBackground(controller->getDocument(), 0), pdf::PDFPageOrganizer::Background::Dotted);
    Q_EMIT backgroundMenu->aboutToShow();
    QVERIFY(window.findChild<QAction*>("actionPageBackgroundDotted")->isChecked());
    QVERIFY(!graph->isChecked());

    plain->trigger();
    QCOMPARE(pdf::PDFPageOrganizer::getBackground(controller->getDocument(), 0), pdf::PDFPageOrganizer::Background::Plain);
}

void EditorMainWindowTest::layersPage()
{
    // A document with a square in the note layer
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath("layers.pdf");
    pdf::PDFObjectReference square;
    {
        pdf::PDFDocumentBuilder builder;
        const pdf::PDFObjectReference page = builder.appendPage(QRectF(0, 0, 595, 842));
        square = builder.createAnnotationSquare(page, QRectF(50, 50, 100, 100), 1.0, Qt::black, Qt::black, "Title", QString(), QString());
        builder.setAnnotationNoteLayer(square);
        const pdf::PDFDocument document = builder.build();
        pdf::PDFDocumentWriter writer(nullptr);
        QVERIFY(writer.write(fileName, &document, false));
    }

    PDFEditorMainWindow window;
    PDFProgramController* controller = window.getProgramController();
    controller->openDocument(fileName);
    QTRY_VERIFY(controller->getDocument());

    PDFSidebarWidget* sidebar = window.findChild<PDFSidebarWidget*>();
    QVERIFY(sidebar);
    QVERIFY(!sidebar->isEmpty(PDFSidebarWidget::OptionalContent));
    QToolBar* layersToolBar = sidebar->findChild<QToolBar*>("layersToolBar");
    QVERIFY(layersToolBar);
    QStringList toolBarActions;
    for (QAction* action : layersToolBar->actions())
    {
        if (!action->isSeparator())
        {
            toolBarActions << action->objectName();
        }
    }
    QCOMPARE(toolBarActions, (QStringList{ "actionLayerNew", "actionLayerDelete", "actionLayerMoveUp", "actionLayerMoveDown" }));

    pdf::PDFDrawWidgetProxy* proxy = controller->getPdfWidget()->getDrawWidgetProxy();
    auto layers = [controller]() { return pdf::PDFDocumentBuilder(controller->getDocument()).getOptionalContentGroupsInOrder(); };
    auto properties = [controller]() { return controller->getDocument()->getCatalog()->getOptionalContentProperties(); };
    const pdf::PDFObjectReference notes = properties()->findOptionalContentGroup("Notes");
    QVERIFY(notes.isValid());
    QCOMPARE(proxy->getActiveLayer(), notes);

    // A new layer is at the top, it is the active layer
    const pdf::PDFObjectReference answers = sidebar->createLayer("Answers");
    QVERIFY(answers.isValid());
    QCOMPARE(layers(), (std::vector<pdf::PDFObjectReference>{ answers, notes }));
    QCOMPARE(proxy->getActiveLayer(), answers);

    // Moved down, renamed, not printed, hidden
    QVERIFY(sidebar->moveLayer(answers, 1));
    QCOMPARE(layers(), (std::vector<pdf::PDFObjectReference>{ notes, answers }));
    QVERIFY(!sidebar->moveLayer(answers, 1));
    QVERIFY(sidebar->renameLayer(answers, "Solutions"));
    QCOMPARE(properties()->getOptionalContentGroup(answers).getName(), QString("Solutions"));
    QVERIFY(sidebar->setLayerPrinted(answers, false));
    QVERIFY(!pdf::PDFDocumentBuilder(controller->getDocument()).isOptionalContentGroupPrinted(answers));
    sidebar->setLayerVisible(answers, false);
    QCOMPARE(proxy->getOptionalContentActivity()->getState(answers), pdf::OCState::OFF);
    QCOMPARE(proxy->getActiveLayer(), answers);

    // The note layer is deleted, its square moves to the other layer
    QVERIFY(!sidebar->deleteLayer(notes, false, notes));
    QVERIFY(sidebar->deleteLayer(notes, false, answers));
    QCOMPARE(layers(), (std::vector<pdf::PDFObjectReference>{ answers }));
    const pdf::PDFAnnotationPtr annotation = pdf::PDFAnnotation::parse(&controller->getDocument()->getStorage(), square);
    QCOMPARE(annotation->getOptionalContent(), answers);

    // Each operation is an undo step
    window.findChild<QAction*>("actionUndo")->trigger();
    QCOMPARE(layers(), (std::vector<pdf::PDFObjectReference>{ notes, answers }));

    // After the undo, the square is in the note layer again: deleting the other layer
    // (without objects) keeps it, deleting the note layer with its objects deletes it
    QCOMPARE(pdf::PDFAnnotation::parse(&controller->getDocument()->getStorage(), square)->getOptionalContent(), notes);
    QVERIFY(sidebar->deleteLayer(answers, true));
    QCOMPARE(layers(), (std::vector<pdf::PDFObjectReference>{ notes }));
    QCOMPARE(controller->getDocument()->getCatalog()->getPage(0)->getAnnotations(), (std::vector<pdf::PDFObjectReference>{ square }));
    QVERIFY(sidebar->deleteLayer(notes, true));
    QVERIFY(controller->getDocument()->getCatalog()->getPage(0)->getAnnotations().empty());
    QVERIFY(!properties()->isValid());

    // Without layers, the page stays available (a layer can be created)
    QVERIFY(!sidebar->isEmpty(PDFSidebarWidget::OptionalContent));
}

void EditorMainWindowTest::openFromMenu()
{
    // File > Open (the file dialog) opens the document; the action was also connected
    // to empty functions and it crashed with std::bad_function_call
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath("open.pdf");
    {
        pdf::PDFDocumentBuilder builder;
        builder.appendPage(QRectF(0, 0, 595, 842));
        const pdf::PDFDocument document = builder.build();
        pdf::PDFDocumentWriter writer(nullptr);
        QVERIFY(writer.write(fileName, &document, false));
    }
    PDFEditorMainWindow window;
    window.show();
    QTimer::singleShot(500, [&]()
    {
        QFileDialog* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
        QLineEdit* fileNameEdit = dialog ? dialog->findChild<QLineEdit*>("fileNameEdit") : nullptr;
        if (fileNameEdit)
        {
            // The path is typed in the dialog and confirmed by Enter
            fileNameEdit->setText(fileName);
            QTest::keyClick(fileNameEdit, Qt::Key_Return);
        }
        else if (dialog)
        {
            dialog->reject();
        }
    });
    window.findChild<QAction*>("actionOpen")->trigger();
    QTRY_VERIFY(window.getProgramController()->getDocument());
    QCOMPARE(window.getProgramController()->getDocument()->getCatalog()->getPageCount(), size_t(1));
}

QTEST_MAIN(EditorMainWindowTest)

#include "tst_editormainwindowtest.moc"
