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

#include "pdfpageorganizer.h"
#include "pdfdocumentbuilder.h"
#include "pdfannotationmanipulator.h"
#include "pdfobjectutils.h"

#include <QTransform>
#include <QColor>

#include <algorithm>
#include <cmath>
#include <functional>

#include "pdfdbgheap.h"

namespace pdf
{

class PDFPageOrganizerHelper
{
public:
    PDFPageOrganizerHelper() = delete;

    /// Operation over the list of the pages. Page list can be modified, the reference
    /// of the page, for which the operation was invoked, is passed as the last argument.
    using PageListOperation = std::function<PDFOperationResult(PDFDocumentBuilder*, std::vector<PDFObjectReference>&, PDFObjectReference)>;

    /// Runs the operation over the flattened page tree of the document, the modified
    /// page list is written back, the modified document is stored into \p document.
    static PDFOperationResult modifyPageList(PDFDocument* document,
                                             PDFInteger pageIndex,
                                             PDFModifiedDocument::ModificationFlags additionalFlags,
                                             PDFModifiedDocument::ModificationFlags* modificationFlags,
                                             const PageListOperation& operation)
    {
        if (modificationFlags)
        {
            *modificationFlags = PDFModifiedDocument::ModificationFlags();
        }

        if (!document)
        {
            return PDFTranslationContext::tr("Invalid document.");
        }

        const PDFInteger pageCount = static_cast<PDFInteger>(document->getCatalog()->getPageCount());
        if (pageIndex < 0 || pageIndex >= pageCount)
        {
            return PDFTranslationContext::tr("Invalid page %1.").arg(pageIndex + 1);
        }

        PDFDocumentModifier modifier(document);
        PDFDocumentBuilder* builder = modifier.getBuilder();
        builder->flattenPageTree();

        std::vector<PDFObjectReference> pages = builder->getPages();
        if (static_cast<PDFInteger>(pages.size()) != pageCount)
        {
            return PDFTranslationContext::tr("Page tree of the document is invalid.");
        }

        const PDFObjectReference pageReference = pages[pageIndex];
        PDFOperationResult result = operation(builder, pages, pageReference);
        if (!result)
        {
            return result;
        }

        builder->setPages(pages);
        modifier.markReset();

        if (modifier.finalize())
        {
            *document = *modifier.getDocument();
        }

        if (modificationFlags)
        {
            *modificationFlags = PDFModifiedDocument::ModificationFlags(PDFModifiedDocument::Reset | PDFModifiedDocument::PreserveUndoRedo) | additionalFlags;
        }

        return true;
    }

    /// Returns the references of the content streams of the page (Contents is a stream or an array of streams)
    static std::vector<PDFObjectReference> getContentReferences(const PDFObjectStorage* storage, const PDFDictionary* pageDictionary)
    {
        std::vector<PDFObjectReference> references;
        if (!pageDictionary)
        {
            return references;
        }

        const PDFObject& contents = pageDictionary->get("Contents");
        if (contents.isReference())
        {
            const PDFObject& dereferenced = storage->getObject(contents);
            if (dereferenced.isStream())
            {
                references.push_back(contents.getReference());
                return references;
            }
        }

        const PDFObject& array = storage->getObject(contents);
        if (array.isArray())
        {
            for (const PDFObject& item : *array.getArray())
            {
                if (item.isReference() && storage->getObject(item).isStream())
                {
                    references.push_back(item.getReference());
                }
            }
        }

        return references;
    }

    /// Returns the private data of the page (/PieceInfo /Inkpage /Private)
    static const PDFDictionary* getPrivateData(const PDFObjectStorage* storage, const PDFDictionary* pageDictionary)
    {
        if (!pageDictionary)
        {
            return nullptr;
        }

        const PDFDictionary* pieceInfo = storage->getDictionaryFromObject(pageDictionary->get("PieceInfo"));
        const PDFDictionary* entry = pieceInfo ? storage->getDictionaryFromObject(pieceInfo->get(PDFPageOrganizer::PIECE_INFO_KEY)) : nullptr;
        return entry ? storage->getDictionaryFromObject(entry->get("Private")) : nullptr;
    }

    /// Returns the reference of the background stream of the page. The reference is
    /// valid only, if the private data refer to the first content stream of the page,
    /// otherwise the content was rearranged by another application and it is not
    /// considered as the background anymore.
    static PDFObjectReference getBackgroundStream(const PDFObjectStorage* storage, const PDFDictionary* pageDictionary)
    {
        const PDFDictionary* privateData = getPrivateData(storage, pageDictionary);
        if (!privateData || !privateData->get("Stream").isReference())
        {
            return PDFObjectReference();
        }

        const PDFObjectReference stream = privateData->get("Stream").getReference();
        const std::vector<PDFObjectReference> contents = getContentReferences(storage, pageDictionary);
        if (contents.empty() || contents.front() != stream)
        {
            return PDFObjectReference();
        }

        return stream;
    }

    static PDFPageOrganizer::Background getBackground(const PDFObjectStorage* storage, const PDFDictionary* pageDictionary)
    {
        if (!getBackgroundStream(storage, pageDictionary).isValid())
        {
            return PDFPageOrganizer::Background::Plain;
        }

        PDFDocumentDataLoaderDecorator loader(storage);
        const QByteArray name = loader.readNameFromDictionary(getPrivateData(storage, pageDictionary), "Background");
        for (PDFPageOrganizer::Background background : PDFPageOrganizer::getBackgrounds())
        {
            if (PDFPageOrganizer::getBackgroundName(background) == name)
            {
                return background;
            }
        }

        // A background of a newer version is kept as it is (it is not plain)
        return PDFPageOrganizer::Background::Ruled;
    }

    static PDFObject createContents(const std::vector<PDFObjectReference>& contents)
    {
        PDFObjectFactory factory;
        if (contents.size() == 1)
        {
            factory << contents.front();
        }
        else
        {
            factory << contents;
        }
        return factory.takeObject();
    }

    /// Writes the background stream (invalid reference removes the background) and the private
    /// data of the page. The content streams of the page and its other private data are kept.
    static void writeBackground(PDFDocumentBuilder* builder, PDFObjectReference pageReference, PDFObjectReference backgroundStream, PDFPageOrganizer::Background background)
    {
        const PDFObjectStorage* storage = builder->getStorage();
        const PDFObject pageObject = storage->getObjectByReference(pageReference);
        const PDFDictionary* pageDictionary = storage->getDictionaryFromObject(pageObject);
        if (!pageDictionary)
        {
            return;
        }

        std::vector<PDFObjectReference> contents = getContentReferences(storage, pageDictionary);
        const PDFObjectReference oldBackgroundStream = getBackgroundStream(storage, pageDictionary);
        if (oldBackgroundStream.isValid())
        {
            contents.erase(contents.begin());
        }
        if (backgroundStream.isValid())
        {
            contents.insert(contents.begin(), backgroundStream);
        }

        PDFDictionaryBuilder pieceInfo = PDFObjectUtils::copyDictionary(storage, pageDictionary->get("PieceInfo"));
        pieceInfo.removeEntry(PDFPageOrganizer::PIECE_INFO_KEY);
        if (backgroundStream.isValid())
        {
            const QByteArray backgroundName = PDFPageOrganizer::getBackgroundName(background);

            PDFObjectFactory privateFactory;
            privateFactory.beginDictionary();
            privateFactory.beginDictionaryItem("Background");
            privateFactory << WrapName(backgroundName.constData());
            privateFactory.endDictionaryItem();
            privateFactory.beginDictionaryItem("Stream");
            privateFactory << backgroundStream;
            privateFactory.endDictionaryItem();
            privateFactory.endDictionary();

            PDFObjectFactory entryFactory;
            entryFactory.beginDictionary();
            entryFactory.beginDictionaryItem("LastModified");
            entryFactory << WrapCurrentDateTime();
            entryFactory.endDictionaryItem();
            entryFactory.beginDictionaryItem("Private");
            entryFactory << privateFactory.takeObject();
            entryFactory.endDictionaryItem();
            entryFactory.endDictionary();

            pieceInfo.setEntry(PDFInplaceOrMemoryString(PDFPageOrganizer::PIECE_INFO_KEY), entryFactory.takeObject());
        }

        PDFDictionaryBuilder updatedPage = PDFObjectUtils::copyDictionary(storage, pageObject);
        if (contents.empty())
        {
            updatedPage.removeEntry("Contents");
        }
        else
        {
            updatedPage.setEntry(PDFInplaceOrMemoryString("Contents"), createContents(contents));
        }

        if (pieceInfo.isEmpty())
        {
            updatedPage.removeEntry("PieceInfo");
        }
        else
        {
            updatedPage.setEntry(PDFInplaceOrMemoryString("PieceInfo"), PDFObject::createDictionary(std::move(pieceInfo)));

            // Page dictionary with /PieceInfo must have /LastModified
            PDFObjectFactory dateFactory;
            dateFactory << WrapCurrentDateTime();
            updatedPage.setEntry(PDFInplaceOrMemoryString("LastModified"), dateFactory.takeObject());
        }

        builder->setObject(pageReference, PDFObject::createDictionary(std::move(updatedPage)));
    }

    static PDFObjectReference createContentStream(PDFDocumentBuilder* builder, const QByteArray& content)
    {
        PDFDictionaryBuilder dictionary;
        dictionary.addEntry(PDFInplaceOrMemoryString("Length"), PDFObject::createInteger(content.size()));
        return builder->addObject(PDFObject::createStream(PDFStream(std::move(dictionary), QByteArray(content))));
    }

    /// Creates a blank page with the size, rotation and background of the source page
    static PDFObjectReference createBlankPage(PDFDocumentBuilder* builder, PDFObjectReference sourcePage)
    {
        const PDFObjectStorage* storage = builder->getStorage();
        const PDFObject sourceObject = storage->getObjectByReference(sourcePage);
        const PDFDictionary* sourceDictionary = storage->getDictionaryFromObject(sourceObject);

        PDFDictionaryBuilder page;
        page.setEntry(PDFInplaceOrMemoryString("Type"), PDFObject::createName("Page"));
        page.setEntry(PDFInplaceOrMemoryString("Resources"), PDFObject::createDictionary(PDFDictionaryBuilder()));

        // Page tree is flattened, the inheritable attributes are in the page dictionary
        // and the parent of the page is the root of the page tree
        for (const char* key : { "Parent", "MediaBox", "CropBox", "Rotate", "UserUnit" })
        {
            if (sourceDictionary && sourceDictionary->hasKey(key))
            {
                page.setEntry(PDFInplaceOrMemoryString(key), sourceDictionary->get(key));
            }
        }

        const PDFObjectReference pageReference = builder->addObject(PDFObject::createDictionary(std::move(page)));

        // The background stream depends only on the boxes and the rotation, which are
        // the same, so the stream is shared with the source page
        const PDFObjectReference backgroundStream = getBackgroundStream(storage, sourceDictionary);
        if (backgroundStream.isValid())
        {
            writeBackground(builder, pageReference, backgroundStream, getBackground(storage, sourceDictionary));
        }

        return pageReference;
    }

    /// Creates a copy of the page, the content is shared, annotations are copied
    static PDFObjectReference createPageCopy(PDFDocumentBuilder* builder, PDFObjectReference sourcePage)
    {
        const PDFObjectStorage* storage = builder->getStorage();
        const PDFObject sourceObject = storage->getObjectByReference(sourcePage);

        // Page tree is flattened, the parent of the copy is the root of the page tree as well
        PDFDictionaryBuilder page = PDFObjectUtils::copyDictionary(storage, sourceObject);

        // Annotations are copied below, the structure tree and article beads refer
        // to the original page only
        for (const char* key : { "Annots", "StructParents", "B" })
        {
            page.removeEntry(key);
        }

        const PDFObjectReference pageReference = builder->addObject(PDFObject::createDictionary(std::move(page)));

        PDFDocumentDataLoaderDecorator loader(storage);
        const std::vector<PDFObjectReference> annotations = loader.readReferenceArrayFromDictionary(storage->getDictionaryFromObject(sourceObject), "Annots");
        for (const PDFObjectReference& annotation : annotations)
        {
            const PDFDictionary* annotationDictionary = storage->getDictionaryFromObject(storage->getObjectByReference(annotation));
            if (!annotationDictionary)
            {
                continue;
            }

            // Popup windows and replies are copied together with the annotation, to which
            // they belong. Widgets belong to the form fields, a copy would be another
            // widget of the same field.
            const QByteArray subtype = loader.readNameFromDictionary(annotationDictionary, "Subtype");
            if (subtype == "Popup" || subtype == "Widget" || annotationDictionary->hasKey("IRT"))
            {
                continue;
            }

            PDFAnnotationManipulator::copyAnnotation(builder, annotation, pageReference);
        }

        return pageReference;
    }

    static QByteArray formatNumber(PDFReal value)
    {
        QByteArray number = QByteArray::number(value, 'f', 3);
        while (number.contains('.') && (number.endsWith('0') || number.endsWith('.')))
        {
            number.chop(1);
        }
        return number == "-0" ? QByteArray("0") : number;
    }

    /// Returns the transformation from the displayed page (origin at the bottom left
    /// corner of the displayed page, width and height of the displayed page) into
    /// the coordinates of the page, together with the size of the displayed page.
    static QTransform getDisplayToPageTransform(const QRectF& box, PageRotation rotation, QSizeF* displayedSize)
    {
        const PDFReal x0 = box.left();
        const PDFReal y0 = box.top();
        const PDFReal w = box.width();
        const PDFReal h = box.height();

        switch (rotation)
        {
            case PageRotation::Rotate90:
                *displayedSize = QSizeF(h, w);
                return QTransform(0.0, 1.0, -1.0, 0.0, x0 + w, y0);

            case PageRotation::Rotate180:
                *displayedSize = QSizeF(w, h);
                return QTransform(-1.0, 0.0, 0.0, -1.0, x0 + w, y0 + h);

            case PageRotation::Rotate270:
                *displayedSize = QSizeF(h, w);
                return QTransform(0.0, -1.0, 1.0, 0.0, x0, y0 + h);

            case PageRotation::None:
                break;
        }

        *displayedSize = QSizeF(w, h);
        return QTransform(1.0, 0.0, 0.0, 1.0, x0, y0);
    }

    static void appendLine(QByteArray& content, QPointF start, QPointF end)
    {
        content.append(formatNumber(start.x()) + ' ' + formatNumber(start.y()) + " m ");
        content.append(formatNumber(end.x()) + ' ' + formatNumber(end.y()) + " l S\n");
    }

    /// Appends a segment to the current path (the path is stroked later)
    static void appendSegment(QByteArray& content, QPointF start, QPointF end)
    {
        content.append(formatNumber(start.x()) + ' ' + formatNumber(start.y()) + " m ");
        content.append(formatNumber(end.x()) + ' ' + formatNumber(end.y()) + " l\n");
    }

    /// Appends a dot: a very short segment, drawn by the round line cap (a segment
    /// of zero length is not drawn by all renderers, the renderer of PDF4QT ignores it)
    static void appendDot(QByteArray& content, QPointF point)
    {
        appendSegment(content, point, point + QPointF(0.01, 0.0));
    }

    /// Sets the stroke color, the line width and the line cap (0 butt, 1 round, 2 square)
    static void appendStrokeStyle(QByteArray& content, QRgb color, PDFReal lineWidth, int lineCap)
    {
        content.append(formatNumber(qRed(color) / 255.0) + ' ' + formatNumber(qGreen(color) / 255.0) + ' ' + formatNumber(qBlue(color) / 255.0) + " RG ");
        content.append(formatNumber(lineWidth) + " w " + QByteArray::number(lineCap) + " J\n");
    }

    /// Index bounds of the lines of a grid with the step, which lie in the interval
    /// [margin, length - margin] (Xournal++ PlainBackgroundView::getIndexBounds,
    /// for the whole page)
    static std::pair<int, int> getIndexBounds(PDFReal step, PDFReal margin, PDFReal length)
    {
        return { int(std::ceil(margin / step)), int(std::floor((length - margin) / step)) };
    }

    // The backgrounds of Xournal++ (GPL-2.0-or-later, https://github.com/xournalpp/xournalpp,
    // src/core/view/background): the same sizes (points), colors and algorithms. They are
    // drawn in the coordinates of Xournal++ - origin in the top left corner of the
    // displayed page, y axis pointing down. Colors of Xournal++ (light background).
    static constexpr QRgb XOURNAL_SILVER = 0xbdbdbd;
    static constexpr QRgb XOURNAL_DODGER_BLUE = 0x40a0ff;
    static constexpr QRgb XOURNAL_DEEP_PINK = 0xff0080;
    static constexpr QRgb XOURNAL_BLACK = 0x000000;
    static constexpr PDFReal XOURNAL_HEADER_SIZE = 80.0;
    static constexpr PDFReal XOURNAL_FOOTER_SIZE = 60.0;
    static constexpr PDFReal XOURNAL_SQUARE_SIZE = 14.17;   // 5 mm

    /// Horizontal lines (Xournal++ RuledBackgroundView), with a vertical margin line
    /// at \p margin (LinedBackgroundView, negative margin is measured from the right
    /// side, zero means no margin line)
    static void appendXournalLines(QByteArray& content, QSizeF size, PDFReal margin)
    {
        const PDFReal lineSpacing = 24.0;
        const PDFReal lineWidth = 0.5;
        const auto [first, last] = getIndexBounds(lineSpacing, 0.0, size.height() - XOURNAL_HEADER_SIZE - XOURNAL_FOOTER_SIZE);

        appendStrokeStyle(content, XOURNAL_DODGER_BLUE, lineWidth, 0);
        for (int i = first; i <= last; ++i)
        {
            const PDFReal y = XOURNAL_HEADER_SIZE + i * lineSpacing;
            appendSegment(content, QPointF(0.0, y), QPointF(size.width(), y));
        }
        content.append("S\n");

        if (!qFuzzyIsNull(margin))
        {
            const PDFReal x = margin < 0.0 ? margin + size.width() : margin;
            appendStrokeStyle(content, XOURNAL_DEEP_PINK, lineWidth, 0);
            appendSegment(content, QPointF(x, 0.0), QPointF(x, size.height()));
            content.append("S\n");
        }
    }

    /// Squares (Xournal++ GraphBackgroundView). With a margin, the grid stops at the
    /// first line inside the margin; every \p boldLineInterval-th line is bold.
    static void appendXournalGraph(QByteArray& content, QSizeF size, PDFReal margin, int boldLineInterval, PDFReal boldLineWidth)
    {
        const PDFReal squareSize = XOURNAL_SQUARE_SIZE;
        const PDFReal lineWidth = 0.5;
        const PDFReal halfLineWidth = 0.5 * (boldLineInterval > 0 ? boldLineWidth : lineWidth);
        const PDFReal clearance = std::max(margin, halfLineWidth);
        const auto [firstX, lastX] = getIndexBounds(squareSize, clearance, size.width());
        const auto [firstY, lastY] = getIndexBounds(squareSize, clearance, size.height());

        // The lines span the page, or the grid rounded to the margin
        PDFReal minX = 0.0;
        PDFReal maxX = size.width();
        PDFReal minY = 0.0;
        PDFReal maxY = size.height();
        if (margin > 0.0)
        {
            minX = squareSize * firstX;
            maxX = squareSize * lastX;
            minY = squareSize * firstY;
            maxY = squareSize * lastY;
        }

        auto appendLines = [&](bool bold)
        {
            for (int i = firstX; i <= lastX; ++i)
            {
                if (boldLineInterval <= 0 || (i % boldLineInterval == 0) == bold)
                {
                    appendSegment(content, QPointF(i * squareSize, minY), QPointF(i * squareSize, maxY));
                }
            }
            for (int i = firstY; i <= lastY; ++i)
            {
                if (boldLineInterval <= 0 || (i % boldLineInterval == 0) == bold)
                {
                    appendSegment(content, QPointF(minX, i * squareSize), QPointF(maxX, i * squareSize));
                }
            }
            content.append("S\n");
        };

        appendStrokeStyle(content, XOURNAL_SILVER, lineWidth, 2);
        appendLines(false);
        if (boldLineInterval > 0)
        {
            appendStrokeStyle(content, XOURNAL_SILVER, boldLineWidth, 2);
            appendLines(true);
        }
    }

    /// Dots in the corners of squares of 5 mm (Xournal++ DottedBackgroundView)
    static void appendXournalDots(QByteArray& content, QSizeF size)
    {
        const PDFReal squareSize = XOURNAL_SQUARE_SIZE;
        const PDFReal lineWidth = 1.5;
        const auto [firstX, lastX] = getIndexBounds(squareSize, 0.5 * lineWidth, size.width());
        const auto [firstY, lastY] = getIndexBounds(squareSize, 0.5 * lineWidth, size.height());

        appendStrokeStyle(content, XOURNAL_SILVER, lineWidth, 1);
        for (int i = firstX; i <= lastX; ++i)
        {
            for (int j = firstY; j <= lastY; ++j)
            {
                appendDot(content, QPointF(i * squareSize, j * squareSize));
            }
        }
        content.append("S\n");
    }

    /// Isometric grid of triangles of 5 mm, centered on the page, as dots or lines
    /// (Xournal++ BaseIsometricBackgroundView, IsoDottedBackgroundView, IsoGraphBackgroundView)
    static void appendXournalIsometric(QByteArray& content, QSizeF size, bool dots)
    {
        const PDFReal triangleSize = XOURNAL_SQUARE_SIZE;
        const PDFReal xstep = std::sqrt(3.0) / 2.0 * triangleSize;
        const PDFReal ystep = triangleSize / 2.0;
        const PDFReal margin = triangleSize;
        const int cols = int(std::floor((size.width() - 2 * margin) / xstep));
        const int rows = int(std::floor((size.height() - 2 * margin) / ystep));
        if (cols <= 0 || rows <= 0)
        {
            return;
        }

        const PDFReal contentWidth = cols * xstep;
        const PDFReal contentHeight = rows * ystep;
        const QPointF offset((size.width() - contentWidth) / 2, (size.height() - contentHeight) / 2);
        auto segment = [&](PDFReal x1, PDFReal y1, PDFReal x2, PDFReal y2)
        {
            appendSegment(content, offset + QPointF(x1, y1), offset + QPointF(x2, y2));
        };

        if (dots)
        {
            appendStrokeStyle(content, XOURNAL_SILVER, 1.5, 1);
            for (int col = 0; col <= cols; ++col)
            {
                const bool isEvenColumn = col % 2 == 0;
                const PDFReal yoffset = isEvenColumn ? ystep : 0.0;
                const int rowsInColumn = isEvenColumn ? rows - 2 : rows;
                for (int row = 0; row <= rowsInColumn; row += 2)
                {
                    appendDot(content, offset + QPointF(col * xstep, yoffset + row * ystep));
                }
            }
            content.append("S\n");
            return;
        }

        appendStrokeStyle(content, XOURNAL_SILVER, 1.0, 1);

        // Horizontal lines on the top and the bottom, vertical lines
        segment(0.0, 0.0, contentWidth, 0.0);
        segment(0.0, contentHeight, contentWidth, contentHeight);
        for (int col = 0; col <= cols; ++col)
        {
            segment(col * xstep, 0.0, col * xstep, contentHeight);
        }

        const int hdiags = cols / 2;
        const int vdiags = rows / 2;
        const int diags = hdiags + vdiags;
        const int hcorr = cols - 2 * hdiags;
        const int vcorr = rows - 2 * vdiags;

        // Diagonals starting in the top left corner (left-down)
        for (int d = 0; d < diags + hcorr * vcorr; ++d)
        {
            PDFReal x1 = contentWidth;
            PDFReal y1 = 0.0;
            if (d < hdiags)
            {
                x1 = xstep + d * 2 * xstep;
            }
            else
            {
                y1 = ystep + (d - hdiags) * 2 * ystep - hcorr * ystep;
            }

            PDFReal x2 = 0.0;
            PDFReal y2 = contentHeight;
            if (d < vdiags)
            {
                y2 = ystep + d * 2 * ystep;
            }
            else
            {
                x2 = xstep + (d - vdiags) * 2 * xstep - vcorr * xstep;
            }

            segment(x1, y1, x2, y2);
        }

        // Diagonals starting in the top right corner (right-down)
        for (int d = 0; d < diags; ++d)
        {
            PDFReal x1 = 0.0;
            PDFReal y1 = 0.0;
            if (d < hdiags)
            {
                x1 = contentWidth - (xstep + d * 2 * xstep) - hcorr * xstep;
            }
            else
            {
                y1 = ystep + (d - hdiags) * 2 * ystep;
            }

            PDFReal x2 = contentWidth;
            PDFReal y2 = contentHeight;
            if (d < vdiags)
            {
                y2 = ystep + d * 2 * ystep + hcorr * ystep;
            }
            else
            {
                x2 = contentWidth - (xstep + (d - vdiags) * 2 * xstep) + (vcorr - hcorr) * xstep;
            }

            segment(x1, y1, x2, y2);
        }
        content.append("S\n");
    }

    /// Music staves of five lines (Xournal++ StavesBackgroundView)
    static void appendXournalStaves(QByteArray& content, QSizeF size)
    {
        const PDFReal lineWidth = 0.5;
        const PDFReal margin = 50.0;
        const PDFReal stavesSpacing = 40.0;
        const PDFReal linesSpacing = 5.0;
        const PDFReal staffHeight = lineWidth + 4 * linesSpacing;
        const PDFReal offsetBetweenStaves = stavesSpacing + staffHeight + 4 * lineWidth;
        const auto [first, last] = getIndexBounds(offsetBetweenStaves, 0.0, size.height() - XOURNAL_HEADER_SIZE - XOURNAL_FOOTER_SIZE);
        const PDFReal right = size.width() - margin;

        appendStrokeStyle(content, XOURNAL_BLACK, lineWidth, 2);
        for (int i = first; i <= last; ++i)
        {
            const PDFReal top = XOURNAL_HEADER_SIZE + i * offsetBetweenStaves;
            for (int line = 0; line < 5; ++line)
            {
                appendSegment(content, QPointF(margin, top + line * linesSpacing), QPointF(right, top + line * linesSpacing));
            }
            appendSegment(content, QPointF(margin, top), QPointF(margin, top + 4 * linesSpacing));
            appendSegment(content, QPointF(right, top), QPointF(right, top + 4 * linesSpacing));
        }
        content.append("S\n");
    }
};

std::vector<PDFPageOrganizer::Background> PDFPageOrganizer::getBackgrounds()
{
    return { Background::Plain, Background::Ruled, Background::Lines, Background::LinesMargin, Background::LinesRightMargin,
             Background::Graph, Background::GraphBorder, Background::GraphBorderBold, Background::Dotted,
             Background::IsometricDotted, Background::IsometricGraph, Background::Staves };
}

QByteArray PDFPageOrganizer::getBackgroundName(Background background)
{
    switch (background)
    {
        case Background::Plain:
            return "Plain";
        case Background::Ruled:
            return "Ruled";
        case Background::Lines:
            return "Lines";
        case Background::LinesMargin:
            return "LinesMargin";
        case Background::LinesRightMargin:
            return "LinesRightMargin";
        case Background::Graph:
            return "Graph";
        case Background::GraphBorder:
            return "GraphBorder";
        case Background::GraphBorderBold:
            return "GraphBorderBold";
        case Background::Dotted:
            return "Dotted";
        case Background::IsometricDotted:
            return "IsometricDotted";
        case Background::IsometricGraph:
            return "IsometricGraph";
        case Background::Staves:
            return "Staves";
    }

    Q_ASSERT(false);
    return "Plain";
}

PDFOperationResult PDFPageOrganizer::addBlankPage(PDFDocument* document, PDFInteger pageIndex, PDFModifiedDocument::ModificationFlags* modificationFlags)
{
    auto operation = [pageIndex](PDFDocumentBuilder* builder, std::vector<PDFObjectReference>& pages, PDFObjectReference page) -> PDFOperationResult
    {
        pages.insert(pages.begin() + pageIndex + 1, PDFPageOrganizerHelper::createBlankPage(builder, page));
        return true;
    };

    return PDFPageOrganizerHelper::modifyPageList(document, pageIndex, PDFModifiedDocument::None, modificationFlags, operation);
}

PDFOperationResult PDFPageOrganizer::insertBlankPage(PDFDocument* document, PDFInteger pageIndex, PDFModifiedDocument::ModificationFlags* modificationFlags)
{
    auto operation = [pageIndex](PDFDocumentBuilder* builder, std::vector<PDFObjectReference>& pages, PDFObjectReference page) -> PDFOperationResult
    {
        pages.insert(pages.begin() + pageIndex, PDFPageOrganizerHelper::createBlankPage(builder, page));
        return true;
    };

    return PDFPageOrganizerHelper::modifyPageList(document, pageIndex, PDFModifiedDocument::None, modificationFlags, operation);
}

PDFOperationResult PDFPageOrganizer::duplicatePage(PDFDocument* document, PDFInteger pageIndex, PDFModifiedDocument::ModificationFlags* modificationFlags)
{
    auto operation = [pageIndex](PDFDocumentBuilder* builder, std::vector<PDFObjectReference>& pages, PDFObjectReference page) -> PDFOperationResult
    {
        pages.insert(pages.begin() + pageIndex + 1, PDFPageOrganizerHelper::createPageCopy(builder, page));
        return true;
    };

    return PDFPageOrganizerHelper::modifyPageList(document, pageIndex, PDFModifiedDocument::Annotation, modificationFlags, operation);
}

PDFOperationResult PDFPageOrganizer::removePage(PDFDocument* document, PDFInteger pageIndex, PDFModifiedDocument::ModificationFlags* modificationFlags)
{
    auto operation = [pageIndex](PDFDocumentBuilder*, std::vector<PDFObjectReference>& pages, PDFObjectReference) -> PDFOperationResult
    {
        if (pages.size() < 2)
        {
            return PDFTranslationContext::tr("The last page of the document can't be removed.");
        }

        pages.erase(pages.begin() + pageIndex);
        return true;
    };

    return PDFPageOrganizerHelper::modifyPageList(document, pageIndex, PDFModifiedDocument::None, modificationFlags, operation);
}

PDFOperationResult PDFPageOrganizer::movePage(PDFDocument* document, PDFInteger pageIndex, PDFInteger targetPageIndex, PDFModifiedDocument::ModificationFlags* modificationFlags)
{
    auto operation = [pageIndex, targetPageIndex](PDFDocumentBuilder*, std::vector<PDFObjectReference>& pages, PDFObjectReference page) -> PDFOperationResult
    {
        if (targetPageIndex < 0 || targetPageIndex >= static_cast<PDFInteger>(pages.size()))
        {
            return PDFTranslationContext::tr("Invalid target page %1.").arg(targetPageIndex + 1);
        }

        pages.erase(pages.begin() + pageIndex);
        pages.insert(pages.begin() + targetPageIndex, page);
        return true;
    };

    return PDFPageOrganizerHelper::modifyPageList(document, pageIndex, PDFModifiedDocument::None, modificationFlags, operation);
}

PDFOperationResult PDFPageOrganizer::setBackground(PDFDocument* document,
                                                   const std::vector<PDFInteger>& pageIndices,
                                                   Background background,
                                                   PDFModifiedDocument::ModificationFlags* modificationFlags)
{
    if (modificationFlags)
    {
        *modificationFlags = PDFModifiedDocument::ModificationFlags();
    }

    if (!document)
    {
        return PDFTranslationContext::tr("Invalid document.");
    }

    const PDFCatalog* catalog = document->getCatalog();
    for (const PDFInteger pageIndex : pageIndices)
    {
        if (pageIndex < 0 || pageIndex >= static_cast<PDFInteger>(catalog->getPageCount()))
        {
            return PDFTranslationContext::tr("Invalid page %1.").arg(pageIndex + 1);
        }
    }

    PDFDocumentModifier modifier(document);
    PDFDocumentBuilder* builder = modifier.getBuilder();
    const PDFObjectStorage* storage = builder->getStorage();

    bool isChanged = false;
    PDFObjectReference backgroundLayer;
    for (const PDFInteger pageIndex : pageIndices)
    {
        const PDFPage* page = catalog->getPage(pageIndex);
        const PDFObjectReference pageReference = page->getPageReference();
        const PDFDictionary* pageDictionary = storage->getDictionaryFromObject(storage->getObjectByReference(pageReference));
        const bool hasBackground = PDFPageOrganizerHelper::getBackgroundStream(storage, pageDictionary).isValid();

        if (background == Background::Plain)
        {
            if (hasBackground)
            {
                PDFPageOrganizerHelper::writeBackground(builder, pageReference, PDFObjectReference(), background);
                isChanged = true;
            }
            continue;
        }

        if (hasBackground && PDFPageOrganizerHelper::getBackground(storage, pageDictionary) == background)
        {
            continue;
        }

        // The background is the content of the layer of the backgrounds (marked content
        // with the name of the layer in the properties of the resources of the page)
        if (!backgroundLayer.isValid())
        {
            backgroundLayer = builder->findOptionalContentGroup(QString::fromLatin1(BACKGROUND_LAYER_NAME));
            if (!backgroundLayer.isValid())
            {
                backgroundLayer = builder->createOptionalContentGroup(QString::fromLatin1(BACKGROUND_LAYER_NAME), true, true, false);
            }
        }

        QByteArray content = "/OC /" + QByteArray(BACKGROUND_PROPERTY_NAME) + " BDC\n";
        content.append(createBackgroundContent(page, background));
        content.append("EMC\n");

        const PDFObjectReference stream = PDFPageOrganizerHelper::createContentStream(builder, content);
        PDFPageOrganizerHelper::writeBackground(builder, pageReference, stream, background);

        // The resources of the page (they can be inherited) get the name of the layer
        PDFDictionaryBuilder resources = PDFObjectUtils::copyDictionary(storage, page->getResources());
        PDFDictionaryBuilder properties = PDFObjectUtils::copyDictionary(storage, resources.get("Properties"));
        properties.setEntry(PDFInplaceOrMemoryString(BACKGROUND_PROPERTY_NAME), PDFObject::createReference(backgroundLayer));
        resources.setEntry(PDFInplaceOrMemoryString("Properties"), PDFObject::createDictionary(std::move(properties)));
        PDFDictionaryBuilder updatedPage = PDFObjectUtils::copyDictionary(storage, storage->getObjectByReference(pageReference));
        updatedPage.setEntry(PDFInplaceOrMemoryString("Resources"), PDFObject::createDictionary(std::move(resources)));
        builder->setObject(pageReference, PDFObject::createDictionary(std::move(updatedPage)));
        isChanged = true;
    }

    if (!isChanged)
    {
        return true;
    }

    modifier.markReset();
    modifier.markPageContentsChanged();

    if (modifier.finalize())
    {
        *document = *modifier.getDocument();
    }

    if (modificationFlags)
    {
        *modificationFlags = PDFModifiedDocument::ModificationFlags(PDFModifiedDocument::Reset | PDFModifiedDocument::PreserveUndoRedo | PDFModifiedDocument::PageContents);
    }

    return true;
}

PDFPageOrganizer::Background PDFPageOrganizer::getBackground(const PDFDocument* document, PDFInteger pageIndex)
{
    if (!document || pageIndex < 0 || pageIndex >= static_cast<PDFInteger>(document->getCatalog()->getPageCount()))
    {
        return Background::Plain;
    }

    const PDFObjectStorage* storage = &document->getStorage();
    const PDFObjectReference pageReference = document->getCatalog()->getPage(pageIndex)->getPageReference();
    return PDFPageOrganizerHelper::getBackground(storage, storage->getDictionaryFromObject(storage->getObjectByReference(pageReference)));
}

PDFObjectReference PDFPageOrganizer::getBackgroundStream(const PDFDocument* document, PDFInteger pageIndex)
{
    if (!document || pageIndex < 0 || pageIndex >= static_cast<PDFInteger>(document->getCatalog()->getPageCount()))
    {
        return PDFObjectReference();
    }

    const PDFObjectStorage* storage = &document->getStorage();
    const PDFObjectReference pageReference = document->getCatalog()->getPage(pageIndex)->getPageReference();
    return PDFPageOrganizerHelper::getBackgroundStream(storage, storage->getDictionaryFromObject(storage->getObjectByReference(pageReference)));
}

QByteArray PDFPageOrganizer::createBackgroundContent(const PDFPage* page, Background background)
{
    if (!page || background == Background::Plain)
    {
        return QByteArray();
    }

    QSizeF size;
    const QTransform transform = PDFPageOrganizerHelper::getDisplayToPageTransform(page->getCropBox(), page->getPageRotation(), &size);

    // Ruled paper: light blue lines from the top margin to the bottom margin, red margin line on the left
    const PDFReal spacing = RULED_LINE_SPACING_MM * PDF_MM_TO_POINT;
    const PDFReal topMargin = 25.0 * PDF_MM_TO_POINT;
    const PDFReal bottomMargin = 12.0 * PDF_MM_TO_POINT;
    const PDFReal leftMargin = 25.0 * PDF_MM_TO_POINT;

    QByteArray content;
    content.append("q\n");
    content.append(PDFPageOrganizerHelper::formatNumber(transform.m11()) + ' ' +
                   PDFPageOrganizerHelper::formatNumber(transform.m12()) + ' ' +
                   PDFPageOrganizerHelper::formatNumber(transform.m21()) + ' ' +
                   PDFPageOrganizerHelper::formatNumber(transform.m22()) + ' ' +
                   PDFPageOrganizerHelper::formatNumber(transform.dx()) + ' ' +
                   PDFPageOrganizerHelper::formatNumber(transform.dy()) + " cm\n");

    if (background != Background::Ruled)
    {
        // Backgrounds of Xournal++: origin in the top left corner, y axis pointing down
        content.append("1 0 0 -1 0 " + PDFPageOrganizerHelper::formatNumber(size.height()) + " cm\n");
        switch (background)
        {
            case Background::Lines:
                PDFPageOrganizerHelper::appendXournalLines(content, size, 0.0);
                break;
            case Background::LinesMargin:
                PDFPageOrganizerHelper::appendXournalLines(content, size, 72.0);
                break;
            case Background::LinesRightMargin:
                PDFPageOrganizerHelper::appendXournalLines(content, size, -72.0);
                break;
            case Background::Graph:
                PDFPageOrganizerHelper::appendXournalGraph(content, size, 0.0, 0, 0.0);
                break;
            case Background::GraphBorder:
                PDFPageOrganizerHelper::appendXournalGraph(content, size, 40.0, 0, 0.0);
                break;
            case Background::GraphBorderBold:
                PDFPageOrganizerHelper::appendXournalGraph(content, size, 20.0, 5, 1.0);
                break;
            case Background::Dotted:
                PDFPageOrganizerHelper::appendXournalDots(content, size);
                break;
            case Background::IsometricDotted:
                PDFPageOrganizerHelper::appendXournalIsometric(content, size, true);
                break;
            case Background::IsometricGraph:
                PDFPageOrganizerHelper::appendXournalIsometric(content, size, false);
                break;
            case Background::Staves:
                PDFPageOrganizerHelper::appendXournalStaves(content, size);
                break;
            case Background::Plain:
            case Background::Ruled:
                Q_ASSERT(false);
                break;
        }
        content.append("Q\n");
        return content;
    }

    content.append("0.5 w 0.62 0.75 0.9 RG\n");
    for (PDFReal y = size.height() - topMargin; y >= bottomMargin; y -= spacing)
    {
        PDFPageOrganizerHelper::appendLine(content, QPointF(0.0, y), QPointF(size.width(), y));
    }

    if (size.width() > leftMargin * 2.0)
    {
        content.append("0.9 0.45 0.45 RG\n");
        PDFPageOrganizerHelper::appendLine(content, QPointF(leftMargin, 0.0), QPointF(leftMargin, size.height()));
    }

    content.append("Q\n");
    return content;
}

}   // namespace pdf
