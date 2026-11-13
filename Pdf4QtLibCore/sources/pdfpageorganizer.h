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

#ifndef PDFPAGEORGANIZER_H
#define PDFPAGEORGANIZER_H

#include "pdfglobal.h"
#include "pdfdocument.h"
#include "pdfutils.h"

#include <vector>

namespace pdf
{

/// Organization of the pages of a document: blank pages, duplication, removal,
/// reordering and backgrounds of the pages. Each operation modifies the document
/// passed in (a new version of the immutable document is stored into it) and
/// reports the modification flags for \ref PDFModifiedDocument.
///
/// A background is a content stream inserted before the content of the page, so
/// it is drawn under it. It is identified by the private data of the page
/// (/PieceInfo /Inkpage), so it can be replaced or removed later without touching
/// the content of the page. A background is drawn in the displayed orientation of
/// the page (the /Rotate of the page is taken into account) over the crop box.
class PDF4QTLIBCORESHARED_EXPORT PDFPageOrganizer
{
public:
    PDFPageOrganizer() = delete;

    enum class Background
    {
        Plain,  ///< No background
        Ruled,  ///< Horizontal lines with a margin line
    };

    /// Key of the private data of the page in the /PieceInfo dictionary
    static constexpr const char* PIECE_INFO_KEY = "Inkpage";

    /// Distance of the lines of the ruled background, in millimeters
    static constexpr PDFReal RULED_LINE_SPACING_MM = 8.0;

    /// Inserts a blank page after the page. The new page has the same size,
    /// rotation and background as the page.
    /// \param document Document
    /// \param pageIndex Index of the page
    /// \param modificationFlags Modification flags (output, can be nullptr)
    static PDFOperationResult addBlankPage(PDFDocument* document, PDFInteger pageIndex, PDFModifiedDocument::ModificationFlags* modificationFlags);

    /// Inserts a blank page before the page. The new page has the same size,
    /// rotation and background as the page.
    /// \param document Document
    /// \param pageIndex Index of the page
    /// \param modificationFlags Modification flags (output, can be nullptr)
    static PDFOperationResult insertBlankPage(PDFDocument* document, PDFInteger pageIndex, PDFModifiedDocument::ModificationFlags* modificationFlags);

    /// Inserts a copy of the page after the page. The content is shared with the
    /// original page, the annotations are copied (with their popup windows and
    /// replies). Widget annotations of the form fields are not copied.
    /// \param document Document
    /// \param pageIndex Index of the page
    /// \param modificationFlags Modification flags (output, can be nullptr)
    static PDFOperationResult duplicatePage(PDFDocument* document, PDFInteger pageIndex, PDFModifiedDocument::ModificationFlags* modificationFlags);

    /// Removes the page from the document. The last page can't be removed.
    /// \param document Document
    /// \param pageIndex Index of the page
    /// \param modificationFlags Modification flags (output, can be nullptr)
    static PDFOperationResult removePage(PDFDocument* document, PDFInteger pageIndex, PDFModifiedDocument::ModificationFlags* modificationFlags);

    /// Moves the page, so it has the target index in the document
    /// \param document Document
    /// \param pageIndex Index of the page
    /// \param targetPageIndex Index of the page after the move
    /// \param modificationFlags Modification flags (output, can be nullptr)
    static PDFOperationResult movePage(PDFDocument* document, PDFInteger pageIndex, PDFInteger targetPageIndex, PDFModifiedDocument::ModificationFlags* modificationFlags);

    /// Sets the background of the pages. A previous background set by this
    /// function is replaced, \p Plain removes it.
    /// \param document Document
    /// \param pageIndices Indices of the pages
    /// \param background Background
    /// \param modificationFlags Modification flags (output, can be nullptr)
    static PDFOperationResult setBackground(PDFDocument* document, const std::vector<PDFInteger>& pageIndices, Background background, PDFModifiedDocument::ModificationFlags* modificationFlags);

    /// Returns the background of the page set by \ref setBackground
    /// \param document Document
    /// \param pageIndex Index of the page
    static Background getBackground(const PDFDocument* document, PDFInteger pageIndex);

    /// Returns the content stream of the background (in the coordinates of the page)
    /// \param page Page
    /// \param background Background
    static QByteArray createBackgroundContent(const PDFPage* page, Background background);
};

}   // namespace pdf

#endif // PDFPAGEORGANIZER_H
