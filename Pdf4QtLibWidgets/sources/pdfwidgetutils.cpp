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

#include <map>
#include <set>
#include <vector>
#include <optional>

#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QLayout>
#include <QToolBar>
#include <QMainWindow>
#include <QGroupBox>
#include <QMessageBox>
#include <QApplication>
#include <QStyleHints>

#include "pdfdbgheap.h"

#ifdef Q_OS_MAC
int qt_default_dpi_x() { return 72; }
int qt_default_dpi_y() { return 72; }
#else
int qt_default_dpi_x() { return 96; }
int qt_default_dpi_y() { return 96; }
#endif

namespace pdf
{

static constexpr bool isScalingNeeded()
{
    return QT_VERSION_MAJOR < 6;
}

int PDFWidgetUtils::getPixelSize(const QPaintDevice* device, pdf::PDFReal sizeMM)
{
    const int width = device->width();
    const int height = device->height();

    if (width > height)
    {
        return PDFReal(width) * sizeMM / PDFReal(device->widthMM());
    }
    else
    {
        return PDFReal(height) * sizeMM / PDFReal(device->heightMM());
    }
}

int PDFWidgetUtils::scaleDPI_x(const QPaintDevice* device, int unscaledSize)
{
    if constexpr (isScalingNeeded())
    {
        const double logicalDPI_x = device->logicalDpiX();
        const double defaultDPI_x = qt_default_dpi_x();
        return (logicalDPI_x / defaultDPI_x) * unscaledSize;
    }
    else
    {
        return unscaledSize;
    }
}

int PDFWidgetUtils::scaleDPI_y(const QPaintDevice* device, int unscaledSize)
{
    if constexpr (isScalingNeeded())
    {
        const double logicalDPI_y = device->logicalDpiY();
        const double defaultDPI_y = qt_default_dpi_y();
        return (logicalDPI_y / defaultDPI_y) * unscaledSize;
    }
    else
    {
        return unscaledSize;
    }
}

PDFReal PDFWidgetUtils::scaleDPI_x(const QPaintDevice* device, PDFReal unscaledSize)
{
    if constexpr (isScalingNeeded())
    {
        const double logicalDPI_x = device->logicalDpiX();
        const double defaultDPI_x = qt_default_dpi_x();
        return (logicalDPI_x / defaultDPI_x) * unscaledSize;
    }
    else
    {
        return unscaledSize;
    }
}

void PDFWidgetUtils::scaleWidget(QWidget* widget, QSize unscaledSize)
{
    if constexpr (!isScalingNeeded())
    {
        return;
    }

    const double logicalDPI_x = widget->logicalDpiX();
    const double logicalDPI_y = widget->logicalDpiY();
    const double defaultDPI_x = qt_default_dpi_x();
    const double defaultDPI_y = qt_default_dpi_y();

    const int width = (logicalDPI_x / defaultDPI_x) * unscaledSize.width();
    const int height = (logicalDPI_y / defaultDPI_y) * unscaledSize.height();

    widget->resize(width, height);
}

QSize PDFWidgetUtils::scaleDPI(const QPaintDevice* widget, QSize unscaledSize)
{
    if constexpr (isScalingNeeded())
    {
        const double logicalDPI_x = widget->logicalDpiX();
        const double logicalDPI_y = widget->logicalDpiY();
        const double defaultDPI_x = qt_default_dpi_x();
        const double defaultDPI_y = qt_default_dpi_y();

        const int width = (logicalDPI_x / defaultDPI_x) * unscaledSize.width();
        const int height = (logicalDPI_y / defaultDPI_y) * unscaledSize.height();

        return QSize(width, height);
    }
    else
    {
        return unscaledSize;
    }
}

void PDFWidgetUtils::style(QWidget* widget)
{
    const int dialogMarginX = scaleDPI_x(widget, 12);
    const int dialogMarginY = scaleDPI_y(widget, 12);
    const int dialogSpacing = scaleDPI_y(widget, 12);

    const int groupBoxMarginX = scaleDPI_x(widget, 20);
    const int groupBoxMarginY = scaleDPI_y(widget, 20);
    const int groupBoxSpacing = scaleDPI_y(widget, 12);

    QList<QWidget*> childWidgets = widget->findChildren<QWidget*>();
    childWidgets.append(widget);

    for (QWidget* childWidget : childWidgets)
    {
        if (qobject_cast<QGroupBox*>(childWidget))
        {
            childWidget->layout()->setContentsMargins(groupBoxMarginX, groupBoxMarginY, groupBoxMarginX, groupBoxMarginY);
            childWidget->layout()->setSpacing(groupBoxSpacing);
        }
        else if (qobject_cast<QDialog*>(childWidget))
        {
            childWidget->layout()->setContentsMargins(dialogMarginX, dialogMarginY, dialogMarginX, dialogMarginY);
            childWidget->layout()->setSpacing(dialogSpacing);
        }
    }
}

void PDFWidgetUtils::setLightTheme()
{
    // inkpage: the dark mode is not supported, the light color scheme is always used
    QApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
}

void PDFWidgetUtils::checkMenuAccessibility(QWidget* widget)
{
    return;

    QList<QMenu*> menus = widget->findChildren<QMenu*>();

    for (QMenu* menu : menus)
    {
        checkMenuAccessibility(menu);
    }
}

void PDFWidgetUtils::checkMenuAccessibility(QMenu* menu)
{
    QStringList actionsDuplicities;
    QStringList actionsWithNoAmpersands;
    std::map<QChar, std::set<QAction*>> actions;

    for (QAction* action : menu->actions())
    {
        if (action->isSeparator())
        {
            continue;
        }

        if (QMenu* submenu = action->menu())
        {
            checkMenuAccessibility(submenu);
        }

        QString text = action->text();
        int i = text.indexOf(QChar('&'));

        if (text.isEmpty())
        {
            continue;
        }

        if (i == -1)
        {
            actionsWithNoAmpersands << text;
        }
        else
        {
            QString accesibilityChar = text.mid(i + 1, 1);
            if (accesibilityChar.size() == 1)
            {
                actions[accesibilityChar.front().toUpper()].insert(action);
            }
        }
    }

    for (const auto& actionItem : actions)
    {
        if (actionItem.second.size() > 1)
        {
            QStringList actionTexts;
            for (QAction* action : actionItem.second)
            {
                actionTexts << action->text();
            }

            actionsDuplicities << QString("'%1': Duplicities = '%2'").arg(QString(actionItem.first), actionTexts.join("', '"));
        }
    }

    QStringList errors;
    errors.append(std::move(actionsDuplicities));
    errors.append(std::move(actionsWithNoAmpersands));

    if (!errors.isEmpty())
    {
        QMessageBox::warning(nullptr, "Accesibility Check", errors.join("<br>"));
    }
}

void PDFWidgetUtils::moveToolBars(QMainWindow* mainWindow, const QList<QToolBar*>& toolBars, Qt::ToolBarArea area)
{
    // Breaks are bound to the toolbar following them, they must be removed before
    // the toolbar is moved, otherwise they stay in the old area.
    std::vector<bool> breaks;
    breaks.reserve(toolBars.size());
    for (QToolBar* toolBar : toolBars)
    {
        const bool hasBreak = mainWindow->toolBarBreak(toolBar);
        breaks.push_back(hasBreak);

        if (hasBreak)
        {
            mainWindow->removeToolBarBreak(toolBar);
        }
    }

    for (qsizetype i = 0; i < toolBars.size(); ++i)
    {
        // A break before the first toolbar has no meaning
        if (i > 0 && breaks[i])
        {
            mainWindow->addToolBarBreak(area);
        }

        mainWindow->addToolBar(area, toolBars[i]);
    }
}

} // namespace pdf
