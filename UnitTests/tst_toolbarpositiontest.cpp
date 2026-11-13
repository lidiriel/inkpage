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
