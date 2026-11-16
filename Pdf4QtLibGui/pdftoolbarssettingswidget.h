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

#ifndef PDFTOOLBARSSETTINGSWIDGET_H
#define PDFTOOLBARSSETTINGSWIDGET_H

#include "pdfviewerglobal.h"

#include <QPointer>
#include <QStringList>
#include <QWidget>

#include <map>
#include <vector>

class QAction;
class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMenu;
class QMenuBar;
class QPushButton;
class QToolBar;

namespace pdfviewer
{
class PDFToolBarManager;

/// Page "UI - Toolbars" of the options: edits the content of the toolbars of the
/// toolbar manager. On the left, the actions of a menu (or all actions), on the
/// right, the content of the selected toolbar. The edited content is applied to
/// the toolbars only by \ref apply.
class PDF4QTLIBGUILIBSHARED_EXPORT PDFToolBarsSettingsWidget : public QWidget
{
    Q_OBJECT

private:
    using BaseClass = QWidget;

public:
    /// \param manager Toolbar manager
    /// \param menuBar Menu bar, its menus and submenus are the categories of the actions
    /// \param parent Parent widget
    explicit PDFToolBarsSettingsWidget(PDFToolBarManager* manager, const QMenuBar* menuBar, QWidget* parent);

    /// Edited content of the toolbars
    const std::map<QToolBar*, QStringList>& getLayouts() const { return m_layouts; }

    /// Applies the edited content to the toolbars and stores it in the settings
    void apply();

    /// Returns the text of the action without the mnemonic
    static QString getDisplayText(const QAction* action);

private:
    struct Category
    {
        QString name;
        QStringList ids;
    };

    void addMenuCategory(const QMenu* menu, const QString& name);
    bool isAvailableAction(const QAction* action) const;
    QToolBar* getCurrentToolBar() const;
    QStringList& getCurrentLayout();
    QString getToolBarUsingItem(const QString& id) const;
    QListWidgetItem* createItem(const QString& id) const;
    void addItems(const QStringList& ids);
    void insertIntoCurrentLayout(const QStringList& ids);

    void updateAvailableList();
    void updateToolBarList(int currentRow);
    void updateButtons();

    void onAdd();
    void onRemove();
    void onMoveUp();
    void onMoveDown();
    void onInsertSeparator();
    void onReset();

    QPointer<PDFToolBarManager> m_manager;
    std::vector<Category> m_categories;
    std::map<QToolBar*, QStringList> m_layouts;

    QComboBox* m_categoryComboBox;
    QLineEdit* m_filterEdit;
    QListWidget* m_availableList;
    QComboBox* m_toolBarComboBox;
    QListWidget* m_toolBarList;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_moveUpButton;
    QPushButton* m_moveDownButton;
    QPushButton* m_separatorButton;
    QPushButton* m_resetButton;
};

}   // namespace pdfviewer

#endif // PDFTOOLBARSSETTINGSWIDGET_H
