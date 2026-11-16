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
#include "pdfwidgetannotation.h"
#include "pdfwidgetutils.h"
#include "pdffontdialog.h"

#include <QAction>
#include <QActionGroup>
#include <QColorDialog>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>
#include <QLocale>

#include "pdfdbgheap.h"

namespace pdfviewer
{

PDFAnnotationStyleToolBar::PDFAnnotationStyleToolBar(QWidget* parent) :
    BaseClass(tr("Style"), parent),
    m_colorGroup(new QActionGroup(this)),
    m_widthGroup(new QActionGroup(this)),
    m_colorActions(),
    m_otherColorAction(nullptr),
    m_fillAction(nullptr),
    m_widthActions(),
    m_fontAction(nullptr)
{
    setObjectName("StyleToolBar");
    m_colorGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    m_widthGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);

    const std::array<QString, COLOR_COUNT> colorNames = { tr("Black"), tr("Green"), tr("Light Blue"), tr("Light Green"), tr("Blue"), tr("Gray"),
                                                          tr("Red"), tr("Magenta"), tr("Orange"), tr("Yellow"), tr("White") };
    const std::array<QColor, COLOR_COUNT> colors = getPredefinedColors();
    for (int i = 0; i < COLOR_COUNT; ++i)
    {
        const QColor color = colors[i];
        QAction* action = new QAction(createColorIcon(color), colorNames[i], this);
        action->setObjectName(QString("actionStyleColor%1").arg(i));
        action->setCheckable(true);
        action->setData(color);
        action->setToolTip(tr("Color: %1").arg(colorNames[i]));
        m_colorGroup->addAction(action);
        addAction(action);
        connect(action, &QAction::triggered, this, [this, color]() { applyColor(color); });
        m_colorActions[i] = action;
    }

    m_otherColorAction = addAction(QIcon(":/resources/cms.svg"), tr("Other Color..."));
    m_otherColorAction->setObjectName("actionStyleOtherColor");
    m_otherColorAction->setToolTip(tr("Choose another color"));
    connect(m_otherColorAction, &QAction::triggered, this, &PDFAnnotationStyleToolBar::onOtherColorTriggered);

    addSeparator();

    m_fillAction = addAction(QIcon(":/resources/style-fill.svg"), tr("Fill"));
    m_fillAction->setObjectName("actionStyleFill");
    m_fillAction->setCheckable(true);
    m_fillAction->setToolTip(tr("Fill the shapes with a light tint of the color"));
    connect(m_fillAction, &QAction::triggered, this, &PDFAnnotationStyleToolBar::applyFill);

    addSeparator();

    const std::array<QString, PEN_WIDTHS.size()> widthNames = { tr("Fine"), tr("Medium"), tr("Thick") };
    const std::array<QString, PEN_WIDTHS.size()> widthIcons = { "style-width-fine", "style-width-medium", "style-width-thick" };
    for (size_t i = 0; i < PEN_WIDTHS.size(); ++i)
    {
        const pdf::PDFReal penWidth = PEN_WIDTHS[i];
        QAction* action = new QAction(QIcon(QString(":/resources/%1.svg").arg(widthIcons[i])), widthNames[i], this);
        action->setObjectName(QString("actionStyleWidth%1").arg(i));
        action->setCheckable(true);
        action->setToolTip(tr("Pen width: %1 (%2 pt)").arg(widthNames[i]).arg(penWidth));
        m_widthGroup->addAction(action);
        addAction(action);
        connect(action, &QAction::triggered, this, [this, penWidth]() { applyPenWidth(penWidth * getPenWidthScale()); });
        m_widthActions[i] = action;
    }

    // inkpage: the font of the inline texts (like the font button of Xournal++, it shows the font)
    addSeparator();
    m_fontAction = addAction(tr("Font"));
    m_fontAction->setObjectName("actionStyleFont");
    m_fontAction->setToolTip(tr("Font of the inline text"));
    if (QToolButton* fontButton = qobject_cast<QToolButton*>(widgetForAction(m_fontAction)))
    {
        fontButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    }
    connect(m_fontAction, &QAction::triggered, this, &PDFAnnotationStyleToolBar::onFontTriggered);

    connect(pdf::PDFAnnotationStyleHub::getInstance(), &pdf::PDFAnnotationStyleHub::activeStyleChanged, this, &PDFAnnotationStyleToolBar::updateActions);
    updateActions();
}

PDFAnnotationStyleToolBar::~PDFAnnotationStyleToolBar()
{

}

void PDFAnnotationStyleToolBar::setAnnotationManager(pdf::PDFWidgetAnnotationManager* annotationManager)
{
    if (m_annotationManager)
    {
        disconnect(m_annotationManager, nullptr, this, nullptr);
    }

    m_annotationManager = annotationManager;

    if (m_annotationManager)
    {
        connect(m_annotationManager, &pdf::PDFWidgetAnnotationManager::selectionChanged, this, &PDFAnnotationStyleToolBar::updateActions);
    }

    updateActions();
}

std::array<QColor, PDFAnnotationStyleToolBar::COLOR_COUNT> PDFAnnotationStyleToolBar::getPredefinedColors()
{
    // Palette of Xournal (xournalpp/palettes/xournal.gpl)
    return { QColor(0, 0, 0), QColor(0, 128, 0), QColor(0, 192, 255), QColor(0, 255, 0), QColor(51, 51, 204), QColor(128, 128, 128),
             QColor(255, 0, 0), QColor(255, 0, 255), QColor(255, 128, 0), QColor(255, 255, 0), QColor(255, 255, 255) };
}

pdf::PDFReal PDFAnnotationStyleToolBar::getPenWidthScale()
{
    const pdf::PDFAnnotationStyleManager* manager = pdf::PDFAnnotationStyleHub::getInstance()->getActiveManager();
    return manager ? manager->getPenWidthScale() : 1.0;
}

QColor PDFAnnotationStyleToolBar::getFillColor(const QColor& strokeColor)
{
    // 35 % of the color and 65 % of white
    auto tint = [](int value) { return qRound(value * 0.35 + 255 * 0.65); };
    return QColor(tint(strokeColor.red()), tint(strokeColor.green()), tint(strokeColor.blue()));
}

void PDFAnnotationStyleToolBar::applyColor(const QColor& color)
{
    if (!color.isValid())
    {
        return;
    }

    if (pdf::PDFAnnotationStyleManager* manager = pdf::PDFAnnotationStyleHub::getInstance()->getActiveManager())
    {
        pdf::PDFAnnotationStyle style = manager->getStyle();
        style.strokeColor = color;
        if (style.fillColor.isValid())
        {
            style.fillColor = getFillColor(color);
        }
        manager->setStyle(style);
    }
    else if (hasSelection())
    {
        m_annotationManager->setSelectedAnnotationsColor(color);
    }

    updateActions();
}

void PDFAnnotationStyleToolBar::applyFill(bool enabled)
{
    if (pdf::PDFAnnotationStyleManager* manager = pdf::PDFAnnotationStyleHub::getInstance()->getActiveManager())
    {
        pdf::PDFAnnotationStyle style = manager->getStyle();
        style.fillColor = enabled ? getFillColor(style.strokeColor) : QColor();
        manager->setStyle(style);
    }

    updateActions();
}

void PDFAnnotationStyleToolBar::applyPenWidth(pdf::PDFReal penWidth)
{
    if (pdf::PDFAnnotationStyleManager* manager = pdf::PDFAnnotationStyleHub::getInstance()->getActiveManager())
    {
        pdf::PDFAnnotationStyle style = manager->getStyle();
        style.penWidth = penWidth;
        manager->setStyle(style);
    }
    else if (hasSelection())
    {
        m_annotationManager->setSelectedAnnotationsBorderWidth(penWidth);
    }

    updateActions();
}

void PDFAnnotationStyleToolBar::applyFont(const QString& fontFamily, pdf::PDFReal fontSize)
{
    if (fontFamily.isEmpty() || fontSize <= 0.0)
    {
        return;
    }

    pdf::PDFAnnotationStyleManager* manager = pdf::PDFAnnotationStyleHub::getInstance()->getActiveManager();
    if (manager && manager->getItems().testFlag(pdf::PDFAnnotationStyleManager::Font))
    {
        pdf::PDFAnnotationStyle style = manager->getStyle();
        style.fontFamily = fontFamily;
        style.fontSize = fontSize;
        manager->setStyle(style);
    }
    else if (!manager && hasSelection())
    {
        m_annotationManager->setSelectedAnnotationsFont(fontFamily, fontSize);
    }

    updateActions();
}

bool PDFAnnotationStyleToolBar::getCurrentFont(QString* fontFamily, pdf::PDFReal* fontSize) const
{
    const pdf::PDFAnnotationStyleManager* manager = pdf::PDFAnnotationStyleHub::getInstance()->getActiveManager();
    if (manager)
    {
        if (!manager->getItems().testFlag(pdf::PDFAnnotationStyleManager::Font))
        {
            return false;
        }

        *fontFamily = manager->getStyle().fontFamily;
        *fontSize = manager->getStyle().fontSize;
        return true;
    }

    return hasSelection() && m_annotationManager->getSelectedInlineTextFont(fontFamily, fontSize);
}

void PDFAnnotationStyleToolBar::onFontTriggered()
{
    QString fontFamily;
    pdf::PDFReal fontSize = 0.0;
    if (!getCurrentFont(&fontFamily, &fontSize))
    {
        return;
    }

    PDFFontDialog dialog(this);
    dialog.setFontFamily(fontFamily);
    dialog.setFontSize(fontSize);
    if (dialog.exec() == QDialog::Accepted)
    {
        applyFont(dialog.getFontFamily(), dialog.getFontSize());
    }
}

void PDFAnnotationStyleToolBar::updateActions()
{
    const pdf::PDFAnnotationStyleManager* manager = pdf::PDFAnnotationStyleHub::getInstance()->getActiveManager();
    const pdf::PDFAnnotationStyleManager::StyleItems items = manager ? manager->getItems() : pdf::PDFAnnotationStyleManager::StyleItems();
    const bool isSelection = !manager && hasSelection();

    const bool canSetColor = isSelection || items.testFlag(pdf::PDFAnnotationStyleManager::StrokeColor);
    const bool canSetFill = items.testFlag(pdf::PDFAnnotationStyleManager::FillColor);
    const bool canSetWidth = isSelection || items.testFlag(pdf::PDFAnnotationStyleManager::PenWidth);

    // The actions display the style of the active tool; the selected annotations
    // can have different styles, so nothing is checked for them.
    const pdf::PDFAnnotationStyle style = manager ? manager->getStyle() : pdf::PDFAnnotationStyle();

    for (QAction* action : m_colorActions)
    {
        action->setEnabled(canSetColor);
        action->setChecked(manager && canSetColor && action->data().value<QColor>().rgb() == style.strokeColor.rgb());
    }
    m_otherColorAction->setEnabled(canSetColor);
    m_otherColorAction->setIcon(manager && canSetColor ? createOtherColorIcon(style.strokeColor) : QIcon(":/resources/cms.svg"));

    m_fillAction->setEnabled(canSetFill);
    m_fillAction->setChecked(canSetFill && style.fillColor.isValid());

    // The widths of the active tool can be a multiple of the predefined widths (highlighter)
    const std::array<QString, PEN_WIDTHS.size()> widthNames = { tr("Fine"), tr("Medium"), tr("Thick") };
    const pdf::PDFReal penWidthScale = getPenWidthScale();
    for (size_t i = 0; i < PEN_WIDTHS.size(); ++i)
    {
        const pdf::PDFReal penWidth = PEN_WIDTHS[i] * penWidthScale;
        m_widthActions[i]->setEnabled(canSetWidth);
        m_widthActions[i]->setChecked(manager && canSetWidth && qFuzzyCompare(style.penWidth, penWidth));
        m_widthActions[i]->setToolTip(tr("Pen width: %1 (%2 pt)").arg(widthNames[i]).arg(penWidth));
    }

    // The font button shows the font of the active tool (or of the selected inline text)
    QString fontFamily;
    pdf::PDFReal fontSize = 0.0;
    const bool canSetFont = getCurrentFont(&fontFamily, &fontSize);
    m_fontAction->setEnabled(canSetFont);
    m_fontAction->setText(canSetFont ? QString("%1 %2").arg(fontFamily, QLocale().toString(fontSize)) : tr("Font"));
}

void PDFAnnotationStyleToolBar::onOtherColorTriggered()
{
    const pdf::PDFAnnotationStyleManager* manager = pdf::PDFAnnotationStyleHub::getInstance()->getActiveManager();
    const QColor initialColor = manager ? manager->getStyle().strokeColor : QColor(Qt::red);

    QColorDialog dialog(initialColor, this);
    dialog.setWindowTitle(tr("Select Color"));
    dialog.setOption(QColorDialog::ShowAlphaChannel, false);
    dialog.setOption(QColorDialog::DontUseNativeDialog, true);
    if (dialog.exec() == QDialog::Accepted)
    {
        applyColor(dialog.selectedColor());
    }
}

QIcon PDFAnnotationStyleToolBar::createColorIcon(const QColor& color) const
{
    const int size = 64;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(90, 90, 90), 3.0));
    painter.setBrush(color);
    painter.drawEllipse(QRectF(8, 8, size - 16, size - 16));
    painter.end();

    return QIcon(pixmap);
}

QIcon PDFAnnotationStyleToolBar::createOtherColorIcon(const QColor& color) const
{
    // A color picker pen above a bar of the current color, so the button
    // cannot be mistaken for one of the predefined (round) color swatches.
    const int size = 64;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QIcon(":/resources/style-other-color.svg").paint(&painter, QRect(14, 0, 44, 44));
    painter.setPen(QPen(QColor(90, 90, 90), 3.0));
    painter.setBrush(color);
    painter.drawRoundedRect(QRectF(4, 48, size - 8, 13), 3.0, 3.0);
    painter.end();

    return QIcon(pixmap);
}

bool PDFAnnotationStyleToolBar::hasSelection() const
{
    return m_annotationManager && m_annotationManager->hasSelection();
}

}   // namespace pdfviewer
