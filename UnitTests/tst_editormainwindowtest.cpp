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

#include <QtTest>
#include <QAction>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QDoubleSpinBox>
#include <QMenu>

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
    void stickyNoteIsInInsertToolBar();
    void pageBackgrounds();

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

QTEST_MAIN(EditorMainWindowTest)

#include "tst_editormainwindowtest.moc"
