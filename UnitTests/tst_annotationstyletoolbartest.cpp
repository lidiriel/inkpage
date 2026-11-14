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

#include "pdfannotationstyletoolbar.h"
#include "pdfannotationstyle.h"

#include <QtTest>
#include <QAction>
#include <QSettings>

using namespace pdf;
using namespace pdfviewer;

class AnnotationStyleToolBarTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void disabledWithoutToolOrSelection();
    void colorChangesStyleOfActiveTool();
    void fillAndPenWidth();
    void itemsOfTheToolEnableActions();
    void deactivatedToolDisablesActions();

private:
    static QAction* action(PDFAnnotationStyleToolBar& toolBar, const QString& name) { return toolBar.findChild<QAction*>(name); }
    static PDFAnnotationStyle defaultStyle();
};

static const QString STYLE_ID = QStringLiteral("UnitTestStyle");

PDFAnnotationStyle AnnotationStyleToolBarTest::defaultStyle()
{
    PDFAnnotationStyle style;
    style.strokeColor = Qt::blue;
    style.fillColor = QColor();
    style.penWidth = 1.0;
    return style;
}

void AnnotationStyleToolBarTest::initTestCase()
{
    // Separate settings, the settings of the user are not touched
    QCoreApplication::setOrganizationName("inkpage-unittests");
    QCoreApplication::setApplicationName("UnitTestsAnnotationStyleToolBar");
    cleanup();
}

void AnnotationStyleToolBarTest::cleanup()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.clear();
}

void AnnotationStyleToolBarTest::disabledWithoutToolOrSelection()
{
    PDFAnnotationStyleToolBar toolBar(nullptr);
    QVERIFY(!PDFAnnotationStyleHub::getInstance()->getActiveManager());
    QVERIFY(!action(toolBar, "actionStyleColor0")->isEnabled());
    QVERIFY(!action(toolBar, "actionStyleOtherColor")->isEnabled());
    QVERIFY(!action(toolBar, "actionStyleFill")->isEnabled());
    QVERIFY(!action(toolBar, "actionStyleWidth0")->isEnabled());
}

void AnnotationStyleToolBarTest::colorChangesStyleOfActiveTool()
{
    PDFAnnotationStyleToolBar toolBar(nullptr);
    PDFAnnotationStyleManager manager(nullptr, PDFAnnotationStyleManager::StrokeColor | PDFAnnotationStyleManager::FillColor | PDFAnnotationStyleManager::PenWidth, defaultStyle(), STYLE_ID);
    manager.activateStyle();
    QVERIFY(manager.isStyleActive());

    // The red color of the palette (7th color)
    QAction* red = action(toolBar, "actionStyleColor6");
    QVERIFY(red->isEnabled());
    QVERIFY(!red->isChecked());
    red->trigger();
    QCOMPARE(manager.getStyle().strokeColor, QColor(255, 0, 0));
    QVERIFY(red->isChecked());
    QVERIFY(!action(toolBar, "actionStyleColor0")->isChecked());

    // The style is stored, another tool with the same style id gets it
    QCOMPARE(PDFAnnotationStyleSettings::getStyle(STYLE_ID, defaultStyle()).strokeColor, QColor(255, 0, 0));

    manager.deactivateStyle();
}

void AnnotationStyleToolBarTest::fillAndPenWidth()
{
    PDFAnnotationStyleToolBar toolBar(nullptr);
    PDFAnnotationStyleManager manager(nullptr, PDFAnnotationStyleManager::StrokeColor | PDFAnnotationStyleManager::FillColor | PDFAnnotationStyleManager::PenWidth, defaultStyle(), STYLE_ID);
    manager.activateStyle();

    // Fill is a light tint of the stroke color
    QAction* fill = action(toolBar, "actionStyleFill");
    QVERIFY(fill->isEnabled());
    QVERIFY(!fill->isChecked());
    fill->trigger();
    QCOMPARE(manager.getStyle().fillColor, PDFAnnotationStyleToolBar::getFillColor(Qt::blue));
    QVERIFY(fill->isChecked());

    // A new color changes the fill too
    action(toolBar, "actionStyleColor9")->trigger();
    QCOMPARE(manager.getStyle().strokeColor, QColor(255, 255, 0));
    QCOMPARE(manager.getStyle().fillColor, PDFAnnotationStyleToolBar::getFillColor(QColor(255, 255, 0)));

    fill->trigger();
    QVERIFY(!manager.getStyle().fillColor.isValid());
    QVERIFY(!fill->isChecked());

    // Pen widths
    QVERIFY(action(toolBar, "actionStyleWidth0")->isChecked());
    action(toolBar, "actionStyleWidth2")->trigger();
    QCOMPARE(manager.getStyle().penWidth, PDFAnnotationStyleToolBar::PEN_WIDTHS[2]);
    QVERIFY(action(toolBar, "actionStyleWidth2")->isChecked());
    QVERIFY(!action(toolBar, "actionStyleWidth0")->isChecked());

    manager.deactivateStyle();
}

void AnnotationStyleToolBarTest::itemsOfTheToolEnableActions()
{
    PDFAnnotationStyleToolBar toolBar(nullptr);

    // A text markup tool has only a color
    PDFAnnotationStyleManager manager(nullptr, PDFAnnotationStyleManager::StrokeColor, defaultStyle(), STYLE_ID);
    manager.activateStyle();
    QVERIFY(action(toolBar, "actionStyleColor0")->isEnabled());
    QVERIFY(action(toolBar, "actionStyleOtherColor")->isEnabled());
    QVERIFY(!action(toolBar, "actionStyleFill")->isEnabled());
    QVERIFY(!action(toolBar, "actionStyleWidth1")->isEnabled());

    // The toolbar follows the active tool
    PDFAnnotationStyleManager freehandManager(nullptr, PDFAnnotationStyleManager::StrokeColor | PDFAnnotationStyleManager::PenWidth, defaultStyle(), STYLE_ID + "2");
    freehandManager.activateStyle();
    QVERIFY(!manager.isStyleActive());
    QVERIFY(action(toolBar, "actionStyleWidth1")->isEnabled());
    QVERIFY(!action(toolBar, "actionStyleFill")->isEnabled());

    freehandManager.deactivateStyle();
}

void AnnotationStyleToolBarTest::deactivatedToolDisablesActions()
{
    PDFAnnotationStyleToolBar toolBar(nullptr);
    {
        PDFAnnotationStyleManager manager(nullptr, PDFAnnotationStyleManager::StrokeColor, defaultStyle(), STYLE_ID);
        manager.activateStyle();
        QVERIFY(action(toolBar, "actionStyleColor0")->isEnabled());
    }

    // The destroyed manager is removed from the hub
    QVERIFY(!PDFAnnotationStyleHub::getInstance()->getActiveManager());
    QVERIFY(!action(toolBar, "actionStyleColor0")->isEnabled());
}

QTEST_MAIN(AnnotationStyleToolBarTest)

#include "tst_annotationstyletoolbartest.moc"
