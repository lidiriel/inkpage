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

#ifndef PDFANNOTATIONSTYLETOOLBAR_H
#define PDFANNOTATIONSTYLETOOLBAR_H

#include "pdfviewerglobal.h"
#include "pdfannotationstyle.h"

#include <QToolBar>
#include <QPointer>

#include <array>

class QActionGroup;

namespace pdf
{
class PDFWidgetAnnotationManager;
}

namespace pdfviewer
{

/// Toolbar of the annotation style (like the colour palette of Xournal++): the
/// predefined colours, another colour from the colour dialog, the fill and the
/// pen width. When an annotation tool is active, the toolbar edits the style of
/// that tool (style hub); otherwise the colour and the pen width are applied to
/// the selected annotations.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFAnnotationStyleToolBar : public QToolBar
{
    Q_OBJECT

private:
    using BaseClass = QToolBar;

public:
    /// Predefined colours (palette of Xournal)
    static constexpr int COLOR_COUNT = 11;

    /// Predefined pen widths (in points): fine, medium, thick
    static constexpr std::array<pdf::PDFReal, 3> PEN_WIDTHS = { 1.0, 2.0, 4.0 };

    explicit PDFAnnotationStyleToolBar(QWidget* parent);
    virtual ~PDFAnnotationStyleToolBar() override;

    /// Sets the annotation manager, whose selected annotations are changed,
    /// when no annotation tool is active (can be nullptr)
    void setAnnotationManager(pdf::PDFWidgetAnnotationManager* annotationManager);

    /// Returns the predefined colours of the toolbar
    static std::array<QColor, COLOR_COUNT> getPredefinedColors();

    /// Returns the fill colour used for the given stroke colour (a lighter tint)
    static QColor getFillColor(const QColor& strokeColor);

    /// Returns the factor of the predefined pen widths of the active tool (1 without active tool)
    static pdf::PDFReal getPenWidthScale();

    /// Applies the colour to the style of the active tool, or to the selected annotations
    void applyColor(const QColor& color);

    /// Turns the fill of the style of the active tool on or off
    void applyFill(bool enabled);

    /// Applies the pen width to the style of the active tool, or to the selected annotations
    void applyPenWidth(pdf::PDFReal penWidth);

    /// Applies the font to the style of the active tool (inline text), or to the selected inline texts
    void applyFont(const QString& fontFamily, pdf::PDFReal fontSize);

    /// Returns the font of the active tool, or of the selected inline text
    /// \returns false, if the font cannot be set
    bool getCurrentFont(QString* fontFamily, pdf::PDFReal* fontSize) const;

    /// Updates the state of the actions from the active style or the selection
    void updateActions();

private:
    void onOtherColorTriggered();
    void onFontTriggered();

    QIcon createColorIcon(const QColor& color) const;
    QIcon createOtherColorIcon(const QColor& color) const;
    bool hasSelection() const;

    QPointer<pdf::PDFWidgetAnnotationManager> m_annotationManager;
    QActionGroup* m_colorGroup;
    QActionGroup* m_widthGroup;
    std::array<QAction*, COLOR_COUNT> m_colorActions;
    QAction* m_otherColorAction;
    QColor m_otherColor; ///< Last color which is not a predefined color, shown by m_otherColorAction
    QAction* m_fillAction;
    std::array<QAction*, PEN_WIDTHS.size()> m_widthActions;
    QAction* m_fontAction;
};

}   // namespace pdfviewer

#endif // PDFANNOTATIONSTYLETOOLBAR_H
