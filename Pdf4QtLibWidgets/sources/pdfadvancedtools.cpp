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

#include "pdfadvancedtools.h"
#include "pdfdocumentbuilder.h"
#include "pdfdrawwidget.h"
#include "pdfutils.h"
#include "pdfcompiler.h"
#include "pdfwidgetformmanager.h"
#include "pdfwidgetannotation.h"
#include "pdfannotationmanipulator.h"
#include "pdfoptionalcontent.h"
#include "pdfwidgetutils.h"
#include "pdfcatalog.h"
#include "insertpagenumbersdialog.h"

#include <QActionGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QColorDialog>
#include <QKeyEvent>
#include <QPushButton>
#include <QTextEdit>
#include <QVector2D>
#include <QVBoxLayout>
#include <QApplication>
#include <QSettings>
#include <QFileDialog>
#include <QImageReader>
#include <QFileInfo>
#include <QMessageBox>

#include <limits>
#include <algorithm>
#include <cmath>

#include "pdfdbgheap.h"

namespace pdf
{

/// Opacity of the fill of the created shape annotations. Fully opaque fill would
/// hide the content of the page below the annotation.
static constexpr float SHAPE_FILL_OPACITY = 0.2f;

/// Default style of the shape annotations (line, polyline, polygon, rectangle,
/// ellipse), which is used until the user selects his own style.
static PDFAnnotationStyle getDefaultShapeStyle()
{
    PDFAnnotationStyle style;
    style.strokeColor = Qt::red;
    style.fillColor = Qt::yellow;
    style.penWidth = 1.0;
    return style;
}

/// Converts the fill color of the style to the color used for the created
/// annotation. Invalid fill color means, that the shape is not filled at all -
/// in that case an invalid color is returned, too. It must not be replaced by a
/// transparent color, because the interior color of an annotation is written
/// without the alpha channel - a transparent black would be stored as an opaque
/// black fill (see PDFObjectFactory::operator<<(WrapAnnotationColor)), while a
/// missing interior color means 'not filled at all'.
static QColor getAnnotationFillColor(const PDFAnnotationStyle& style)
{
    QColor color = style.fillColor;

    if (!color.isValid())
    {
        return QColor();
    }

    color.setAlphaF(SHAPE_FILL_OPACITY);
    return color;
}

/// Returns the brush, which is used to preview the fill of the created annotation.
/// Annotation, which is not filled, is previewed using a fully transparent brush.
static QBrush getAnnotationFillBrush(const PDFAnnotationStyle& style)
{
    const QColor color = getAnnotationFillColor(style);
    return QBrush(color.isValid() ? color : QColor(Qt::transparent), Qt::SolidPattern);
}

PDFCreateStickyNoteTool::PDFCreateStickyNoteTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QActionGroup* actionGroup, QObject* parent) :
    BaseClass(proxy, parent),
    m_toolManager(toolManager),
    m_actionGroup(actionGroup),
    m_pickTool(nullptr),
    m_icon(pdf::TextAnnotationIcon::Comment)
{
    m_pickTool = new PDFPickTool(proxy, PDFPickTool::Mode::Points, this);
    m_pickTool->setSnapToAnnotations(true);
    addTool(m_pickTool);
    connect(m_pickTool, &PDFPickTool::pointPicked, this, &PDFCreateStickyNoteTool::onPointPicked);
    connect(m_actionGroup, &QActionGroup::triggered, this, &PDFCreateStickyNoteTool::onActionTriggered);

    updateActions();
}

void PDFCreateStickyNoteTool::updateActions()
{
    BaseClass::updateActions();

    if (m_actionGroup)
    {
        const bool isEnabled = getDocument() && getDocument()->getStorage().getSecurityHandler()->isAllowed(PDFSecurityHandler::Permission::ModifyInteractiveItems);
        m_actionGroup->setEnabled(isEnabled);

        if (!isActive() && m_actionGroup->checkedAction())
        {
            m_actionGroup->checkedAction()->setChecked(false);
        }
    }
}

void PDFCreateStickyNoteTool::onActionTriggered(QAction* action)
{
    setActive(action && action->isChecked());

    if (action)
    {
        m_icon = static_cast<TextAnnotationIcon>(action->data().toInt());
    }
}

void PDFCreateStickyNoteTool::onPointPicked(PDFInteger pageIndex, QPointF pagePoint)
{
    bool ok = false;
    QString text = QInputDialog::getText(getProxy()->getWidget(), tr("Sticky note"), tr("Enter text to be displayed in the sticky note"), QLineEdit::Normal, QString(), &ok);

    if (ok && !text.isEmpty())
    {
        PDFDocumentModifier modifier(getDocument());

        QString author = PDFAuthorSettings::getAuthorName();
        PDFObjectReference page = getDocument()->getCatalog()->getPage(pageIndex)->getPageReference();
        modifier.getBuilder()->createAnnotationText(page, QRectF(pagePoint, QSizeF(0, 0)), m_icon, author, QString(), text, false);
        modifier.markAnnotationsChanged();

        if (modifier.finalize())
        {
            Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
        }

        setActive(false);
    }
    else
    {
        m_pickTool->resetTool();
    }
}

PDFCreateHyperlinkTool::PDFCreateHyperlinkTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QAction* action, QObject* parent) :
    BaseClass(proxy, action, parent),
    m_toolManager(toolManager),
    m_pickTool(nullptr)
{
    m_pickTool = new PDFPickTool(proxy, PDFPickTool::Mode::Rectangles, this);
    m_pickTool->setSnapToAnnotations(true);
    addTool(m_pickTool);
    connect(m_pickTool, &PDFPickTool::rectanglePicked, this, &PDFCreateHyperlinkTool::onRectanglePicked);

    updateActions();
}

LinkHighlightMode PDFCreateHyperlinkTool::getHighlightMode() const
{
    return m_highlightMode;
}

void PDFCreateHyperlinkTool::setHighlightMode(const LinkHighlightMode& highlightMode)
{
    m_highlightMode = highlightMode;
}

void PDFCreateHyperlinkTool::onRectanglePicked(PDFInteger pageIndex, QRectF pageRectangle)
{
    bool ok = false;
    QString url = QInputDialog::getText(getProxy()->getWidget(), tr("Hyperlink"), tr("Enter url address of the hyperlink"), QLineEdit::Normal, QString(), &ok);

    if (ok && !url.isEmpty())
    {
        PDFDocumentModifier modifier(getDocument());

        PDFObjectReference page = getDocument()->getCatalog()->getPage(pageIndex)->getPageReference();
        modifier.getBuilder()->createAnnotationLink(page, pageRectangle, url, m_highlightMode);
        modifier.markAnnotationsChanged();

        if (modifier.finalize())
        {
            Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
        }

        setActive(false);
    }
}

PDFCreateInDocumentHyperlinkTool::PDFCreateInDocumentHyperlinkTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QActionGroup* actionGroup, QObject* parent) :
    BaseClass(proxy, parent),
    m_toolManager(toolManager),
    m_actionGroup(actionGroup),
    m_pickTool(nullptr)
{
    m_pickTool = new PDFPickTool(proxy, PDFPickTool::Mode::Rectangles, this);
    m_pickTool->setSnapToAnnotations(true);
    addTool(m_pickTool);
    connect(m_pickTool, &PDFPickTool::rectanglePicked, this, &PDFCreateInDocumentHyperlinkTool::onLinkRectanglePicked);
    connect(m_actionGroup, &QActionGroup::triggered, this, &PDFCreateInDocumentHyperlinkTool::onActionTriggered);

    updateActions();
}

LinkHighlightMode PDFCreateInDocumentHyperlinkTool::getHighlightMode() const
{
    return m_highlightMode;
}

void PDFCreateInDocumentHyperlinkTool::setHighlightMode(const LinkHighlightMode& highlightMode)
{
    m_highlightMode = highlightMode;
}

void PDFCreateInDocumentHyperlinkTool::updateActions()
{
    BaseClass::updateActions();

    if (m_actionGroup)
    {
        const bool isEnabled = getDocument() && getDocument()->getStorage().getSecurityHandler()->isAllowed(PDFSecurityHandler::Permission::ModifyInteractiveItems);
        m_actionGroup->setEnabled(isEnabled);

        if (!isActive() && m_actionGroup->checkedAction())
        {
            m_actionGroup->checkedAction()->setChecked(false);
        }
    }
}

void PDFCreateInDocumentHyperlinkTool::setActiveImpl(bool active)
{
    BaseClass::setActiveImpl(active);

    if (active)
    {
        resetPendingLink();
        Q_EMIT messageDisplayRequest(tr("Select hyperlink rectangle."), 5000);
    }
    else
    {
        if (!m_isPickingTarget)
        {
            resetPendingLink();
        }
        m_pickTool->resetTool();
    }
}

void PDFCreateInDocumentHyperlinkTool::onActionTriggered(QAction* action)
{
    if (action)
    {
        m_destinationType = static_cast<DestinationType>(action->data().toInt());
        m_inheritZoom = action->property("inheritZoom").toBool();
    }

    setActive(action && action->isChecked());
}

void PDFCreateInDocumentHyperlinkTool::onLinkRectanglePicked(PDFInteger pageIndex, QRectF pageRectangle)
{
    if (pageRectangle.isEmpty())
    {
        m_pickTool->resetTool();
        return;
    }

    m_linkPageIndex = pageIndex;
    m_linkRectangle = pageRectangle;

    if (isRectangleDestination())
    {
        Q_EMIT messageDisplayRequest(tr("Select target rectangle."), 5000);
        m_isPickingTarget = true;
        m_toolManager->pickRectangle([this](PDFInteger targetPageIndex, QRectF targetRectangle) { onTargetRectanglePicked(targetPageIndex, targetRectangle); });
    }
    else
    {
        Q_EMIT messageDisplayRequest(tr("Select target page."), 5000);
        m_isPickingTarget = true;
        m_toolManager->pickPage([this](PDFInteger targetPageIndex) { onTargetPagePicked(targetPageIndex); });
    }
}

void PDFCreateInDocumentHyperlinkTool::onTargetPagePicked(PDFInteger pageIndex)
{
    createLinkAnnotation(createDestination(pageIndex, QRectF()));
}

void PDFCreateInDocumentHyperlinkTool::onTargetRectanglePicked(PDFInteger pageIndex, QRectF pageRectangle)
{
    if (pageRectangle.isEmpty())
    {
        resetPendingLink();
        updateActions();
        return;
    }

    createLinkAnnotation(createDestination(pageIndex, pageRectangle));
}

void PDFCreateInDocumentHyperlinkTool::createLinkAnnotation(const PDFDestination& destination)
{
    if (m_linkPageIndex < 0 || m_linkRectangle.isEmpty() || !destination.isValid())
    {
        return;
    }

    PDFDocumentModifier modifier(getDocument());

    PDFObjectReference page = getDocument()->getCatalog()->getPage(m_linkPageIndex)->getPageReference();
    PDFObjectReference action = modifier.getBuilder()->createActionGoTo(destination);
    modifier.getBuilder()->createAnnotationLink(page, m_linkRectangle, action, m_highlightMode);
    modifier.markAnnotationsChanged();

    if (modifier.finalize())
    {
        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    }

    resetPendingLink();
    setActive(false);
    updateActions();
}

PDFDestination PDFCreateInDocumentHyperlinkTool::createDestination(PDFInteger pageIndex, QRectF pageRectangle) const
{
    PDFDestination destination;
    destination.setDestinationType(m_destinationType);
    destination.setPageIndex(pageIndex);
    destination.setPageReference(getDocument()->getCatalog()->getPage(pageIndex)->getPageReference());
    destination.setZoom(m_inheritZoom ? std::numeric_limits<PDFReal>::quiet_NaN() : getProxy()->getZoom());

    if (!pageRectangle.isEmpty())
    {
        destination.setLeft(pageRectangle.left());
        destination.setRight(pageRectangle.right());
        destination.setTop(pageRectangle.bottom());
        destination.setBottom(pageRectangle.top());
    }

    return destination;
}

bool PDFCreateInDocumentHyperlinkTool::isRectangleDestination() const
{
    switch (m_destinationType)
    {
        case DestinationType::FitR:
        case DestinationType::XYZ:
            return true;

        case DestinationType::Fit:
        case DestinationType::FitH:
        case DestinationType::FitV:
        case DestinationType::FitB:
        case DestinationType::FitBH:
        case DestinationType::FitBV:
        case DestinationType::Invalid:
        case DestinationType::Named:
            break;
    }

    return false;
}

void PDFCreateInDocumentHyperlinkTool::resetPendingLink()
{
    m_isPickingTarget = false;
    m_linkPageIndex = -1;
    m_linkRectangle = QRectF();
}

PDFCreateFreeTextTool::PDFCreateFreeTextTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QAction* action, QObject* parent) :
    BaseClass(proxy, action, parent),
    m_toolManager(toolManager),
    m_pickTool(nullptr),
    m_styleManager(nullptr)
{
    // inkpage: the font, its size and the color of the text are set by the style toolbar
    PDFAnnotationStyle defaultStyle;
    defaultStyle.strokeColor = Qt::black;
    defaultStyle.fontFamily = m_style.fontFamily;
    defaultStyle.fontSize = m_style.fontSize;
    m_styleManager = new PDFAnnotationStyleManager(this, PDFAnnotationStyleManager::StrokeColor | PDFAnnotationStyleManager::Font, defaultStyle, PDFAnnotationStyleSettings::STYLE_INLINE_TEXT);

    m_pickTool = new PDFPickTool(proxy, PDFPickTool::Mode::Rectangles, this);
    m_pickTool->setSnapToAnnotations(true);
    addTool(m_pickTool);
    connect(m_pickTool, &PDFPickTool::rectanglePicked, this, &PDFCreateFreeTextTool::onRectanglePicked);
    m_pickTool->setPointerCursor(QCursor(Qt::IBeamCursor));

    updateActions();
}

void PDFCreateFreeTextTool::setActiveImpl(bool active)
{
    BaseClass::setActiveImpl(active);

    if (active)
    {
        m_styleManager->activateStyle();
    }
    else
    {
        m_styleManager->deactivateStyle();
    }
}

const PDFAnnotationStyle& PDFCreateFreeTextTool::getStyle() const
{
    return m_styleManager->getStyle();
}

bool PDFCreateFreeTextTool::configureFreeText(QString& text)
{
    // The dialog starts with the style of the tool
    const PDFAnnotationStyle& toolStyle = m_styleManager->getStyle();
    m_style.fontFamily = toolStyle.fontFamily;
    m_style.fontSize = toolStyle.fontSize;
    m_style.textColor = toolStyle.strokeColor.isValid() ? toolStyle.strokeColor : QColor(Qt::black);

    QDialog dialog(getProxy()->getWidget());
    dialog.setWindowTitle(tr("Free text annotation"));

    QVBoxLayout* rootLayout = new QVBoxLayout(&dialog);
    QFormLayout* formLayout = new QFormLayout();

    QTextEdit* textEdit = new QTextEdit(text, &dialog);
    textEdit->setMinimumHeight(140);

    QFontComboBox* fontCombo = new QFontComboBox(&dialog);
    if (!m_style.fontFamily.isEmpty())
    {
        fontCombo->setCurrentFont(QFont(m_style.fontFamily));
    }

    QDoubleSpinBox* fontSizeSpinBox = new QDoubleSpinBox(&dialog);
    fontSizeSpinBox->setRange(1.0, 512.0);
    fontSizeSpinBox->setDecimals(1);
    fontSizeSpinBox->setValue(m_style.fontSize);

    QPushButton* colorButton = new QPushButton(tr("Select"), &dialog);
    auto updateColorButton = [colorButton](const QColor& color)
    {
        const QColor effectiveColor = color.isValid() ? color : QColor(Qt::black);
        colorButton->setText(effectiveColor.name(QColor::HexRgb));
        colorButton->setStyleSheet(QString("background-color: %1;").arg(effectiveColor.name(QColor::HexRgb)));
    };
    updateColorButton(m_style.textColor);

    QComboBox* alignmentCombo = new QComboBox(&dialog);
    alignmentCombo->addItem(tr("Left"), static_cast<int>(Qt::AlignLeft));
    alignmentCombo->addItem(tr("Center"), static_cast<int>(Qt::AlignHCenter));
    alignmentCombo->addItem(tr("Right"), static_cast<int>(Qt::AlignRight));

    const Qt::Alignment horizontalAlignment = Qt::Alignment(m_style.textAlignment) & Qt::AlignHorizontal_Mask;
    int alignmentIndex = alignmentCombo->findData(static_cast<int>(horizontalAlignment));
    if (alignmentIndex < 0)
    {
        alignmentIndex = 0;
    }
    alignmentCombo->setCurrentIndex(alignmentIndex);

    QCheckBox* autoResizeCheckBox = new QCheckBox(tr("Automatically expand annotation to fit text"), &dialog);
    autoResizeCheckBox->setChecked(m_autoResizeToContents);

    formLayout->addRow(tr("Text:"), textEdit);
    formLayout->addRow(tr("Font:"), fontCombo);
    formLayout->addRow(tr("Size:"), fontSizeSpinBox);
    formLayout->addRow(tr("Color:"), colorButton);
    formLayout->addRow(tr("Alignment:"), alignmentCombo);
    formLayout->addRow(QString(), autoResizeCheckBox);
    rootLayout->addLayout(formLayout);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    rootLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QColor selectedColor = m_style.textColor;
    connect(colorButton, &QPushButton::clicked, &dialog, [&dialog, &selectedColor, updateColorButton]()
    {
        QColor color = QColorDialog::getColor(selectedColor, &dialog, QObject::tr("Text color"));
        if (color.isValid())
        {
            selectedColor = color;
            updateColorButton(selectedColor);
        }
    });

    if (dialog.exec() != QDialog::Accepted)
    {
        return false;
    }

    text = textEdit->toPlainText();
    m_style.fontFamily = fontCombo->currentFont().family();
    m_style.fontSize = fontSizeSpinBox->value();
    m_style.textColor = selectedColor.isValid() ? selectedColor : QColor(Qt::black);
    m_style.textAlignment = TextAlignment(Qt::AlignTop | Qt::Alignment(alignmentCombo->currentData().toInt()));
    m_autoResizeToContents = autoResizeCheckBox->isChecked();

    // The choice of the dialog becomes the style of the tool (shown in the style toolbar)
    PDFAnnotationStyle updatedStyle = m_styleManager->getStyle();
    updatedStyle.fontFamily = m_style.fontFamily;
    updatedStyle.fontSize = m_style.fontSize;
    updatedStyle.strokeColor = m_style.textColor;
    m_styleManager->setStyle(updatedStyle);

    return !text.trimmed().isEmpty();
}

void PDFCreateFreeTextTool::onRectanglePicked(PDFInteger pageIndex, QRectF pageRectangle)
{
    QString text;
    if (!configureFreeText(text))
    {
        return;
    }

    PDFDocumentModifier modifier(getDocument());

    QString author = PDFAuthorSettings::getAuthorName();
    PDFObjectReference page = getDocument()->getCatalog()->getPage(pageIndex)->getPageReference();
    const PDFObjectReference annotation = modifier.getBuilder()->createAnnotationFreeText(page, pageRectangle, author, QString(), text, m_style, m_autoResizeToContents);
    modifier.getBuilder()->setAnnotationNoteLayer(annotation, getProxy()->getActiveLayer());
    modifier.markAnnotationsChanged();

    if (modifier.finalize())
    {
        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    }

    setActive(false);
}

PDFCreateImageTool::PDFCreateImageTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QAction* action, QObject* parent) :
    BaseClass(proxy, action, parent),
    m_toolManager(toolManager),
    m_pickTool(nullptr)
{
    m_pickTool = new PDFPickTool(proxy, PDFPickTool::Mode::Rectangles, this);
    m_pickTool->setSnapToAnnotations(true);
    addTool(m_pickTool);
    connect(m_pickTool, &PDFPickTool::rectanglePicked, this, &PDFCreateImageTool::onRectanglePicked);
    m_pickTool->setPointerCursor(QCursor(Qt::CrossCursor));

    updateActions();
}

QRectF PDFCreateImageTool::fitImageIntoRectangle(QSizeF imageSize, QRectF rectangle)
{
    rectangle = rectangle.normalized();
    if (imageSize.isEmpty() || rectangle.isEmpty())
    {
        return rectangle;
    }

    const QSizeF size = imageSize.scaled(rectangle.size(), Qt::KeepAspectRatio);
    const PDFReal left = rectangle.left() + (rectangle.width() - size.width()) / 2.0;
    return QRectF(left, rectangle.bottom() - size.height(), size.width(), size.height());
}

QString PDFCreateImageTool::getImageFileFilter()
{
    QStringList patterns;
    for (const QByteArray& format : QImageReader::supportedImageFormats())
    {
        patterns << QString("*.%1").arg(QString::fromLatin1(format));
    }
    return tr("Images (%1)").arg(patterns.join(' '));
}

void PDFCreateImageTool::onRectanglePicked(PDFInteger pageIndex, QRectF pageRectangle)
{
    const QString fileName = QFileDialog::getOpenFileName(getProxy()->getWidget(), tr("Insert Image"), QString(), getImageFileFilter());
    if (fileName.isEmpty())
    {
        return;
    }

    QImageReader reader(fileName);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull())
    {
        QMessageBox::critical(getProxy()->getWidget(), tr("Error"), tr("Cannot read the image '%1': %2").arg(fileName, reader.errorString()));
        return;
    }

    PDFDocumentModifier modifier(getDocument());
    const PDFObjectReference page = getDocument()->getCatalog()->getPage(pageIndex)->getPageReference();
    const QRectF imageRectangle = fitImageIntoRectangle(image.size(), pageRectangle);
    modifier.getBuilder()->createAnnotationImage(page, imageRectangle, image, PDFAuthorSettings::getAuthorName(), QString(), QFileInfo(fileName).fileName());
    modifier.markAnnotationsChanged();

    if (modifier.finalize())
    {
        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    }

    setActive(false);
}

PDFCreateAnnotationTool::PDFCreateAnnotationTool(PDFDrawWidgetProxy* proxy, QAction* action, QObject* parent) :
    BaseClass(proxy, action, parent)
{

}

void PDFCreateAnnotationTool::updateActions()
{
    if (QAction* action = getAction())
    {
        const bool isEnabled = getDocument() && getDocument()->getStorage().getSecurityHandler()->isAllowed(PDFSecurityHandler::Permission::ModifyInteractiveItems);
        action->setChecked(isActive());
        action->setEnabled(isEnabled);
    }
}

PDFCreateLineTypeTool::PDFCreateLineTypeTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, PDFCreateLineTypeTool::Type type, QAction* action, QObject* parent) :
    BaseClass(proxy, action, parent),
    m_toolManager(toolManager),
    m_pickTool(nullptr),
    m_styleManager(nullptr),
    m_type(type)
{
    PDFPickTool::Mode mode = (type != Type::Rectangle) ? PDFPickTool::Mode::Points : PDFPickTool::Mode::Rectangles;
    m_pickTool = new PDFPickTool(proxy, mode, this);
    m_pickTool->setSnapToAnnotations(true);
    addTool(m_pickTool);
    connect(m_pickTool, &PDFPickTool::pointPicked, this, &PDFCreateLineTypeTool::onPointPicked);
    connect(m_pickTool, &PDFPickTool::rectanglePicked, this, &PDFCreateLineTypeTool::onRectanglePicked);
    m_pickTool->setDrawSelectionRectangle(false);

    m_styleManager = new PDFAnnotationStyleManager(this,
                                                   PDFAnnotationStyleManager::StrokeColor | PDFAnnotationStyleManager::FillColor | PDFAnnotationStyleManager::PenWidth,
                                                   getDefaultShapeStyle(),
                                                   PDFAnnotationStyleSettings::STYLE_SHAPE);
    connect(m_styleManager, &PDFAnnotationStyleManager::styleChanged, this, &PDFCreateLineTypeTool::onStyleChanged);

    updateActions();
}

void PDFCreateLineTypeTool::setActiveImpl(bool active)
{
    BaseClass::setActiveImpl(active);

    if (active)
    {
        Q_EMIT messageDisplayRequest(tr("Use key 'C' to show/hide large cross. Use key 'O' to switch on/off orthogonal mode."), 25000);
        m_styleManager->activateStyle();
        m_pickTool->setPointerCursor(PDFAnnotationCursors::createCrossCursor(m_styleManager->getStyle().strokeColor));
    }
    else
    {
        m_styleManager->deactivateStyle();
    }
}

void PDFCreateLineTypeTool::onStyleChanged(const PDFAnnotationStyle& style)
{
    m_pickTool->setPointerCursor(PDFAnnotationCursors::createCrossCursor(style.strokeColor));
    Q_EMIT getProxy()->repaintNeeded();
}

void PDFCreateLineTypeTool::onPointPicked(PDFInteger pageIndex, QPointF pagePoint)
{
    Q_UNUSED(pageIndex);
    Q_UNUSED(pagePoint);

    if (isOrthogonalMode())
    {
        m_pickTool->makeLastPointOrthogonal();
    }

    if (m_type == Type::Line && m_pickTool->getPickedPoints().size() == 2)
    {
        finishDefinition();
    }
}

void PDFCreateLineTypeTool::onRectanglePicked(PDFInteger pageIndex, QRectF pageRectangle)
{
    m_rectPageIndex = pageIndex;
    m_rectOnPage = pageRectangle;

    finishDefinition();
}

void PDFCreateLineTypeTool::finishDefinition()
{
    const std::vector<QPointF>& pickedPoints = m_pickTool->getPickedPoints();

    const PDFAnnotationStyle& style = m_styleManager->getStyle();
    const PDFReal penWidth = style.penWidth;
    const QColor strokeColor = style.strokeColor;
    const QColor fillColor = getAnnotationFillColor(style);

    switch (m_type)
    {
        case Type::Line:
        {
            if (pickedPoints.size() >= 2)
            {
                PDFDocumentModifier modifier(getDocument());

                QString author = PDFAuthorSettings::getAuthorName();
                PDFObjectReference page = getDocument()->getCatalog()->getPage(m_pickTool->getPageIndex())->getPageReference();
                const PDFObjectReference annotation = modifier.getBuilder()->createAnnotationLine(page, QRectF(), pickedPoints.front(), pickedPoints.back(), penWidth, fillColor, strokeColor, author, QString(), QString(), AnnotationLineEnding::None, AnnotationLineEnding::None);
                modifier.getBuilder()->setAnnotationNoteLayer(annotation, getProxy()->getActiveLayer());
                modifier.markAnnotationsChanged();

                if (modifier.finalize())
                {
                    Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
                }

                setActive(false);
            }
            break;
        }

        case Type::PolyLine:
        {
            if (pickedPoints.size() >= 3)
            {
                PDFDocumentModifier modifier(getDocument());

                QPolygonF polygon;
                for (const QPointF& point : pickedPoints)
                {
                    polygon << point;
                }

                QString author = PDFAuthorSettings::getAuthorName();
                PDFObjectReference page = getDocument()->getCatalog()->getPage(m_pickTool->getPageIndex())->getPageReference();
                const PDFObjectReference annotation = modifier.getBuilder()->createAnnotationPolyline(page, polygon, penWidth, fillColor, strokeColor, author, QString(), QString(), AnnotationLineEnding::None, AnnotationLineEnding::None);
                modifier.getBuilder()->setAnnotationNoteLayer(annotation, getProxy()->getActiveLayer());
                modifier.markAnnotationsChanged();

                if (modifier.finalize())
                {
                    Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
                }

                setActive(false);
            }
            break;
        }

        case Type::Polygon:
        {
            if (pickedPoints.size() >= 3)
            {
                PDFDocumentModifier modifier(getDocument());

                QPolygonF polygon;
                for (const QPointF& point : pickedPoints)
                {
                    polygon << point;
                }
                if (!polygon.isClosed())
                {
                    polygon << pickedPoints.front();
                }

                QString author = PDFAuthorSettings::getAuthorName();
                PDFObjectReference page = getDocument()->getCatalog()->getPage(m_pickTool->getPageIndex())->getPageReference();
                PDFObjectReference annotation = modifier.getBuilder()->createAnnotationPolygon(page, polygon, penWidth, fillColor, strokeColor, author, QString(), QString());
                modifier.getBuilder()->setAnnotationNoteLayer(annotation, getProxy()->getActiveLayer());
                if (fillColor.isValid())
                {
                    modifier.getBuilder()->setAnnotationFillOpacity(annotation, fillColor.alphaF());
                }
                modifier.getBuilder()->updateAnnotationAppearanceStreams(annotation);
                modifier.markAnnotationsChanged();

                if (modifier.finalize())
                {
                    Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
                }

                setActive(false);
            }
            break;
        }

        case Type::Rectangle:
        {
            if (!m_rectOnPage.isEmpty())
            {
                PDFDocumentModifier modifier(getDocument());

                QPolygonF polygon;
                polygon << m_rectOnPage.topLeft();
                polygon << m_rectOnPage.topRight();
                polygon << m_rectOnPage.bottomRight();
                polygon << m_rectOnPage.bottomLeft();
                polygon << m_rectOnPage.topLeft();

                QString author = PDFAuthorSettings::getAuthorName();
                PDFObjectReference page = getDocument()->getCatalog()->getPage(m_pickTool->getPageIndex())->getPageReference();
                PDFObjectReference annotation = modifier.getBuilder()->createAnnotationPolygon(page, polygon, penWidth, fillColor, strokeColor, author, QString(), QString());
                modifier.getBuilder()->setAnnotationNoteLayer(annotation, getProxy()->getActiveLayer());
                if (fillColor.isValid())
                {
                    modifier.getBuilder()->setAnnotationFillOpacity(annotation, fillColor.alphaF());
                }
                modifier.getBuilder()->updateAnnotationAppearanceStreams(annotation);
                modifier.markAnnotationsChanged();

                if (modifier.finalize())
                {
                    Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
                }

                setActive(false);
            }
            break;
        }

        default:
            Q_ASSERT(false);
            break;
    }

    m_pickTool->resetTool();
}

QColor PDFCreateLineTypeTool::getFillColor() const
{
    return m_styleManager->getStyle().fillColor;
}

void PDFCreateLineTypeTool::setFillColor(const QColor& fillColor)
{
    PDFAnnotationStyle style = m_styleManager->getStyle();
    style.fillColor = fillColor;
    m_styleManager->setStyle(style);
}

bool PDFCreateLineTypeTool::canHaveOrthogonalMode() const
{
    switch (m_type)
    {
    case Type::Line:
    case Type::PolyLine:
    case Type::Polygon:
        return true;

    case Type::Rectangle:
        break;
    }

    return false;
}

bool PDFCreateLineTypeTool::isOrthogonalMode() const
{
    return canHaveOrthogonalMode() && m_orthogonalMode;
}

QColor PDFCreateLineTypeTool::getStrokeColor() const
{
    return m_styleManager->getStyle().strokeColor;
}

void PDFCreateLineTypeTool::setStrokeColor(const QColor& strokeColor)
{
    PDFAnnotationStyle style = m_styleManager->getStyle();
    style.strokeColor = strokeColor;
    m_styleManager->setStyle(style);
}

PDFReal PDFCreateLineTypeTool::getPenWidth() const
{
    return m_styleManager->getStyle().penWidth;
}

void PDFCreateLineTypeTool::setPenWidth(PDFReal penWidth)
{
    PDFAnnotationStyle style = m_styleManager->getStyle();
    style.penWidth = penWidth;
    m_styleManager->setStyle(style);
}

void PDFCreateLineTypeTool::keyPressEvent(QWidget* widget, QKeyEvent* event)
{
    Q_UNUSED(widget);

    if (event->key() == Qt::Key_O && canHaveOrthogonalMode())
    {
        m_orthogonalMode = !m_orthogonalMode;
        Q_EMIT getProxy()->repaintNeeded();

        if (m_orthogonalMode)
        {
            Q_EMIT messageDisplayRequest(tr("Orthogonal mode is enabled."), 5000);
        }
        else
        {
            Q_EMIT messageDisplayRequest(tr("Orthogonal mode is disabled."), 5000);
        }
    }

    switch (m_type)
    {
        case Type::PolyLine:
        case Type::Polygon:
        {
            if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
            {
                finishDefinition();
                event->accept();
            }
            else
            {
                event->ignore();
            }

            break;
        }

        default:
            event->ignore();
            break;
    }

    if (!event->isAccepted())
    {
        BaseClass::keyPressEvent(widget, event);
    }
}

void PDFCreateLineTypeTool::keyReleaseEvent(QWidget* widget, QKeyEvent* event)
{
    Q_UNUSED(widget);

    switch (m_type)
    {
        case Type::PolyLine:
        case Type::Polygon:
        {
            if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
            {
                event->accept();
            }
            else
            {
                event->ignore();
            }

            break;
        }

        default:
            event->ignore();
            break;
    }

    if (!event->isAccepted())
    {
        BaseClass::keyReleaseEvent(widget, event);
    }
}

void PDFCreateLineTypeTool::drawPage(QPainter* painter,
                                     PDFInteger pageIndex,
                                     const PDFPrecompiledPage* compiledPage,
                                     PDFTextLayoutGetter& layoutGetter,
                                     const QTransform& pagePointToDevicePointMatrix,
                                     const PDFColorConvertor& convertor,
                                     QList<PDFRenderError>& errors) const
{
    Q_UNUSED(pageIndex);
    Q_UNUSED(compiledPage);
    Q_UNUSED(layoutGetter);
    Q_UNUSED(errors);

    BaseClass::drawPage(painter, pageIndex, compiledPage, layoutGetter, pagePointToDevicePointMatrix, convertor, errors);

    if (pageIndex != m_pickTool->getPageIndex())
    {
        return;
    }

    const std::vector<QPointF>& points = m_pickTool->getPickedPoints();
    if (points.empty())
    {
        return;
    }

    QPointF snappedPoint = m_pickTool->getSnappedPoint();
    QPointF mousePoint = pagePointToDevicePointMatrix.inverted().map(snappedPoint);

    if (isOrthogonalMode())
    {
        mousePoint = m_pickTool->getOrthogonalPoint(mousePoint);
    }

    painter->setWorldTransform(QTransform(pagePointToDevicePointMatrix), true);

    // Jakub Melka: the preview must be color adjusted exactly the same way as the created
    // annotation will be, otherwise it would change its color right after it is created
    PDFColorConvertor annotationConvertor = convertor;
    PDFRenderer::applyFeaturesToAnnotationColorConvertor(getProxy()->getFeatures(), annotationConvertor);

    const PDFAnnotationStyle& style = m_styleManager->getStyle();
    QPen pen = annotationConvertor.convert(QPen(style.strokeColor), false, false);
    QBrush brush = annotationConvertor.convert(getAnnotationFillBrush(style), false, false);
    pen.setWidthF(style.penWidth);
    painter->setPen(qMove(pen));
    painter->setBrush(qMove(brush));
    painter->setRenderHint(QPainter::Antialiasing);

    switch (m_type)
    {
        case Type::Line:
        case Type::PolyLine:
        {
            for (size_t i = 1; i < points.size(); ++i)
            {
                painter->drawLine(points[i - 1], points[i]);
            }
            painter->drawLine(points.back(), mousePoint);
            break;
        }

        case Type::Polygon:
        {
            QPainterPath path;
            path.moveTo(points.front());
            for (size_t i = 1; i < points.size(); ++i)
            {
                path.lineTo(points[i]);
            }
            path.lineTo(mousePoint);
            path.closeSubpath();

            painter->drawPath(path);
            break;
        }

        case Type::Rectangle:
        {
            QPointF startPoint = points.front();
            qreal x1 = qMin(startPoint.x(), mousePoint.x());
            qreal y1 = qMin(startPoint.y(), mousePoint.y());
            qreal x2 = qMax(startPoint.x(), mousePoint.x());
            qreal y2 = qMax(startPoint.y(), mousePoint.y());

            QRectF rect(x1, y1, x2 - x1, y2 - y1);
            painter->drawRect(rect);
            break;
        }

        default:
            Q_ASSERT(false);
            break;
    }
}

PDFCreateEllipseTool::PDFCreateEllipseTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QAction* action, QObject* parent) :
    BaseClass(proxy, action, parent),
    m_toolManager(toolManager),
    m_pickTool(nullptr),
    m_styleManager(nullptr)
{
    m_pickTool = new PDFPickTool(proxy, PDFPickTool::Mode::Rectangles, this);
    m_pickTool->setSnapToAnnotations(true);
    m_pickTool->setDrawSelectionRectangle(false);
    addTool(m_pickTool);
    connect(m_pickTool, &PDFPickTool::rectanglePicked, this, &PDFCreateEllipseTool::onRectanglePicked);

    m_styleManager = new PDFAnnotationStyleManager(this,
                                                   PDFAnnotationStyleManager::StrokeColor | PDFAnnotationStyleManager::FillColor | PDFAnnotationStyleManager::PenWidth,
                                                   getDefaultShapeStyle(),
                                                   PDFAnnotationStyleSettings::STYLE_SHAPE);
    connect(m_styleManager, &PDFAnnotationStyleManager::styleChanged, this, &PDFCreateEllipseTool::onStyleChanged);

    updateActions();
}

void PDFCreateEllipseTool::setActiveImpl(bool active)
{
    BaseClass::setActiveImpl(active);

    if (active)
    {
        m_styleManager->activateStyle();
        m_pickTool->setPointerCursor(PDFAnnotationCursors::createCrossCursor(m_styleManager->getStyle().strokeColor));
    }
    else
    {
        m_styleManager->deactivateStyle();
    }
}

void PDFCreateEllipseTool::onStyleChanged(const PDFAnnotationStyle& style)
{
    m_pickTool->setPointerCursor(PDFAnnotationCursors::createCrossCursor(style.strokeColor));
    Q_EMIT getProxy()->repaintNeeded();
}

PDFReal PDFCreateEllipseTool::getPenWidth() const
{
    return m_styleManager->getStyle().penWidth;
}

void PDFCreateEllipseTool::setPenWidth(PDFReal penWidth)
{
    PDFAnnotationStyle style = m_styleManager->getStyle();
    style.penWidth = penWidth;
    m_styleManager->setStyle(style);
}

QColor PDFCreateEllipseTool::getStrokeColor() const
{
    return m_styleManager->getStyle().strokeColor;
}

void PDFCreateEllipseTool::setStrokeColor(const QColor& strokeColor)
{
    PDFAnnotationStyle style = m_styleManager->getStyle();
    style.strokeColor = strokeColor;
    m_styleManager->setStyle(style);
}

QColor PDFCreateEllipseTool::getFillColor() const
{
    return m_styleManager->getStyle().fillColor;
}

void PDFCreateEllipseTool::setFillColor(const QColor& fillColor)
{
    PDFAnnotationStyle style = m_styleManager->getStyle();
    style.fillColor = fillColor;
    m_styleManager->setStyle(style);
}

void PDFCreateEllipseTool::drawPage(QPainter* painter,
                                    PDFInteger pageIndex,
                                    const PDFPrecompiledPage* compiledPage,
                                    PDFTextLayoutGetter& layoutGetter,
                                    const QTransform& pagePointToDevicePointMatrix,
                                    const PDFColorConvertor& convertor,
                                    QList<PDFRenderError>& errors) const
{
    BaseClass::drawPage(painter, pageIndex, compiledPage, layoutGetter, pagePointToDevicePointMatrix, convertor, errors);

    if (pageIndex != m_pickTool->getPageIndex())
    {
        return;
    }

    const std::vector<QPointF>& points = m_pickTool->getPickedPoints();
    if (points.empty())
    {
        return;
    }

    QPointF mousePoint = pagePointToDevicePointMatrix.inverted().map(m_pickTool->getSnappedPoint());

    painter->setWorldTransform(QTransform(pagePointToDevicePointMatrix), true);

    // Jakub Melka: see the comment in PDFCreateLineTypeTool::drawPage
    PDFColorConvertor annotationConvertor = convertor;
    PDFRenderer::applyFeaturesToAnnotationColorConvertor(getProxy()->getFeatures(), annotationConvertor);

    const PDFAnnotationStyle& style = m_styleManager->getStyle();
    QPen pen = annotationConvertor.convert(QPen(style.strokeColor), false, false);
    QBrush brush = annotationConvertor.convert(getAnnotationFillBrush(style), false, false);
    pen.setWidthF(style.penWidth);
    painter->setPen(qMove(pen));
    painter->setBrush(qMove(brush));
    painter->setRenderHint(QPainter::Antialiasing);

    QPointF point = points.front();
    qreal xMin = qMin(point.x(), mousePoint.x());
    qreal xMax = qMax(point.x(), mousePoint.x());
    qreal yMin = qMin(point.y(), mousePoint.y());
    qreal yMax = qMax(point.y(), mousePoint.y());
    qreal width = xMax - xMin;
    qreal height = yMax - yMin;

    if (!qFuzzyIsNull(width) && !qFuzzyIsNull(height))
    {
        QRectF rect(xMin, yMin, width, height);
        painter->drawEllipse(rect);
    }
}

void PDFCreateEllipseTool::onRectanglePicked(PDFInteger pageIndex, QRectF pageRectangle)
{
    if (pageRectangle.isEmpty())
    {
        return;
    }

    PDFDocumentModifier modifier(getDocument());

    QString author = PDFAuthorSettings::getAuthorName();
    PDFObjectReference page = getDocument()->getCatalog()->getPage(pageIndex)->getPageReference();
    const PDFAnnotationStyle& style = m_styleManager->getStyle();
    const QColor fillColor = getAnnotationFillColor(style);
    PDFObjectReference annotation = modifier.getBuilder()->createAnnotationCircle(page, pageRectangle, style.penWidth, fillColor, style.strokeColor, author, QString(), QString());
    modifier.getBuilder()->setAnnotationNoteLayer(annotation, getProxy()->getActiveLayer());
    if (fillColor.isValid())
    {
        modifier.getBuilder()->setAnnotationFillOpacity(annotation, fillColor.alphaF());
    }
    modifier.getBuilder()->updateAnnotationAppearanceStreams(annotation);
    modifier.markAnnotationsChanged();

    if (modifier.finalize())
    {
        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    }

    setActive(false);
}

PDFCreateFreehandCurveTool::PDFCreateFreehandCurveTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QAction* action, QObject* parent, Type type) :
    BaseClass(proxy, action, parent),
    m_type(type),
    m_toolManager(toolManager),
    m_styleManager(nullptr),
    m_pageIndex(-1)
{
    const bool isHighlighter = m_type == Type::Highlighter;

    // inkpage: the highlighter has its own style - yellow, medium width of the highlighter
    PDFAnnotationStyle defaultStyle;
    defaultStyle.strokeColor = isHighlighter ? QColor(255, 255, 0) : QColor(Qt::red);
    defaultStyle.fillColor = QColor();
    defaultStyle.penWidth = isHighlighter ? 2.0 * HIGHLIGHTER_PEN_WIDTH_SCALE : 1.0;

    m_styleManager = new PDFAnnotationStyleManager(this,
                                                   PDFAnnotationStyleManager::StrokeColor | PDFAnnotationStyleManager::PenWidth,
                                                   defaultStyle,
                                                   isHighlighter ? PDFAnnotationStyleSettings::STYLE_HIGHLIGHTER : PDFAnnotationStyleSettings::STYLE_FREEHAND);
    if (isHighlighter)
    {
        m_styleManager->setPenWidthScale(HIGHLIGHTER_PEN_WIDTH_SCALE);
    }

    connect(m_styleManager, &PDFAnnotationStyleManager::styleChanged, this, [this](const PDFAnnotationStyle&)
    {
        updateCursor();
        Q_EMIT getProxy()->repaintNeeded();
    });
}

void PDFCreateFreehandCurveTool::updateCursor()
{
    const PDFAnnotationStyle& style = m_styleManager->getStyle();
    if (m_type == Type::Highlighter)
    {
        setCursor(PDFAnnotationCursors::createHighlighterCursor(style.strokeColor));
    }
    else
    {
        setCursor(PDFAnnotationCursors::createPenCursor(style.strokeColor, style.penWidth));
    }
}

void PDFCreateFreehandCurveTool::setActiveImpl(bool active)
{
    BaseClass::setActiveImpl(active);

    if (active)
    {
        m_styleManager->activateStyle();
        updateCursor();
    }
    else
    {
        m_styleManager->deactivateStyle();
        resetTool();
    }
}

void PDFCreateFreehandCurveTool::setEventCompressionSuspended(bool suspended)
{
    if (suspended && !m_compressionBeforeStroke)
    {
        m_compressionBeforeStroke = QCoreApplication::testAttribute(Qt::AA_CompressHighFrequencyEvents);
        QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, false);
    }
    else if (!suspended && m_compressionBeforeStroke)
    {
        QCoreApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, *m_compressionBeforeStroke);
        m_compressionBeforeStroke.reset();
    }
}

void PDFCreateFreehandCurveTool::addPoint(const QPointF& pagePoint, bool force)
{
    if (!m_pickedPoints.empty())
    {
        const PDFReal distance = QLineF(m_pickedPoints.back(), pagePoint).length();
        if (distance < MINIMAL_POINT_DISTANCE && !(force && distance > 0.0))
        {
            return;
        }
    }

    m_pickedPoints.push_back(pagePoint);
}

void PDFCreateFreehandCurveTool::drawPage(QPainter* painter,
                                          PDFInteger pageIndex,
                                          const PDFPrecompiledPage* compiledPage,
                                          PDFTextLayoutGetter& layoutGetter,
                                          const QTransform& pagePointToDevicePointMatrix,
                                          const PDFColorConvertor& convertor,
                                          QList<PDFRenderError>& errors) const
{
    BaseClass::drawPage(painter, pageIndex, compiledPage, layoutGetter, pagePointToDevicePointMatrix, convertor, errors);

    if (pageIndex != m_pageIndex || m_pickedPoints.empty())
    {
        return;
    }

    painter->setWorldTransform(QTransform(pagePointToDevicePointMatrix), true);

    // Jakub Melka: see the comment in PDFCreateLineTypeTool::drawPage
    PDFColorConvertor annotationConvertor = convertor;
    PDFRenderer::applyFeaturesToAnnotationColorConvertor(getProxy()->getFeatures(), annotationConvertor);

    const PDFAnnotationStyle& style = m_styleManager->getStyle();
    QPen pen = annotationConvertor.convert(QPen(style.strokeColor), false, false);
    pen.setWidthF(style.penWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);

    if (m_type == Type::Highlighter)
    {
        // The stroke is drawn as one path, so the translucent segments do not overlap
        QColor color = pen.color();
        color.setAlphaF(HIGHLIGHTER_OPACITY);
        pen.setColor(color);
        painter->setCompositionMode(QPainter::CompositionMode_Multiply);
    }

    // The same smooth curve as the created ink annotation
    painter->setPen(qMove(pen));
    painter->drawPath(PDFInkAnnotation::createSmoothPath(QPolygonF(QList<QPointF>(m_pickedPoints.cbegin(), m_pickedPoints.cend()))));
}

void PDFCreateFreehandCurveTool::mousePressEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);
    event->accept();

    if (event->button() == Qt::LeftButton)
    {
        // Try to perform pick point (subpixel position: the pen of a tablet is precise)
        QPointF pagePoint;
        PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->position(), &pagePoint);
        if (pageIndex != -1 &&    // We have picked some point on page
                (m_pageIndex == -1 || m_pageIndex == pageIndex)) // We are under current page
        {
            m_pageIndex = pageIndex;
            addPoint(pagePoint, false);
            setEventCompressionSuspended(true);
        }
    }
    else if (event->button() == Qt::RightButton)
    {
        resetTool();
    }

    Q_EMIT getProxy()->repaintNeeded();
}

void PDFCreateFreehandCurveTool::mouseReleaseEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);
    event->accept();

    if (event->button() == Qt::LeftButton)
    {
        // Try to perform pick point
        QPointF pagePoint;
        PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->position(), &pagePoint);
        if (pageIndex != -1 &&    // We have picked some point on page
                (m_pageIndex == pageIndex)) // We are under current page
        {
            m_pageIndex = pageIndex;
            addPoint(pagePoint, true);

            if (m_pickedPoints.size() >= 3)
            {
                // inkpage: the pen and the highlighter stay active, so several strokes
                // can be drawn (handwriting); Esc or another tool ends the drawing
                createAnnotation();
            }
        }

        resetTool();
    }

    Q_EMIT getProxy()->repaintNeeded();
}

void PDFCreateFreehandCurveTool::mouseMoveEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);
    event->accept();

    if (event->buttons() & Qt::LeftButton && m_pageIndex != -1)
    {
        // Try to add point to the path
        QPointF pagePoint;
        PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->position(), &pagePoint);
        if (pageIndex == m_pageIndex)
        {
            addPoint(pagePoint, false);
        }

        getProxy()->repaintNeeded();
    }
}

PDFReal PDFCreateFreehandCurveTool::getPenWidth() const
{
    return m_styleManager->getStyle().penWidth;
}

void PDFCreateFreehandCurveTool::setPenWidth(const PDFReal& penWidth)
{
    PDFAnnotationStyle style = m_styleManager->getStyle();
    style.penWidth = penWidth;
    m_styleManager->setStyle(style);
}

QColor PDFCreateFreehandCurveTool::getStrokeColor() const
{
    return m_styleManager->getStyle().strokeColor;
}

void PDFCreateFreehandCurveTool::setStrokeColor(const QColor& strokeColor)
{
    PDFAnnotationStyle style = m_styleManager->getStyle();
    style.strokeColor = strokeColor;
    m_styleManager->setStyle(style);
}

void PDFCreateFreehandCurveTool::createAnnotation()
{
    PDFDocumentModifier modifier(getDocument());

    QPolygonF polygon;
    for (const QPointF& point : m_pickedPoints)
    {
        polygon << point;
    }

    QString author = PDFAuthorSettings::getAuthorName();
    PDFObjectReference page = getDocument()->getCatalog()->getPage(m_pageIndex)->getPageReference();
    const PDFAnnotationStyle& style = m_styleManager->getStyle();
    PDFDocumentBuilder* builder = modifier.getBuilder();

    if (m_type == Type::Highlighter)
    {
        // An ink annotation is drawn as one path (a polyline is drawn segment by segment,
        // the translucent segments would be darker where they overlap). The multiply
        // blend mode keeps the text below the stroke black, like a highlighter pen.
        PDFObjectReference annotation = builder->createAnnotationInk(page, polygon, style.penWidth, style.strokeColor, author, QString(), QString());
        builder->setAnnotationNoteLayer(annotation, getProxy()->getActiveLayer());
        builder->setAnnotationOpacity(annotation, HIGHLIGHTER_OPACITY);
        builder->setAnnotationBlendMode(annotation, BlendMode::Multiply);
        builder->updateAnnotationAppearanceStreams(annotation);
    }
    else
    {
        // inkpage: an ink annotation (not a polyline): it is drawn as a smooth curve,
        // and it can hold several paths (the eraser cuts a stroke into pieces)
        const PDFObjectReference annotation = builder->createAnnotationInk(page, polygon, style.penWidth, style.strokeColor, author, QString(), QString());
        builder->setAnnotationNoteLayer(annotation, getProxy()->getActiveLayer());
    }
    modifier.markAnnotationsChanged();

    if (modifier.finalize())
    {
        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    }
}

void PDFCreateFreehandCurveTool::resetTool()
{
    m_pageIndex = -1;
    m_pickedPoints.clear();
    setEventCompressionSuspended(false);
}

PDFEraserTool::PDFEraserTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QAction* action, QObject* parent, Mode mode) :
    BaseClass(proxy, action, parent),
    m_toolManager(toolManager),
    m_styleManager(nullptr),
    m_mode(mode)
{
    // The size of the eraser is set by the widths of the style toolbar (medium: 10 points);
    // both erasers have the same size
    PDFAnnotationStyle defaultStyle;
    defaultStyle.strokeColor = QColor();
    defaultStyle.fillColor = QColor();
    defaultStyle.penWidth = 2.0 * ERASER_SIZE_SCALE;

    m_styleManager = new PDFAnnotationStyleManager(this, PDFAnnotationStyleManager::PenWidth, defaultStyle, PDFAnnotationStyleSettings::STYLE_ERASER);
    m_styleManager->setPenWidthScale(ERASER_SIZE_SCALE);
    connect(m_styleManager, &PDFAnnotationStyleManager::styleChanged, this, &PDFEraserTool::updateCursor);

    // The circle of the cursor follows the zoom
    connect(proxy, &PDFDrawWidgetProxy::drawSpaceChanged, this, &PDFEraserTool::updateCursor);
}

PDFEraserTool::~PDFEraserTool()
{
    cancelErasing();
}

PDFReal PDFEraserTool::getEraserSize() const
{
    return m_styleManager->getStyle().penWidth;
}

void PDFEraserTool::setEraserSize(PDFReal size)
{
    PDFAnnotationStyle style = m_styleManager->getStyle();
    style.penWidth = size;
    m_styleManager->setStyle(style);
}

void PDFEraserTool::setActiveImpl(bool active)
{
    BaseClass::setActiveImpl(active);

    if (active)
    {
        m_styleManager->activateStyle();
        updateCursor();
    }
    else
    {
        cancelErasing();
        m_styleManager->deactivateStyle();
    }
}

void PDFEraserTool::updateCursor()
{
    if (!isActive())
    {
        return;
    }

    // Size of a point on the screen (the zoom of the displayed pages)
    const PDFWidgetSnapshot snapshot = getProxy()->getSnapshot();
    const PDFReal scale = snapshot.items.empty() ? 1.0 : std::sqrt(std::abs(snapshot.items.front().pageToDeviceMatrix.determinant()));
    setCursor(PDFAnnotationCursors::createEraserCursor(getEraserSize() * scale));
}

void PDFEraserTool::startErasing(PDFInteger pageIndex, const QPointF& pagePoint)
{
    cancelErasing();

    const PDFDocument* document = getDocument();
    const PDFPage* page = document ? document->getCatalog()->getPage(pageIndex) : nullptr;
    if (!page || !document->getStorage().getSecurityHandler()->isAllowed(PDFSecurityHandler::Permission::ModifyInteractiveItems))
    {
        return;
    }

    // The annotations of the active layer are erased (see PDFOptionalContentActivity::getActiveLayer,
    // the note layer, when no layer has been chosen), only when the layer is visible
    const PDFOptionalContentProperties* properties = document->getCatalog()->getOptionalContentProperties();
    PDFObjectReference layer = getProxy()->getActiveLayer();
    if (!layer.isValid() && properties && properties->isValid())
    {
        layer = properties->findOptionalContentGroup(QString::fromLatin1(PDFOptionalContentProperties::NOTE_LAYER_NAME));
    }

    const PDFOptionalContentActivity* activity = getProxy()->getOptionalContentActivity();
    if (!layer.isValid() || (activity && activity->getState(layer) == OCState::OFF))
    {
        return;
    }

    m_pageIndex = pageIndex;
    m_lastPoint = pagePoint;

    for (const PDFObjectReference& reference : page->getAnnotations())
    {
        const PDFAnnotationPtr annotation = PDFAnnotation::parse(&document->getStorage(), reference);
        if (!annotation || annotation->getOptionalContent() != layer)
        {
            continue;
        }

        const PDFAnnotation::Flags flags = annotation->getEffectiveFlags();
        if (flags.testFlag(PDFAnnotation::Hidden) || flags.testFlag(PDFAnnotation::NoView) || flags.testFlag(PDFAnnotation::Locked))
        {
            continue;
        }

        PDFAnnotationManipulator::EraserStrokes strokes = PDFAnnotationManipulator::getEraserStrokes(annotation.data());
        if (!strokes.isSupported || strokes.strokes.empty())
        {
            continue;
        }

        Item item;
        item.annotation = reference;
        item.strokes = std::move(strokes.strokes);
        item.penWidth = strokes.penWidth;
        item.isInk = strokes.isInk;
        item.isFilled = strokes.isFilled;
        item.color = PDFAnnotation::getDrawColorFromAnnotationColor(annotation->getColor(), annotation->getStrokeOpacity());
        item.blendMode = annotation->getBlendMode();
        m_items.push_back(std::move(item));
    }
}

void PDFEraserTool::eraseAt(const QPointF& center)
{
    const PDFReal radius = getEraserSize() / 2.0;

    for (Item& item : m_items)
    {
        if (item.strokes.empty())
        {
            continue;
        }

        // The eraser touches the visible stroke, which is wider than its middle line
        const PDFReal effectiveRadius = radius + item.penWidth / 2.0;

        if (item.isFilled)
        {
            // A filled shape is erased whole, when the eraser is on its border or inside it
            const QPolygonF& outline = item.strokes.front();
            if (outline.containsPoint(center, Qt::OddEvenFill) || PDFAnnotationManipulator::isStrokeInCircle(outline, center, effectiveRadius))
            {
                item.strokes.clear();
                item.isModified = true;
            }
            continue;
        }

        if (m_mode == Mode::WholeStroke)
        {
            const size_t count = item.strokes.size();
            item.strokes.erase(std::remove_if(item.strokes.begin(), item.strokes.end(), [&](const QPolygonF& stroke) { return PDFAnnotationManipulator::isStrokeInCircle(stroke, center, effectiveRadius); }), item.strokes.end());
            item.isModified = item.isModified || item.strokes.size() != count;
            continue;
        }

        bool isErased = false;
        std::vector<QPolygonF> strokes = PDFAnnotationManipulator::eraseStrokes(item.strokes, center, effectiveRadius, &isErased);
        if (isErased)
        {
            item.strokes = std::move(strokes);
            item.isModified = true;
        }
    }
}

void PDFEraserTool::eraseTo(const QPointF& pagePoint)
{
    // The eraser goes along the line from the previous point, so a fast move does
    // not leave gaps: the circle moves by a quarter of its size at most
    const QLineF line(m_lastPoint, pagePoint);
    const PDFReal step = qMax(getEraserSize() / 4.0, 0.25);
    const int count = qMax(1, int(std::ceil(line.length() / step)));
    for (int i = 1; i <= count; ++i)
    {
        eraseAt(line.pointAt(qreal(i) / count));
    }

    m_lastPoint = pagePoint;
    updatePreview();
}

void PDFEraserTool::updatePreview()
{
    if (PDFWidgetAnnotationManager* annotationManager = getProxy()->getAnnotationManager())
    {
        std::set<PDFObjectReference> hiddenAnnotations;
        for (const Item& item : m_items)
        {
            if (item.isModified)
            {
                hiddenAnnotations.insert(item.annotation);
            }
        }
        annotationManager->setPreviewHiddenAnnotations(std::move(hiddenAnnotations));
    }

    Q_EMIT getProxy()->repaintNeeded();
}

void PDFEraserTool::cancelErasing()
{
    const bool wasErasing = isErasing();
    m_pageIndex = -1;
    m_items.clear();

    if (wasErasing)
    {
        updatePreview();
    }
}

void PDFEraserTool::finishErasing()
{
    const PDFDocument* document = getDocument();
    const PDFPage* page = (document && m_pageIndex != -1) ? document->getCatalog()->getPage(m_pageIndex) : nullptr;
    const std::vector<Item> items = std::move(m_items);
    cancelErasing();

    const bool isModified = std::any_of(items.cbegin(), items.cend(), [](const Item& item) { return item.isModified; });
    if (!page || !isModified)
    {
        return;
    }

    PDFDocumentModifier modifier(document);
    PDFDocumentBuilder* builder = modifier.getBuilder();
    for (const Item& item : items)
    {
        if (!item.isModified)
        {
            continue;
        }

        if (item.strokes.empty())
        {
            builder->removeAnnotation(page->getPageReference(), item.annotation);
        }
        else if (item.isInk)
        {
            PDFAnnotationManipulator::setParts(builder, item.annotation, item.strokes);
        }
        else
        {
            PDFAnnotationManipulator::convertToInk(builder, item.annotation, item.strokes);
        }
    }
    modifier.markAnnotationsChanged();

    if (modifier.finalize())
    {
        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    }
}

void PDFEraserTool::drawPage(QPainter* painter,
                             PDFInteger pageIndex,
                             const PDFPrecompiledPage* compiledPage,
                             PDFTextLayoutGetter& layoutGetter,
                             const QTransform& pagePointToDevicePointMatrix,
                             const PDFColorConvertor& convertor,
                             QList<PDFRenderError>& errors) const
{
    BaseClass::drawPage(painter, pageIndex, compiledPage, layoutGetter, pagePointToDevicePointMatrix, convertor, errors);

    if (pageIndex != m_pageIndex)
    {
        return;
    }

    // The remaining strokes of the erased annotations, drawn as their ink will be
    PDFColorConvertor annotationConvertor = convertor;
    PDFRenderer::applyFeaturesToAnnotationColorConvertor(getProxy()->getFeatures(), annotationConvertor);

    painter->save();
    painter->setWorldTransform(QTransform(pagePointToDevicePointMatrix), true);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);

    for (const Item& item : m_items)
    {
        if (!item.isModified || item.strokes.empty() || item.penWidth <= 0.0)
        {
            continue;
        }

        QPen pen(annotationConvertor.convert(item.color, false, false));
        pen.setWidthF(item.penWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->setCompositionMode(item.blendMode == BlendMode::Multiply ? QPainter::CompositionMode_Multiply : QPainter::CompositionMode_SourceOver);

        QPainterPath path;
        for (const QPolygonF& stroke : item.strokes)
        {
            path.addPath(PDFInkAnnotation::createSmoothPath(stroke));
        }
        painter->drawPath(path);
    }

    painter->restore();
}

void PDFEraserTool::mousePressEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);
    event->accept();

    if (event->button() == Qt::LeftButton)
    {
        QPointF pagePoint;
        const PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->position(), &pagePoint);
        if (pageIndex != -1)
        {
            startErasing(pageIndex, pagePoint);
            if (isErasing())
            {
                eraseTo(pagePoint);
            }
        }
    }
    else if (event->button() == Qt::RightButton)
    {
        cancelErasing();
    }
}

void PDFEraserTool::mouseMoveEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);
    event->accept();

    if (isErasing() && event->buttons().testFlag(Qt::LeftButton))
    {
        QPointF pagePoint;
        if (getProxy()->getPageUnderPoint(event->position(), &pagePoint) == m_pageIndex)
        {
            eraseTo(pagePoint);
        }
    }
}

void PDFEraserTool::mouseReleaseEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);
    event->accept();

    if (event->button() == Qt::LeftButton && isErasing())
    {
        QPointF pagePoint;
        if (getProxy()->getPageUnderPoint(event->position(), &pagePoint) == m_pageIndex)
        {
            eraseTo(pagePoint);
        }
        finishErasing();
    }
}

PDFCreateStampTool::PDFCreateStampTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QActionGroup* actionGroup, QObject* parent) :
    BaseClass(proxy, parent),
    m_pageIndex(-1),
    m_toolManager(toolManager),
    m_actionGroup(actionGroup),
    m_pickTool(nullptr)
{
    m_pickTool = new PDFPickTool(proxy, PDFPickTool::Mode::Points, this);
    m_pickTool->setSnapToAnnotations(true);
    addTool(m_pickTool);
    connect(m_pickTool, &PDFPickTool::pointPicked, this, &PDFCreateStampTool::onPointPicked);
    connect(m_actionGroup, &QActionGroup::triggered, this, &PDFCreateStampTool::onActionTriggered);

    m_stampAnnotation.setStrokingOpacity(0.5);
    m_stampAnnotation.setFillingOpacity(0.5);

    updateActions();
}

void PDFCreateStampTool::drawPage(QPainter* painter,
                                  PDFInteger pageIndex,
                                  const PDFPrecompiledPage* compiledPage,
                                  PDFTextLayoutGetter& layoutGetter,
                                  const QTransform& pagePointToDevicePointMatrix,
                                  const PDFColorConvertor& convertor,
                                  QList<PDFRenderError>& errors) const
{
    Q_UNUSED(compiledPage);
    Q_UNUSED(layoutGetter);
    Q_UNUSED(pagePointToDevicePointMatrix);
    Q_UNUSED(errors);

    if (pageIndex != m_pageIndex)
    {
        return;
    }

    const PDFPage* page = getDocument()->getCatalog()->getPage(pageIndex);
    QRectF rectangle = m_stampAnnotation.getRectangle();
    QTransform matrix = getProxy()->getAnnotationManager()->prepareTransformations(pagePointToDevicePointMatrix, painter->device(), m_stampAnnotation.getFlags(), page, rectangle);
    painter->setWorldTransform(QTransform(matrix), true);

    AnnotationDrawParameters parameters;
    parameters.painter = painter;
    parameters.annotation = const_cast<PDFStampAnnotation*>(&m_stampAnnotation);
    parameters.key.first = PDFAppeareanceStreams::Appearance::Normal;
    parameters.colorConvertor = convertor;
    PDFRenderer::applyFeaturesToAnnotationColorConvertor(getProxy()->getFeatures(), parameters.colorConvertor);

    m_stampAnnotation.draw(parameters);
}

void PDFCreateStampTool::mouseMoveEvent(QWidget* widget, QMouseEvent* event)
{
    BaseClass::mouseMoveEvent(widget, event);

    // Try to add point to the path
    QPointF pagePoint;
    m_pageIndex = getProxy()->getPageUnderPoint(event->pos(), &pagePoint);
    if (m_pageIndex != -1)
    {
        m_stampAnnotation.setRectangle(QRectF(pagePoint, QSizeF(0, 0)));
    }
}

void PDFCreateStampTool::updateActions()
{
    BaseClass::updateActions();

    if (m_actionGroup)
    {
        const bool isEnabled = getDocument() && getDocument()->getStorage().getSecurityHandler()->isAllowed(PDFSecurityHandler::Permission::ModifyInteractiveItems);
        m_actionGroup->setEnabled(isEnabled);

        if (!isActive() && m_actionGroup->checkedAction())
        {
            m_actionGroup->checkedAction()->setChecked(false);
        }
    }
}

void PDFCreateStampTool::onActionTriggered(QAction* action)
{
    setActive(action && action->isChecked());

    if (action)
    {
        m_stampAnnotation.setStamp(static_cast<Stamp>(action->data().toInt()));
    }
}

void PDFCreateStampTool::onPointPicked(PDFInteger pageIndex, QPointF pagePoint)
{
    PDFDocumentModifier modifier(getDocument());

    QString author = PDFAuthorSettings::getAuthorName();
    PDFObjectReference page = getDocument()->getCatalog()->getPage(pageIndex)->getPageReference();
    modifier.getBuilder()->createAnnotationStamp(page, QRectF(pagePoint, QSizeF(0, 0)), m_stampAnnotation.getStamp(), author, QString(), QString());
    modifier.markAnnotationsChanged();

    if (modifier.finalize())
    {
        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    }

    setActive(false);
}

PDFCreateHighlightTextTool::PDFCreateHighlightTextTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QActionGroup* actionGroup, QObject* parent) :
    BaseClass(proxy, parent),
    m_toolManager(toolManager),
    m_actionGroup(actionGroup),
    m_styleManager(nullptr),
    m_type(AnnotationType::Highlight),
    m_isCursorOverText(false)
{
    connect(m_actionGroup, &QActionGroup::triggered, this, &PDFCreateHighlightTextTool::onActionTriggered);

    PDFAnnotationStyle defaultStyle;
    defaultStyle.strokeColor = getDefaultColor();
    defaultStyle.fillColor = QColor();

    m_styleManager = new PDFAnnotationStyleManager(this, PDFAnnotationStyleManager::StrokeColor, defaultStyle, getStyleId());
    connect(m_styleManager, &PDFAnnotationStyleManager::styleChanged, this, &PDFCreateHighlightTextTool::onStyleChanged);
    m_color = m_styleManager->getStyle().strokeColor;

    updateActions();
}

void PDFCreateHighlightTextTool::drawPage(QPainter* painter,
                                          PDFInteger pageIndex,
                                          const PDFPrecompiledPage* compiledPage,
                                          PDFTextLayoutGetter& layoutGetter,
                                          const QTransform& pagePointToDevicePointMatrix,
                                          const PDFColorConvertor& convertor,
                                          QList<PDFRenderError>& errors) const
{
    Q_UNUSED(compiledPage);
    Q_UNUSED(errors);

    // Jakub Melka: the selection is a preview of the highlight annotation, which will be
    // created from it, so it is color adjusted only when annotations are color adjusted
    PDFColorConvertor annotationConvertor = convertor;
    PDFRenderer::applyFeaturesToAnnotationColorConvertor(getProxy()->getFeatures(), annotationConvertor);

    pdf::PDFTextSelectionPainter textSelectionPainter(&m_textSelection);
    textSelectionPainter.draw(painter, pageIndex, layoutGetter, pagePointToDevicePointMatrix, annotationConvertor);
}

void PDFCreateHighlightTextTool::mousePressEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);

    if (event->button() == Qt::LeftButton)
    {
        QPointF pagePoint;
        const PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->pos(), &pagePoint);
        if (pageIndex != -1)
        {
            m_selectionInfo.pageIndex = pageIndex;
            m_selectionInfo.selectionStartPoint = pagePoint;
            event->accept();
        }
        else
        {
            m_selectionInfo = SelectionInfo();
        }

        setSelection(pdf::PDFTextSelection());
        updateCursor();
    }
}

void PDFCreateHighlightTextTool::mouseReleaseEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);

    if (event->button() == Qt::LeftButton)
    {
        if (m_selectionInfo.pageIndex != -1)
        {
            QPointF pagePoint;
            const PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->pos(), &pagePoint);

            if (m_selectionInfo.pageIndex == pageIndex)
            {
                // Jakub Melka: handle the selection
                PDFTextLayoutGetter textLayoutGetter = getProxy()->getTextLayoutCompiler()->getTextLayoutLazy(pageIndex);
                PDFTextLayout textLayout = textLayoutGetter;
                setSelection(textLayout.createTextSelection(pageIndex, m_selectionInfo.selectionStartPoint, pagePoint, m_color));

                QPolygonF quadrilaterals;
                PDFTextSelectionPainter textSelectionPainter(&m_textSelection);
                QPainterPath path = textSelectionPainter.prepareGeometry(pageIndex, textLayoutGetter, QTransform(), &quadrilaterals);

                if (!path.isEmpty())
                {
                    PDFDocumentModifier modifier(getDocument());

                    PDFObjectReference page = getDocument()->getCatalog()->getPage(pageIndex)->getPageReference();
                    PDFObjectReference annotationReference;
                    switch (m_type)
                    {
                        case AnnotationType::Highlight:
                            annotationReference = modifier.getBuilder()->createAnnotationHighlight(page, quadrilaterals, m_color);
                            modifier.getBuilder()->setAnnotationOpacity(annotationReference, 0.2);
                            modifier.getBuilder()->updateAnnotationAppearanceStreams(annotationReference);
                            break;

                        case AnnotationType::Underline:
                            annotationReference = modifier.getBuilder()->createAnnotationUnderline(page, quadrilaterals, m_color);
                            break;

                        case AnnotationType::Squiggly:
                            annotationReference = modifier.getBuilder()->createAnnotationSquiggly(page, quadrilaterals, m_color);
                            break;

                        case AnnotationType::StrikeOut:
                            annotationReference = modifier.getBuilder()->createAnnotationStrikeout(page, quadrilaterals, m_color);
                            break;

                        default:
                            Q_ASSERT(false);
                            break;
                    }

                    modifier.markAnnotationsChanged();

                    if (modifier.finalize())
                    {
                        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
                    }
                }
            }

            setSelection(pdf::PDFTextSelection());

            m_selectionInfo = SelectionInfo();
            event->accept();
            updateCursor();
        }
    }
}

void PDFCreateHighlightTextTool::mouseMoveEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);

    QPointF pagePoint;
    const PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->pos(), &pagePoint);
    PDFTextLayout textLayout = getProxy()->getTextLayoutCompiler()->getTextLayoutLazy(pageIndex);
    m_isCursorOverText = textLayout.isHoveringOverTextBlock(pagePoint);

    if (m_selectionInfo.pageIndex != -1)
    {
        if (m_selectionInfo.pageIndex == pageIndex)
        {
            // Jakub Melka: handle the selection
            setSelection(textLayout.createTextSelection(pageIndex, m_selectionInfo.selectionStartPoint, pagePoint, m_color));
        }
        else
        {
            setSelection(pdf::PDFTextSelection());
        }

        event->accept();
    }

    updateCursor();
}

void PDFCreateHighlightTextTool::updateActions()
{
    BaseClass::updateActions();

    if (m_actionGroup)
    {
        const bool isEnabled = getDocument() && getDocument()->getStorage().getSecurityHandler()->isAllowed(PDFSecurityHandler::Permission::ModifyInteractiveItems);
        m_actionGroup->setEnabled(isEnabled);

        if (!isActive() && m_actionGroup->checkedAction())
        {
            m_actionGroup->checkedAction()->setChecked(false);
        }
    }
}

void PDFCreateHighlightTextTool::setActiveImpl(bool active)
{
    BaseClass::setActiveImpl(active);

    if (!active)
    {
        // Just clear the text selection
        setSelection(PDFTextSelection());

        m_styleManager->deactivateStyle();
    }
    else
    {
        // Layouts are cached on demand. Annotating a page must not start
        // indexing every page of a potentially large document.
        m_styleManager->activateStyle();
    }
}

void PDFCreateHighlightTextTool::onActionTriggered(QAction* action)
{
    if (action)
    {
        AnnotationType type = static_cast<AnnotationType>(action->data().toInt());

        if (m_type != type)
        {
            m_type = type;

            // Each kind of the text markup has its own persisted style, so the color
            // selected by the user for the highlight is not lost, when he switches
            // to the underline and back.
            PDFAnnotationStyle defaultStyle;
            defaultStyle.strokeColor = getDefaultColor();
            defaultStyle.fillColor = QColor();
            m_styleManager->setStyleId(getStyleId(), defaultStyle);
        }
    }

    setActive(action && action->isChecked());
}

void PDFCreateHighlightTextTool::onStyleChanged(const PDFAnnotationStyle& style)
{
    m_color = style.strokeColor;
    updateCursor();

    if (!m_textSelection.isEmpty())
    {
        // Recreate the selection, so it is drawn using the new color
        setSelection(PDFTextSelection());
    }

    Q_EMIT getProxy()->repaintNeeded();
}

QString PDFCreateHighlightTextTool::getStyleId() const
{
    switch (m_type)
    {
        case AnnotationType::Highlight:
            return PDFAnnotationStyleSettings::STYLE_HIGHLIGHT;

        case AnnotationType::Underline:
            return PDFAnnotationStyleSettings::STYLE_UNDERLINE;

        case AnnotationType::Squiggly:
            return PDFAnnotationStyleSettings::STYLE_SQUIGGLY;

        case AnnotationType::StrikeOut:
            return PDFAnnotationStyleSettings::STYLE_STRIKEOUT;

        default:
            Q_ASSERT(false);
            break;
    }

    return PDFAnnotationStyleSettings::STYLE_HIGHLIGHT;
}

QColor PDFCreateHighlightTextTool::getDefaultColor() const
{
    switch (m_type)
    {
        case AnnotationType::Highlight:
            return QColor(Qt::yellow);

        case AnnotationType::Underline:
            return QColor(Qt::black);

        case AnnotationType::Squiggly:
        case AnnotationType::StrikeOut:
            return QColor(Qt::red);

        default:
            Q_ASSERT(false);
            break;
    }

    return QColor(Qt::yellow);
}

void PDFCreateHighlightTextTool::updateCursor()
{
    if (isActive())
    {
        if (m_type == AnnotationType::Highlight)
        {
            setCursor(PDFAnnotationCursors::createHighlighterCursor(m_color));
        }
        else if (m_isCursorOverText)
        {
            setCursor(QCursor(Qt::IBeamCursor));
        }
        else
        {
            setCursor(QCursor(Qt::ArrowCursor));
        }
    }
}

void PDFCreateHighlightTextTool::setSelection(PDFTextSelection&& textSelection)
{
    if (m_textSelection != textSelection)
    {
        m_textSelection = qMove(textSelection);
        Q_EMIT getProxy()->repaintNeeded();
    }
}

PDFCreateRedactRectangleTool::PDFCreateRedactRectangleTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QAction* action, QObject* parent) :
    BaseClass(proxy, action, parent),
    m_toolManager(toolManager),
    m_pickTool(nullptr),
    m_styleManager(nullptr),
    m_color(getRedactColor())
{
    m_pickTool = new PDFPickTool(proxy, PDFPickTool::Mode::Rectangles, this);
    m_pickTool->setSnapToAnnotations(true);
    m_pickTool->setSelectionRectangleColor(m_color, true);
    addTool(m_pickTool);
    connect(m_pickTool, &PDFPickTool::rectanglePicked, this, &PDFCreateRedactRectangleTool::onRectanglePicked);

    PDFAnnotationStyle defaultStyle;
    defaultStyle.strokeColor = m_color;
    defaultStyle.fillColor = QColor();

    m_styleManager = new PDFAnnotationStyleManager(this, PDFAnnotationStyleManager::StrokeColor, defaultStyle, PDFAnnotationStyleSettings::STYLE_REDACT);
    connect(m_styleManager, &PDFAnnotationStyleManager::styleChanged, this, &PDFCreateRedactRectangleTool::onStyleChanged);

    updateActions();
}

QColor PDFCreateRedactRectangleTool::getRedactColor()
{
    // Jakub Melka: the redaction color was originally stored in a separate group
    // of the settings. Value stored by the older versions of the application is
    // used as a default value, so the choice of the user is not lost.
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.beginGroup("RedactTool");
    QColor legacyColor = settings.value("redactColor", QColor(Qt::black)).value<QColor>();
    settings.endGroup();

    if (!legacyColor.isValid())
    {
        legacyColor = QColor(Qt::black);
    }

    PDFAnnotationStyle defaultStyle;
    defaultStyle.strokeColor = legacyColor;
    defaultStyle.fillColor = QColor();

    const QColor color = PDFAnnotationStyleSettings::getStyle(PDFAnnotationStyleSettings::STYLE_REDACT, defaultStyle).strokeColor;
    return color.isValid() ? color : QColor(Qt::black);
}

void PDFCreateRedactRectangleTool::setRedactColor(const QColor& color)
{
    PDFAnnotationStyle style = PDFAnnotationStyleSettings::getStyle(PDFAnnotationStyleSettings::STYLE_REDACT, PDFAnnotationStyle());
    style.strokeColor = color;
    style.fillColor = QColor();
    PDFAnnotationStyleSettings::setStyle(PDFAnnotationStyleSettings::STYLE_REDACT, style);
}

void PDFCreateRedactRectangleTool::setActiveImpl(bool active)
{
    BaseClass::setActiveImpl(active);

    if (!active)
    {
        m_styleManager->deactivateStyle();
    }
    else
    {
        // Jakub Melka: the style window is shown first, because showing it reloads
        // the style, which can be changed by the other redaction tool meanwhile.
        m_styleManager->activateStyle();
        m_color = m_styleManager->getStyle().strokeColor;
        m_pickTool->setSelectionRectangleColor(m_color, true);
    }
}

void PDFCreateRedactRectangleTool::onStyleChanged(const PDFAnnotationStyle& style)
{
    m_color = style.strokeColor;
    m_pickTool->setSelectionRectangleColor(m_color, true);
    Q_EMIT getProxy()->repaintNeeded();
}

void PDFCreateRedactRectangleTool::onRectanglePicked(PDFInteger pageIndex, QRectF pageRectangle)
{
    if (pageRectangle.isEmpty())
    {
        return;
    }

    PDFDocumentModifier modifier(getDocument());

    PDFObjectReference page = getDocument()->getCatalog()->getPage(pageIndex)->getPageReference();
    // Jakub Melka: the border color is the same as the fill color, so the redaction
    // marker is displayed as a solid area without a differently colored border
    PDFObjectReference annotation = modifier.getBuilder()->createAnnotationRedact(page, pageRectangle, m_color, m_color);
    modifier.getBuilder()->updateAnnotationAppearanceStreams(annotation);
    modifier.markAnnotationsChanged();

    if (modifier.finalize())
    {
        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    }
}

PDFCreateInsertPageNumbersTool::PDFCreateInsertPageNumbersTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QAction* action, QObject* parent) :
    BaseClass(proxy, action, parent),
    m_toolManager(toolManager),
    m_pickTool(nullptr)
{
    m_pickTool = new PDFPickTool(proxy, PDFPickTool::Mode::Rectangles, this);
    m_pickTool->setSnapToAnnotations(true);
    m_pickTool->setSelectionRectangleColor(Qt::black);
    addTool(m_pickTool);
    connect(m_pickTool, &PDFPickTool::rectanglePicked, this, &PDFCreateInsertPageNumbersTool::onRectanglePicked);

    updateActions();
}

void PDFCreateInsertPageNumbersTool::onRectanglePicked(PDFInteger pageIndex, QRectF pageRectangle)
{
    if (pageRectangle.isEmpty() || !getDocument())
    {
        return;
    }

    const PDFPage* referencePage = getDocument()->getCatalog()->getPage(pageIndex);
    const QRectF referenceMediaBox = referencePage->getMediaBox();

    // Anchor the picked rectangle to its nearest media box corner, so the same
    // relative position (and rectangle size) can be reproduced on pages whose
    // media box differs in size from the reference page.
    const bool anchorLeft = std::abs(pageRectangle.center().x() - referenceMediaBox.left()) <= std::abs(referenceMediaBox.right() - pageRectangle.center().x());
    const bool anchorTop = std::abs(pageRectangle.center().y() - referenceMediaBox.top()) <= std::abs(referenceMediaBox.bottom() - pageRectangle.center().y());

    const PDFReal offsetX = anchorLeft ? (pageRectangle.left() - referenceMediaBox.left()) : (referenceMediaBox.right() - pageRectangle.right());
    const PDFReal offsetY = anchorTop ? (pageRectangle.top() - referenceMediaBox.top()) : (referenceMediaBox.bottom() - pageRectangle.bottom());
    const PDFReal width = pageRectangle.width();
    const PDFReal height = pageRectangle.height();

    std::vector<PDFInteger> visiblePages;
    if (IDrawWidget* drawWidget = getProxy()->getWidget()->getDrawWidget())
    {
        visiblePages = drawWidget->getCurrentPages();
    }

    InsertPageNumbersDialog dialog(getDocument()->getCatalog()->getPageCount(), visiblePages, getProxy()->getWidget());
    if (dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    std::vector<PDFInteger> selectedPages = dialog.getSelectedPages();
    std::sort(selectedPages.begin(), selectedPages.end());

    const PDFPageLabel::NumberingStyle numberingStyle = dialog.getNumberingStyle();
    const QString formatPattern = dialog.getFormatPattern();
    const int startNumber = dialog.getStartNumber();
    const QFont font = dialog.getFont();
    const QColor color = dialog.getColor();
    const Qt::Alignment alignment = dialog.getAlignment();
    const QString totalPagesText = QString::number(getDocument()->getCatalog()->getPageCount());

    PDFDocumentModifier modifier(getDocument());

    for (size_t i = 0; i < selectedPages.size(); ++i)
    {
        const PDFInteger targetPageIndex = selectedPages[i] - 1;
        const PDFPage* targetPage = getDocument()->getCatalog()->getPage(targetPageIndex);
        const QRectF targetMediaBox = targetPage->getMediaBox();

        const PDFReal x = anchorLeft ? (targetMediaBox.left() + offsetX) : (targetMediaBox.right() - offsetX - width);
        const PDFReal y = anchorTop ? (targetMediaBox.top() + offsetY) : (targetMediaBox.bottom() - offsetY - height);

        QRectF targetRect;
        targetRect.setLeft(x);
        targetRect.setTop(y);
        targetRect.setWidth(width);
        targetRect.setHeight(height);

        const QString numberText = PDFPageLabel::formatPageNumber(numberingStyle, startNumber + PDFInteger(i));
        const QString labelText = QString(formatPattern).arg(numberText).arg(totalPagesText);

        PDFPageContentStreamBuilder contentStreamBuilder(modifier.getBuilder(), PDFContentStreamBuilder::CoordinateSystem::PDF, PDFPageContentStreamBuilder::Mode::PlaceAfter);
        QPainter* painter = contentStreamBuilder.begin(targetPage->getPageReference());
        if (painter)
        {
            painter->setFont(font);
            painter->setPen(QPen(color));

            // CoordinateSystem::PDF flips the painter's y-axis to match PDF space;
            // counteract it locally around the text, otherwise glyphs draw mirrored.
            painter->save();
            painter->translate(targetRect.center());
            painter->scale(1.0, -1.0);
            QRectF localRect(-targetRect.width() * 0.5, -targetRect.height() * 0.5, targetRect.width(), targetRect.height());
            painter->drawText(localRect, int(alignment), labelText);
            painter->restore();

            contentStreamBuilder.end(painter);
            modifier.markPageContentsChanged();
        }
    }

    if (modifier.finalize())
    {
        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    }

    // Deactivate the tool after stamping, so an accidental extra rectangle pick
    // does not stamp the already numbered document a second time.
    setActive(false);
}

PDFDeleteAnnotationTool::PDFDeleteAnnotationTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QAction* action, QObject* parent) :
    BaseClass(proxy, action, parent),
    m_toolManager(toolManager),
    m_selectionPageIndex(-1),
    m_currentPageIndex(-1),
    m_isCursorOverAnnotation(false)
{
    updateActions();
}

void PDFDeleteAnnotationTool::drawPage(QPainter* painter,
                                       PDFInteger pageIndex,
                                       const PDFPrecompiledPage* compiledPage,
                                       PDFTextLayoutGetter& layoutGetter,
                                       const QTransform& pagePointToDevicePointMatrix,
                                       const PDFColorConvertor& convertor,
                                       QList<PDFRenderError>& errors) const
{
    Q_UNUSED(compiledPage);
    Q_UNUSED(layoutGetter);
    Q_UNUSED(errors);

    const std::vector<AnnotationInfo> markedAnnotations = getMarkedAnnotations();

    painter->save();
    painter->setWorldTransform(QTransform(pagePointToDevicePointMatrix), true);
    painter->setRenderHint(QPainter::Antialiasing);

    // Mark the annotations, which will be deleted
    // Jakub Melka: these are just UI markers, not a preview of an annotation,
    // so the colors are adjusted always (the same way as the selection
    // rectangle of the pick tool, see PDFPickTool::getAdjustedSelectionRectangleColor).
    QColor markColor = convertor.convert(QColor(Qt::red), false, true);
    QPen markPen(markColor);
    markPen.setWidthF(0.0);
    markPen.setCosmetic(true);

    QColor fillColor = markColor;
    fillColor.setAlphaF(0.2f);

    painter->setPen(markPen);
    painter->setBrush(QBrush(fillColor));

    for (const AnnotationInfo& annotationInfo : markedAnnotations)
    {
        if (annotationInfo.pageIndex == pageIndex)
        {
            painter->drawRect(annotationInfo.rectangle);
        }
    }

    // Draw the selection rectangle
    const QRectF selectionRectangle = getSelectionRectangle();
    if (selectionRectangle.isValid() && m_selectionPageIndex == pageIndex)
    {
        QPen selectionPen(convertor.convert(QColor(Qt::blue), false, true));
        selectionPen.setCosmetic(true);
        selectionPen.setStyle(Qt::DashLine);
        painter->setPen(selectionPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(selectionRectangle);
    }

    painter->restore();
}

void PDFDeleteAnnotationTool::mousePressEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);

    if (event->button() == Qt::LeftButton)
    {
        QPointF pagePoint;
        const PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->pos(), &pagePoint);

        if (pageIndex != -1)
        {
            m_selectionPageIndex = pageIndex;
            m_selectionStartPoint = pagePoint;
            m_currentPageIndex = pageIndex;
            m_currentPagePoint = pagePoint;
            event->accept();
        }
        else
        {
            resetTool();
        }

        Q_EMIT getProxy()->repaintNeeded();
    }
}

void PDFDeleteAnnotationTool::mouseReleaseEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);

    if (event->button() == Qt::LeftButton && m_selectionPageIndex != -1)
    {
        QPointF pagePoint;
        const PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->pos(), &pagePoint);

        if (pageIndex == m_selectionPageIndex)
        {
            m_currentPagePoint = pagePoint;
            deleteAnnotations(getMarkedAnnotations());
        }

        resetTool();
        event->accept();
        updateCursor();
        Q_EMIT getProxy()->repaintNeeded();
    }
}

void PDFDeleteAnnotationTool::mouseMoveEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);

    QPointF pagePoint;
    const PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->pos(), &pagePoint);

    m_currentPageIndex = pageIndex;
    m_currentPagePoint = pagePoint;

    const std::vector<AnnotationInfo> markedAnnotations = getMarkedAnnotations();
    m_isCursorOverAnnotation = !markedAnnotations.empty();

    if (m_selectionPageIndex != -1)
    {
        event->accept();
    }

    updateCursor();
    Q_EMIT getProxy()->repaintNeeded();
}

void PDFDeleteAnnotationTool::updateActions()
{
    BaseClass::updateActions();

    if (QAction* action = getAction())
    {
        const bool isEnabled = getDocument() && getDocument()->getStorage().getSecurityHandler()->isAllowed(PDFSecurityHandler::Permission::ModifyInteractiveItems);
        action->setChecked(isActive());
        action->setEnabled(isEnabled);
    }
}

void PDFDeleteAnnotationTool::setActiveImpl(bool active)
{
    BaseClass::setActiveImpl(active);

    if (active)
    {
        Q_EMIT messageDisplayRequest(tr("Click on an annotation to delete it. Drag a rectangle to delete all annotations inside it."), 15000);
    }
    else
    {
        resetTool();
    }

    updateCursor();
}

std::vector<PDFDeleteAnnotationTool::AnnotationInfo> PDFDeleteAnnotationTool::getDeletableAnnotations(PDFInteger pageIndex) const
{
    std::vector<AnnotationInfo> result;

    const PDFDocument* document = getDocument();
    PDFWidget* widget = getProxy()->getWidget();

    if (pageIndex == -1 || !document || !widget)
    {
        return result;
    }

    PDFWidgetAnnotationManager* annotationManager = widget->getAnnotationManager();
    if (!annotationManager)
    {
        return result;
    }

    const PDFPage* page = document->getCatalog()->getPage(pageIndex);
    if (!page)
    {
        return result;
    }

    const PDFAnnotationManager::PageAnnotations& pageAnnotations = annotationManager->getPageAnnotations(pageIndex);
    for (const PDFAnnotationManager::PageAnnotation& pageAnnotation : pageAnnotations.annotations)
    {
        const PDFAnnotation* annotation = pageAnnotation.annotation.get();

        // Popup annotations and replies are not deleted directly - they are
        // deleted together with the annotation, which owns them.
        if (!annotation || annotation->isReplyTo() || annotation->getType() == AnnotationType::Popup)
        {
            continue;
        }

        // Use the same permissions and visibility checks as the Delete shortcut.
        if (!annotationManager->canDeleteAnnotation(pageAnnotation))
        {
            continue;
        }

        AnnotationInfo annotationInfo;
        annotationInfo.pageIndex = pageIndex;
        annotationInfo.pageReference = page->getPageReference();
        annotationInfo.annotationReference = annotation->getSelfReference();
        annotationInfo.rectangle = annotation->getRectangle().normalized();

        if (annotationInfo.annotationReference.isValid() && annotationInfo.rectangle.isValid())
        {
            result.push_back(annotationInfo);
        }
    }

    return result;
}

std::vector<PDFDeleteAnnotationTool::AnnotationInfo> PDFDeleteAnnotationTool::getMarkedAnnotations() const
{
    std::vector<AnnotationInfo> result;

    const PDFInteger pageIndex = (m_selectionPageIndex != -1) ? m_selectionPageIndex : m_currentPageIndex;
    if (pageIndex == -1)
    {
        return result;
    }

    const QRectF selectionRectangle = getSelectionRectangle();

    for (const AnnotationInfo& annotationInfo : getDeletableAnnotations(pageIndex))
    {
        const bool isMarked = selectionRectangle.isValid() ? selectionRectangle.intersects(annotationInfo.rectangle)
                                                           : annotationInfo.rectangle.contains(m_currentPagePoint);

        if (isMarked)
        {
            result.push_back(annotationInfo);
        }
    }

    return result;
}

void PDFDeleteAnnotationTool::deleteAnnotations(const std::vector<AnnotationInfo>& annotations)
{
    if (annotations.empty())
    {
        return;
    }

    PDFDocumentModifier modifier(getDocument());
    modifier.markAnnotationsChanged();

    for (const AnnotationInfo& annotationInfo : annotations)
    {
        modifier.getBuilder()->removeAnnotation(annotationInfo.pageReference, annotationInfo.annotationReference);
    }

    if (modifier.finalize())
    {
        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
    }
}

QRectF PDFDeleteAnnotationTool::getSelectionRectangle() const
{
    if (m_selectionPageIndex == -1 || m_currentPageIndex != m_selectionPageIndex)
    {
        return QRectF();
    }

    const QRectF rectangle = QRectF(m_selectionStartPoint, m_currentPagePoint).normalized();

    // A very small rectangle means, that the user just clicked on an annotation.
    // In that case we do not use the rectangle selection at all, because a click
    // must delete the annotation under the cursor, even if the annotation is
    // larger than the (almost empty) rectangle.
    const qreal minimalSize = 2.0;
    if (rectangle.width() < minimalSize && rectangle.height() < minimalSize)
    {
        return QRectF();
    }

    return rectangle;
}

void PDFDeleteAnnotationTool::resetTool()
{
    m_selectionPageIndex = -1;
    m_selectionStartPoint = QPointF();
    m_currentPageIndex = -1;
    m_currentPagePoint = QPointF();
    m_isCursorOverAnnotation = false;
}

void PDFDeleteAnnotationTool::updateCursor()
{
    if (isActive())
    {
        setCursor(QCursor(m_isCursorOverAnnotation ? Qt::PointingHandCursor : Qt::ArrowCursor));
    }
}

PDFCreateRedactTextTool::PDFCreateRedactTextTool(PDFDrawWidgetProxy* proxy, PDFToolManager* toolManager, QAction* action, QObject* parent) :
    BaseClass(proxy, action, parent),
    m_toolManager(toolManager),
    m_styleManager(nullptr),
    m_color(PDFCreateRedactRectangleTool::getRedactColor()),
    m_isCursorOverText(false)
{
    PDFAnnotationStyle defaultStyle;
    defaultStyle.strokeColor = m_color;
    defaultStyle.fillColor = QColor();

    // Jakub Melka: both redaction tools share the same style, so the user doesn't
    // have to set the redaction color twice.
    m_styleManager = new PDFAnnotationStyleManager(this, PDFAnnotationStyleManager::StrokeColor, defaultStyle, PDFAnnotationStyleSettings::STYLE_REDACT);
    connect(m_styleManager, &PDFAnnotationStyleManager::styleChanged, this, &PDFCreateRedactTextTool::onStyleChanged);

    updateActions();
}

void PDFCreateRedactTextTool::drawPage(QPainter* painter,
                                       PDFInteger pageIndex,
                                       const PDFPrecompiledPage* compiledPage,
                                       PDFTextLayoutGetter& layoutGetter,
                                       const QTransform& pagePointToDevicePointMatrix,
                                       const PDFColorConvertor& convertor,
                                       QList<PDFRenderError>& errors) const
{
    Q_UNUSED(compiledPage);
    Q_UNUSED(errors);

    // Jakub Melka: the selection is a preview of the redaction annotation, which will be
    // created from it, so it is color adjusted only when annotations are color adjusted
    PDFColorConvertor annotationConvertor = convertor;
    PDFRenderer::applyFeaturesToAnnotationColorConvertor(getProxy()->getFeatures(), annotationConvertor);

    pdf::PDFTextSelectionPainter textSelectionPainter(&m_textSelection);
    textSelectionPainter.draw(painter, pageIndex, layoutGetter, pagePointToDevicePointMatrix, annotationConvertor);
}

void PDFCreateRedactTextTool::mousePressEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);

    if (event->button() == Qt::LeftButton)
    {
        QPointF pagePoint;
        const PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->pos(), &pagePoint);
        if (pageIndex != -1)
        {
            m_selectionInfo.pageIndex = pageIndex;
            m_selectionInfo.selectionStartPoint = pagePoint;
            event->accept();
        }
        else
        {
            m_selectionInfo = SelectionInfo();
        }

        setSelection(pdf::PDFTextSelection());
        updateCursor();
    }
}

void PDFCreateRedactTextTool::mouseReleaseEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);

    if (event->button() == Qt::LeftButton)
    {
        if (m_selectionInfo.pageIndex != -1)
        {
            QPointF pagePoint;
            const PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->pos(), &pagePoint);

            if (m_selectionInfo.pageIndex == pageIndex)
            {
                // Jakub Melka: handle the selection
                PDFTextLayoutGetter textLayoutGetter = getProxy()->getTextLayoutCompiler()->getTextLayoutLazy(pageIndex);
                PDFTextLayout textLayout = textLayoutGetter;
                setSelection(textLayout.createTextSelection(pageIndex, m_selectionInfo.selectionStartPoint, pagePoint, m_color));

                QPolygonF quadrilaterals;
                PDFTextSelectionPainter textSelectionPainter(&m_textSelection);
                QPainterPath path = textSelectionPainter.prepareGeometry(pageIndex, textLayoutGetter, QTransform(), &quadrilaterals);

                if (!path.isEmpty())
                {
                    PDFDocumentModifier modifier(getDocument());

                    PDFObjectReference page = getDocument()->getCatalog()->getPage(pageIndex)->getPageReference();
                    // Jakub Melka: the border color is the same as the fill color, so the redaction
                    // marker is displayed as a solid area without a differently colored border
                    modifier.getBuilder()->createAnnotationRedact(page, quadrilaterals, m_color, m_color);
                    modifier.markAnnotationsChanged();

                    if (modifier.finalize())
                    {
                        Q_EMIT m_toolManager->documentModified(PDFModifiedDocument(modifier.getDocument(), nullptr, modifier.getFlags()));
                    }
                }
            }

            setSelection(pdf::PDFTextSelection());

            m_selectionInfo = SelectionInfo();
            event->accept();
            updateCursor();
        }
    }
}

void PDFCreateRedactTextTool::mouseMoveEvent(QWidget* widget, QMouseEvent* event)
{
    Q_UNUSED(widget);

    QPointF pagePoint;
    const PDFInteger pageIndex = getProxy()->getPageUnderPoint(event->pos(), &pagePoint);
    PDFTextLayout textLayout = getProxy()->getTextLayoutCompiler()->getTextLayoutLazy(pageIndex);
    m_isCursorOverText = textLayout.isHoveringOverTextBlock(pagePoint);

    if (m_selectionInfo.pageIndex != -1)
    {
        if (m_selectionInfo.pageIndex == pageIndex)
        {
            // Jakub Melka: handle the selection
            setSelection(textLayout.createTextSelection(pageIndex, m_selectionInfo.selectionStartPoint, pagePoint, m_color));
        }
        else
        {
            setSelection(pdf::PDFTextSelection());
        }

        event->accept();
    }

    updateCursor();
}

void PDFCreateRedactTextTool::updateActions()
{
    if (QAction* action = getAction())
    {
        const bool isEnabled = getDocument() && getDocument()->getStorage().getSecurityHandler()->isAllowed(PDFSecurityHandler::Permission::ModifyInteractiveItems);
        action->setChecked(isActive());
        action->setEnabled(isEnabled);
    }
}

void PDFCreateRedactTextTool::setActiveImpl(bool active)
{
    BaseClass::setActiveImpl(active);

    if (!active)
    {
        // Just clear the text selection
        setSelection(PDFTextSelection());

        m_styleManager->deactivateStyle();
    }
    else
    {
        // Layouts are cached on demand, just as for text highlighting.
        m_styleManager->activateStyle();
        m_color = m_styleManager->getStyle().strokeColor;
    }
}

void PDFCreateRedactTextTool::onStyleChanged(const PDFAnnotationStyle& style)
{
    m_color = style.strokeColor;

    if (!m_textSelection.isEmpty())
    {
        setSelection(PDFTextSelection());
    }

    Q_EMIT getProxy()->repaintNeeded();
}

void PDFCreateRedactTextTool::updateCursor()
{
    if (isActive())
    {
        if (m_isCursorOverText)
        {
            setCursor(QCursor(Qt::IBeamCursor));
        }
        else
        {
            setCursor(QCursor(Qt::ArrowCursor));
        }
    }
}

void PDFCreateRedactTextTool::setSelection(PDFTextSelection&& textSelection)
{
    if (m_textSelection != textSelection)
    {
        m_textSelection = qMove(textSelection);
        getProxy()->repaintNeeded();
    }
}

} // namespace pdf
