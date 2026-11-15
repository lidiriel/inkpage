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
#include "pdfcms.h"
#include "pdfcolorspaces.h"
#include "pdffont.h"
#include "pdfoptionalcontent.h"
#include "pdfrenderer.h"

#include <QtTest>
#include <QImage>
#include <QPainter>

using namespace pdf;

class PageOrganizerTest : public QObject
{
    Q_OBJECT

private slots:
    void addBlankPageAfterPageKeepsSize();
    void insertBlankPageBeforePage();
    void duplicatePageSharesContentAndCopiesAnnotations();
    void removePage();
    void removeLastPageFails();
    void movePage();
    void invalidPageFails();
    void ruledBackgroundIsDrawnUnderContent();
    void backgroundIsInTheBackgroundLayer();
    void plainBackgroundRemovesRuledBackground();
    void ruledBackgroundIsSetOnlyOnce();
    void ruledBackgroundFollowsPageRotation();
    void blankPageTakesBackgroundOfPage();
    void xournalBackgroundsAreStoredAndDrawn();
    void xournalBackgroundsGeometry();
    void xournalBackgroundFollowsPageRotation();

private:
    static constexpr QRectF A4 = QRectF(0, 0, 595, 842);
    static constexpr QRectF A5_LANDSCAPE = QRectF(0, 0, 595, 420);

    /// Document with pages of different sizes, the first page has a black square
    /// in its content, the second page has a square annotation
    static PDFDocument createDocument(int pageCount = 3);

    static std::vector<PDFObjectReference> pageReferences(const PDFDocument& document);
    static std::vector<PDFObjectReference> contentReferences(const PDFDocument& document, PDFInteger pageIndex);
    static std::vector<PDFObjectReference> annotations(const PDFDocument& document, PDFInteger pageIndex);
    static QImage renderPage(const PDFDocument& document, PDFInteger pageIndex, PDFObjectReference hiddenLayer = PDFObjectReference());

    /// Number of rows (or columns), which are mostly covered by the color of the ruled lines
    static int countRuledLines(const QImage& image, Qt::Orientation orientation);
};

PDFDocument PageOrganizerTest::createDocument(int pageCount)
{
    PDFDocumentBuilder builder;

    PDFPageContentStreamBuilder contentBuilder(&builder);
    QPainter* painter = contentBuilder.beginNewPage(A4);
    painter->fillRect(QRectF(200, 300, 100, 100), Qt::black);
    contentBuilder.end(painter);

    for (int i = 1; i < pageCount; ++i)
    {
        const PDFObjectReference page = builder.appendPage(i == 1 ? A5_LANDSCAPE : A4);
        if (i == 1)
        {
            builder.createAnnotationSquare(page, QRectF(100, 100, 50, 30), 2.0, Qt::yellow, Qt::black, "Title", "Subject", "Contents");
        }
    }

    return builder.build();
}

std::vector<PDFObjectReference> PageOrganizerTest::pageReferences(const PDFDocument& document)
{
    std::vector<PDFObjectReference> result;
    for (size_t i = 0; i < document.getCatalog()->getPageCount(); ++i)
    {
        result.push_back(document.getCatalog()->getPage(i)->getPageReference());
    }
    return result;
}

std::vector<PDFObjectReference> PageOrganizerTest::contentReferences(const PDFDocument& document, PDFInteger pageIndex)
{
    const PDFObjectStorage& storage = document.getStorage();
    const PDFDictionary* page = storage.getDictionaryFromObject(storage.getObjectByReference(document.getCatalog()->getPage(pageIndex)->getPageReference()));
    std::vector<PDFObjectReference> result;
    const PDFObject& contents = page->get("Contents");
    if (contents.isReference() && storage.getObject(contents).isStream())
    {
        result.push_back(contents.getReference());
    }
    else if (storage.getObject(contents).isArray())
    {
        for (const PDFObject& item : *storage.getObject(contents).getArray())
        {
            result.push_back(item.getReference());
        }
    }
    return result;
}

std::vector<PDFObjectReference> PageOrganizerTest::annotations(const PDFDocument& document, PDFInteger pageIndex)
{
    const PDFObjectStorage& storage = document.getStorage();
    const PDFDictionary* page = storage.getDictionaryFromObject(storage.getObjectByReference(document.getCatalog()->getPage(pageIndex)->getPageReference()));
    PDFDocumentDataLoaderDecorator loader(&storage);
    return loader.readReferenceArrayFromDictionary(page, "Annots");
}

QImage PageOrganizerTest::renderPage(const PDFDocument& document, PDFInteger pageIndex, PDFObjectReference hiddenLayer)
{
    PDFCMSGeneric cms;
    PDFFontCache fontCache(32, 32);
    PDFICCProfileCache iccProfileCache;
    PDFOptionalContentActivity activity(&document, OCUsage::View, nullptr);
    if (hiddenLayer.isValid())
    {
        activity.setState(hiddenLayer, OCState::OFF);
    }
    fontCache.setDocument(PDFModifiedDocument(const_cast<PDFDocument*>(&document), &activity));
    iccProfileCache.setDocument(PDFModifiedDocument(const_cast<PDFDocument*>(&document), &activity));

    PDFRenderer renderer(&document, &fontCache, &iccProfileCache, &cms, &activity, PDFRenderer::Features(), PDFMeshQualitySettings());

    // One pixel per point, in the displayed orientation of the page
    const PDFPage* page = document.getCatalog()->getPage(pageIndex);
    const QSizeF size = page->getRotatedMediaBox().size();
    QImage image(size.toSize(), QImage::Format_RGB888);
    image.fill(Qt::white);

    QPainter painter(&image);
    renderer.render(&painter, QRectF(0, 0, image.width(), image.height()), pageIndex);
    painter.end();
    return image;
}

int PageOrganizerTest::countRuledLines(const QImage& image, Qt::Orientation orientation)
{
    auto isLineColor = [&image](int x, int y)
    {
        const QColor color = image.pixelColor(x, y);
        return color.blue() - color.red() > 30;
    };

    const int lineCount = orientation == Qt::Horizontal ? image.height() : image.width();
    const int lineLength = orientation == Qt::Horizontal ? image.width() : image.height();

    int result = 0;
    for (int line = 0; line < lineCount; ++line)
    {
        int covered = 0;
        for (int i = 0; i < lineLength; ++i)
        {
            const bool isColored = orientation == Qt::Horizontal ? isLineColor(i, line) : isLineColor(line, i);
            covered += isColored ? 1 : 0;
        }

        if (covered > lineLength * 8 / 10)
        {
            ++result;
        }
    }
    return result;
}

void PageOrganizerTest::addBlankPageAfterPageKeepsSize()
{
    PDFDocument document = createDocument();
    const std::vector<PDFObjectReference> originalPages = pageReferences(document);

    PDFModifiedDocument::ModificationFlags flags;
    QVERIFY(PDFPageOrganizer::addBlankPage(&document, 1, &flags));
    QVERIFY(flags.testFlag(PDFModifiedDocument::Reset));

    const std::vector<PDFObjectReference> pages = pageReferences(document);
    QCOMPARE(pages.size(), size_t(4));
    QCOMPARE(pages[0], originalPages[0]);
    QCOMPARE(pages[1], originalPages[1]);
    QCOMPARE(pages[3], originalPages[2]);

    const PDFPage* blankPage = document.getCatalog()->getPage(2);
    QCOMPARE(blankPage->getMediaBox(), A5_LANDSCAPE);
    QVERIFY(contentReferences(document, 2).empty());
    QVERIFY(annotations(document, 2).empty());
}

void PageOrganizerTest::insertBlankPageBeforePage()
{
    PDFDocument document = createDocument();
    const std::vector<PDFObjectReference> originalPages = pageReferences(document);

    QVERIFY(PDFPageOrganizer::insertBlankPage(&document, 0, nullptr));

    const std::vector<PDFObjectReference> pages = pageReferences(document);
    QCOMPARE(pages.size(), size_t(4));
    QCOMPARE(pages[1], originalPages[0]);
    QCOMPARE(pages[2], originalPages[1]);
    QCOMPARE(pages[3], originalPages[2]);
    QCOMPARE(document.getCatalog()->getPage(0)->getMediaBox(), A4);
    QVERIFY(contentReferences(document, 0).empty());
}

void PageOrganizerTest::duplicatePageSharesContentAndCopiesAnnotations()
{
    PDFDocument document = createDocument();
    QVERIFY(PDFPageOrganizer::duplicatePage(&document, 0, nullptr));
    QVERIFY(PDFPageOrganizer::duplicatePage(&document, 2, nullptr));

    // Page 0 and its copy 1, page 2 (annotation) and its copy 3, page 4
    QCOMPARE(document.getCatalog()->getPageCount(), size_t(5));
    QVERIFY(document.getCatalog()->getPage(0)->getPageReference() != document.getCatalog()->getPage(1)->getPageReference());
    QCOMPARE(contentReferences(document, 1), contentReferences(document, 0));
    QVERIFY(!contentReferences(document, 1).empty());

    const std::vector<PDFObjectReference> originalAnnotations = annotations(document, 2);
    const std::vector<PDFObjectReference> copiedAnnotations = annotations(document, 3);
    QCOMPARE(originalAnnotations.size(), size_t(1));
    QCOMPARE(copiedAnnotations.size(), size_t(1));
    QVERIFY(originalAnnotations.front() != copiedAnnotations.front());
    QCOMPARE(document.getCatalog()->getPage(3)->getMediaBox(), A5_LANDSCAPE);

    // The copy of the annotation belongs to the copy of the page
    const PDFObjectStorage& storage = document.getStorage();
    const PDFDictionary* copiedAnnotation = storage.getDictionaryFromObject(storage.getObjectByReference(copiedAnnotations.front()));
    QCOMPARE(copiedAnnotation->get("P").getReference(), document.getCatalog()->getPage(3)->getPageReference());

    // The duplicated page is rendered as the original one
    QCOMPARE(renderPage(document, 1), renderPage(document, 0));
}

void PageOrganizerTest::removePage()
{
    PDFDocument document = createDocument();
    const std::vector<PDFObjectReference> originalPages = pageReferences(document);

    QVERIFY(PDFPageOrganizer::removePage(&document, 1, nullptr));
    const std::vector<PDFObjectReference> pages = pageReferences(document);
    QCOMPARE(pages.size(), size_t(2));
    QCOMPARE(pages[0], originalPages[0]);
    QCOMPARE(pages[1], originalPages[2]);
}

void PageOrganizerTest::removeLastPageFails()
{
    PDFDocument document = createDocument(1);
    const PDFOperationResult result = PDFPageOrganizer::removePage(&document, 0, nullptr);
    QVERIFY(!result);
    QVERIFY(!result.getErrorMessage().isEmpty());
    QCOMPARE(document.getCatalog()->getPageCount(), size_t(1));
}

void PageOrganizerTest::movePage()
{
    PDFDocument document = createDocument();
    const std::vector<PDFObjectReference> originalPages = pageReferences(document);

    // Move down (later in the document)
    QVERIFY(PDFPageOrganizer::movePage(&document, 0, 1, nullptr));
    QCOMPARE(pageReferences(document), (std::vector<PDFObjectReference>{ originalPages[1], originalPages[0], originalPages[2] }));

    // Move up to the beginning
    QVERIFY(PDFPageOrganizer::movePage(&document, 2, 0, nullptr));
    QCOMPARE(pageReferences(document), (std::vector<PDFObjectReference>{ originalPages[2], originalPages[1], originalPages[0] }));

    QVERIFY(!PDFPageOrganizer::movePage(&document, 0, 3, nullptr));
    QVERIFY(!PDFPageOrganizer::movePage(&document, 0, -1, nullptr));
    QCOMPARE(pageReferences(document), (std::vector<PDFObjectReference>{ originalPages[2], originalPages[1], originalPages[0] }));
}

void PageOrganizerTest::invalidPageFails()
{
    PDFDocument document = createDocument();
    QVERIFY(!PDFPageOrganizer::addBlankPage(&document, 3, nullptr));
    QVERIFY(!PDFPageOrganizer::insertBlankPage(&document, -1, nullptr));
    QVERIFY(!PDFPageOrganizer::duplicatePage(&document, 5, nullptr));
    QVERIFY(!PDFPageOrganizer::removePage(&document, 3, nullptr));
    QVERIFY(!PDFPageOrganizer::setBackground(&document, { 0, 3 }, PDFPageOrganizer::Background::Ruled, nullptr));
    QVERIFY(!PDFPageOrganizer::addBlankPage(nullptr, 0, nullptr));
    QCOMPARE(document.getCatalog()->getPageCount(), size_t(3));
    QCOMPARE(PDFPageOrganizer::getBackground(&document, 0), PDFPageOrganizer::Background::Plain);
}

void PageOrganizerTest::ruledBackgroundIsDrawnUnderContent()
{
    PDFDocument document = createDocument();
    const std::vector<PDFObjectReference> originalContents = contentReferences(document, 0);

    PDFModifiedDocument::ModificationFlags flags;
    QVERIFY(PDFPageOrganizer::setBackground(&document, { 0 }, PDFPageOrganizer::Background::Ruled, &flags));
    QVERIFY(flags.testFlag(PDFModifiedDocument::PageContents));
    QCOMPARE(PDFPageOrganizer::getBackground(&document, 0), PDFPageOrganizer::Background::Ruled);
    QCOMPARE(PDFPageOrganizer::getBackground(&document, 1), PDFPageOrganizer::Background::Plain);

    // Background is the first content stream, the content of the page follows unchanged
    const std::vector<PDFObjectReference> contents = contentReferences(document, 0);
    QCOMPARE(contents.size(), originalContents.size() + 1);
    QVERIFY(std::equal(originalContents.cbegin(), originalContents.cend(), contents.cbegin() + 1));

    const QImage image = renderPage(document, 0);
    const int lines = countRuledLines(image, Qt::Horizontal);
    const int expectedLines = int((842.0 - (25.0 + 12.0) * PDF_MM_TO_POINT) / (PDFPageOrganizer::RULED_LINE_SPACING_MM * PDF_MM_TO_POINT)) + 1;
    QVERIFY2(qAbs(lines - expectedLines) <= 1, qPrintable(QString("lines %1, expected %2").arg(lines).arg(expectedLines)));
    QCOMPARE(countRuledLines(image, Qt::Vertical), 0);

    // The content of the page (square drawn in the coordinates of Qt) is drawn over the background
    // (the ruled lines cross the square, they must not be visible inside of it)
    for (int y = 302; y < 398; ++y)
    {
        for (int x = 202; x < 298; ++x)
        {
            QCOMPARE(image.pixelColor(x, y), QColor(Qt::black));
        }
    }
}

void PageOrganizerTest::backgroundIsInTheBackgroundLayer()
{
    // A document with a note layer
    PDFDocument document = createDocument();
    PDFObjectReference notes;
    {
        PDFDocumentBuilder builder(&document);
        notes = builder.createOptionalContentGroup("Notes");
        document = builder.build();
    }

    QVERIFY(PDFPageOrganizer::setBackground(&document, { 0, 1 }, PDFPageOrganizer::Background::Ruled, nullptr));

    // One layer of the backgrounds, at the bottom of the list
    const PDFOptionalContentProperties* properties = document.getCatalog()->getOptionalContentProperties();
    const PDFObjectReference background = properties->findOptionalContentGroup(PDFPageOrganizer::BACKGROUND_LAYER_NAME);
    QVERIFY(background.isValid());
    QCOMPARE(properties->getAllOptionalContentGroups().size(), size_t(2));
    QCOMPARE(PDFDocumentBuilder(&document).getOptionalContentGroupsInOrder(), (std::vector<PDFObjectReference>{ notes, background }));

    for (PDFInteger pageIndex : { 0, 1 })
    {
        // The background is marked as the content of the layer
        const PDFObjectReference stream = PDFPageOrganizer::getBackgroundStream(&document, pageIndex);
        QVERIFY(stream.isValid());
        QCOMPARE(contentReferences(document, pageIndex).front(), stream);
        const QByteArray content = document.getDecodedStream(document.getObjectByReference(stream).getStream());
        QVERIFY(content.startsWith("/OC /InkpageBackground BDC\n"));
        QVERIFY(content.endsWith("EMC\n"));

        // The name of the layer is in the resources of the page
        const PDFDictionary* resources = document.getDictionaryFromObject(document.getCatalog()->getPage(pageIndex)->getResources());
        QVERIFY(resources);
        const PDFDictionary* layers = document.getDictionaryFromObject(resources->get("Properties"));
        QVERIFY(layers);
        QVERIFY(layers->get(PDFPageOrganizer::BACKGROUND_PROPERTY_NAME).isReference());
        QCOMPARE(layers->get(PDFPageOrganizer::BACKGROUND_PROPERTY_NAME).getReference(), background);
    }

    // The background is not drawn, when its layer is hidden (the content of the page is)
    QVERIFY(countRuledLines(renderPage(document, 0), Qt::Horizontal) > 10);
    const QImage hidden = renderPage(document, 0, background);
    QCOMPARE(countRuledLines(hidden, Qt::Horizontal), 0);
    QCOMPARE(hidden.pixelColor(250, 350), QColor(Qt::black));

    // Another background uses the same layer
    QVERIFY(PDFPageOrganizer::setBackground(&document, { 0 }, PDFPageOrganizer::Background::Graph, nullptr));
    QCOMPARE(document.getCatalog()->getOptionalContentProperties()->getAllOptionalContentGroups().size(), size_t(2));
    QCOMPARE(PDFPageOrganizer::getBackground(&document, 0), PDFPageOrganizer::Background::Graph);
}

void PageOrganizerTest::plainBackgroundRemovesRuledBackground()
{
    PDFDocument document = createDocument();
    const std::vector<PDFObjectReference> originalContents = contentReferences(document, 0);
    const QImage originalImage = renderPage(document, 0);

    QVERIFY(PDFPageOrganizer::setBackground(&document, { 0, 1, 2 }, PDFPageOrganizer::Background::Ruled, nullptr));
    QVERIFY(PDFPageOrganizer::setBackground(&document, { 0, 1, 2 }, PDFPageOrganizer::Background::Plain, nullptr));

    for (PDFInteger i = 0; i < 3; ++i)
    {
        QCOMPARE(PDFPageOrganizer::getBackground(&document, i), PDFPageOrganizer::Background::Plain);
    }
    QCOMPARE(contentReferences(document, 0), originalContents);
    QVERIFY(contentReferences(document, 1).empty());
    QCOMPARE(renderPage(document, 0), originalImage);

    const PDFObjectStorage& storage = document.getStorage();
    const PDFDictionary* page = storage.getDictionaryFromObject(storage.getObjectByReference(document.getCatalog()->getPage(0)->getPageReference()));
    QVERIFY(!page->hasKey("PieceInfo"));

    // Plain background on a page without a background does not modify the document
    PDFModifiedDocument::ModificationFlags flags;
    QVERIFY(PDFPageOrganizer::setBackground(&document, { 0 }, PDFPageOrganizer::Background::Plain, &flags));
    QCOMPARE(flags, PDFModifiedDocument::ModificationFlags());
}

void PageOrganizerTest::ruledBackgroundIsSetOnlyOnce()
{
    PDFDocument document = createDocument();
    QVERIFY(PDFPageOrganizer::setBackground(&document, { 0 }, PDFPageOrganizer::Background::Ruled, nullptr));
    const std::vector<PDFObjectReference> contents = contentReferences(document, 0);

    PDFModifiedDocument::ModificationFlags flags;
    QVERIFY(PDFPageOrganizer::setBackground(&document, { 0 }, PDFPageOrganizer::Background::Ruled, &flags));
    QCOMPARE(flags, PDFModifiedDocument::ModificationFlags());
    QCOMPARE(contentReferences(document, 0), contents);
}

void PageOrganizerTest::ruledBackgroundFollowsPageRotation()
{
    for (PageRotation rotation : { PageRotation::Rotate90, PageRotation::Rotate180, PageRotation::Rotate270 })
    {
        PDFDocumentBuilder builder;
        const PDFObjectReference page = builder.appendPage(A4);
        builder.setPageRotation(page, rotation);
        PDFDocument document = builder.build();

        QVERIFY(PDFPageOrganizer::setBackground(&document, { 0 }, PDFPageOrganizer::Background::Ruled, nullptr));

        // Lines are horizontal on the displayed page
        const QImage image = renderPage(document, 0);
        QVERIFY2(countRuledLines(image, Qt::Horizontal) > 10, qPrintable(QString("rotation %1").arg(int(rotation))));
        QCOMPARE(countRuledLines(image, Qt::Vertical), 0);

        // Margin line is on the left side of the displayed page, lines start at the top margin
        auto isMarginColor = [&image](int x, int y) { const QColor color = image.pixelColor(x, y); return color.red() - color.blue() > 30; };
        const int marginX = qRound(25.0 * PDF_MM_TO_POINT);
        QVERIFY(isMarginColor(marginX, image.height() / 2) || isMarginColor(marginX - 1, image.height() / 2) || isMarginColor(marginX + 1, image.height() / 2));
        QCOMPARE(countRuledLines(image.copy(0, 0, image.width(), qRound(20.0 * PDF_MM_TO_POINT)), Qt::Horizontal), 0);
    }
}

void PageOrganizerTest::blankPageTakesBackgroundOfPage()
{
    PDFDocument document = createDocument();
    QVERIFY(PDFPageOrganizer::setBackground(&document, { 0 }, PDFPageOrganizer::Background::Ruled, nullptr));
    QVERIFY(PDFPageOrganizer::addBlankPage(&document, 0, nullptr));

    QCOMPARE(PDFPageOrganizer::getBackground(&document, 1), PDFPageOrganizer::Background::Ruled);
    QCOMPARE(contentReferences(document, 1).size(), size_t(1));
    QCOMPARE(contentReferences(document, 1).front(), contentReferences(document, 0).front());
    QVERIFY(countRuledLines(renderPage(document, 1), Qt::Horizontal) > 10);

    // Background of the blank page can be removed independently
    QVERIFY(PDFPageOrganizer::setBackground(&document, { 1 }, PDFPageOrganizer::Background::Plain, nullptr));
    QCOMPARE(PDFPageOrganizer::getBackground(&document, 0), PDFPageOrganizer::Background::Ruled);
    QCOMPARE(PDFPageOrganizer::getBackground(&document, 1), PDFPageOrganizer::Background::Plain);
}

void PageOrganizerTest::xournalBackgroundsAreStoredAndDrawn()
{
    QCOMPARE(PDFPageOrganizer::getBackgrounds().size(), size_t(12));
    QCOMPARE(PDFPageOrganizer::getBackgrounds().front(), PDFPageOrganizer::Background::Plain);

    auto countColoredPixels = [](const QImage& image)
    {
        int coloredPixels = 0;
        for (int y = 0; y < image.height(); ++y)
        {
            for (int x = 0; x < image.width(); ++x)
            {
                coloredPixels += image.pixelColor(x, y) != QColor(Qt::white) ? 1 : 0;
            }
        }
        return coloredPixels;
    };

    // The page has a content (a black square), the background adds to it
    const int contentPixels = countColoredPixels(renderPage(createDocument(1), 0));

    for (PDFPageOrganizer::Background background : PDFPageOrganizer::getBackgrounds())
    {
        if (background == PDFPageOrganizer::Background::Plain)
        {
            continue;
        }

        const QString name = QString::fromLatin1(PDFPageOrganizer::getBackgroundName(background));
        PDFDocument document = createDocument(1);
        QVERIFY(PDFPageOrganizer::setBackground(&document, { 0 }, background, nullptr));
        QCOMPARE(PDFPageOrganizer::getBackground(&document, 0), background);

        // The background is drawn (also the dots)
        const int coloredPixels = countColoredPixels(renderPage(document, 0)) - contentPixels;
        QVERIFY2(coloredPixels > 1000, qPrintable(QString("%1: %2 pixels").arg(name).arg(coloredPixels)));

        // The background can be replaced and removed
        QVERIFY(PDFPageOrganizer::setBackground(&document, { 0 }, PDFPageOrganizer::Background::Ruled, nullptr));
        QCOMPARE(PDFPageOrganizer::getBackground(&document, 0), PDFPageOrganizer::Background::Ruled);
        QVERIFY(PDFPageOrganizer::setBackground(&document, { 0 }, PDFPageOrganizer::Background::Plain, nullptr));
        QCOMPARE(PDFPageOrganizer::getBackground(&document, 0), PDFPageOrganizer::Background::Plain);
    }
}

void PageOrganizerTest::xournalBackgroundsGeometry()
{
    // The numbers of lines and dots on an A4 page, as Xournal++ draws them
    PDFDocument document = createDocument(1);
    const PDFPage* page = document.getCatalog()->getPage(0);
    auto segmentCount = [page](PDFPageOrganizer::Background background)
    {
        return int(PDFPageOrganizer::createBackgroundContent(page, background).count(" l\n"));
    };

    // Lines every 24 pt between the header (80 pt) and the footer (60 pt): (842 - 140) / 24
    QCOMPARE(segmentCount(PDFPageOrganizer::Background::Lines), 30);
    QCOMPARE(segmentCount(PDFPageOrganizer::Background::LinesMargin), 31);
    QCOMPARE(segmentCount(PDFPageOrganizer::Background::LinesRightMargin), 31);
    QVERIFY(PDFPageOrganizer::createBackgroundContent(page, PDFPageOrganizer::Background::LinesMargin).contains("72 0 m 72 842 l"));
    QVERIFY(PDFPageOrganizer::createBackgroundContent(page, PDFPageOrganizer::Background::LinesRightMargin).contains("523 0 m 523 842 l"));

    // Squares of 5 mm (14.17 pt): 41 vertical and 59 horizontal lines, 41 x 59 dots
    QCOMPARE(segmentCount(PDFPageOrganizer::Background::Graph), 41 + 59);
    QCOMPARE(segmentCount(PDFPageOrganizer::Background::Dotted), 41 * 59);

    // With the border of 40 pt, the grid is rounded to the squares inside the border
    const QByteArray border = PDFPageOrganizer::createBackgroundContent(page, PDFPageOrganizer::Background::GraphBorder);
    QCOMPARE(int(border.count(" l\n")), 37 + 54);
    QVERIFY(border.contains("42.51 42.51 m 552.63 42.51 l"));

    // Bold lines: two line widths
    const QByteArray bold = PDFPageOrganizer::createBackgroundContent(page, PDFPageOrganizer::Background::GraphBorderBold);
    QVERIFY(bold.contains(" 0.5 w 2 J"));
    QVERIFY(bold.contains(" 1 w 2 J"));

    // 12 staves of 5 lines and 2 bar lines
    QCOMPARE(segmentCount(PDFPageOrganizer::Background::Staves), 12 * 7);

    // Xournal++ coordinates: origin in the top left corner, y axis pointing down
    QVERIFY(PDFPageOrganizer::createBackgroundContent(page, PDFPageOrganizer::Background::Lines).contains("1 0 0 -1 0 842 cm"));
}

void PageOrganizerTest::xournalBackgroundFollowsPageRotation()
{
    for (PageRotation rotation : { PageRotation::None, PageRotation::Rotate90, PageRotation::Rotate180, PageRotation::Rotate270 })
    {
        PDFDocumentBuilder builder;
        const PDFObjectReference page = builder.appendPage(A4);
        builder.setPageRotation(page, rotation);
        PDFDocument document = builder.build();

        QVERIFY(PDFPageOrganizer::setBackground(&document, { 0 }, PDFPageOrganizer::Background::Lines, nullptr));

        // Lines are horizontal on the displayed page, the header (80 pt) is on its top
        const QImage image = renderPage(document, 0);
        QVERIFY2(countRuledLines(image, Qt::Horizontal) > 10, qPrintable(QString("rotation %1").arg(int(rotation))));
        QCOMPARE(countRuledLines(image, Qt::Vertical), 0);
        QCOMPARE(countRuledLines(image.copy(0, 0, image.width(), 75), Qt::Horizontal), 0);
        QVERIFY(countRuledLines(image.copy(0, 75, image.width(), 10), Qt::Horizontal) > 0);
    }
}

QTEST_MAIN(PageOrganizerTest)

#include "tst_pageorganizertest.moc"
