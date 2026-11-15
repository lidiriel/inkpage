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

#include "pdffontdialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFontDatabase>
#include <QGroupBox>

#include "pdfdbgheap.h"

namespace pdfviewer
{

PDFFontDialog::PDFFontDialog(QWidget* parent) :
    BaseClass(parent),
    m_searchEdit(new QLineEdit(this)),
    m_familyList(new QListWidget(this)),
    m_sizeSpinBox(new QDoubleSpinBox(this)),
    m_sizeList(new QListWidget(this)),
    m_previewLabel(new QLabel(this))
{
    setWindowTitle(tr("Font"));
    setObjectName("fontDialog");

    m_searchEdit->setObjectName("fontSearchEdit");
    m_searchEdit->setPlaceholderText(tr("Search font family"));
    m_searchEdit->setClearButtonEnabled(true);

    m_familyList->setObjectName("fontFamilyList");
    m_familyList->addItems(QFontDatabase::families());

    m_sizeSpinBox->setObjectName("fontSizeSpinBox");
    m_sizeSpinBox->setRange(1.0, 400.0);
    m_sizeSpinBox->setDecimals(1);
    m_sizeSpinBox->setSuffix(tr(" pt"));

    m_sizeList->setObjectName("fontSizeList");
    for (const pdf::PDFReal size : COMMON_SIZES)
    {
        QListWidgetItem* item = new QListWidgetItem(QString::number(size), m_sizeList);
        item->setData(Qt::UserRole, size);
    }
    m_sizeList->setFixedWidth(m_sizeList->fontMetrics().horizontalAdvance(QStringLiteral("0000000")));

    m_previewLabel->setObjectName("fontPreviewLabel");
    m_previewLabel->setText(tr("The quick brown fox jumps over the lazy dog. 0123456789"));
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setMinimumHeight(m_previewLabel->fontMetrics().height() * 4);
    m_previewLabel->setFrameShape(QFrame::StyledPanel);
    m_previewLabel->setAutoFillBackground(true);
    m_previewLabel->setBackgroundRole(QPalette::Base);
    m_previewLabel->setAlignment(Qt::AlignCenter);

    QVBoxLayout* familyLayout = new QVBoxLayout();
    familyLayout->addWidget(new QLabel(tr("Family"), this));
    familyLayout->addWidget(m_searchEdit);
    familyLayout->addWidget(m_familyList, 1);

    QVBoxLayout* sizeLayout = new QVBoxLayout();
    sizeLayout->addWidget(new QLabel(tr("Size"), this));
    sizeLayout->addWidget(m_sizeSpinBox);
    sizeLayout->addWidget(m_sizeList, 1);

    QHBoxLayout* chooserLayout = new QHBoxLayout();
    chooserLayout->addLayout(familyLayout, 1);
    chooserLayout->addLayout(sizeLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(chooserLayout, 1);
    layout->addWidget(new QLabel(tr("Preview"), this));
    layout->addWidget(m_previewLabel);
    layout->addWidget(buttonBox);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &PDFFontDialog::setFilter);
    connect(m_familyList, &QListWidget::currentRowChanged, this, &PDFFontDialog::updatePreview);
    connect(m_familyList, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
    connect(m_sizeSpinBox, &QDoubleSpinBox::valueChanged, this, &PDFFontDialog::updatePreview);
    connect(m_sizeList, &QListWidget::itemClicked, this, &PDFFontDialog::onSizeItemClicked);

    setFontFamily(QStringLiteral("Helvetica"));
    setFontSize(10.0);
    resize(520, 480);
}

PDFFontDialog::~PDFFontDialog()
{

}

QString PDFFontDialog::getFontFamily() const
{
    const QListWidgetItem* item = m_familyList->currentItem();
    return item ? item->text() : QString();
}

void PDFFontDialog::setFontFamily(const QString& fontFamily)
{
    if (fontFamily.isEmpty())
    {
        return;
    }

    QList<QListWidgetItem*> items = m_familyList->findItems(fontFamily, Qt::MatchFixedString);
    if (items.isEmpty())
    {
        // The family of the text is not installed (for example a standard PDF font
        // as Helvetica), it is kept as the first item of the list
        m_familyList->insertItem(0, fontFamily);
        items = m_familyList->findItems(fontFamily, Qt::MatchFixedString);
    }

    m_familyList->setCurrentItem(items.front());
    m_familyList->scrollToItem(items.front(), QAbstractItemView::PositionAtCenter);
    updatePreview();
}

pdf::PDFReal PDFFontDialog::getFontSize() const
{
    return m_sizeSpinBox->value();
}

void PDFFontDialog::setFontSize(pdf::PDFReal fontSize)
{
    m_sizeSpinBox->setValue(fontSize);
    updatePreview();
}

void PDFFontDialog::setFilter(const QString& text)
{
    const QString filter = text.trimmed();
    for (int row = 0; row < m_familyList->count(); ++row)
    {
        QListWidgetItem* item = m_familyList->item(row);
        item->setHidden(!filter.isEmpty() && !item->text().contains(filter, Qt::CaseInsensitive));
    }

    // The chosen family is the first one shown, if the current one is hidden
    QListWidgetItem* currentItem = m_familyList->currentItem();
    if (!currentItem || currentItem->isHidden())
    {
        for (int row = 0; row < m_familyList->count(); ++row)
        {
            if (!m_familyList->item(row)->isHidden())
            {
                m_familyList->setCurrentRow(row);
                break;
            }
        }
    }
}

int PDFFontDialog::getVisibleFamilyCount() const
{
    int count = 0;
    for (int row = 0; row < m_familyList->count(); ++row)
    {
        count += m_familyList->item(row)->isHidden() ? 0 : 1;
    }
    return count;
}

void PDFFontDialog::onSizeItemClicked()
{
    if (const QListWidgetItem* item = m_sizeList->currentItem())
    {
        setFontSize(item->data(Qt::UserRole).toDouble());
    }
}

void PDFFontDialog::updatePreview()
{
    // The size of the preview is the size in points on the screen, limited to stay readable
    QFont font(getFontFamily());
    font.setPointSizeF(qBound(6.0, getFontSize(), 48.0));
    m_previewLabel->setFont(font);

    for (int row = 0; row < m_sizeList->count(); ++row)
    {
        QListWidgetItem* item = m_sizeList->item(row);
        if (qFuzzyCompare(item->data(Qt::UserRole).toDouble(), getFontSize()))
        {
            m_sizeList->setCurrentItem(item);
            return;
        }
    }
    m_sizeList->clearSelection();
}

}   // namespace pdfviewer
