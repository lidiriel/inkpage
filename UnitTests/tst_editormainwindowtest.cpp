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

#include <QtTest>
#include <QAction>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QDoubleSpinBox>

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

QTEST_MAIN(EditorMainWindowTest)

#include "tst_editormainwindowtest.moc"
