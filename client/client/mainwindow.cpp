#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    // 初始化 MainWindow 自身在 .ui 中定义的界面控件。
    ui->setupUi(this);

    // 创建登录对话框，并以 MainWindow 为父对象管理生命周期。
    _log_dlg=new LoginDialog(this);
    // 将登录页作为中央区域的初始页面。
    setCentralWidget(_log_dlg);
    // 显示登录页。
    //_log_dlg->show();

    // 建立“切换到注册页”信号与槽连接。
    connect(_log_dlg, &LoginDialog::switchRegister,
            this, &MainWindow::SlotSwitchReg);

    // 创建注册对话框，先不显示，等待后续切换。
    _reg_dlg = new RegisterDialog(this);

    // 设置为自定义无边框窗口风格（按你的 demo 逻辑补充）。
    _log_dlg->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    _reg_dlg->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    _reg_dlg->hide();
}

MainWindow::~MainWindow()
{
    delete ui;
    // if(_log_dlg){
    //     delete _log_dlg;
    //     _log_dlg = nullptr;
    // }
    // if(_reg_dlg){
    //     delete _reg_dlg;
    //     _reg_dlg = nullptr;
    // }
}
void MainWindow::SlotSwitchReg(){
    // 登录页触发切换后，将中央区域切换为注册页并刷新显示状态。
    setCentralWidget(_reg_dlg);
    _log_dlg->hide();
    _reg_dlg->show();
}
