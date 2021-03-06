#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "dialogs/aboutdialog.h"
#include "dialogs/textdialog.h"
#include "windowmanager.h"
#include "propertyeditor.h"
#include "managers/contextmenumanager.h"

#include <QStandardPaths>

#include "widgets/guiAPI/coordinategui.h"
#include "widgets/guiAPI/entitygui.h"
#include "widgets/guiAPI/buttongui.h"

using namespace lc::ui;

MainWindow::MainWindow()
    :
    ui(new Ui::MainWindow),
    linePatternSelect(&_cadMdiChild, this, true, true),
    lineWidthSelect(_cadMdiChild.metaInfoManager(), this, true, true),
    colorSelect(_cadMdiChild.metaInfoManager(), this, true, true),
    _cliCommand(this),
    _toolbar(&_luaInterface, this),
    _layers(nullptr, this)
{
    ContextMenuManager::GetContextMenuManager(this);
    _contextMenuManagerId = ContextMenuManager::GetInstanceId(this);

    ui->setupUi(this);
    // new document and set mainwindow attributes
    _cadMdiChild.newDocument();
    setWindowTitle(QObject::tr("LibreCAD"));
    setUnifiedTitleAndToolBarOnMac(true);
    setCentralWidget(&_cadMdiChild);

    _layers.setMdiChild(&_cadMdiChild);

    // add widgets to correct positions
    addDockWidget(Qt::RightDockWidgetArea, &_layers);
    addDockWidget(Qt::BottomDockWidgetArea, &_cliCommand);
    addDockWidget(Qt::TopDockWidgetArea, &_toolbar);

    _toolbar.initializeToolbar(&linePatternSelect, &lineWidthSelect, &colorSelect);
    _cadMdiChild.viewer()->autoScale();

    initMenuAPI();

    // connect required signals and slots
    ConnectInputEvents();

    // open qt bridge and run lua scripts
    _luaInterface.initLua(this);

    _toolbar.addSnapOptions();

    // add lua script
    kaguya::State state(_luaInterface.luaState());
    state.dostring("run_luascript = function() lc.LuaScript(mainWindow):show() end");
    state.dostring("run_customizetoolbar = function() mainWindow:runCustomizeToolbar() end");
    state["run_aboutdialog"] = kaguya::function([&] {
        auto aboutDialog = new dialog::AboutDialog(this);
        aboutDialog->show();
    });
    state["run_textdialog"] = kaguya::function([&] {
        auto textDialog = new dialog::TextDialog(this, this);
        textDialog->show();
    });

    api::Menu* luaMenu = addMenu("Lua");
    luaMenu->addItem("Run script", state["run_luascript"]);
    luaMenu->addItem("Customize Toolbar", state["run_customizetoolbar"]);

    api::Menu* viewMenu = addMenu("View");
    state.dostring("changeLayout = function() mainWindow:changeDockLayout(1) end");
    viewMenu->addItem("Default Layout 1", state["changeLayout"]);
    state.dostring("changeLayout = function() mainWindow:changeDockLayout(2) end");
    viewMenu->addItem("Default Layout 2", state["changeLayout"]);
    state.dostring("changeLayout = function() mainWindow:changeDockLayout(3) end");
    viewMenu->addItem("Default Layout 3", state["changeLayout"]);
    state.dostring("changeLayout = function() mainWindow:loadDockLayout() end");
    viewMenu->addItem("Load Dock Layout", state["changeLayout"]);
    state.dostring("changeLayout = function() mainWindow:saveDockLayout() end");
    viewMenu->addItem("Save Dock Layout", state["changeLayout"]);

    api::Menu* aboutMenu = addMenu("About");
    aboutMenu->addItem("About", state["run_aboutdialog"]);

    api::Menu* textMenu = menuByName("Create")->menuByName("Text");
    if (textMenu != nullptr) {
        textMenu->addItem("Text Dialog", state["run_textdialog"]);
    }

    _toolbar.generateButtonsMap();
    readUiSettings();

    _cadMdiChild.viewer()->setContextMenuManagerId(_contextMenuManagerId);

    PropertyEditor* propertyEditor = PropertyEditor::GetPropertyEditor(this);
    this->addDockWidget(Qt::BottomDockWidgetArea, propertyEditor);

    this->resizeDocks({ &_cliCommand, propertyEditor }, { 65, 35 }, Qt::Horizontal);

    /*QStringList fontPaths = QStandardPaths::standardLocations(QStandardPaths::FontsLocation);
    std::vector<std::string> fontPathList;
    for (QString font : fontPaths) {
        fontPathList.push_back(font.toStdString());
    }
    _cadMdiChild.viewer()->documentCanvas()->addFontsFromPath(fontPathList);*/
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::runOperation(kaguya::LuaRef operation, const std::string& init_method) {
    _cliCommand.setFocus();
    _luaInterface.finishOperation();
    _cadMdiChild.viewer()->setOperationActive(true);
    kaguya::State state(_luaInterface.luaState());

    // if current operation had extra operation _toolbar icons, add them
    if (!operation["operation_options"].isNilref())
    {
        if (operation_options.find(operation["command_line"].get<std::string>() + init_method) != operation_options.end()) {
            std::vector<kaguya::LuaRef>& options = operation_options[operation["command_line"].get<std::string>() + init_method];

            for (auto op : options) {
                // run operation which adds option icon to _toolbar
                op();
            }
        } else if (operation_options.find(operation["command_line"]) != operation_options.end()) {
            std::vector<kaguya::LuaRef>& options = operation_options[operation["command_line"]];

            for (auto op : options) {
                // run operation which adds option icon to _toolbar
                op();
            }
        }
    }

    // add _toolbar cancel button
    state.dostring("finish_op = function() finish_operation() end");
    _toolbar.addButton("", ":/icons/quit.svg", "Current operation", state["finish_op"], "Cancel");
    state["finish_op"] = nullptr;

    // call operation to run CreateOperations init method etc
    _luaInterface.setOperation(operation.call<kaguya::LuaRef>());
    kaguya::LuaRef op = _luaInterface.operation();
    if (init_method == "") {
        if (!op["_init_default"].isNilref()) {
            op["_init_default"](op);
        }
    }
    else {
        op[init_method.c_str()](op);
    }

    _oldOperation = operation;
    _oldOpInitMethod = init_method;
}

void MainWindow::addOperationOptions(std::string operation, std::vector<kaguya::LuaRef> options) {
    operation_options[operation] = options;
}

void MainWindow::operationFinished() {
    // remove operation group
    _toolbar.removeGroupByName("Current operation");
    _cadMdiChild.viewer()->setOperationActive(false);
}

lc::ui::widgets::CliCommand* MainWindow::cliCommand() {
    return &_cliCommand;
}

lc::ui::CadMdiChild* MainWindow::cadMdiChild() {
    return &_cadMdiChild;
}

lc::ui::widgets::Toolbar* MainWindow::toolbar() {
    return &_toolbar;
}

lc::ui::widgets::Layers* MainWindow::layers() {
    return &_layers;
}

lc::ui::LuaInterface* MainWindow::luaInterface() {
    return &_luaInterface;
}

int MainWindow::contextMenuManagerId() {
    return _contextMenuManagerId;
}

void MainWindow::ConnectInputEvents()
{
    // CadMdiChild connections, main window should not know about proxy
    QObject::connect(&_cadMdiChild, &CadMdiChild::mousePressEvent, this, &MainWindow::triggerMousePressed);
    QObject::connect(&_cadMdiChild, &CadMdiChild::mouseReleaseEvent, this, &MainWindow::triggerMouseReleased);
    QObject::connect(&_cadMdiChild, &CadMdiChild::mouseMoveEvent, this, &MainWindow::triggerMouseMoved);
    QObject::connect(&_cadMdiChild, &CadMdiChild::selectionChangeEvent, this, &MainWindow::triggerSelectionChanged);
    QObject::connect(&_cadMdiChild, &CadMdiChild::selectionChangeEvent, this, &MainWindow::selectionChanged);
    QObject::connect(&_cadMdiChild, &CadMdiChild::keyPressEventx, this, &MainWindow::triggerKeyPressed);
    QObject::connect(&_cadMdiChild, &CadMdiChild::keyPressed, &_cliCommand, &widgets::CliCommand::onKeyPressed);

    // CliCommand connections
    QObject::connect(&_cliCommand, &widgets::CliCommand::coordinateEntered, this, &MainWindow::triggerCoordinateEntered);
    QObject::connect(&_cliCommand, &widgets::CliCommand::relativeCoordinateEntered, this, &MainWindow::triggerRelativeCoordinateEntered);
    QObject::connect(&_cliCommand, &widgets::CliCommand::numberEntered, this, &MainWindow::triggerNumberEntered);
    QObject::connect(&_cliCommand, &widgets::CliCommand::textEntered, this, &MainWindow::triggerTextEntered);
    QObject::connect(&_cliCommand, &widgets::CliCommand::finishOperation, this, &MainWindow::triggerFinishOperation);
    QObject::connect(&_cliCommand, &widgets::CliCommand::commandEntered, this, &MainWindow::triggerCommandEntered);

    // Layers to select tools connections
    QObject::connect(&_layers, &widgets::Layers::layerChanged, &linePatternSelect, &widgets::LinePatternSelect::onLayerChanged);
    QObject::connect(&_layers, &widgets::Layers::layerChanged, &lineWidthSelect, &widgets::LineWidthSelect::onLayerChanged);
    QObject::connect(&_layers, &widgets::Layers::layerChanged, &colorSelect, &widgets::ColorSelect::onLayerChanged);

    // Other
    QObject::connect(this, &MainWindow::point, this, &MainWindow::triggerPoint);
    QObject::connect(findMenuItemByObjectName("actionExit"), &QAction::triggered, this, &MainWindow::close);

    // File connections
    QObject::connect(findMenuItemByObjectName("actionNew"), &QAction::triggered, this, &MainWindow::newFile);
    QObject::connect(findMenuItemByObjectName("actionOpen"), &QAction::triggered, this, &MainWindow::openFile);
    QObject::connect(findMenuItemByObjectName("actionSave_2"), &QAction::triggered, &_cadMdiChild, &CadMdiChild::saveFile);
    QObject::connect(findMenuItemByObjectName("actionSave_As"), &QAction::triggered, &_cadMdiChild, &CadMdiChild::saveAsFile);

    // Edit connections
    QObject::connect(findMenuItemByObjectName("actionUndo"), &QAction::triggered, this, &MainWindow::undo);
    QObject::connect(findMenuItemByObjectName("actionRedo"), &QAction::triggered, this, &MainWindow::redo);
    QObject::connect(findMenuItemByObjectName("actionSelect_All"), &QAction::triggered, this, &MainWindow::selectAll);
    QObject::connect(findMenuItemByObjectName("actionSelect_None"), &QAction::triggered, this, &MainWindow::selectNone);
    QObject::connect(findMenuItemByObjectName("actionInvert_Selection"), &QAction::triggered, this, &MainWindow::invertSelection);
    QObject::connect(findMenuItemByObjectName("actionClear_Undoable_Stack"), &QAction::triggered, this, &MainWindow::clearUndoableStack);
    QObject::connect(findMenuItemByObjectName("actionAuto_Scale"), &QAction::triggered, this, &MainWindow::autoScale);
}

void MainWindow::runLastOperation() {
    if (!_oldOperation.isNilref()) {
        runOperation(_oldOperation, _oldOpInitMethod);
    }
}

/* Menu functions */

void MainWindow::connectMenuItem(const std::string& itemName, kaguya::LuaRef callback)
{
    lc::ui::api::MenuItem* menuItem = findMenuItemByObjectName(itemName.c_str());
    menuItem->addCallback(callback);
}

void MainWindow::initMenuAPI() {
    QList<QMenu*> allMenus = menuBar()->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);

    int menuPosition = 0;
    for (QMenu* current_menu : allMenus)
    {
        api::Menu* menu = static_cast<api::Menu*>(current_menu);
        this->menuBar()->addAction(menu->menuAction());
        menuMap[menu->title()] = menu;
        menu->updatePositionVariable(menuPosition);
        menuPosition++;

        QList<QMenu*> allMenusOfCurrentMenu = menu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);

        for (QMenu* currentChildMenu : allMenusOfCurrentMenu)
        {
            if (currentChildMenu != nullptr) {
                menu->addAction(currentChildMenu->menuAction());
            }
        }

        addActionsAsMenuItem(menu);
        fixMenuPositioning(menu);
    }
}

void MainWindow::addActionsAsMenuItem(lc::ui::api::Menu* menu) {
    QList<QAction*> actions = menu->actions();
    QList<QAction*> menuItemsToBeAdded;

    for (QAction* action : actions)
    {
        if (action->menu()) {
            addActionsAsMenuItem(static_cast<api::Menu*>(action->menu()));
        }
        else if (action->isSeparator()) {
            QAction* sep = new QAction();
            sep->setSeparator(true);
            menu->removeAction(action);
            menuItemsToBeAdded.push_back(sep);
        }
        else {
            lc::ui::api::MenuItem* newMenuItem = new lc::ui::api::MenuItem(action->text().toStdString().c_str());

            QString oldObjectName = action->objectName();
            action->setObjectName(oldObjectName + QString("changed"));
            newMenuItem->setObjectName(oldObjectName);

            menu->removeAction(action);
            menuItemsToBeAdded.push_back(newMenuItem);
        }
    }

    for (QAction* it : menuItemsToBeAdded)
    {
        menu->addAction(it);
    }

    // reorder menu to appear below
    for (QAction* action : actions)
    {
        if (action->menu()) {
            menu->insertMenu(menuItemsToBeAdded.last(), action->menu());
        }
    }
}

void MainWindow::fixMenuPositioning(lc::ui::api::Menu* menu) {
    QList<QAction*> actions = menu->actions();

    int pos = 0;
    for (QAction* action : actions) {
        lc::ui::api::Menu* actionMenu = qobject_cast<lc::ui::api::Menu*>(action->menu());
        lc::ui::api::MenuItem* actionItem = qobject_cast<lc::ui::api::MenuItem*>(action);

        if (actionMenu != nullptr) {
            actionMenu->updatePositionVariable(pos);
        }

        if (actionItem != nullptr) {
            actionItem->updatePositionVariable(pos);
        }
        pos++;
    }
}

/* Menu Lua GUI API */

lc::ui::api::MenuItem* MainWindow::findMenuItem(std::string label) {
    return findMenuItemBy(label, true);
}

lc::ui::api::MenuItem* MainWindow::findMenuItemByObjectName(std::string objectName) {
    return findMenuItemBy(objectName, false);
}

api::MenuItem* MainWindow::findMenuItemBy(std::string objectName, bool searchByLabel) {
    QString name = QString(objectName.c_str());

    for (auto key : menuMap.keys())
    {
        api::MenuItem* foundIt = findMenuItemRecur(menuMap[key], name, searchByLabel);

        if (foundIt != nullptr) {
            return foundIt;
        }
    }

    return nullptr;
}

api::MenuItem* MainWindow::findMenuItemRecur(QMenu* menu, QString objectName, bool searchByLabel) {
    QList<QAction*> actions = menu->actions();

    for (QAction* action : actions)
    {
        if (action->menu()) {
            api::MenuItem* foundIt = findMenuItemRecur(action->menu(), objectName, searchByLabel);
            if (foundIt != nullptr) {
                return foundIt;
            }
        }
        else if (!action->isSeparator()) {
            if (searchByLabel) {
                if (objectName == action->text()) {
                    return static_cast<api::MenuItem*>(action);
                }
            }
            else {
                if (objectName == action->objectName()) {
                    return static_cast<api::MenuItem*>(action);
                }
            }
        }
    }

    return nullptr;
}

bool MainWindow::checkForMenuOfSameLabel(const std::string& label) {
    QList<QString> keys = menuMap.keys();

    for (QString key : keys)
    {
        std::string keystr = key.toStdString();
        keystr.erase(std::remove(keystr.begin(), keystr.end(), '&'), keystr.end());

        if (keystr == label) {
            return true;
        }
    }

    return false;
}

api::Menu* MainWindow::addMenu(const std::string& menuName) {
    if (checkForMenuOfSameLabel(menuName)) {
        return nullptr;
    }

    api::Menu* newMenu = new api::Menu(menuName.c_str());
    addMenu(newMenu);

    return newMenu;
}

void MainWindow::addMenu(lc::ui::api::Menu* menu) {
    if (checkForMenuOfSameLabel(menu->label())) {
        return;
    }

    menuMap[menu->title()] = menu;
    menuBar()->addMenu(menu);

    QList<QAction*> menuList = menuBar()->actions();
    menu->updatePositionVariable(menuList.size() - 1);
}

api::Menu* MainWindow::menuByName(const std::string& menuName) {
    QList<QString> keys = menuMap.keys();

    for (QString key : keys)
    {
        std::string keystr = key.toStdString();
        keystr.erase(std::remove(keystr.begin(), keystr.end(), '&'), keystr.end());

        if (keystr == menuName) {
            return menuMap[key];
        }
    }

    return nullptr;
}

lc::ui::api::Menu* MainWindow::menuByPosition(int pos) {
    QList<QAction*> menuList = menuBar()->actions();

    if (pos < 0 || pos >= menuList.size()) {
        return nullptr;
    }

    return dynamic_cast<lc::ui::api::Menu*>(menuList[pos]->menu());
}

void MainWindow::removeFromMenuMap(std::string menuName) {
    auto iter = menuMap.begin();
    QString key;
    for(; iter != menuMap.end(); ++iter)
    {
        std::string keystr = iter.key().toStdString();
        keystr.erase(std::remove(keystr.begin(), keystr.end(), '&'), keystr.end());

        if (keystr == menuName) {
            key = iter.key();
            break;
        }
    }

    menuMap.remove(key);
}

void MainWindow::removeMenu(const char* menuLabel) {
    lc::ui::api::Menu* menuremove = menuByName(menuLabel);
    if (menuremove != nullptr) {
        menuremove->remove();
    }
}

void MainWindow::removeMenu(int position) {
    lc::ui::api::Menu* menuremove = menuByPosition(position);
    if (menuremove != nullptr) {
        menuremove->remove();
    }
}

/* Trigger slots */

void MainWindow::triggerMousePressed()
{
    lc::geo::Coordinate cursorPos = _cadMdiChild.cursor()->position();
    kaguya::State state(_luaInterface.luaState());
    state["mousePressed"] = kaguya::NewTable();
    state["mousePressed"]["position"] = cursorPos;
    state["mousePressed"]["widget"] = &_cadMdiChild;
    _luaInterface.triggerEvent("point", state["mousePressed"]);

    emit point(cursorPos);
}

void MainWindow::triggerMouseReleased()
{
    kaguya::State state(_luaInterface.luaState());
    state["mouseRelease"] = kaguya::NewTable();
    state["mouseRelease"]["widget"] = &_cadMdiChild;
    _luaInterface.triggerEvent("mouseRelease", state["mouseRelease"]);
}

void MainWindow::triggerSelectionChanged()
{
    kaguya::State state(_luaInterface.luaState());
    state["selectionChanged"] = kaguya::NewTable();
    state["selectionChanged"]["widget"] = &_cadMdiChild;
    _luaInterface.triggerEvent("selectionChanged", state["selectionChanged"]);
}

void MainWindow::triggerMouseMoved()
{
    lc::geo::Coordinate cursorPos = _cadMdiChild.cursor()->position();
    kaguya::State state(_luaInterface.luaState());
    state["mouseMove"] = kaguya::NewTable();
    state["mouseMove"]["position"] = cursorPos;
    state["mouseMove"]["widget"] = &_cadMdiChild;
    _luaInterface.triggerEvent("mouseMove", state["mouseMove"]);
}

void MainWindow::triggerKeyPressed(int key)
{
    if (key == Qt::Key_Escape)
    {
        // run finish operation
        auto state = _luaInterface.luaState();
        _luaInterface.triggerEvent("finishOperation", kaguya::LuaRef(state));
    }
    else
    {
        kaguya::State state(_luaInterface.luaState());
        state["keyEvent"] = kaguya::NewTable();
        state["keyEvent"]["key"] = key;
        state["keyEvent"]["widget"] = &_cadMdiChild;
        _luaInterface.triggerEvent("keyPressed", state["keyEvent"]);
    }
}

void MainWindow::triggerCoordinateEntered(lc::geo::Coordinate coordinate)
{
    kaguya::State state(_luaInterface.luaState());
    state["coordinateEntered"] = kaguya::NewTable();
    state["coordinateEntered"]["position"] = coordinate;
    state["coordinateEntered"]["widget"] = &_cadMdiChild;
    _luaInterface.triggerEvent("point", state["coordinateEntered"]);

    emit point(coordinate);
}

void MainWindow::triggerRelativeCoordinateEntered(lc::geo::Coordinate coordinate)
{
    kaguya::State state(_luaInterface.luaState());
    state["relCoordinateEntered"] = kaguya::NewTable();
    state["relCoordinateEntered"]["position"] = lastPoint + coordinate;
    state["relCoordinateEntered"]["widget"] = &_cadMdiChild;
    _luaInterface.triggerEvent("point", state["relCoordinateEntered"]);

    emit point(lastPoint + coordinate);
}

void MainWindow::triggerNumberEntered(double number)
{
    kaguya::State state(_luaInterface.luaState());
    state["numberEntered"] = kaguya::NewTable();
    state["numberEntered"]["number"] = number;
    state["numberEntered"]["widget"] = &_cadMdiChild;
    _luaInterface.triggerEvent("number", state["numberEntered"]);
}

void MainWindow::triggerTextEntered(QString text)
{
    kaguya::State state(_luaInterface.luaState());
    state["textEntered"] = kaguya::NewTable();
    state["textEntered"]["text"] = text.toStdString();
    state["textEntered"]["widget"] = &_cadMdiChild;
    _luaInterface.triggerEvent("text", state["textEntered"]);
}

void MainWindow::triggerFinishOperation()
{
    auto state = _luaInterface.luaState();
    _luaInterface.triggerEvent("operationFinished", kaguya::LuaRef(state));
    _luaInterface.triggerEvent("finishOperation", kaguya::LuaRef(state));
}

void MainWindow::triggerCommandEntered(QString command)
{
    _cliCommand.runCommand(command.toStdString().c_str());
}

void MainWindow::triggerPoint(lc::geo::Coordinate coordinate)
{
    lastPoint = coordinate;
}

void MainWindow::newFile()
{
    /*
        TODO : Ask user if he wishes to save the file before replacing current window with new file
    */

    WindowManager::newFile(this);
}

void MainWindow::openFile()
{
    WindowManager::openFile();
}

// Edit slots
void MainWindow::undo()
{
    _cadMdiChild.undoManager()->undo();
    _cadMdiChild.viewer()->update();
}

void MainWindow::clearUndoableStack()
{
    _cadMdiChild.undoManager()->removeUndoables();
}

void MainWindow::redo()
{
    _cadMdiChild.undoManager()->redo();
    _cadMdiChild.viewer()->update();
}

void MainWindow::selectAll()
{
    _cadMdiChild.viewer()->docCanvas()->selectAll();
    _cadMdiChild.viewer()->update();
}

void MainWindow::selectNone()
{
    _cadMdiChild.viewer()->docCanvas()->removeSelection();
    _cadMdiChild.viewer()->update();
}

void MainWindow::invertSelection()
{
    _cadMdiChild.viewer()->docCanvas()->inverseSelection();
    _cadMdiChild.viewer()->update();
}

void MainWindow::autoScale() {
    _cadMdiChild.viewer()->autoScale();
    _cadMdiChild.viewer()->update();
};

void MainWindow::runCustomizeToolbar() {
    _customizeToolbar = new widgets::CustomizeToolbar(toolbar());
    connect(_customizeToolbar, &widgets::CustomizeToolbar::customizeWidgetClosed, this, &MainWindow::writeSettings);
    connect(_customizeToolbar, &widgets::CustomizeToolbar::defaultSettingsLoad, this, &MainWindow::loadDefaultSettings);

    _customizeToolbar->show();
}

void MainWindow::writeSettings() {
    _uiSettings.writeSettings(_customizeToolbar);
}

void MainWindow::readUiSettings() {
    _customizeToolbar = new widgets::CustomizeToolbar(toolbar());
    _customizeToolbar->setCloseMode(widgets::CustomizeToolbar::CloseMode::Save);
    _uiSettings.readSettings(_customizeToolbar);
    _customizeToolbar->close();
}

void MainWindow::loadDefaultSettings() {
    _uiSettings.readSettings(_customizeToolbar, true);
}

void MainWindow::selectionChanged() {
    std::vector<lc::entity::CADEntity_CSPtr> selectedEntities = _cadMdiChild.selection();
    PropertyEditor* propertyEditor = PropertyEditor::GetPropertyEditor(this);

    propertyEditor->clear(selectedEntities);

    for (lc::entity::CADEntity_CSPtr selectedEntity : selectedEntities) {
        propertyEditor->addEntity(selectedEntity);
    }

    if (selectedEntities.size() == 0) {
        propertyEditor->hide();
    }
    else {
        propertyEditor->show();
    }
}

std::string MainWindow::lastOperationName() {
    return _oldOperation["name"].get<std::string>();
}

kaguya::LuaRef MainWindow::currentOperation() {
    return _luaInterface.operation();
}

void MainWindow::changeDockLayout(int i) {
    if (i == 1) {
        addDockWidget(Qt::RightDockWidgetArea, &_layers);
        addDockWidget(Qt::BottomDockWidgetArea, &_cliCommand);
        addDockWidget(Qt::TopDockWidgetArea, &_toolbar);
        PropertyEditor* propertyEditor = PropertyEditor::GetPropertyEditor(this);
        addDockWidget(Qt::BottomDockWidgetArea, propertyEditor);
        resizeDocks({ &_cliCommand, propertyEditor }, { 65, 35 }, Qt::Horizontal);
    }

    if (i == 2) {
        addDockWidget(Qt::LeftDockWidgetArea, &_layers);
        addDockWidget(Qt::BottomDockWidgetArea, &_cliCommand);
        addDockWidget(Qt::TopDockWidgetArea, &_toolbar);
        PropertyEditor* propertyEditor = PropertyEditor::GetPropertyEditor(this);
        addDockWidget(Qt::RightDockWidgetArea, propertyEditor);
    }

    if (i == 3) {
        addDockWidget(Qt::LeftDockWidgetArea, &_layers);
        addDockWidget(Qt::BottomDockWidgetArea, &_cliCommand);
        addDockWidget(Qt::TopDockWidgetArea, &_toolbar);
        PropertyEditor* propertyEditor = PropertyEditor::GetPropertyEditor(this);
        addDockWidget(Qt::BottomDockWidgetArea, propertyEditor);
    }
}

void MainWindow::saveDockLayout() {
    int layersPos = this->dockWidgetArea(&_layers);
    int cliCommandPos = this->dockWidgetArea(&_cliCommand);
    int toolbarPos = this->dockWidgetArea(&_toolbar);
    PropertyEditor* propertyEditor = PropertyEditor::GetPropertyEditor(this);
    int propertyEditorPos = this->dockWidgetArea(propertyEditor);

    std::map<std::string, int> positions = {
        {"Toolbar", toolbarPos},
        {"CliCommand", cliCommandPos},
        {"Layers", layersPos},
        {"PropertyEditor", propertyEditorPos}
    };

    std::map<std::string, int> widths = {
        {"Toolbar", _toolbar.width()},
        {"CliCommand", _cliCommand.width()},
        {"Layers", _layers.width()},
        {"PropertyEditor", propertyEditor->width()}
    };

    std::map<int, int> posCount;
    std::map<int, int> posWidth;
    for (auto iter = positions.begin(); iter != positions.end(); ++iter) {
        if (posCount.find(iter->second) == posCount.end()) {
            posCount[iter->second] = 0;
            posWidth[iter->second] = 0;
        }
        posCount[iter->second]++;
        posWidth[iter->second] += widths[iter->first];
    }

    std::map<std::string, int> posProportions;

    for (auto iter = positions.begin(); iter != positions.end(); ++iter) {
        if (posCount[iter->second] > 1) {
            int percent = std::round(((double)widths[iter->first] / (double)posWidth[iter->second]) * 100);
            posProportions[iter->first] = percent;
        }
    }

    _uiSettings.writeDockSettings(positions, posProportions);
}

void MainWindow::loadDockLayout() {
    std::map<std::string, int> dockProportions;
    std::map<std::string, int> dockPositions = _uiSettings.readDockSettings(dockProportions);

    PropertyEditor* propertyEditor = PropertyEditor::GetPropertyEditor(this);
    std::map<std::string, QDockWidget*> dockWidgets = {
        {"Layers", &_layers},
        {"CliCommand", &_cliCommand},
        {"Toolbar", &_toolbar},
        {"PropertyEditor", propertyEditor}
    };

    std::map<int, QList<QDockWidget*>> resizeWidgets;
    std::map<int, QList<int>> resizeProportions;

    if (dockPositions.size() > 0) {
        for (auto iter = dockWidgets.begin(); iter != dockWidgets.end(); ++iter) {
            int pos = dockPositions[iter->first];
            addDockWidget((Qt::DockWidgetArea)pos, iter->second);

            if (dockProportions.find(iter->first) != dockProportions.end()) {
                if (resizeWidgets.find(pos) == resizeWidgets.end()) {
                    resizeWidgets[pos] = QList<QDockWidget*>();
                    resizeProportions[pos] = QList<int>();
                }

                resizeWidgets[pos].append(iter->second);
                resizeProportions[pos].append(dockProportions[iter->first]);
            }
        }

        for (auto iter = resizeWidgets.begin(); iter != resizeWidgets.end(); ++iter) {
            bool horiz = true;
            if ((iter->first == (int)Qt::LeftDockWidgetArea) || (iter->first == (int)Qt::RightDockWidgetArea)) {
                horiz = false;
            }
            resizeDocks(iter->second, resizeProportions[iter->first], (horiz) ? Qt::Horizontal : Qt::Vertical);
        }
    }
}
