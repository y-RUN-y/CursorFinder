#include "widget.h"
#include "ui_widget.h"
#include <QtMath>
#include <QtDebug>
#include <QPainter>
#include <QtWin>
#include <QTimeLine>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QScreen>
#include <QMenu>
#include <QAction>
#include <QScreen>
#include <QFileDialog>
//1.直接设置图标-动画会鬼畜 故改方案
//2.不按下左键时检测检测鼠标移动要用setMouseTraching(true)但此时检测频率较低 卡顿；而按住左键moveEvent就没有这个问题
//但由于不可实施，改为QTimer跟随
//3.不能鼠标穿透 否则setCursor(Qt::BlankCursor);失效
/*判断指针类型
    CURSORINFO info;
    info.cbSize = sizeof(CURSORINFO);
    GetCursorInfo(&info);
    qDebug() << info.hCursor << LoadCursor(NULL, IDC_ARROW) << LoadCursor(NULL, IDC_IBEAM);
 */
Widget::Widget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    iniSet = new QSettings(iniPath, QSettings::IniFormat);
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool); //Qt::Tool弹出时不会抢占焦点
    setAttribute(Qt::WA_TranslucentBackground);
    qApp->setQuitOnLastWindowClosed(false); //否则对话框关闭后就会quit
    QtWin::taskbarDeleteTab(this); //删除任务栏图标
    setCursor(Qt::BlankCursor); //BlankCursor也算isCursorVisible，只有ShowCursor(false);isCursorHide()才是true
    setGeometry(qApp->screens().at(0)->geometry() - QMargins(0, 0, 0, 1)); //不能showFullScreen()，否则会自动隐藏任务栏

    //检测是否自定义光标图像
    switch (checkCursorPath()) {
    case -1:
        sysTray->showMessage("错误", "读取光标文件错误，使用默认光标");
    case 0:
        cursorPix = QPixmap(":/images/cursor.png");
        break;
    case 1:
        cursorPix = QPixmap(CursorImgPath);
    }

    QGraphicsOpacityEffect* labelOpacity = new QGraphicsOpacityEffect(ui->label);
    labelOpacity->setOpacity(0);
    ui->label->setGraphicsEffect(labelOpacity);

    QTimer* timer_cursor = new QTimer(this);
    timer_cursor->setInterval(10);
    timer_cursor->callOnTimeout([=]() {
        ui->label->move(QCursor::pos());
    });

    QTimeLine* TL_scale = new QTimeLine(AnimationTime, this);
    TL_scale->setUpdateInterval(10);
    connect(TL_scale, &QTimeLine::valueChanged, [=](qreal value) {
        int width = 32 + value * (MaxSize-32);
        labelOpacity->setOpacity(1);
        QPixmap pix = cursorPix.scaledToWidth(width);
        //qDebug() << pix.size() << value;
        setCursorPix(pix);
    });

    connect(TL_scale, &QTimeLine::finished, [=]() {
        if (TL_scale->direction() == QTimeLine::Backward) {
            qDebug() << "Backward";
            timer_cursor->stop();
            hide();
        }
    });

    QTimer* timer_detect = new QTimer(this);
    timer_detect->callOnTimeout([=]() {
        if (isCursorHide()) { //BlankCursor也算isCursorVisible，只有ShowCursor(false);isCursorHide()才是true 所以不用担心show时 return
            //qDebug() << "hide";
            return;
        }

        static QPoint llPos = QCursor::pos();
        static QPoint lPos = QCursor::pos();
        QPoint nowPos = QCursor::pos();

        double dist = distance(nowPos, lPos);
        double speed = dist / timer_detect->interval() * 1000;

        double angle = includeAngle(llPos, lPos, nowPos);
        bool isCorner = angle < 90;
        if (isCorner)
            emit cornerDetected(nowPos, QTime::currentTime());
        else if (speed < 350)
            emit slowSpeedDetected();

        llPos = lPos;
        lPos = nowPos;
    });
    timer_detect->start(20);

    connect(this, &Widget::cornerDetected, [=](QPoint pos, QTime now) {
        if (isStopWhileFullScreen && isForeFullScreen()) return; //不放在timer_detect检测 防止性能损耗

        static QPoint lastCornerPos = QCursor::pos();
        static QTime lastCorner;

        if (lastCorner.isValid() && lastCorner.msecsTo(now) <= Gap && distance(lastCornerPos, pos) >= Dist) { //检测两次Turn的时间间隔 & distance；判断为shake
            if (isHidden() && TL_scale->state() == QTimeLine::NotRunning) {
                qDebug() << "Forward";
                timer_cursor->start();
                TL_scale->setDirection(QTimeLine::Forward);
                TL_scale->start();
                setCursorPix(cursorPix.scaledToWidth(20));
                show();
            }
        }

        lastCornerPos = pos;
        lastCorner = now;
    });

    connect(this, &Widget::slowSpeedDetected, [=]() {
        static bool lock = false; //防止时间差多次触发
        if (isVisible() && TL_scale->state() == QTimeLine::NotRunning && !lock) {
            lock = true;
            QTimer::singleShot(150, [=]() {
                lock = false;
                if (TL_scale->state() == QTimeLine::NotRunning) {
                    TL_scale->setDirection(QTimeLine::Backward);
                    TL_scale->start();
                }
            });
        }
    });

    readIni();
    initSysTray();
}

Widget::~Widget()
{
    delete ui;
}

double Widget::distance(const QPoint& lhs, const QPoint& rhs)
{
    QPoint delta = rhs - lhs;
    return qSqrt(qPow(delta.x(), 2) + qPow(delta.y(), 2));
}

double Widget::includeAngle(const QPoint& A, const QPoint& B, const QPoint& C)
{
    double a = distance(B, C);
    double b = distance(A, C);
    double c = distance(A, B);

    if (qFuzzyIsNull(a * c))
        return 360;
    return qRadiansToDegrees(qAcos((c * c + a * a - b * b) / (2 * a * c)));
}

void Widget::setCursorPix(const QPixmap& pix)
{
    ui->label->setGeometry(QRect(QCursor::pos(), pix.size()));
    ui->label->setPixmap(pix);
}

void Widget::initSysTray()
{
    if (sysTray) return;
    sysTray = new QSystemTrayIcon(QIcon(":/images/icon.ico"), this);
    sysTray->setToolTip("CursorFinder");

    QMenu* menu = new QMenu(); //如果设this为parent 则第一次show时 会卡顿，此处故意内存泄漏

    QAction* act_fullScreen = new QAction("全屏时禁用", menu);
    QAction* act_autoStart = new QAction("自启动", menu);
    QAction* act_setting = new QAction("设置", menu);
    QAction* act_setCursorImgPath = new QAction("修改光标图片", menu);
    QAction* act_resetCursorImgPath = new QAction("重置光标图片", menu);
    QAction* act_quit = new QAction("退出", menu);
    act_fullScreen->setCheckable(true);
    act_fullScreen->setChecked(isStopWhileFullScreen);
    connect(act_fullScreen, &QAction::toggled, [=](bool checked) {
        isStopWhileFullScreen = checked;
        sysTray->showMessage("CursorFinder", checked ? "已开启[全屏时禁用]" : "已关闭[全屏时禁用]");
        iniSet->setValue("DetectFullScreen", isStopWhileFullScreen);
    });
    act_autoStart->setCheckable(true);
    act_autoStart->setChecked(isAutoRun());
    connect(act_autoStart, &QAction::toggled, [=](bool checked) {
        setAutoRun(checked);
        sysTray->showMessage("CursorFinder", checked ? "已添加启动项" : "已移除启动项");
    });
    connect(act_setting, &QAction::triggered, [=]() {
        SettingDialog dia(this);
        dia.setValue(Gap, Dist, AnimationTime, MaxSize);
        connect(&dia, &SettingDialog::apply, [=](int gap, int distance, int anitime, int maxsize) {
            qDebug() << gap << distance << anitime << maxsize;
            Gap = gap;
            Dist = distance;
            AnimationTime = anitime;
            MaxSize = maxsize;
            writeIni();
        });
        dia.exec();
    });
    connect(act_setCursorImgPath, &QAction::triggered, [=](){
        CursorImgPath = QFileDialog::getOpenFileName(NULL, "请选择文件",".","*png");
        if(CursorImgPath != ""){
            cursorPix = QPixmap(CursorImgPath);
            iniSet->setValue("CursorImgPath",CursorImgPath);
        }
    });
    connect(act_resetCursorImgPath, &QAction::triggered, [=](){
        CursorImgPath = "";
        cursorPix = QPixmap(":images/cursor.png");
        iniSet->setValue("CursorImgPath",CursorImgPath);
    });
    connect(act_quit, &QAction::triggered, qApp, &QApplication::quit);

    menu->addAction(act_fullScreen);
    menu->addAction(act_autoStart);
    menu->addAction(act_setting);
    menu->addAction(act_setCursorImgPath);
    menu->addAction(act_resetCursorImgPath);
    menu->addAction(act_quit);
    sysTray->setContextMenu(menu);
    sysTray->show();
    sysTray->showMessage("CursorFinder", "CursorFinder已启动");
}

void Widget::setAutoRun(bool isAuto) //如果想区分是（开机启动|手动启动）可以加上启动参数 用来判别
{
    QSettings reg(Reg_AutoRun, QSettings::NativeFormat);
    if (isAuto)
        reg.setValue(AppName, AppPath);
    else
        reg.remove(AppName);
}

bool Widget::isAutoRun()
{
    QSettings reg(Reg_AutoRun, QSettings::NativeFormat);
    return reg.value(AppName).toString() == AppPath;
}

void Widget::writeIni()
{
    qDebug() << "#writeIni";
    iniSet->setValue("GapTime", Gap);
    iniSet->setValue("Distance", Dist);
    iniSet->setValue("AnimationTime", AnimationTime);
    iniSet->setValue("MaxSize",MaxSize);
}

void Widget::readIni()
{
    qDebug() << "#readIni";
    Gap = iniSet->value("GapTime", Gap).toInt();
    Dist = iniSet->value("Distance", Dist).toInt();
    isStopWhileFullScreen = iniSet->value("DetectFullScreen", isStopWhileFullScreen).toBool();
    AnimationTime = iniSet->value("AnimationTime", AnimationTime).toInt();
    MaxSize = iniSet->value("MaxSize", MaxSize).toInt();
    CursorImgPath = iniSet->value("CursorImgPath", CursorImgPath).toString();
}

bool Widget::isCursorHide()
{
    CURSORINFO info;
    info.cbSize = sizeof(CURSORINFO);
    GetCursorInfo(&info);
    return info.flags != CURSOR_SHOWING;
}

bool Widget::isForeFullScreen()
{
    QRect Screen = qApp->primaryScreen()->geometry();
    HWND Hwnd = GetForegroundWindow();

    HWND H_leftBottom = topWinFromPoint(Screen.bottomLeft()); //获取左下角像素所属窗口，非全屏是任务栏
    if (Hwnd != H_leftBottom) return false;

    RECT Rect;
    GetWindowRect(Hwnd, &Rect);
    if (Rect.right - Rect.left >= Screen.width() && Rect.bottom - Rect.top >= Screen.height()) //确保窗口大小(二重验证)
        return true;
    return false;
}

int Widget::checkCursorPath(){
    if(CursorImgPath == "") return 0;
    QFileInfo in(CursorImgPath);
    if(in.isFile()) return 1;
    return -1;
}

HWND Widget::topWinFromPoint(const QPoint& pos)
{
    HWND hwnd = WindowFromPoint({ pos.x(), pos.y() });
    while (GetParent(hwnd) != NULL)
        hwnd = GetParent(hwnd);
    return hwnd;
}

void Widget::paintEvent(QPaintEvent* event) // 不绘制会导致setCursor(blank)失效
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen); //取消边框//pen决定边框颜色
    painter.setBrush(QColor(10, 10, 10, 1)); //几近透明
    painter.drawRect(rect());
}

bool Widget::nativeEvent(const QByteArray& eventType, void* message, long* result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(result);
    MSG* msg = (MSG*)message;
    static const UINT WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
    if (msg->message == WM_TASKBARCREATED) { //获取任务栏重启信息，再次隐藏任务栏图标
        qDebug() << "TaskbarCreated";
        QtWin::taskbarDeleteTab(this);
        return true;
    }
    return false; //此处返回false，留给其他事件处理器处理
}
