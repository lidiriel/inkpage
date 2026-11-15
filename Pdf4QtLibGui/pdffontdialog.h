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

#ifndef PDFFONTDIALOG_H
#define PDFFONTDIALOG_H

#include "pdfviewerglobal.h"
#include "pdfglobal.h"

#include <QDialog>

#include <array>

class QLabel;
class QLineEdit;
class QListWidget;
class QDoubleSpinBox;

namespace pdfviewer
{

/// Window choosing the font of a text (like the font window of Xournal++): the
/// families of the installed fonts with a search field, the size (with the common
/// sizes) and a preview of the text in the chosen font.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFFontDialog : public QDialog
{
    Q_OBJECT

private:
    using BaseClass = QDialog;

public:
    /// Common sizes of the fonts (points)
    static constexpr std::array<pdf::PDFReal, 16> COMMON_SIZES = { 6, 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 36, 48, 72 };

    explicit PDFFontDialog(QWidget* parent);
    virtual ~PDFFontDialog() override;

    /// Returns the chosen font family
    QString getFontFamily() const;

    /// Selects the font family in the list (a family, which is not installed, is
    /// kept, it is shown as the first item of the list)
    void setFontFamily(const QString& fontFamily);

    /// Returns the chosen font size (points)
    pdf::PDFReal getFontSize() const;

    /// Sets the font size (points)
    void setFontSize(pdf::PDFReal fontSize);

    /// Shows only the families containing the text (the search field)
    void setFilter(const QString& text);

    /// Returns the number of the families shown in the list
    int getVisibleFamilyCount() const;

private:
    void updatePreview();
    void onSizeItemClicked();

    QLineEdit* m_searchEdit;
    QListWidget* m_familyList;
    QDoubleSpinBox* m_sizeSpinBox;
    QListWidget* m_sizeList;
    QLabel* m_previewLabel;
};

}   // namespace pdfviewer

#endif // PDFFONTDIALOG_H
