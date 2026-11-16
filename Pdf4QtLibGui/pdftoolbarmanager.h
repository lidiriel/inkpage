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

#ifndef PDFTOOLBARMANAGER_H
#define PDFTOOLBARMANAGER_H

#include "pdfviewerglobal.h"

#include <QObject>
#include <QPointer>
#include <QStringList>

#include <functional>
#include <map>
#include <vector>

class QAction;
class QIcon;
class QSettings;
class QToolBar;
class QWidget;
class QWidgetAction;

namespace pdfviewer
{

/// Content of the customizable toolbars. The content of a toolbar (its layout) is
/// a list of item ids: the object name of an action, the object name of a widget
/// item (a widget wrapped in an action owned by the manager, such as the page
/// number spin box), or the separator id. The default content of a toolbar is
/// its content when it is registered; only the toolbars which differ from their
/// default content are stored in the settings.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFToolBarManager : public QObject
{
    Q_OBJECT

private:
    using BaseClass = QObject;

public:
    explicit PDFToolBarManager(QObject* parent);

    /// Returns all actions which can be put in a toolbar (searched by object name)
    using ActionProvider = std::function<std::vector<QAction*>()>;

    static QString getSeparatorId() { return QStringLiteral("separator"); }
    static QString getSettingsGroup() { return QStringLiteral("ToolBars"); }

    void setActionProvider(ActionProvider provider) { m_actionProvider = std::move(provider); }

    /// Creates an item from a widget, which is not an action (page number spin box,
    /// tool button with a popup...). The returned action is owned by the manager
    /// and keeps the widget when it is removed from a toolbar.
    /// \param id Item id (object name of the action)
    /// \param text Name of the item, displayed in the toolbar manager
    /// \param icon Icon of the item, displayed in the toolbar manager
    /// \param widget Widget of the item
    QWidgetAction* createWidgetItem(const QString& id, const QString& text, const QIcon& icon, QWidget* widget);

    /// Registers a toolbar, its current content becomes its default content
    void registerToolBar(QToolBar* toolBar);

    std::vector<QToolBar*> getToolBars() const;
    std::vector<QAction*> getWidgetItems() const;

    /// Returns the item (action or widget item) with the given id, or nullptr
    QAction* findItem(const QString& id) const;

    QStringList getDefaultLayout(const QToolBar* toolBar) const;

    /// Returns the current content of the toolbar
    static QStringList getLayout(const QToolBar* toolBar);

    /// Replaces the content of the toolbar. Unknown ids, items already in the
    /// toolbar and widget items used in another toolbar are skipped.
    void setLayout(QToolBar* toolBar, const QStringList& layout);

    /// Restores the default content of the toolbar
    void resetLayout(QToolBar* toolBar);

    /// Applies the layouts stored in the settings to the registered toolbars
    void readSettings(QSettings& settings);

    /// Stores the layouts of the registered toolbars which differ from their default content
    void writeSettings(QSettings& settings) const;

private:
    struct ToolBarInfo
    {
        QPointer<QToolBar> toolBar;
        QStringList defaultLayout;
    };

    const ToolBarInfo* getToolBarInfo(const QToolBar* toolBar) const;
    static QString getItemId(const QAction* action);
    bool isUsedByAnotherToolBar(const QAction* action, const QToolBar* toolBar) const;

    ActionProvider m_actionProvider;
    std::vector<ToolBarInfo> m_toolBars;
    std::vector<QWidgetAction*> m_widgetItems;

    /// Actions of the default contents, they can be restored even when the
    /// action provider does not return them
    std::map<QString, QPointer<QAction>> m_defaultActions;
};

}   // namespace pdfviewer

#endif // PDFTOOLBARMANAGER_H
