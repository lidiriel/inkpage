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

#include "pdfpageorganizer.h"
#include "pdfdocumentbuilder.h"
#include "pdfannotationmanipulator.h"
#include "pdfobjectutils.h"

#include <QTransform>

#include <algorithm>
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
        const QByteArray background = loader.readNameFromDictionary(getPrivateData(storage, pageDictionary), "Background");
        return background == "Ruled" ? PDFPageOrganizer::Background::Ruled : PDFPageOrganizer::Background::Plain;
    }

    static QByteArray getBackgroundName(PDFPageOrganizer::Background background)
    {
        switch (background)
        {
            case PDFPageOrganizer::Background::Ruled:
                return "Ruled";

            case PDFPageOrganizer::Background::Plain:
                break;
        }

        return "Plain";
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
            const QByteArray backgroundName = getBackgroundName(background);

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
};

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

        const PDFObjectReference stream = PDFPageOrganizerHelper::createContentStream(builder, createBackgroundContent(page, background));
        PDFPageOrganizerHelper::writeBackground(builder, pageReference, stream, background);
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
