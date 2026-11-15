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

#include "pdfannotationstyle.h"

#include <QCoreApplication>
#include <QSettings>
#include <QPainter>
#include <QPixmap>

#include <cmath>

#include "pdfdbgheap.h"

namespace pdf
{

static QSettings createSettings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
}

PDFAnnotationStyle PDFAnnotationStyleSettings::getStyle(const QString& styleId, const PDFAnnotationStyle& defaultStyle)
{
    PDFAnnotationStyle style = defaultStyle;

    QSettings settings = createSettings();
    settings.beginGroup("AnnotationStyles");
    settings.beginGroup(styleId);

    const QColor strokeColor = settings.value("strokeColor", defaultStyle.strokeColor).value<QColor>();
    if (strokeColor.isValid())
    {
        style.strokeColor = strokeColor;
    }

    // Jakub Melka: invalid fill color is a valid value - it means, that the shape
    // is not filled at all. Therefore we must distinguish between "not stored yet"
    // and "stored as invalid".
    if (settings.contains("fillColor"))
    {
        style.fillColor = settings.value("fillColor").value<QColor>();
    }

    style.penWidth = qBound(0.0, settings.value("penWidth", defaultStyle.penWidth).toDouble(), 100.0);

    const QString fontFamily = settings.value("fontFamily", defaultStyle.fontFamily).toString();
    if (!fontFamily.isEmpty())
    {
        style.fontFamily = fontFamily;
    }
    style.fontSize = qBound(1.0, settings.value("fontSize", defaultStyle.fontSize).toDouble(), 400.0);

    settings.endGroup();
    settings.endGroup();

    return style;
}

void PDFAnnotationStyleSettings::setStyle(const QString& styleId, const PDFAnnotationStyle& style)
{
    QSettings settings = createSettings();
    settings.beginGroup("AnnotationStyles");
    settings.beginGroup(styleId);
    settings.setValue("strokeColor", style.strokeColor);
    settings.setValue("fillColor", style.fillColor);
    settings.setValue("penWidth", style.penWidth);
    settings.setValue("fontFamily", style.fontFamily);
    settings.setValue("fontSize", style.fontSize);
    settings.endGroup();
    settings.endGroup();
}

QCursor PDFAnnotationCursors::createPenCursor(QColor color, PDFReal penWidth)
{
    const int size = 32;
    const qreal diameter = qBound(5.0, 3.0 + 2.0 * penWidth, 22.0);
    const QColor outline = color.lightness() < 160 ? QColor(Qt::white) : QColor(60, 60, 60);

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(outline, 1.5));
    painter.setBrush(color);
    painter.drawEllipse(QPointF(size / 2.0, size / 2.0), diameter / 2.0, diameter / 2.0);
    painter.end();

    return QCursor(pixmap, size / 2, size / 2);
}

QCursor PDFAnnotationCursors::createEraserCursor(qreal diameter)
{
    diameter = qBound(4.0, diameter, 250.0);
    const int size = int(std::ceil(diameter)) + 4;

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPointF center(size / 2.0, size / 2.0);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::white, 3.0));
    painter.drawEllipse(center, diameter / 2.0, diameter / 2.0);
    painter.setPen(QPen(QColor(60, 60, 60), 1.0));
    painter.drawEllipse(center, diameter / 2.0, diameter / 2.0);
    painter.end();

    return QCursor(pixmap, size / 2, size / 2);
}

QCursor PDFAnnotationCursors::createHighlighterCursor(QColor color)
{
    const int size = 32;
    QColor fill = color;
    fill.setAlpha(170);

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color.darker(160), 1.0));
    painter.setBrush(fill);
    painter.drawRect(QRectF(size / 2.0 - 4.0, size / 2.0 - 10.0, 8.0, 20.0));
    painter.end();

    return QCursor(pixmap, size / 2, size / 2);
}

QCursor PDFAnnotationCursors::createCrossCursor(QColor color)
{
    const int size = 32;
    const QPointF center(size / 2.0, size / 2.0);
    const qreal arm = 9.0;
    const QColor outline = color.lightness() < 160 ? QColor(Qt::white) : QColor(60, 60, 60);

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Outline first, so the cross is visible on both light and dark content
    for (const QPen& pen : { QPen(outline, 4.0, Qt::SolidLine, Qt::RoundCap), QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap) })
    {
        painter.setPen(pen);
        painter.drawLine(center - QPointF(arm, 0.0), center + QPointF(arm, 0.0));
        painter.drawLine(center - QPointF(0.0, arm), center + QPointF(0.0, arm));
    }
    painter.end();

    return QCursor(pixmap, size / 2, size / 2);
}

PDFAnnotationStyleHub::PDFAnnotationStyleHub() :
    BaseClass(nullptr)
{

}

PDFAnnotationStyleHub* PDFAnnotationStyleHub::getInstance()
{
    static PDFAnnotationStyleHub hub;
    return &hub;
}

void PDFAnnotationStyleHub::setActiveManager(PDFAnnotationStyleManager* manager)
{
    if (m_activeManager != manager)
    {
        m_activeManager = manager;
        Q_EMIT activeStyleChanged();
    }
}

void PDFAnnotationStyleHub::notifyStyleChanged(PDFAnnotationStyleManager* manager)
{
    if (manager && m_activeManager == manager)
    {
        Q_EMIT activeStyleChanged();
    }
}

PDFAnnotationStyleManager::PDFAnnotationStyleManager(QObject* parent,
                                                     StyleItems items,
                                                     PDFAnnotationStyle defaultStyle,
                                                     QString styleId) :
    BaseClass(parent),
    m_items(items),
    m_defaultStyle(std::move(defaultStyle)),
    m_styleId(std::move(styleId))
{
    m_style = PDFAnnotationStyleSettings::getStyle(m_styleId, m_defaultStyle);
}

PDFAnnotationStyleManager::~PDFAnnotationStyleManager()
{
    deactivateStyle();
}

void PDFAnnotationStyleManager::setStyle(PDFAnnotationStyle style)
{
    if (m_style == style)
    {
        return;
    }

    m_style = std::move(style);
    PDFAnnotationStyleSettings::setStyle(m_styleId, m_style);

    Q_EMIT styleChanged(m_style);
    PDFAnnotationStyleHub::getInstance()->notifyStyleChanged(this);
}

void PDFAnnotationStyleManager::setStyleId(QString styleId, PDFAnnotationStyle defaultStyle)
{
    if (m_styleId == styleId)
    {
        return;
    }

    m_styleId = std::move(styleId);
    m_defaultStyle = std::move(defaultStyle);
    m_style = PDFAnnotationStyleSettings::getStyle(m_styleId, m_defaultStyle);

    Q_EMIT styleChanged(m_style);
    PDFAnnotationStyleHub::getInstance()->notifyStyleChanged(this);
}

void PDFAnnotationStyleManager::reloadStyle()
{
    PDFAnnotationStyle style = PDFAnnotationStyleSettings::getStyle(m_styleId, m_defaultStyle);

    if (m_style != style)
    {
        m_style = std::move(style);
        Q_EMIT styleChanged(m_style);
        PDFAnnotationStyleHub::getInstance()->notifyStyleChanged(this);
    }
}

void PDFAnnotationStyleManager::activateStyle()
{
    // Jakub Melka: style can be changed by another tool, which shares the same
    // style id, so it must be reloaded before it is shown in the toolbar.
    reloadStyle();
    PDFAnnotationStyleHub::getInstance()->setActiveManager(this);
}

void PDFAnnotationStyleManager::deactivateStyle()
{
    PDFAnnotationStyleHub* hub = PDFAnnotationStyleHub::getInstance();
    if (hub->getActiveManager() == this)
    {
        hub->setActiveManager(nullptr);
    }
}

bool PDFAnnotationStyleManager::isStyleActive() const
{
    return PDFAnnotationStyleHub::getInstance()->getActiveManager() == this;
}

}   // namespace pdf
