#include "keyboardhooker.h"
#include "mainwindow.h"
#include "shlobj.h"
#include <QApplication>
#include <QLineEdit>
#include <QMessageBox>

extern MainWindow *w;
HHOOK KeyBoardHooker::keyboardHook;
QMap<QString, unsigned int> KeyBoardHooker::settings;
QMap<QString, unsigned int> KeyBoardHooker::tempSettings;

KeyBoardHooker::KeyBoardHooker(QObject *parent) : QObject(parent)
{
    hInstance = GetModuleHandle(nullptr);
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardHookProc, hInstance, 0);

    if(keyboardHook == nullptr)
        QMessageBox::critical(nullptr, "Критична помилка", "<p style='font-size:12pt'>Програма не може виконувати основну функцію (керувати мишкою за допомогою клавіатури) через помилку під'єднання до системних подій.</p>");
}

QMap<QString, unsigned int> *KeyBoardHooker::getSettings()
{
    return &settings;
}

QMap<QString, unsigned int> *KeyBoardHooker::getTempSettings()
{
    return &tempSettings;
}

QString KeyBoardHooker::getKeyNameByVirtualKey(unsigned int vk)
{
    CHAR szName[128];
    UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
    int result = 0;
    switch (vk)
    {
        case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
        case VK_RCONTROL: case VK_RMENU:
        case VK_LWIN: case VK_RWIN: case VK_APPS:
        case VK_PRIOR: case VK_NEXT:
        case VK_END: case VK_HOME:
        case VK_INSERT: case VK_DELETE:
        case VK_DIVIDE: case VK_NUMLOCK:
            scanCode |= KF_EXTENDED;
        default:
            result = GetKeyNameTextA(static_cast<LONG>(scanCode << 16), szName, 128);
    }
    if(result == 0)
    {
        //throw std::system_error(std::error_code(static_cast<int>(GetLastError()), std::system_category()), "WinAPI error ocurred.");
        return QString();
    }
    return QString(szName);
}

QString KeyBoardHooker::getSettingNameByKeyName(QString keyName, bool settingMap)
{
    QMap<QString, unsigned int> settings = settingMap ? tempSettings : KeyBoardHooker::settings;
    QMap<QString, unsigned int>::const_iterator i = settings.constBegin();
    while (i != settings.constEnd()) {
        if(i.key() != QString("speed x") && i.key() != QString("speed y") && i.key() != QString("autorun") &&
                i.key() != QString("hot key") && i.key() != QString("Ctrl state") && i.key() != QString("Alt state") &&
                i.key() != QString("another key state") && getKeyNameByVirtualKey(i.value()) == keyName)
            return i.key();
        ++i;
    }

    return QString();
}

void KeyBoardHooker::configureSettings(int code)
{
    switch(code)
    {
        case -1: ;
        case 0: settings.insert("speed x", 5);
        case 1: settings.insert("speed y", 5);
        case 2: settings.insert("up", 0x26/*0x68*/);
        case 3: settings.insert("down", 0x28/*0x62*/);
        case 4: settings.insert("right", 0x27/*0x66*/);
        case 5: settings.insert("left", 0x25/*0x64*/);
        case 6: settings.insert("top-right", 0x69);
        case 7: settings.insert("top-left", 0x67);
        case 8: settings.insert("down-right", 0x63);
        case 9: settings.insert("down-left", 0x61);
        case 10: settings.insert("click", 0x65);
        case 11: settings.insert("right click", 0x60);
        case 12: settings.insert("autorun", Qt::Checked);
        case 13: settings.insert("hot key", Qt::Checked);
        case 14: settings.insert("Ctrl state", HOTKEYF_CONTROL);
        case 15: settings.insert("Alt state", HOTKEYF_ALT);
        case 16: settings.insert("another key state", 0x76); break;
        case 100: ; break;
    }
}

void KeyBoardHooker::setNewKeyValue(QString key, unsigned int value)
{
    settings[key] = value;
}

bool KeyBoardHooker::setTempSetting(QString key, unsigned int value)
{
    if(!tempSettings.contains(key)) // во временных настройках нет такого ключа
    {
        tempSettings.insert(key, value);
        return false; // кнопка не была добавлена в временный массив ранее
    }
    else  // во временных настройках есть такой ключ
    {
        tempSettings[key] = value;
        return false;
    }
    return true; // кнопка была добавлена в временный массив ранее
}

KeyBoardHooker &KeyBoardHooker::instance()
{
    static KeyBoardHooker instance;
    return instance;
}

void KeyBoardHooker::unhookExit()
{
    UnhookWindowsHookEx(KeyBoardHooker::keyboardHook);
    QCoreApplication::quit();
}

void KeyBoardHooker::setParent(MainWindow *parent)
{
    this->parent = parent;
}

bool KeyBoardHooker::isContainKey(unsigned int key, bool settingMap)
{
    foreach(UINT value, settingMap ? tempSettings : settings)
    {
        if(value == key)
            return true;
    }
    return false;
}

bool KeyBoardHooker::isSM0ContainKeyWithoutCrossing(unsigned int key)
{
    QMap<QString, unsigned int>::const_iterator i = settings.constBegin();
    while (i != settings.constEnd()) {
        if(!tempSettings.contains(i.key()) && i.value() == key)
            return true;
        ++i;
    }

    return false;
}

LRESULT CALLBACK KeyBoardHooker::keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    bool bAltKeyDown = 0;
    bool bControlKeyDown = 0;
    KBDLLHOOKSTRUCT *p = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);

    bAltKeyDown = GetAsyncKeyState(VK_MENU) >> ((sizeof(SHORT) * 8) - 1);
    bControlKeyDown = GetAsyncKeyState(VK_CONTROL) >> ((sizeof(SHORT) * 8) - 1);

    // Проверка, если F12 + CTRL были нажаты, если да -> завершение работы программы
    if (p->vkCode == VK_F12 && bControlKeyDown)
        KeyBoardHooker::unhookExit();
    // Проверка, если F11 + CTRL были нажаты и окно было скрыто, если да -> открыть окно программы
    if (p->vkCode == VK_F11 && bControlKeyDown && !w->isVisible())
        w->show();

    if (wParam == WM_SYSKEYDOWN || wParam == WM_KEYDOWN)
    {
        if(w->isActiveWindow() && w->getFocusedLineEdit() != nullptr && p->vkCode != VK_TAB)
        {
            if(w->isStartKeyLineEdit(w->getFocusedLineEdit()) && p->vkCode != VK_MENU && p->vkCode != VK_CONTROL)
            {
                if(!tempSettings.contains("Ctrl state"))
                {
                    setTempSetting("Ctrl state", 0);
                    setTempSetting("Alt state", 0);
                    setTempSetting("another key state", 0);
                }
                QString hotKey = "";
                if(bControlKeyDown)
                {
                    hotKey += "Ctrl + ";
                    tempSettings["Ctrl state"] = HOTKEYF_CONTROL;
                }
                if(bAltKeyDown)
                {
                    hotKey += "Alt + ";
                    tempSettings["Alt state"] = HOTKEYF_ALT;
                }
                tempSettings["another key state"] = p->vkCode;
                hotKey += getKeyNameByVirtualKey(p->vkCode);

                 w->getFocusedLineEdit()->setText(hotKey);
                 return 0;
            }

            QString tmpKeyName = getKeyNameByVirtualKey(p->vkCode);
            QString tmpSettingName0 = getSettingNameByKeyName( w->getFocusedLineEdit()->text());
            QString tmpSettingName1 = getSettingNameByKeyName( w->getFocusedLineEdit()->text(), 1);
            if(tmpKeyName == w->getFocusedLineEdit()->text())
                return 0;            
            if( !tmpKeyName.isEmpty() && !isContainKey(p->vkCode, 1) && !isSM0ContainKeyWithoutCrossing(p->vkCode))
            {
                setTempSetting(tmpSettingName1.isEmpty() ? tmpSettingName0 : tmpSettingName1, p->vkCode);
                w->getFocusedLineEdit()->setText(tmpKeyName);
                return 0;
            }
            else QMessageBox::warning(w, "Увага", "<p style='font-size:12pt'>Обрана клавіша вже використовується.</p>");
        }

        POINT currentPos;
        GetCursorPos(&currentPos);

        // Virtual key codes reference: http://msdn.microsoft.com/en-us/library/dd375731%28v=VS.85%29.aspx
        if(p->vkCode == settings["right click"]) // Compare virtual keycode to hex values and log keys accordingly
            mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, static_cast<DWORD>(currentPos.x), static_cast<DWORD>(currentPos.y), 0, 0);
        else if(p->vkCode == settings["down-left"])
            SetCursorPos(static_cast<int>(static_cast<unsigned long>(currentPos.x) - settings["speed x"]), static_cast<int>(static_cast<unsigned long>(currentPos.y) + settings["speed y"]));
        else if(p->vkCode == settings["down"])
            SetCursorPos(currentPos.x, static_cast<int>(static_cast<unsigned long>(currentPos.y) + settings["speed y"]));
        else if(p->vkCode == settings["down-right"])
            SetCursorPos(static_cast<int>(static_cast<unsigned long>(currentPos.x) + settings["speed x"]), static_cast<int>(static_cast<unsigned long>(currentPos.y) + settings["speed y"]));
        else if(p->vkCode == settings["left"])
            SetCursorPos(static_cast<int>(static_cast<unsigned long>(currentPos.x) - settings["speed x"]), currentPos.y);
        else if(p->vkCode == settings["click"])
            mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, static_cast<DWORD>(currentPos.x), static_cast<DWORD>(currentPos.y), 0, 0);
        else if(p->vkCode == settings["right"])
            SetCursorPos(static_cast<int>(static_cast<unsigned long>(currentPos.x) + settings["speed x"]), currentPos.y);
        else if(p->vkCode == settings["top-left"])
            SetCursorPos(static_cast<int>(static_cast<unsigned long>(currentPos.x) - settings["speed x"]), static_cast<int>(static_cast<unsigned long>(currentPos.y) - settings["speed y"]));
        else if(p->vkCode == settings["up"])
            SetCursorPos(currentPos.x, static_cast<int>(static_cast<unsigned long>(currentPos.y) - settings["speed y"]));
        else if(p->vkCode == settings["top-right"])
            SetCursorPos(static_cast<int>(static_cast<unsigned long>(currentPos.x) + settings["speed x"]), static_cast<int>(static_cast<unsigned long>(currentPos.y) - settings["speed y"]));
        else return CallNextHookEx(nullptr, nCode, wParam, lParam);
    }
    else return CallNextHookEx(nullptr, nCode, wParam, lParam);

    //return 1;
}