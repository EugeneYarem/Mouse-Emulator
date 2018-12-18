#include "keeper.h"
#include "keyboardhooker.h"
#include "mainwindow.h"
#include "typeinfo.h"
#include "shlobj.h"
#include "ui_mainwindow.h"
#include "windows.h"
#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSystemTrayIcon>

QLineEdit * MainWindow::focusedLineEdit = nullptr;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setFixedSize(this->sizeHint().width(), this->sizeHint().height() + 30);

    itWasChanged = false;
    isAllowClose = false;
    keeper = new Keeper(this);

    createTrayActions();
    initialiseTray();
    createMenuBar();
    createConnect();
    installEventFilters();
}

MainWindow::~MainWindow()
{
    if(itWasChanged)
        keeper->saveSettings(*KeyBoardHooker::getSettings());
    trayIcon->hide();
    delete ui;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if(dynamic_cast<QLineEdit *>(watched) != nullptr)
    {
        if(event->type() == QEvent::FocusIn)
            focusedLineEdit = dynamic_cast<QLineEdit *>(watched);
        if(event->type() == QEvent::FocusOut)
            focusedLineEdit = nullptr;
        if (event->type() == QEvent::KeyPress && dynamic_cast<QKeyEvent *>(event)->key() != Qt::Key_Tab)
        {
            this->ui->pushButtonCancel->setEnabled(true);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool MainWindow::isStartKeyLineEdit(const QLineEdit *line)
{
    return line == this->ui->lineEditStarKey;
}

int MainWindow::load(QMap<QString, unsigned int> *settings)
{
    return this->keeper->loadSettings(*settings);
}

int MainWindow::save(const QMap<QString, unsigned int> *settings)
{
    return this->keeper->saveSettings(*settings);
}

HRESULT MainWindow::changeLnk(WORD wHotKey)
{
    HRESULT hRes = 0;
#ifdef Q_OS_WIN32

    // Создание ярлыка
    // Входные параметры:
    //  pwzShortCutFileName - путь и имя ярлыка, например, "C:\\Блокнот.lnk"
    //  Если не указан путь, ярлык будет создан в папке, указанной в следующем параметре.
    //  Прим.: Windows сама НЕ добавляет к имени расширение .lnk
    //  pszPathAndFileName  - путь и имя exe-файла, например, "C:\\Windows\\NotePad.Exe"
    //  pszWorkingDirectory - рабочий каталог, например, "C:\\Windows"
    //  pszArguments        - аргументы командной строки, например, "C:\\Doc\\Text.Txt"
    //  wHotKey             - горячая клавиша, например, для Ctrl+Alt+A     HOTKEY(HOTKEYF_ALT|HOTKEYF_CONTROL,'A')
    //  iCmdShow            - начальный вид, например, SW_SHOWNORMAL
    //  pszIconFileName     - путь и имя файла, содержащего иконку, например, "C:\\Windows\\NotePad.Exe"
    //  int iIconIndex      - индекс иконки в файле, нумеруется с 0

    CoInitialize(nullptr);
    IShellLink * pSL;
    IPersistFile * pPF;

    // Получение экземпляра компонента "Ярлык"
    hRes = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, reinterpret_cast<LPVOID *>(&pSL));
    if( SUCCEEDED(hRes) )
    {
        hRes = pSL->SetPath(QDir::toNativeSeparators(QApplication::applicationFilePath()).toStdWString().c_str());
        if( SUCCEEDED(hRes) )
        {
            hRes = pSL->SetArguments(L"");
            if( SUCCEEDED(hRes) )
            {
                hRes = pSL->SetIconLocation(QDir::toNativeSeparators(QApplication::applicationFilePath()).toStdWString().c_str(), 0);
                if( SUCCEEDED(hRes) )
                {
                    hRes = pSL->SetHotkey(wHotKey);
                    if( SUCCEEDED(hRes) )
                    {
                        hRes = pSL->SetShowCmd(SW_SHOWNORMAL);
                        if( SUCCEEDED(hRes) )
                        {
                            // Получение компонента хранилища параметров
                            hRes = pSL->QueryInterface(IID_IPersistFile,reinterpret_cast<LPVOID *>(&pPF));
                            if( SUCCEEDED(hRes) )
                            {
                                // Сохранение созданного ярлыка
                                QProcessEnvironment env(QProcessEnvironment::systemEnvironment());
                                hRes = pPF->Save(QDir::toNativeSeparators(env.value("USERPROFILE") + "/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Mouse Emulator Pro.lnk").toStdWString().c_str(),TRUE);
                                pPF->Release();
                            }
                        }
                    }
                }
            }
        }
        pSL->Release();
    }
    CoUninitialize();

#endif
    return hRes;
}

QString MainWindow::getHotKeyCombinationString(const QMap<QString, unsigned int> *settings)
{
    QString tmp = "";
    if(settings->value("Ctrl state") == HOTKEYF_CONTROL)
        tmp += "Ctrl + ";
    if(settings->value("Alt state") == HOTKEYF_ALT)
        tmp += "Alt + ";
    return tmp + KeyBoardHooker::getKeyNameByVirtualKey(settings->value("another key state"));
}

QLineEdit *MainWindow::getFocusedLineEdit()
{
    return focusedLineEdit;
}

void MainWindow::allowClose(bool value)
{
    isAllowClose = value;
}

void MainWindow::displaySettings(const QMap<QString, unsigned int> *settings)
{
    ui->sliderSpeedX->setValue(static_cast<int>(settings->value("speed x")));
    ui->sliderSpeedY->setValue(static_cast<int>(settings->value("speed y")));
    ui->checkBoxAutoStart->setCheckState(static_cast<Qt::CheckState>(settings->value("autorun")));
    ui->checkBoxStartKey->setCheckState(static_cast<Qt::CheckState>(settings->value("hot key")));

    QString tmp = KeyBoardHooker::getKeyNameByVirtualKey(settings->value("up"));
    if( !tmp.isEmpty())
        ui->lineEditUp->setText(tmp);

    tmp = KeyBoardHooker::getKeyNameByVirtualKey(settings->value("down"));
    if( !tmp.isEmpty())
        ui->lineEditDown->setText(tmp);

    tmp = KeyBoardHooker::getKeyNameByVirtualKey(settings->value("right"));
    if( !tmp.isEmpty())
        ui->lineEditRight->setText(tmp);

    tmp = KeyBoardHooker::getKeyNameByVirtualKey(settings->value("left"));
    if( !tmp.isEmpty())
        ui->lineEditLeft->setText(tmp);

    tmp = KeyBoardHooker::getKeyNameByVirtualKey(settings->value("top-right"));
    if( !tmp.isEmpty())
        ui->lineEditUpRight->setText(tmp);

    tmp = KeyBoardHooker::getKeyNameByVirtualKey(settings->value("top-left"));
    if( !tmp.isEmpty())
        ui->lineEditUpLeft->setText(tmp);

    tmp = KeyBoardHooker::getKeyNameByVirtualKey(settings->value("down-right"));
    if( !tmp.isEmpty())
        ui->lineEditDownRight->setText(tmp);

    tmp = KeyBoardHooker::getKeyNameByVirtualKey(settings->value("down-left"));
    if( !tmp.isEmpty())
        ui->lineEditDownLeft->setText(tmp);

    tmp = KeyBoardHooker::getKeyNameByVirtualKey(settings->value("click"));
    if( !tmp.isEmpty())
        ui->lineEditClick->setText(tmp);

    tmp = KeyBoardHooker::getKeyNameByVirtualKey(settings->value("right click"));
    if( !tmp.isEmpty())
        ui->lineEditRightClick->setText(tmp);

    if(settings->value("hot key") == Qt::Checked)
        ui->lineEditStarKey->setText( getHotKeyCombinationString(settings) );

    this->ui->pushButtonCancel->setEnabled(false);
}

void MainWindow::installEventFilters()
{
    ui->lineEditUp->installEventFilter(this);
    ui->lineEditDown->installEventFilter(this);
    ui->lineEditRight->installEventFilter(this);
    ui->lineEditLeft->installEventFilter(this);
    ui->lineEditUpRight->installEventFilter(this);
    ui->lineEditUpLeft->installEventFilter(this);
    ui->lineEditDownRight->installEventFilter(this);
    ui->lineEditDownLeft->installEventFilter(this);
    ui->lineEditClick->installEventFilter(this);
    ui->lineEditRightClick->installEventFilter(this);
    ui->lineEditStarKey->installEventFilter(this);
}

void MainWindow::createConnect()
{
    this->ui->sliderSpeedX->setRange(1, 100);
    this->ui->spinBoxSpeedX->setRange(1, 100);
    this->ui->sliderSpeedY->setRange(1, 100);
    this->ui->spinBoxSpeedY->setRange(1, 100);

    connect(this->ui->sliderSpeedX, &QSlider::valueChanged, [this] (int value) {
                                this->ui->spinBoxSpeedX->setValue(value);
                                KeyBoardHooker::setTempSetting("speed x", static_cast<unsigned int>(value));
                                this->ui->pushButtonCancel->setEnabled(true);
    });
    connect(this->ui->spinBoxSpeedX, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
                                this->ui->sliderSpeedX->setValue(value);
                                KeyBoardHooker::setTempSetting("speed x", static_cast<unsigned int>(value));
                                this->ui->pushButtonCancel->setEnabled(true);
    });
    connect(this->ui->sliderSpeedY, &QSlider::valueChanged, [this] (int value) {
                                this->ui->spinBoxSpeedY->setValue(value);
                                KeyBoardHooker::setTempSetting("speed y", static_cast<unsigned int>(value));
                                this->ui->pushButtonCancel->setEnabled(true);
    });
    connect(this->ui->spinBoxSpeedY, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this] (int value) {
                                this->ui->sliderSpeedY->setValue(value);
                                KeyBoardHooker::setTempSetting("speed y", static_cast<unsigned int>(value));
                                this->ui->pushButtonCancel->setEnabled(true);
    });
    connect(this->ui->pushButtonOk, &QPushButton::pressed, [this] () {
                                if( !KeyBoardHooker::getTempSettings()->isEmpty())
                                {
                                    QMessageBox msg(QMessageBox::Question, "Підтвердження дії", "<p style='font-size:10pt'>Ви впевнені, що хочете змінити налаштування?<p>", QMessageBox::Yes | QMessageBox::No);
                                    msg.setButtonText(QMessageBox::Yes, "Так");
                                    msg.setButtonText(QMessageBox::No, "Ні");
                                    if(msg.exec() == QMessageBox::Yes)
                                    {
                                        if(static_cast<Qt::CheckState>(KeyBoardHooker::getSettings()->value("autorun")) == Qt::Checked &&
                                                this->ui->checkBoxAutoStart->checkState() == Qt::Unchecked)
                                        {
                                            #ifdef Q_OS_WIN32
                                                QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
                                                settings.remove("Mouse Emulator Pro");
                                            #endif
                                        }
                                        if(static_cast<Qt::CheckState>(KeyBoardHooker::getSettings()->value("autorun")) == Qt::Unchecked &&
                                                this->ui->checkBoxAutoStart->checkState() == Qt::Checked)
                                        {
                                            #ifdef Q_OS_WIN32
                                                QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
                                                settings.setValue("Mouse Emulator Pro", QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
                                                settings.sync();
                                            #endif
                                        }

                                        if(static_cast<Qt::CheckState>(KeyBoardHooker::getSettings()->value("hot key")) == Qt::Checked &&
                                                this->ui->checkBoxStartKey->checkState() == Qt::Unchecked)
                                        {
                                            WORD wHotKey = MAKEWORD(0, 0);
                                            this->changeLnk(wHotKey);
                                            ui->lineEditStarKey->setText("");
                                        }
                                        else
                                        {
                                            UINT i = KeyBoardHooker::getTempSettings()->value("another key state");
                                            if(ui->lineEditStarKey->text() != this->getHotKeyCombinationString(KeyBoardHooker::getSettings()))
                                            {
                                                if(i == 0 || i == VK_LMENU || i == VK_RMENU || i == VK_LCONTROL || i == VK_RCONTROL)
                                                {
                                                    QMessageBox::information(this, "Повідомлення", "<p style='font-size:10pt'>Ви не вказали клавішу або їх комбінацію, тому буде встановлено стандартну комбінацію <b>Ctrl + Alt + F7</b>. Комбінації <b>Ctrl + Alt</b>, <b>Ctrl + Ctrl</b>, <b>Alt + Alt</b> не можуть використовуватися.</p>");
                                                    KeyBoardHooker::setTempSetting("Ctrl state", HOTKEYF_CONTROL);
                                                    KeyBoardHooker::setTempSetting("Alt state", HOTKEYF_ALT);
                                                    KeyBoardHooker::setTempSetting("another key state", 0x76);
                                                }
                                                else if(KeyBoardHooker::getTempSettings()->value("Ctrl state") == 0 && KeyBoardHooker::getTempSettings()->value("Alt state") == 0 &&
                                                        (KeyBoardHooker::getTempSettings()->value("another key state") < VK_F1 || KeyBoardHooker::getTempSettings()->value("another key state") > VK_F12))
                                                {
                                                    QMessageBox::information(this, "Повідомлення", "<p style='font-size:10pt'>Обрана клавіша не належить до клавіш <b>F1-F12</b>, тому буде встановлено стандартну комбінацію <b>Ctrl + Alt + F7</b>. Обрану клавішу можна використати тільки у комбінації з <b>Ctrl</b>, або з <b>Alt</b>, або з <b>Ctrl + Alt</b>.</p>");
                                                    KeyBoardHooker::setTempSetting("Ctrl state", HOTKEYF_CONTROL);
                                                    KeyBoardHooker::setTempSetting("Alt state", HOTKEYF_ALT);
                                                    KeyBoardHooker::setTempSetting("another key state", 0x76);
                                                }
                                                else if(KeyBoardHooker::getTempSettings()->value("Ctrl state") == HOTKEYF_CONTROL && KeyBoardHooker::getTempSettings()->value("Alt state") == 0 &&
                                                        KeyBoardHooker::getTempSettings()->value("another key state") == VK_F11)
                                                {
                                                    QMessageBox::information(this, "Повідомлення", "<p style='font-size:10pt'>Комбінація <b>Ctrl + F11</b> слугує для відкриття вікна програми, коли програма схована в трей. Будь ласка, оберіть нову комбінацію. Поки буде встановлено стандартну комбінацію <b>Ctrl + Alt + F7</b>.</p>");
                                                    KeyBoardHooker::setTempSetting("Ctrl state", HOTKEYF_CONTROL);
                                                    KeyBoardHooker::setTempSetting("Alt state", HOTKEYF_ALT);
                                                    KeyBoardHooker::setTempSetting("another key state", 0x76);
                                                }
                                                WORD wHotKey = MAKEWORD(KeyBoardHooker::getTempSettings()->value("another key state"), KeyBoardHooker::getTempSettings()->value("Ctrl state") | KeyBoardHooker::getTempSettings()->value("Alt state"));
                                                this->changeLnk(wHotKey);
                                                ui->lineEditStarKey->setText( getHotKeyCombinationString(KeyBoardHooker::getTempSettings()) );
                                                QMessageBox::information(this, "Повідомлення", "<p style='font-size:10pt'>Якщо після закриття програми ви не зможете запустити її за допомогою обраної клавіші або комбінації, будь ласка, перезавантажте систему. Ця проблема може виникнути тільки в тому ж сеансі роботи з ПК, під час якого було змінено \"гарячу клавішу\" або комбінацію.</p><p style='font-size:10pt'>Також можливо, що обрана клавіша / комбінація зайнята іншою програмою.</p>");
                                            }
                                            else
                                            {
                                                KeyBoardHooker::setTempSetting("Ctrl state", KeyBoardHooker::getSettings()->value("Ctrl state"));
                                                KeyBoardHooker::setTempSetting("Alt state", KeyBoardHooker::getSettings()->value("Alt state"));
                                                KeyBoardHooker::setTempSetting("another key state", KeyBoardHooker::getSettings()->value("another key state"));
                                            }
                                        }

                                        QMap<QString, unsigned int>::const_iterator i = KeyBoardHooker::getTempSettings()->constBegin();
                                        while (i != KeyBoardHooker::getTempSettings()->constEnd()) {
                                            KeyBoardHooker::setNewKeyValue(i.key(), i.value());
                                            ++i;
                                        }

                                        KeyBoardHooker::getTempSettings()->clear();
                                        itWasChanged = true;

                                        this->ui->pushButtonCancel->setEnabled(false);
                                    }
                                }
    });
    connect(this->ui->pushButtonCancel, &QPushButton::pressed, [this] () {
                                if( !KeyBoardHooker::getTempSettings()->isEmpty())
                                {
                                    KeyBoardHooker::getTempSettings()->clear();
                                    this->displaySettings(KeyBoardHooker::getSettings());
                                }
    });
    connect(this->ui->pushButtonStandart, &QPushButton::pressed, [this] () {
                                    QMessageBox msg(QMessageBox::Question, "Підтвердження дії", "<p style='font-size:10pt'>Ви впевнені, що хочете встановити стандартні налаштування?</p>", QMessageBox::Yes | QMessageBox::No);
                                    msg.setButtonText(QMessageBox::Yes, "Так");
                                    msg.setButtonText(QMessageBox::No, "Ні");
                                    if(msg.exec() == QMessageBox::Yes)
                                    {
                                        if(static_cast<Qt::CheckState>(KeyBoardHooker::getSettings()->value("autorun")) == Qt::Unchecked)
                                        {
                                            #ifdef Q_OS_WIN32
                                                QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
                                                settings.setValue("Mouse Emulator Pro", QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
                                                settings.sync();
                                            #endif
                                        }
                                        KeyBoardHooker::getTempSettings()->clear();
                                        KeyBoardHooker::configureSettings(0);
                                        this->displaySettings(KeyBoardHooker::getSettings());
                                        QDir::setCurrent(QApplication::applicationDirPath());
                                        QFile("settings.json").remove();
                                    }
    });
    connect(this->ui->checkBoxAutoStart, &QCheckBox::stateChanged, [this] (int state) {
                                        KeyBoardHooker::setTempSetting("autorun", static_cast<unsigned int>(state));
                                        this->ui->pushButtonCancel->setEnabled(true);
    });
    connect(this->ui->checkBoxStartKey, &QCheckBox::stateChanged, [this] (int state) {
                                        if(static_cast<unsigned int>(state) == Qt::Checked)
                                            this->ui->lineEditStarKey->setEnabled(true);
                                        else this->ui->lineEditStarKey->setEnabled(false);

                                        KeyBoardHooker::setTempSetting("hot key", static_cast<unsigned int>(state));
                                        KeyBoardHooker::setTempSetting("Ctrl state", 0);
                                        KeyBoardHooker::setTempSetting("Alt state", 0);
                                        KeyBoardHooker::setTempSetting("another key state", 0);
                                        this->ui->pushButtonCancel->setEnabled(true);
    });
}

void MainWindow::createMenuBar()
{
    ui->menuBar->addAction("Вийти", qApp, &QCoreApplication::quit);
    ui->menuBar->addSeparator();
    ui->menuBar->addAction("Про програму", [this]
                            {
                                QMessageBox::information(this, "Про програму",
                                    "<div style='font-size:10pt'><p><b><u>Розробник:</u></b> Яремченко Євгеній.</p>"
                                    "<p><b><u>Контакти:</u></b> kpyto2012@gmail.com.</p>"
                                    "<p><b><u>Про програму.</u></b></p>"
                                    "<p><b><u style='font-size:12pt'>1.</u></b> Програма дозволяє керувати мишею за допомогою клавіатури.</p>"
                                    "<p><b><u style='font-size:12pt'>2.</u></b> Програма може рухати курсор та емітувати натискання лівої та правої кнопок миші.</p>"
                                    "<p><b><u style='font-size:12pt'>3.</u></b> Програма дозволяє обирати клавіші для виконання вказаних раніше функції миші, а також налаштовувати швидкість руху курсору.</p>"
                                    "<p><b><u style='font-size:12pt'>4.</u></b> Програма за бажанням може бути додана до автозапуску Windows.</p>"
                                    "<p><b><u style='font-size:12pt'>5.</u></b> Програма може запускатися за допомогою натискання на клавіатурі обраної клавіші або їх комбінації.</p>"
                                    "<p><b><u style='font-size:12pt'>6.</u></b> Для закриття програми може бути використана комбінація <b>Ctrl+F12</b> або відповідний пункт головного меню чи меню, яке може бути викликане "
                                    "натисканням правої кнопки миші на значку програми в області повідомлень.</p>"
                                    "<p><b><u style='font-size:12pt'>7.</u></b> Для відкриття вікна програми може бути використана комбінація <b>Ctrl+F11</b> або відповідний пункт меню, яке може бути викликане "
                                    "натисканням правої кнопки миші на значку програми в області повідомлень.</p></div>"
                                );
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{ 
    if (!isAllowClose && trayIcon->isVisible()) {
        QMessageBox::information(this, "Інформація",
                                 "<p style='font-size:10pt'>Програма продовжить працювати після закриття цього вікна. "
                                    "Для припинення її роботи у системному треї клацніть правою кнопкою миші "
                                    "на значок програми і виберіть пункт <b>Вийти</b>.</p>");
        hide();
        event->ignore();
    }
    else QMainWindow::closeEvent(event);
}

void MainWindow::initialiseTray()
{
    trayIcon = new QSystemTrayIcon(QIcon(":/icon.ico"), this);
    trayIcon->setToolTip("Відкрити вікно налаштувань програми");

    trayMenu = new QMenu(this);
    trayMenu->addAction(openAction);
    trayMenu->addSeparator();
    trayMenu->addAction(quitAction);
    trayIcon->setContextMenu(trayMenu);

    connect(trayIcon, &QSystemTrayIcon::activated, [this] (QSystemTrayIcon::ActivationReason reason) {
                            switch (reason) {
                                case QSystemTrayIcon::Trigger:
                                    this->isVisible() ? this->hide() : this->show();
                                break;
                                default: ; break;
                            }
    });

    trayIcon->show();
}

void MainWindow::createTrayActions()
{
    openAction = new QAction("Відкрити вікно налаштувань", this);
    openAction->setIcon(QIcon(":/open.png"));
    openAction->setShortcut(QKeySequence(tr("Ctrl+F11")));
    openAction->setShortcutVisibleInContextMenu(true);
    connect(openAction, &QAction::triggered, this, &QWidget::showNormal);

    quitAction = new QAction("Вийти", this);
    quitAction->setIcon(QIcon(":/close.png"));
    quitAction->setShortcut(QKeySequence(tr("Ctrl+F12")));    
    quitAction->setShortcutVisibleInContextMenu(true);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
}