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

#ifndef PDFANNOTATIONSTYLE_H
#define PDFANNOTATIONSTYLE_H

#include "pdfwidgetsglobal.h"
#include "pdfglobal.h"

#include <QColor>
#include <QCursor>
#include <QPointer>


namespace pdf
{

/// Style of a newly created annotation. Not all items are used by all annotation
/// tools - for example a text markup annotation uses the stroke color only.
struct PDF4QTLIBWIDGETSSHARED_EXPORT PDFAnnotationStyle
{
    bool operator==(const PDFAnnotationStyle&) const = default;

    /// Color of the stroke (or the color of the text markup / redaction)
    QColor strokeColor = Qt::red;

    /// Color of the fill. Invalid color means, that the shape is not filled.
    QColor fillColor = QColor();

    /// Width of the pen used for the stroke
    PDFReal penWidth = 1.0;

    /// Font of a text (inline text)
    QString fontFamily = QStringLiteral("Helvetica");

    /// Size of the font of a text (points)
    PDFReal fontSize = 10.0;
};

/// Persistent storage of annotation styles. Each annotation tool (or a group of
/// tools, which logically share the style, such as the two redaction tools) uses
/// its own style identified by a style id. Styles are stored in the application
/// settings, so the user's choice is remembered across the restarts of the
/// application, and it is not reset when the user switches between the tools.
class PDF4QTLIBWIDGETSSHARED_EXPORT PDFAnnotationStyleSettings
{
public:
    /// Identifiers of the persisted styles. Tools using the same identifier
    /// share the style.
    static constexpr QLatin1String STYLE_HIGHLIGHT = QLatin1String("Highlight");
    static constexpr QLatin1String STYLE_UNDERLINE = QLatin1String("Underline");
    static constexpr QLatin1String STYLE_SQUIGGLY = QLatin1String("Squiggly");
    static constexpr QLatin1String STYLE_STRIKEOUT = QLatin1String("StrikeOut");
    static constexpr QLatin1String STYLE_REDACT = QLatin1String("Redact");
    static constexpr QLatin1String STYLE_SHAPE = QLatin1String("Shape");
    static constexpr QLatin1String STYLE_FREEHAND = QLatin1String("Freehand");
    static constexpr QLatin1String STYLE_HIGHLIGHTER = QLatin1String("Highlighter");
    static constexpr QLatin1String STYLE_ERASER = QLatin1String("Eraser");
    static constexpr QLatin1String STYLE_INLINE_TEXT = QLatin1String("InlineText");

    /// Returns the style stored for the given style id. If no style has been
    /// stored yet, then \p defaultStyle is returned.
    /// \param styleId Style identifier
    /// \param defaultStyle Style used, when nothing is stored yet
    static PDFAnnotationStyle getStyle(const QString& styleId, const PDFAnnotationStyle& defaultStyle);

    /// Stores the style for the given style id.
    /// \param styleId Style identifier
    /// \param style Style to be stored
    static void setStyle(const QString& styleId, const PDFAnnotationStyle& style);
};

/// Manages the annotation style of a single tool - it loads and stores the style
/// in the application settings and it shows the style window while the tool is
/// active. All annotation tools use this class, so the way of setting the style
/// of a created annotation is the same for all of them.
class PDFAnnotationStyleManager;

/// Mouse pointers of the annotation tools, they show the tool and its style
/// (like Xournal++): the color of the annotation, which will be created.
class PDF4QTLIBWIDGETSSHARED_EXPORT PDFAnnotationCursors
{
public:
    PDFAnnotationCursors() = delete;

    /// Pen (freehand drawing): a dot in the color of the pen, its size follows the pen width
    /// \param color Color of the pen
    /// \param penWidth Width of the pen (points)
    static QCursor createPenCursor(QColor color, PDFReal penWidth);

    /// Highlighter: a block in the color of the highlight
    /// \param color Color of the highlight
    static QCursor createHighlighterCursor(QColor color);

    /// Shapes: a small cross in the color of the shape
    /// \param color Color of the shape
    static QCursor createCrossCursor(QColor color);

    /// Eraser: a circle of the size of the eraser (it is visible on a light
    /// and on a dark page)
    /// \param diameter Diameter of the eraser (pixels of the screen)
    static QCursor createEraserCursor(qreal diameter);
};

/// Central point of the annotation styles. The annotation tool, which is active,
/// registers its style manager here, so the style toolbar of the application can
/// display and change the style of the active tool. There is one hub per application.
class PDF4QTLIBWIDGETSSHARED_EXPORT PDFAnnotationStyleHub : public QObject
{
    Q_OBJECT

private:
    using BaseClass = QObject;

    explicit PDFAnnotationStyleHub();

public:
    /// Returns the hub of the application
    static PDFAnnotationStyleHub* getInstance();

    /// Returns the style manager of the active annotation tool, or nullptr,
    /// if no annotation tool is active.
    PDFAnnotationStyleManager* getActiveManager() const { return m_activeManager; }

signals:
    /// Emitted, when the active style manager changes, or when its style changes
    void activeStyleChanged();

private:
    friend class PDFAnnotationStyleManager;

    void setActiveManager(PDFAnnotationStyleManager* manager);
    void notifyStyleChanged(PDFAnnotationStyleManager* manager);

    QPointer<PDFAnnotationStyleManager> m_activeManager;
};

/// Manages the annotation style of a single tool - it loads and stores the style
/// in the application settings, and it registers the style in the style hub while
/// the tool is active, so the style toolbar edits the style of the active tool. All
/// annotation tools use this class, so the way of setting the style of a created
/// annotation is the same for all of them.
class PDF4QTLIBWIDGETSSHARED_EXPORT PDFAnnotationStyleManager : public QObject
{
    Q_OBJECT

private:
    using BaseClass = QObject;

public:
    enum StyleItem
    {
        StrokeColor = 0x0001,   ///< Color of the stroke / text markup / redaction
        FillColor   = 0x0002,   ///< Color of the fill (can be turned off by the user)
        PenWidth    = 0x0004,   ///< Width of the pen
        Font        = 0x0008    ///< Font of a text (family and size)
    };
    Q_DECLARE_FLAGS(StyleItems, StyleItem)

    /// Creates the style manager.
    /// \param parent Parent object (the tool)
    /// \param items Style items, which are used by the tool
    /// \param defaultStyle Style used, when nothing is stored in the settings yet
    /// \param styleId Identifier of the persisted style
    explicit PDFAnnotationStyleManager(QObject* parent,
                                       StyleItems items,
                                       PDFAnnotationStyle defaultStyle,
                                       QString styleId);
    virtual ~PDFAnnotationStyleManager() override;

    const PDFAnnotationStyle& getStyle() const { return m_style; }

    /// Returns the style items, which are used by the tool
    StyleItems getItems() const { return m_items; }

    /// Returns the factor of the predefined pen widths of the style toolbar. A tool
    /// drawing wide strokes (highlighter) uses larger widths than the other tools,
    /// so its fine / medium / thick widths are the predefined widths multiplied by it.
    PDFReal getPenWidthScale() const { return m_penWidthScale; }

    /// Sets the factor of the predefined pen widths (see getPenWidthScale)
    /// \param penWidthScale Factor of the predefined pen widths
    void setPenWidthScale(PDFReal penWidthScale) { m_penWidthScale = penWidthScale; }

    /// Sets the style and stores it in the application settings.
    /// \param style New style
    void setStyle(PDFAnnotationStyle style);

    /// Sets the identifier of the persisted style and loads the style stored for
    /// it. Use this when a single tool creates several kinds of annotations, which
    /// have their own style (for example highlight / underline / strikeout).
    /// \param styleId Identifier of the persisted style
    /// \param defaultStyle Style used, when nothing is stored for the new style id
    void setStyleId(QString styleId, PDFAnnotationStyle defaultStyle);

    /// Reloads the style from the application settings. Several tools can share
    /// the same style id (all shape tools, or both redaction tools), and each of
    /// them has its own style manager with its own cached copy of the style. The
    /// cached copy must be refreshed before it is used, otherwise a tool would
    /// create annotations using an outdated style and it would eventually store
    /// that outdated style back over the choice made by the user in another tool.
    void reloadStyle();

    /// Makes the style the active style of the style hub (the style toolbar edits it).
    /// Call this when the tool is activated. The style is reloaded from the settings
    /// first, so a style shared with another tool is up to date.
    void activateStyle();

    /// Removes the style from the style hub. Call this when the tool is deactivated.
    void deactivateStyle();

    /// Returns true, if the style is the active style of the style hub
    bool isStyleActive() const;

signals:
    void styleChanged(const pdf::PDFAnnotationStyle& style);

private:
    StyleItems m_items;
    PDFAnnotationStyle m_defaultStyle;
    QString m_styleId;
    PDFAnnotationStyle m_style;
    PDFReal m_penWidthScale = 1.0;
};

// Jakub Melka: operators must be declared inside the namespace, not at the global
// scope. Namespace 'pdf' declares its own operator| (for the OCState enumeration),
// which hides all operators declared at the global scope from the unqualified name
// lookup performed inside this namespace. If the operators were declared globally,
// combining the flags in any code inside namespace 'pdf' would silently fall back
// to the built-in integer operator and would not compile.
Q_DECLARE_OPERATORS_FOR_FLAGS(PDFAnnotationStyleManager::StyleItems)

}   // namespace pdf

#endif // PDFANNOTATIONSTYLE_H
