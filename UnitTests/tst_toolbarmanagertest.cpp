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

#include "pdftoolbarmanager.h"

#include <QtTest>
#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QWidgetAction>

using namespace pdfviewer;

/// Main window with two toolbars: "First" (A, B, separator, page spin box) and "Second" (C)
class ToolBarFixture
{
public:
    ToolBarFixture() :
        manager(&window)
    {
        for (const char* name : { "actionA", "actionB", "actionC", "actionD" })
        {
            QAction* action = new QAction(QString::fromLatin1(name), &window);
            action->setObjectName(name);
            actions.push_back(action);
        }

        spinBox = new QSpinBox(&window);
        spinBox->setObjectName("spinBox");

        first = window.addToolBar("First");
        first->setObjectName("FirstToolBar");
        first->addAction(actions[0]);
        first->addAction(actions[1]);
        first->addSeparator();
        first->addAction(manager.createWidgetItem("widgetSpinBox", "Spin box", QIcon(), spinBox));

        second = window.addToolBar("Second");
        second->setObjectName("SecondToolBar");
        second->addAction(actions[2]);

        // actionD is in no toolbar, it is found by the action provider
        manager.setActionProvider([this]() { return actions; });
        manager.registerToolBar(first);
        manager.registerToolBar(second);
    }

    QMainWindow window;
    PDFToolBarManager manager;
    std::vector<QAction*> actions;
    QSpinBox* spinBox = nullptr;
    QToolBar* first = nullptr;
    QToolBar* second = nullptr;
};

class ToolBarManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultLayout();
    void setLayoutReordersAndSkips();
    void widgetItemIsKeptWhenRemoved();
    void widgetItemOnlyInOneToolBar();
    void separatorsAreDeleted();
    void submenuOpensAtOnce();
    void resetLayout();
    void settingsStoreOnlyChangedToolBars();
    void settingsRestoreEmptyToolBar();
    void settingsIgnoreUnknownIds();

private:
    static QStringList ids(std::initializer_list<const char*> list);
};

QStringList ToolBarManagerTest::ids(std::initializer_list<const char*> list)
{
    QStringList result;
    for (const char* id : list)
    {
        result << QString::fromLatin1(id);
    }
    return result;
}

void ToolBarManagerTest::defaultLayout()
{
    ToolBarFixture fixture;
    QCOMPARE(fixture.manager.getToolBars().size(), size_t(2));
    QCOMPARE(fixture.manager.getDefaultLayout(fixture.first), ids({ "actionA", "actionB", "separator", "widgetSpinBox" }));
    QCOMPARE(PDFToolBarManager::getLayout(fixture.first), fixture.manager.getDefaultLayout(fixture.first));
    QCOMPARE(fixture.manager.getDefaultLayout(fixture.second), ids({ "actionC" }));
    QCOMPARE(fixture.manager.getWidgetItems().size(), size_t(1));
    QCOMPARE(fixture.manager.findItem("actionD"), fixture.actions[3]);
    QVERIFY(!fixture.manager.findItem("separator"));
    QVERIFY(!fixture.manager.findItem("unknown"));
}

void ToolBarManagerTest::setLayoutReordersAndSkips()
{
    ToolBarFixture fixture;

    // Order changed, an action of the provider added, unknown id and duplicate skipped
    fixture.manager.setLayout(fixture.first, ids({ "actionD", "unknown", "actionB", "separator", "actionA", "actionB" }));
    QCOMPARE(PDFToolBarManager::getLayout(fixture.first), ids({ "actionD", "actionB", "separator", "actionA" }));

    // An action can be in several toolbars
    fixture.manager.setLayout(fixture.second, ids({ "actionC", "actionA" }));
    QCOMPARE(PDFToolBarManager::getLayout(fixture.second), ids({ "actionC", "actionA" }));
    QVERIFY(fixture.first->actions().contains(fixture.actions[0]));
}

void ToolBarManagerTest::widgetItemIsKeptWhenRemoved()
{
    ToolBarFixture fixture;
    QPointer<QSpinBox> spinBox = fixture.spinBox;
    QAction* widgetItem = fixture.manager.findItem("widgetSpinBox");
    QVERIFY(qobject_cast<QWidgetAction*>(widgetItem));
    QCOMPARE(fixture.first->widgetForAction(widgetItem), spinBox.data());

    fixture.manager.setLayout(fixture.first, ids({ "actionA" }));
    QVERIFY(spinBox);
    QVERIFY(!fixture.first->widgetForAction(widgetItem));

    // The same widget comes back
    fixture.manager.setLayout(fixture.first, ids({ "widgetSpinBox", "actionA" }));
    QCOMPARE(fixture.first->widgetForAction(widgetItem), spinBox.data());

    // Moved to another toolbar
    fixture.manager.setLayout(fixture.first, ids({ "actionA" }));
    fixture.manager.setLayout(fixture.second, ids({ "actionC", "widgetSpinBox" }));
    QCOMPARE(fixture.second->widgetForAction(widgetItem), spinBox.data());
}

void ToolBarManagerTest::widgetItemOnlyInOneToolBar()
{
    ToolBarFixture fixture;
    fixture.manager.setLayout(fixture.second, ids({ "actionC", "widgetSpinBox" }));
    QCOMPARE(PDFToolBarManager::getLayout(fixture.second), ids({ "actionC" }));
    QCOMPARE(fixture.first->widgetForAction(fixture.manager.findItem("widgetSpinBox")), fixture.spinBox);
}

void ToolBarManagerTest::separatorsAreDeleted()
{
    ToolBarFixture fixture;
    QPointer<QAction> separator;
    for (QAction* action : fixture.first->actions())
    {
        if (action->isSeparator())
        {
            separator = action;
        }
    }
    QVERIFY(separator);

    fixture.manager.setLayout(fixture.first, ids({ "actionA", "separator", "separator", "actionB" }));
    QVERIFY(!separator);
    QCOMPARE(PDFToolBarManager::getLayout(fixture.first), ids({ "actionA", "separator", "separator", "actionB" }));
}

void ToolBarManagerTest::submenuOpensAtOnce()
{
    ToolBarFixture fixture;
    QMenu* menu = new QMenu("Menu", &fixture.window);
    menu->addAction("Item");
    menu->menuAction()->setObjectName("actionMenu");
    fixture.actions.push_back(menu->menuAction());

    fixture.manager.setLayout(fixture.second, ids({ "actionMenu" }));
    QToolButton* button = qobject_cast<QToolButton*>(fixture.second->widgetForAction(menu->menuAction()));
    QVERIFY(button);
    QCOMPARE(button->popupMode(), QToolButton::InstantPopup);
}

void ToolBarManagerTest::resetLayout()
{
    ToolBarFixture fixture;
    fixture.manager.setLayout(fixture.first, ids({ "actionD" }));
    fixture.manager.resetLayout(fixture.first);
    QCOMPARE(PDFToolBarManager::getLayout(fixture.first), ids({ "actionA", "actionB", "separator", "widgetSpinBox" }));
    QCOMPARE(fixture.first->widgetForAction(fixture.manager.findItem("widgetSpinBox")), fixture.spinBox);
}

void ToolBarManagerTest::settingsStoreOnlyChangedToolBars()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath("settings.ini");

    {
        ToolBarFixture fixture;
        fixture.manager.setLayout(fixture.second, ids({ "actionD", "separator", "actionC" }));
        QSettings settings(fileName, QSettings::IniFormat);
        fixture.manager.writeSettings(settings);
        QVERIFY(!settings.contains("ToolBars/FirstToolBar"));
        QVERIFY(settings.contains("ToolBars/SecondToolBar"));
    }

    {
        ToolBarFixture fixture;
        QSettings settings(fileName, QSettings::IniFormat);
        fixture.manager.readSettings(settings);
        QCOMPARE(PDFToolBarManager::getLayout(fixture.first), fixture.manager.getDefaultLayout(fixture.first));
        QCOMPARE(PDFToolBarManager::getLayout(fixture.second), ids({ "actionD", "separator", "actionC" }));

        // Back to the default content: the entry is removed
        fixture.manager.resetLayout(fixture.second);
        fixture.manager.writeSettings(settings);
        QVERIFY(!settings.contains("ToolBars/SecondToolBar"));
    }
}

void ToolBarManagerTest::settingsRestoreEmptyToolBar()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString fileName = directory.filePath("settings.ini");

    {
        ToolBarFixture fixture;
        fixture.manager.setLayout(fixture.first, QStringList());
        QSettings settings(fileName, QSettings::IniFormat);
        fixture.manager.writeSettings(settings);
    }

    ToolBarFixture fixture;
    QSettings settings(fileName, QSettings::IniFormat);
    fixture.manager.readSettings(settings);
    QVERIFY(fixture.first->actions().isEmpty());
    QCOMPARE(PDFToolBarManager::getLayout(fixture.second), ids({ "actionC" }));
}

void ToolBarManagerTest::settingsIgnoreUnknownIds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
    settings.setValue("ToolBars/SecondToolBar", "actionPlugin;actionB");

    ToolBarFixture fixture;
    fixture.manager.readSettings(settings);
    QCOMPARE(PDFToolBarManager::getLayout(fixture.second), ids({ "actionB" }));
}

QTEST_MAIN(ToolBarManagerTest)

#include "tst_toolbarmanagertest.moc"
