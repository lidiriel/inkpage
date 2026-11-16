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

#include "pdftoolbarssettingswidget.h"
#include "pdftoolbarmanager.h"
#include "pdfwidgetutils.h"

#include <QAction>
#include <QComboBox>
#include <QCoreApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QStyle>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "pdfdbgheap.h"

namespace pdfviewer
{

PDFToolBarsSettingsWidget::PDFToolBarsSettingsWidget(PDFToolBarManager* manager, const QMenuBar* menuBar, QWidget* parent) :
    BaseClass(parent),
    m_manager(manager),
    m_categoryComboBox(new QComboBox(this)),
    m_filterEdit(new QLineEdit(this)),
    m_availableList(new QListWidget(this)),
    m_toolBarComboBox(new QComboBox(this)),
    m_toolBarList(new QListWidget(this)),
    m_addButton(new QPushButton(style()->standardIcon(QStyle::SP_ArrowRight), tr("&Add"), this)),
    m_removeButton(new QPushButton(style()->standardIcon(QStyle::SP_ArrowLeft), tr("&Remove"), this)),
    m_moveUpButton(new QPushButton(style()->standardIcon(QStyle::SP_ArrowUp), tr("Move &Up"), this)),
    m_moveDownButton(new QPushButton(style()->standardIcon(QStyle::SP_ArrowDown), tr("Move &Down"), this)),
    m_separatorButton(new QPushButton(tr("&Separator"), this)),
    m_resetButton(new QPushButton(tr("Rese&t"), this))
{
    Q_ASSERT(manager);

    setObjectName("toolBarsSettingsWidget");
    m_categoryComboBox->setObjectName("toolBarsCategoryComboBox");
    m_filterEdit->setObjectName("toolBarsFilterEdit");
    m_availableList->setObjectName("toolBarsAvailableList");
    m_toolBarComboBox->setObjectName("toolBarsToolBarComboBox");
    m_toolBarList->setObjectName("toolBarsToolBarList");
    m_addButton->setObjectName("toolBarsAddButton");
    m_removeButton->setObjectName("toolBarsRemoveButton");
    m_moveUpButton->setObjectName("toolBarsMoveUpButton");
    m_moveDownButton->setObjectName("toolBarsMoveDownButton");
    m_separatorButton->setObjectName("toolBarsSeparatorButton");
    m_resetButton->setObjectName("toolBarsResetButton");

    m_addButton->setToolTip(tr("Add the selected actions to the toolbar"));
    m_removeButton->setToolTip(tr("Remove the selected item from the toolbar"));
    m_separatorButton->setToolTip(tr("Insert a separator after the selected item"));
    m_resetButton->setToolTip(tr("Restore the default content of the toolbar"));
    m_filterEdit->setPlaceholderText(tr("Search"));
    m_filterEdit->setClearButtonEnabled(true);
    m_availableList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_toolBarList->setSelectionMode(QAbstractItemView::SingleSelection);

    // Icons of the size of the toolbar icons
    const QSize iconSize = pdf::PDFWidgetUtils::scaleDPI(this, QSize(24, 24));
    m_availableList->setIconSize(iconSize);
    m_toolBarList->setIconSize(iconSize);

    // Categories: all actions, the menus and submenus, then the items which are not actions
    m_categories.push_back({ tr("All"), QStringList() });
    if (menuBar)
    {
        for (const QAction* action : menuBar->actions())
        {
            if (const QMenu* menu = action->menu(); menu && action->isVisible())
            {
                addMenuCategory(menu, getDisplayText(action));
            }
        }
    }

    Category widgetsCategory{ tr("Other Items"), QStringList() };
    for (const QAction* action : m_manager->getWidgetItems())
    {
        widgetsCategory.ids << action->objectName();
    }
    m_categories.push_back(std::move(widgetsCategory));

    QSet<QString> allIds;
    auto addToAll = [this, &allIds](const QString& id)
    {
        if (!allIds.contains(id))
        {
            allIds.insert(id);
            m_categories.front().ids << id;
        }
    };
    for (size_t i = 1; i < m_categories.size(); ++i)
    {
        for (const QString& id : m_categories[i].ids)
        {
            addToAll(id);
        }
    }

    // Actions of the toolbars which are in no menu
    for (QToolBar* toolBar : m_manager->getToolBars())
    {
        m_layouts[toolBar] = PDFToolBarManager::getLayout(toolBar);
        for (const QString& id : m_manager->getDefaultLayout(toolBar) + m_layouts[toolBar])
        {
            if (id != PDFToolBarManager::getSeparatorId() && m_manager->findItem(id))
            {
                addToAll(id);
            }
        }

        m_toolBarComboBox->addItem(toolBar->windowTitle());
    }

    std::erase_if(m_categories, [](const Category& category) { return category.ids.isEmpty(); });
    for (const Category& category : m_categories)
    {
        m_categoryComboBox->addItem(category.name);
    }

    // Layout: actions | add, remove | toolbar
    QVBoxLayout* availableLayout = new QVBoxLayout();
    QLabel* availableLabel = new QLabel(tr("A&ctions"), this);
    availableLabel->setBuddy(m_categoryComboBox);
    availableLayout->addWidget(availableLabel);
    availableLayout->addWidget(m_categoryComboBox);
    availableLayout->addWidget(m_filterEdit);
    availableLayout->addWidget(m_availableList, 1);

    QVBoxLayout* transferLayout = new QVBoxLayout();
    transferLayout->addStretch(1);
    transferLayout->addWidget(m_addButton);
    transferLayout->addWidget(m_removeButton);
    transferLayout->addStretch(1);

    QVBoxLayout* toolBarLayout = new QVBoxLayout();
    QLabel* toolBarLabel = new QLabel(tr("Tool&bar"), this);
    toolBarLabel->setBuddy(m_toolBarComboBox);
    toolBarLayout->addWidget(toolBarLabel);
    toolBarLayout->addWidget(m_toolBarComboBox);
    toolBarLayout->addWidget(m_toolBarList, 1);
    QGridLayout* toolBarButtonsLayout = new QGridLayout();
    toolBarButtonsLayout->addWidget(m_moveUpButton, 0, 0);
    toolBarButtonsLayout->addWidget(m_moveDownButton, 0, 1);
    toolBarButtonsLayout->addWidget(m_separatorButton, 1, 0);
    toolBarButtonsLayout->addWidget(m_resetButton, 1, 1);
    toolBarLayout->addLayout(toolBarButtonsLayout);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(availableLayout, 1);
    layout->addLayout(transferLayout);
    layout->addLayout(toolBarLayout, 1);

    connect(m_categoryComboBox, &QComboBox::currentIndexChanged, this, &PDFToolBarsSettingsWidget::updateAvailableList);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &PDFToolBarsSettingsWidget::updateAvailableList);
    connect(m_toolBarComboBox, &QComboBox::currentIndexChanged, this, [this]() { updateToolBarList(-1); });
    connect(m_availableList, &QListWidget::itemSelectionChanged, this, &PDFToolBarsSettingsWidget::updateButtons);
    connect(m_toolBarList, &QListWidget::itemSelectionChanged, this, &PDFToolBarsSettingsWidget::updateButtons);
    connect(m_availableList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) { addItems({ item->data(Qt::UserRole).toString() }); });
    connect(m_toolBarList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) { m_toolBarList->setCurrentItem(item); onRemove(); });
    connect(m_addButton, &QPushButton::clicked, this, &PDFToolBarsSettingsWidget::onAdd);
    connect(m_removeButton, &QPushButton::clicked, this, &PDFToolBarsSettingsWidget::onRemove);
    connect(m_moveUpButton, &QPushButton::clicked, this, &PDFToolBarsSettingsWidget::onMoveUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &PDFToolBarsSettingsWidget::onMoveDown);
    connect(m_separatorButton, &QPushButton::clicked, this, &PDFToolBarsSettingsWidget::onInsertSeparator);
    connect(m_resetButton, &QPushButton::clicked, this, &PDFToolBarsSettingsWidget::onReset);

    updateToolBarList(-1);
}

void PDFToolBarsSettingsWidget::apply()
{
    if (!m_manager)
    {
        return;
    }

    m_manager->setLayouts(m_layouts);

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    m_manager->writeSettings(settings);
}

QString PDFToolBarsSettingsWidget::getDisplayText(const QAction* action)
{
    // "&&" is a literal ampersand, a single "&" marks the mnemonic
    QString text = action->text();
    text.replace(QLatin1String("&&"), QString(QChar(0x1)));
    text.remove(QLatin1Char('&'));
    text.replace(QChar(0x1), QLatin1Char('&'));
    return text;
}

void PDFToolBarsSettingsWidget::addMenuCategory(const QMenu* menu, const QString& name)
{
    // The category of the menu is placed before the categories of its submenus
    const size_t index = m_categories.size();
    m_categories.push_back({ name, QStringList() });

    QStringList ids;
    for (const QAction* action : menu->actions())
    {
        if (action->isSeparator() || !action->isVisible())
        {
            continue;
        }

        // An action of a submenu can be in a toolbar (page background)
        if (isAvailableAction(action))
        {
            ids << action->objectName();
        }

        if (const QMenu* submenu = action->menu())
        {
            addMenuCategory(submenu, QString("%1 > %2").arg(name, getDisplayText(action)));
        }
    }

    m_categories[index].ids = std::move(ids);
}

bool PDFToolBarsSettingsWidget::isAvailableAction(const QAction* action) const
{
    // Only the actions found by the manager can be stored in a toolbar
    return !action->objectName().isEmpty() && !action->text().isEmpty() && m_manager->findItem(action->objectName()) == action;
}

QToolBar* PDFToolBarsSettingsWidget::getCurrentToolBar() const
{
    const std::vector<QToolBar*> toolBars = m_manager ? m_manager->getToolBars() : std::vector<QToolBar*>();
    const int index = m_toolBarComboBox->currentIndex();
    return (index >= 0 && index < int(toolBars.size())) ? toolBars[index] : nullptr;
}

QStringList& PDFToolBarsSettingsWidget::getCurrentLayout()
{
    return m_layouts[getCurrentToolBar()];
}

QString PDFToolBarsSettingsWidget::getToolBarUsingItem(const QString& id) const
{
    for (const auto& [toolBar, layout] : m_layouts)
    {
        if (toolBar && layout.contains(id))
        {
            return toolBar->windowTitle();
        }
    }
    return QString();
}

QListWidgetItem* PDFToolBarsSettingsWidget::createItem(const QString& id) const
{
    QListWidgetItem* item = new QListWidgetItem();
    item->setData(Qt::UserRole, id);

    if (id == PDFToolBarManager::getSeparatorId())
    {
        item->setText(tr("— Separator —"));
    }
    else if (const QAction* action = m_manager->findItem(id))
    {
        item->setIcon(action->icon());
        item->setText(getDisplayText(action));
        item->setToolTip(action->toolTip());
    }
    else
    {
        item->setText(id);
    }

    // The texts of the items without icon are aligned with the other ones
    if (item->icon().isNull())
    {
        QPixmap pixmap(m_toolBarList->iconSize());
        pixmap.fill(Qt::transparent);
        item->setIcon(QIcon(pixmap));
    }

    return item;
}

void PDFToolBarsSettingsWidget::addItems(const QStringList& ids)
{
    QStringList addedIds;
    for (const QString& id : ids)
    {
        // An item is only once in a toolbar, a widget item in only one toolbar
        const bool isWidgetItem = qobject_cast<const QWidgetAction*>(m_manager->findItem(id)) != nullptr;
        if (!getCurrentLayout().contains(id) && !addedIds.contains(id) && (!isWidgetItem || getToolBarUsingItem(id).isEmpty()))
        {
            addedIds << id;
        }
    }

    if (!addedIds.isEmpty())
    {
        insertIntoCurrentLayout(addedIds);
    }
}

void PDFToolBarsSettingsWidget::insertIntoCurrentLayout(const QStringList& ids)
{
    // After the selected item, at the end otherwise
    QStringList& layout = getCurrentLayout();
    const QList<QListWidgetItem*> selectedItems = m_toolBarList->selectedItems();
    int position = selectedItems.isEmpty() ? layout.size() : m_toolBarList->row(selectedItems.front()) + 1;
    for (const QString& id : ids)
    {
        layout.insert(position++, id);
    }

    updateToolBarList(position - 1);
}

void PDFToolBarsSettingsWidget::updateAvailableList()
{
    m_availableList->clear();

    const int categoryIndex = m_categoryComboBox->currentIndex();
    if (!m_manager || categoryIndex < 0 || categoryIndex >= int(m_categories.size()))
    {
        updateButtons();
        return;
    }

    const QString filter = m_filterEdit->text().trimmed();
    const QStringList& layout = getCurrentLayout();
    for (const QString& id : m_categories[categoryIndex].ids)
    {
        QListWidgetItem* item = createItem(id);
        if (!filter.isEmpty() && !item->text().contains(filter, Qt::CaseInsensitive))
        {
            delete item;
            continue;
        }

        // Greyed: already in the toolbar, or a widget displayed in another toolbar
        QString usedBy;
        if (layout.contains(id))
        {
            usedBy = tr("Already in the toolbar");
        }
        else if (qobject_cast<const QWidgetAction*>(m_manager->findItem(id)))
        {
            const QString toolBarName = getToolBarUsingItem(id);
            if (!toolBarName.isEmpty())
            {
                usedBy = tr("Already in the toolbar \"%1\"").arg(toolBarName);
            }
        }

        if (!usedBy.isEmpty())
        {
            item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
            item->setToolTip(usedBy);
        }

        m_availableList->addItem(item);
    }

    updateButtons();
}

void PDFToolBarsSettingsWidget::updateToolBarList(int currentRow)
{
    m_toolBarList->clear();
    for (const QString& id : getCurrentLayout())
    {
        m_toolBarList->addItem(createItem(id));
    }

    if (currentRow >= 0 && currentRow < m_toolBarList->count())
    {
        m_toolBarList->setCurrentRow(currentRow);
        m_toolBarList->scrollToItem(m_toolBarList->item(currentRow));
    }

    updateAvailableList();
}

void PDFToolBarsSettingsWidget::updateButtons()
{
    const QList<QListWidgetItem*> selectedItems = m_toolBarList->selectedItems();
    const int row = selectedItems.isEmpty() ? -1 : m_toolBarList->row(selectedItems.front());
    const bool hasToolBar = getCurrentToolBar() != nullptr;

    m_addButton->setEnabled(hasToolBar && !m_availableList->selectedItems().isEmpty());
    m_removeButton->setEnabled(row >= 0);
    m_moveUpButton->setEnabled(row > 0);
    m_moveDownButton->setEnabled(row >= 0 && row + 1 < m_toolBarList->count());
    m_separatorButton->setEnabled(hasToolBar);
    m_resetButton->setEnabled(hasToolBar && getCurrentLayout() != m_manager->getDefaultLayout(getCurrentToolBar()));
}

void PDFToolBarsSettingsWidget::onAdd()
{
    QStringList ids;
    for (int i = 0; i < m_availableList->count(); ++i)
    {
        QListWidgetItem* item = m_availableList->item(i);
        if (item->isSelected())
        {
            ids << item->data(Qt::UserRole).toString();
        }
    }
    addItems(ids);
}

void PDFToolBarsSettingsWidget::onRemove()
{
    const QList<QListWidgetItem*> selectedItems = m_toolBarList->selectedItems();
    if (selectedItems.isEmpty())
    {
        return;
    }

    QStringList& layout = getCurrentLayout();
    const int row = m_toolBarList->row(selectedItems.front());
    layout.removeAt(row);
    updateToolBarList(qMin(row, int(layout.size()) - 1));
}

void PDFToolBarsSettingsWidget::onMoveUp()
{
    const QList<QListWidgetItem*> selectedItems = m_toolBarList->selectedItems();
    const int row = selectedItems.isEmpty() ? -1 : m_toolBarList->row(selectedItems.front());
    if (row > 0)
    {
        getCurrentLayout().swapItemsAt(row, row - 1);
        updateToolBarList(row - 1);
    }
}

void PDFToolBarsSettingsWidget::onMoveDown()
{
    const QList<QListWidgetItem*> selectedItems = m_toolBarList->selectedItems();
    const int row = selectedItems.isEmpty() ? -1 : m_toolBarList->row(selectedItems.front());
    QStringList& layout = getCurrentLayout();
    if (row >= 0 && row + 1 < layout.size())
    {
        layout.swapItemsAt(row, row + 1);
        updateToolBarList(row + 1);
    }
}

void PDFToolBarsSettingsWidget::onInsertSeparator()
{
    insertIntoCurrentLayout({ PDFToolBarManager::getSeparatorId() });
}

void PDFToolBarsSettingsWidget::onReset()
{
    if (QToolBar* toolBar = getCurrentToolBar())
    {
        // A widget item of the default content can be in another toolbar: it is moved
        QStringList defaultLayout = m_manager->getDefaultLayout(toolBar);
        for (const QString& id : defaultLayout)
        {
            if (qobject_cast<const QWidgetAction*>(m_manager->findItem(id)))
            {
                for (auto& [otherToolBar, layout] : m_layouts)
                {
                    if (otherToolBar != toolBar)
                    {
                        layout.removeAll(id);
                    }
                }
            }
        }

        getCurrentLayout() = std::move(defaultLayout);
        updateToolBarList(-1);
    }
}

}   // namespace pdfviewer
