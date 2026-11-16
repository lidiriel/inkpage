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
#include "pdftoolbarssettingswidget.h"

#include <QtTest>
#include <QAction>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QPushButton>
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
    void initTestCase();
    void defaultLayout();
    void setLayoutReordersAndSkips();
    void widgetItemIsKeptWhenRemoved();
    void widgetItemOnlyInOneToolBar();
    void separatorsAreDeleted();
    void setLayoutsMovesWidgetItem();
    void submenuOpensAtOnce();
    void resetLayout();
    void settingsStoreOnlyChangedToolBars();
    void settingsRestoreEmptyToolBar();
    void settingsIgnoreUnknownIds();
    void settingsWidgetCategories();
    void settingsWidgetAddRemove();
    void settingsWidgetMoveSeparatorReset();
    void settingsWidgetMovesWidgetItem();
    void settingsWidgetFilter();

private:
    static QStringList ids(std::initializer_list<const char*> list);
    static QStringList listIds(const QListWidget* list);
    static QListWidgetItem* findItem(const QListWidget* list, const QString& id);

    QTemporaryDir m_settingsDirectory;
};

void ToolBarManagerTest::initTestCase()
{
    // Separate settings, the settings of the user are not touched (written by the settings widget)
    QVERIFY(m_settingsDirectory.isValid());
    QCoreApplication::setOrganizationName("inkpage-unittests");
    QCoreApplication::setApplicationName("UnitTestsToolBarManager");
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDirectory.path());
}

QStringList ToolBarManagerTest::listIds(const QListWidget* list)
{
    QStringList result;
    for (int i = 0; i < list->count(); ++i)
    {
        result << list->item(i)->data(Qt::UserRole).toString();
    }
    return result;
}

QListWidgetItem* ToolBarManagerTest::findItem(const QListWidget* list, const QString& id)
{
    for (int i = 0; i < list->count(); ++i)
    {
        if (list->item(i)->data(Qt::UserRole).toString() == id)
        {
            return list->item(i);
        }
    }
    return nullptr;
}

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

void ToolBarManagerTest::setLayoutsMovesWidgetItem()
{
    ToolBarFixture fixture;

    // The second toolbar comes first in the map order or not: the widget moves anyway
    std::map<QToolBar*, QStringList> layouts;
    layouts[fixture.first] = ids({ "actionA" });
    layouts[fixture.second] = ids({ "widgetSpinBox", "actionC" });
    fixture.manager.setLayouts(layouts);
    QCOMPARE(PDFToolBarManager::getLayout(fixture.first), ids({ "actionA" }));
    QCOMPARE(PDFToolBarManager::getLayout(fixture.second), ids({ "widgetSpinBox", "actionC" }));
    QCOMPARE(fixture.second->widgetForAction(fixture.manager.findItem("widgetSpinBox")), fixture.spinBox);

    // An unchanged toolbar is not rebuilt (its separator action is the same)
    fixture.manager.setLayout(fixture.first, ids({ "actionA", "separator" }));
    QAction* separator = fixture.first->actions().back();
    layouts.clear();
    layouts[fixture.first] = ids({ "actionA", "separator" });
    layouts[fixture.second] = ids({ "actionC" });
    fixture.manager.setLayouts(layouts);
    QCOMPARE(fixture.first->actions().back(), separator);
    QCOMPARE(PDFToolBarManager::getLayout(fixture.second), ids({ "actionC" }));
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

/// Menu bar: "&File" (A, B, submenu "&More" with D), "&Edit" (C)
static void createMenus(ToolBarFixture& fixture)
{
    QMenuBar* menuBar = fixture.window.menuBar();
    QMenu* fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction(fixture.actions[0]);
    fileMenu->addAction(fixture.actions[1]);
    fileMenu->addSeparator();
    QMenu* moreMenu = fileMenu->addMenu("&More");
    moreMenu->addAction(fixture.actions[3]);
    QMenu* editMenu = menuBar->addMenu("&Edit");
    editMenu->addAction(fixture.actions[2]);

    // An action without object name cannot be stored in a toolbar
    editMenu->addAction("Unnamed");
}

void ToolBarManagerTest::settingsWidgetCategories()
{
    ToolBarFixture fixture;
    createMenus(fixture);
    PDFToolBarsSettingsWidget widget(&fixture.manager, fixture.window.menuBar(), nullptr);

    QComboBox* categories = widget.findChild<QComboBox*>("toolBarsCategoryComboBox");
    QStringList names;
    for (int i = 0; i < categories->count(); ++i)
    {
        names << categories->itemText(i);
    }
    QCOMPARE(names, (QStringList{ "All", "File", "File > More", "Edit", "Other Items" }));

    QListWidget* available = widget.findChild<QListWidget*>("toolBarsAvailableList");
    QCOMPARE(listIds(available), ids({ "actionA", "actionB", "actionD", "actionC", "widgetSpinBox" }));
    categories->setCurrentIndex(2);
    QCOMPARE(listIds(available), ids({ "actionD" }));
    QCOMPARE(available->item(0)->text(), QString("actionD"));

    // The toolbars and the content of the first one, the actions in it are greyed
    QComboBox* toolBars = widget.findChild<QComboBox*>("toolBarsToolBarComboBox");
    QCOMPARE(toolBars->count(), 2);
    QCOMPARE(toolBars->itemText(0), QString("First"));
    QListWidget* toolBarList = widget.findChild<QListWidget*>("toolBarsToolBarList");
    QCOMPARE(listIds(toolBarList), ids({ "actionA", "actionB", "separator", "widgetSpinBox" }));
    categories->setCurrentIndex(0);
    QVERIFY(!(findItem(available, "actionA")->flags() & Qt::ItemIsEnabled));
    QVERIFY(findItem(available, "actionC")->flags() & Qt::ItemIsEnabled);
}

void ToolBarManagerTest::settingsWidgetAddRemove()
{
    ToolBarFixture fixture;
    createMenus(fixture);
    PDFToolBarsSettingsWidget widget(&fixture.manager, fixture.window.menuBar(), nullptr);
    QListWidget* available = widget.findChild<QListWidget*>("toolBarsAvailableList");
    QListWidget* toolBarList = widget.findChild<QListWidget*>("toolBarsToolBarList");
    QPushButton* addButton = widget.findChild<QPushButton*>("toolBarsAddButton");
    QPushButton* removeButton = widget.findChild<QPushButton*>("toolBarsRemoveButton");

    // Second toolbar: A and D added after C
    widget.findChild<QComboBox*>("toolBarsToolBarComboBox")->setCurrentIndex(1);
    QCOMPARE(listIds(toolBarList), ids({ "actionC" }));
    QVERIFY(!addButton->isEnabled());
    findItem(available, "actionD")->setSelected(true);
    findItem(available, "actionA")->setSelected(true);
    QVERIFY(addButton->isEnabled());
    addButton->click();
    QCOMPARE(listIds(toolBarList), ids({ "actionC", "actionA", "actionD" }));
    QVERIFY(!(findItem(available, "actionD")->flags() & Qt::ItemIsEnabled));

    // Inserted after the selected item
    toolBarList->setCurrentRow(0);
    findItem(available, "actionB")->setSelected(true);
    addButton->click();
    QCOMPARE(listIds(toolBarList), ids({ "actionC", "actionB", "actionA", "actionD" }));

    // Removed: the item is available again
    toolBarList->setCurrentRow(2);
    QVERIFY(removeButton->isEnabled());
    removeButton->click();
    QCOMPARE(listIds(toolBarList), ids({ "actionC", "actionB", "actionD" }));
    QVERIFY(findItem(available, "actionA")->flags() & Qt::ItemIsEnabled);

    // Nothing is changed before apply
    QCOMPARE(PDFToolBarManager::getLayout(fixture.second), ids({ "actionC" }));
    widget.apply();
    QCOMPARE(PDFToolBarManager::getLayout(fixture.second), ids({ "actionC", "actionB", "actionD" }));
    QCOMPARE(PDFToolBarManager::getLayout(fixture.first), fixture.manager.getDefaultLayout(fixture.first));

    // Stored in the settings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    QCOMPARE(settings.value("ToolBars/SecondToolBar").toString(), QString("actionC;actionB;actionD"));
    QVERIFY(!settings.contains("ToolBars/FirstToolBar"));
    settings.remove("ToolBars");
}

void ToolBarManagerTest::settingsWidgetMoveSeparatorReset()
{
    ToolBarFixture fixture;
    PDFToolBarsSettingsWidget widget(&fixture.manager, fixture.window.menuBar(), nullptr);
    QListWidget* toolBarList = widget.findChild<QListWidget*>("toolBarsToolBarList");
    QPushButton* upButton = widget.findChild<QPushButton*>("toolBarsMoveUpButton");
    QPushButton* downButton = widget.findChild<QPushButton*>("toolBarsMoveDownButton");
    QPushButton* resetButton = widget.findChild<QPushButton*>("toolBarsResetButton");
    QVERIFY(!resetButton->isEnabled());

    toolBarList->setCurrentRow(0);
    QVERIFY(!upButton->isEnabled());
    downButton->click();
    QCOMPARE(listIds(toolBarList), ids({ "actionB", "actionA", "separator", "widgetSpinBox" }));
    QCOMPARE(toolBarList->currentRow(), 1);
    upButton->click();
    QCOMPARE(listIds(toolBarList), ids({ "actionA", "actionB", "separator", "widgetSpinBox" }));

    toolBarList->setCurrentRow(3);
    QVERIFY(!downButton->isEnabled());
    widget.findChild<QPushButton*>("toolBarsSeparatorButton")->click();
    QCOMPARE(listIds(toolBarList), ids({ "actionA", "actionB", "separator", "widgetSpinBox", "separator" }));
    QCOMPARE(toolBarList->item(4)->text(), QString("— Separator —"));

    QVERIFY(resetButton->isEnabled());
    resetButton->click();
    QCOMPARE(listIds(toolBarList), fixture.manager.getDefaultLayout(fixture.first));
    QVERIFY(!resetButton->isEnabled());
}

void ToolBarManagerTest::settingsWidgetMovesWidgetItem()
{
    ToolBarFixture fixture;
    PDFToolBarsSettingsWidget widget(&fixture.manager, fixture.window.menuBar(), nullptr);
    QListWidget* available = widget.findChild<QListWidget*>("toolBarsAvailableList");
    QListWidget* toolBarList = widget.findChild<QListWidget*>("toolBarsToolBarList");
    QComboBox* toolBars = widget.findChild<QComboBox*>("toolBarsToolBarComboBox");

    // In the first toolbar: greyed in the second one
    toolBars->setCurrentIndex(1);
    QListWidgetItem* spinBoxItem = findItem(available, "widgetSpinBox");
    QVERIFY(!(spinBoxItem->flags() & Qt::ItemIsEnabled));
    QVERIFY(spinBoxItem->toolTip().contains("First"));

    // Removed from the first toolbar (double click), added to the second one (double click)
    toolBars->setCurrentIndex(0);
    Q_EMIT toolBarList->itemDoubleClicked(toolBarList->item(3));
    QCOMPARE(listIds(toolBarList), ids({ "actionA", "actionB", "separator" }));
    toolBars->setCurrentIndex(1);
    Q_EMIT available->itemDoubleClicked(findItem(available, "widgetSpinBox"));
    QCOMPARE(listIds(toolBarList), ids({ "actionC", "widgetSpinBox" }));

    widget.apply();
    QCOMPARE(fixture.second->widgetForAction(fixture.manager.findItem("widgetSpinBox")), fixture.spinBox);

    // Reset of the first toolbar takes the widget back
    toolBars->setCurrentIndex(0);
    widget.findChild<QPushButton*>("toolBarsResetButton")->click();
    QCOMPARE(widget.getLayouts().at(fixture.second), ids({ "actionC" }));
    widget.apply();
    QCOMPARE(fixture.first->widgetForAction(fixture.manager.findItem("widgetSpinBox")), fixture.spinBox);

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.remove("ToolBars");
}

void ToolBarManagerTest::settingsWidgetFilter()
{
    ToolBarFixture fixture;
    createMenus(fixture);
    fixture.actions[3]->setText("&Delete Page");
    PDFToolBarsSettingsWidget widget(&fixture.manager, fixture.window.menuBar(), nullptr);
    QListWidget* available = widget.findChild<QListWidget*>("toolBarsAvailableList");

    widget.findChild<QLineEdit*>("toolBarsFilterEdit")->setText("page");
    QCOMPARE(listIds(available), ids({ "actionD" }));
    QCOMPARE(available->item(0)->text(), QString("Delete Page"));
    widget.findChild<QLineEdit*>("toolBarsFilterEdit")->clear();
    QCOMPARE(available->count(), 5);
}

QTEST_MAIN(ToolBarManagerTest)

#include "tst_toolbarmanagertest.moc"
