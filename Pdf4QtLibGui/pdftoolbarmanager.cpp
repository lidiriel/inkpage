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

#include "pdftoolbarmanager.h"

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QSet>
#include <QSettings>
#include <QToolBar>
#include <QToolButton>
#include <QWidgetAction>

#include "pdfdbgheap.h"

namespace pdfviewer
{

// The layout is stored as one string: an empty toolbar is then an empty string,
// distinct from a missing key (default content).
static const QChar LAYOUT_SEPARATOR = QLatin1Char(';');

PDFToolBarManager::PDFToolBarManager(QObject* parent) :
    BaseClass(parent)
{

}

QWidgetAction* PDFToolBarManager::createWidgetItem(const QString& id, const QString& text, const QIcon& icon, QWidget* widget)
{
    // The default widget of a QWidgetAction is only hidden when the action is
    // removed from a toolbar (QToolBar::addWidget would create an action
    // owned by the toolbar instead).
    QWidgetAction* action = new QWidgetAction(this);
    action->setObjectName(id);
    action->setText(text);
    action->setIcon(icon);
    action->setDefaultWidget(widget);
    m_widgetItems.push_back(action);
    return action;
}

void PDFToolBarManager::registerToolBar(QToolBar* toolBar)
{
    Q_ASSERT(toolBar);
    Q_ASSERT(!toolBar->objectName().isEmpty());
    Q_ASSERT(!getToolBarInfo(toolBar));

    ToolBarInfo info;
    info.toolBar = toolBar;
    info.defaultLayout = getLayout(toolBar);
    for (QAction* action : toolBar->actions())
    {
        if (!action->isSeparator())
        {
            m_defaultActions[action->objectName()] = action;
        }
    }
    m_toolBars.push_back(std::move(info));
}

std::vector<QToolBar*> PDFToolBarManager::getToolBars() const
{
    std::vector<QToolBar*> result;
    for (const ToolBarInfo& info : m_toolBars)
    {
        if (info.toolBar)
        {
            result.push_back(info.toolBar);
        }
    }
    return result;
}

std::vector<QAction*> PDFToolBarManager::getWidgetItems() const
{
    return std::vector<QAction*>(m_widgetItems.cbegin(), m_widgetItems.cend());
}

QAction* PDFToolBarManager::findItem(const QString& id) const
{
    if (id.isEmpty() || id == getSeparatorId())
    {
        return nullptr;
    }

    for (QWidgetAction* action : m_widgetItems)
    {
        if (action->objectName() == id)
        {
            return action;
        }
    }

    auto it = m_defaultActions.find(id);
    if (it != m_defaultActions.cend() && it->second)
    {
        return it->second;
    }

    if (m_actionProvider)
    {
        for (QAction* action : m_actionProvider())
        {
            if (action && action->objectName() == id && !action->isSeparator())
            {
                return action;
            }
        }
    }

    return nullptr;
}

QStringList PDFToolBarManager::getDefaultLayout(const QToolBar* toolBar) const
{
    const ToolBarInfo* info = getToolBarInfo(toolBar);
    return info ? info->defaultLayout : QStringList();
}

QStringList PDFToolBarManager::getLayout(const QToolBar* toolBar)
{
    QStringList layout;
    for (const QAction* action : toolBar->actions())
    {
        const QString id = getItemId(action);
        if (!id.isEmpty())
        {
            layout << id;
        }
    }
    return layout;
}

void PDFToolBarManager::setLayout(QToolBar* toolBar, const QStringList& layout)
{
    Q_ASSERT(toolBar);

    // The items are removed one by one: QToolBar::clear() would delete the widgets
    // of the actions created by QToolBar::addWidget. The separators belong to the toolbar.
    for (QAction* action : toolBar->actions())
    {
        toolBar->removeAction(action);
        if (action->isSeparator() && action->parent() == toolBar)
        {
            delete action;
        }
    }

    QSet<QAction*> addedActions;
    for (const QString& id : layout)
    {
        if (id == getSeparatorId())
        {
            toolBar->addSeparator();
            continue;
        }

        QAction* action = findItem(id);
        if (!action || addedActions.contains(action) || isUsedByAnotherToolBar(action, toolBar))
        {
            continue;
        }

        toolBar->addAction(action);
        addedActions.insert(action);

        // An action which only opens a submenu (page background) opens it at once
        if (action->menu() && action->menu()->menuAction() == action)
        {
            if (QToolButton* button = qobject_cast<QToolButton*>(toolBar->widgetForAction(action)))
            {
                button->setPopupMode(QToolButton::InstantPopup);
            }
        }
    }
}

void PDFToolBarManager::setLayouts(const std::map<QToolBar*, QStringList>& layouts)
{
    // The changed toolbars are emptied first, so their widget items are free
    std::vector<std::pair<QToolBar*, QStringList>> changedLayouts;
    for (const auto& [toolBar, layout] : layouts)
    {
        if (toolBar && getLayout(toolBar) != layout)
        {
            setLayout(toolBar, QStringList());
            changedLayouts.emplace_back(toolBar, layout);
        }
    }

    for (const auto& [toolBar, layout] : changedLayouts)
    {
        setLayout(toolBar, layout);
    }
}

void PDFToolBarManager::resetLayout(QToolBar* toolBar)
{
    if (const ToolBarInfo* info = getToolBarInfo(toolBar))
    {
        setLayout(toolBar, info->defaultLayout);
    }
}

void PDFToolBarManager::readSettings(QSettings& settings)
{
    settings.beginGroup(getSettingsGroup());
    for (QToolBar* toolBar : getToolBars())
    {
        const QString key = toolBar->objectName();
        if (settings.contains(key))
        {
            const QString value = settings.value(key).toString();
            setLayout(toolBar, value.isEmpty() ? QStringList() : value.split(LAYOUT_SEPARATOR, Qt::SkipEmptyParts));
        }
    }
    settings.endGroup();
}

void PDFToolBarManager::writeSettings(QSettings& settings) const
{
    settings.beginGroup(getSettingsGroup());
    for (const ToolBarInfo& info : m_toolBars)
    {
        if (!info.toolBar)
        {
            continue;
        }

        const QString key = info.toolBar->objectName();
        const QStringList layout = getLayout(info.toolBar);
        if (layout == info.defaultLayout)
        {
            settings.remove(key);
        }
        else
        {
            settings.setValue(key, layout.join(LAYOUT_SEPARATOR));
        }
    }
    settings.endGroup();
}

const PDFToolBarManager::ToolBarInfo* PDFToolBarManager::getToolBarInfo(const QToolBar* toolBar) const
{
    for (const ToolBarInfo& info : m_toolBars)
    {
        if (info.toolBar == toolBar)
        {
            return &info;
        }
    }
    return nullptr;
}

QString PDFToolBarManager::getItemId(const QAction* action)
{
    return action->isSeparator() ? getSeparatorId() : action->objectName();
}

bool PDFToolBarManager::isUsedByAnotherToolBar(const QAction* action, const QToolBar* toolBar) const
{
    // A widget can be displayed only once, the actions can be in several toolbars
    if (!qobject_cast<const QWidgetAction*>(action))
    {
        return false;
    }

    for (QToolBar* otherToolBar : getToolBars())
    {
        if (otherToolBar != toolBar && otherToolBar->actions().contains(const_cast<QAction*>(action)))
        {
            return true;
        }
    }
    return false;
}

}   // namespace pdfviewer
