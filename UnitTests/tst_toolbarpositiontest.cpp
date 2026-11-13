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

#include "pdfwidgetutils.h"

#include <QtTest>
#include <QLayout>
#include <QToolBar>
#include <QMainWindow>

class ToolBarPositionTest : public QObject
{
    Q_OBJECT

private slots:
    void moveToRightKeepsOrderAndColumns();
    void moveBackToTopKeepsOrderAndRows();
    void moveEmptyListDoesNothing();

private:
    struct Fixture
    {
        QMainWindow window;
        QToolBar* standard = nullptr;
        QToolBar* pluginA = nullptr;
        QToolBar* pluginB = nullptr;

        /// Standard toolbar in the first row, toolbars of the plugins in the second row,
        /// as the editor creates them
        Fixture()
        {
            window.setCentralWidget(new QWidget(&window));
            standard = createToolBar("Standard");
            window.addToolBarBreak();
            pluginA = createToolBar("PluginA");
            pluginB = createToolBar("PluginB");
            window.resize(800, 600);
            window.show();
            layout();
        }

        QToolBar* createToolBar(const QString& name)
        {
            QToolBar* toolBar = window.addToolBar(name);
            toolBar->setObjectName(name);
            for (int i = 0; i < 4; ++i)
            {
                toolBar->addAction(QString("%1 %2").arg(name).arg(i));
            }
            return toolBar;
        }

        QList<QToolBar*> toolBars() const { return { standard, pluginA, pluginB }; }

        void layout()
        {
            window.layout()->activate();
            QCoreApplication::processEvents();
        }
    };
};

void ToolBarPositionTest::moveToRightKeepsOrderAndColumns()
{
    Fixture fixture;
    pdf::PDFWidgetUtils::moveToolBars(&fixture.window, fixture.toolBars(), Qt::RightToolBarArea);
    fixture.layout();

    for (QToolBar* toolBar : fixture.toolBars())
    {
        QCOMPARE(fixture.window.toolBarArea(toolBar), Qt::RightToolBarArea);
        QCOMPARE(toolBar->orientation(), Qt::Vertical);
    }

    // The break before the toolbars of the plugins is kept, they form the second column
    QVERIFY(!fixture.window.toolBarBreak(fixture.standard));
    QVERIFY(fixture.window.toolBarBreak(fixture.pluginA));
    QVERIFY(!fixture.window.toolBarBreak(fixture.pluginB));

    const QRect standard = fixture.standard->geometry();
    const QRect pluginA = fixture.pluginA->geometry();
    const QRect pluginB = fixture.pluginB->geometry();
    QVERIFY(standard.left() != pluginA.left());
    QCOMPARE(pluginA.left(), pluginB.left());
    QVERIFY(pluginA.bottom() < pluginB.top());

    // Nothing is left in the top area
    for (QToolBar* toolBar : fixture.window.findChildren<QToolBar*>(Qt::FindDirectChildrenOnly))
    {
        QVERIFY(fixture.window.toolBarArea(toolBar) != Qt::TopToolBarArea);
    }
}

void ToolBarPositionTest::moveBackToTopKeepsOrderAndRows()
{
    Fixture fixture;
    pdf::PDFWidgetUtils::moveToolBars(&fixture.window, fixture.toolBars(), Qt::RightToolBarArea);
    fixture.layout();
    pdf::PDFWidgetUtils::moveToolBars(&fixture.window, fixture.toolBars(), Qt::TopToolBarArea);
    fixture.layout();

    for (QToolBar* toolBar : fixture.toolBars())
    {
        QCOMPARE(fixture.window.toolBarArea(toolBar), Qt::TopToolBarArea);
        QCOMPARE(toolBar->orientation(), Qt::Horizontal);
    }

    QVERIFY(!fixture.window.toolBarBreak(fixture.standard));
    QVERIFY(fixture.window.toolBarBreak(fixture.pluginA));
    QVERIFY(!fixture.window.toolBarBreak(fixture.pluginB));

    // Standard toolbar in the first row, the toolbars of the plugins side by side in the second row
    const QRect standard = fixture.standard->geometry();
    const QRect pluginA = fixture.pluginA->geometry();
    const QRect pluginB = fixture.pluginB->geometry();
    QVERIFY(standard.bottom() < pluginA.top());
    QCOMPARE(pluginA.top(), pluginB.top());
    QVERIFY(pluginA.right() < pluginB.left());
}

void ToolBarPositionTest::moveEmptyListDoesNothing()
{
    Fixture fixture;
    pdf::PDFWidgetUtils::moveToolBars(&fixture.window, {}, Qt::RightToolBarArea);
    fixture.layout();

    for (QToolBar* toolBar : fixture.toolBars())
    {
        QCOMPARE(fixture.window.toolBarArea(toolBar), Qt::TopToolBarArea);
    }
    QVERIFY(fixture.window.toolBarBreak(fixture.pluginA));
}

QTEST_MAIN(ToolBarPositionTest)

#include "tst_toolbarpositiontest.moc"
