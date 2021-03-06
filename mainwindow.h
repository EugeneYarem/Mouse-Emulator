#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/***************************************
 * Этот класс реализует окно программы. Управляет отображением настроек, а также их изменением пользователем.
 * В процессе сохранения настроек позволяет добавить (удалить) программу в атозапуск и создать ярлык с нужными горячими клавишами.
 * Для этого используются функции класса Keeper: addToAutorun, removeFromAutorun, changeLnk
 * ************************************/

#include "translationinfo.h"
#include <QMainWindow>

class Keeper;
class QLineEdit;
class QSystemTrayIcon;

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

    bool itWasChanged; // указывает на то, были ли изменены настройки или нет
    bool isAllowClose; // указывает, можно ли завершить программу
    int hookNotExecuted; // id события о том, что хук не установлен
    int keyIsUsedEvent; // id события о том, что нажатая в текстовом поле клавиша уже используется для других функций мышки
    Keeper *keeper; // указатель на класс, который сохраняет и загружает настройки из файла
    QAction *aboutAction;
    QAction *englishAction;
    QAction *openAction;
    QAction *quitAction;
    QAction *quitActionForMB; // MB - menu bar
    QAction *russianAction;
    QAction *ukrainianAction;
    QMenu *trayMenu;
    QSystemTrayIcon *trayIcon;
    static QLineEdit * focusedLineEdit;

protected:
    void closeEvent(QCloseEvent *event) override;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    bool event(QEvent* ev) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    // настройка клавиши запуска приложения имеет особый характер, т.к. позволяет устанавливать комбинации клавиш
    bool isStartKeyLineEdit(const QLineEdit *); // проверяет, активное поле - это поле для стартовой комбинации или нет
    int getHookNotExecutedEventId();
    int getKeyIsUsedEventId();
    QVector<int> *load(QMap<QString, unsigned int> *settings); // вызывает соответствующий метод класса Keeper
    int save(const QMap<QString, unsigned int> *settings); // вызывает соответствующий метод класса Keeper
    QString getHotKeyCombinationString(const QMap<QString, unsigned int> *settings); // генерирует строку с именами клавиш, входящих в комбинацию для запуска
    static QLineEdit *getFocusedLineEdit();
    void allowClose(bool);
    void displaySettings(const QMap<QString, unsigned int> *settings); // зполняет необходимые виджеты формы нужными данными, полученными от KeyBoardHooker
    void showHookNotExecutedError();
    void showKeyIsUsedError();

private:
    Ui::MainWindow *ui;
    void createConnects();
    void createConnectActions();
    void createConnectButtons();
    void createConnectCheckBoxes();
    void createConnectLanguages();
    void createConnectLineEdits();
    void createConnectSliders();
    void createConnectSpinBoxes();
    void createMenuBar();
    void createTrayActions();
    void initialiseTray();
    void installEventFilters();
    void retranslateApp(Language);
    void setCaptionsToSomeUiEl();

};

#endif // MAINWINDOW_H
