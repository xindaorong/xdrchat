# Day17--Qt 构造函数通用流程总结

## 1. 核心结论

在当前 `xdrchat` 客户端项目里，很多源文件的构造函数看起来很像，不是偶然，而是因为 Qt 界面类通常都在做同一组初始化工作。

可以把它总结成一句话：

> 构造函数负责把这个对象从“刚创建出来”初始化成“可以被用户操作、可以接收信号、可以处理后台结果”的完整状态。

所以在 `LoginDialog`、`RegisterDialog`、`ResetDialog`、`MainWindow` 这些类里，构造函数一般都会做下面几件事：

1. 初始化父类和成员变量。
2. 加载 `.ui` 文件生成的界面对象。
3. 初始化控件状态，比如样式、密码框模式、页面索引。
4. 连接本地控件信号，比如按钮点击、输入框编辑完成。
5. 初始化业务回调表，比如 `_handlers`。
6. 连接全局模块信号，比如 `HttpMgr` 的 HTTP 完成信号。

## 2. 通用结构

典型 Qt 构造函数可以写成这个结构：

```cpp
SomeDialog::SomeDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SomeDialog)
    , _some_member(...)
{
    ui->setupUi(this);

    // 1. 初始化界面状态
    // 2. 连接本地控件信号
    // 3. 初始化业务 handlers
    // 4. 连接全局模块信号
}
```

其中最重要的一句是：

```cpp
ui->setupUi(this);
```

这句执行之后，`.ui` 文件里拖出来的控件才真正被创建并绑定到 `ui` 指针上。

例如：

```cpp
ui->pushButton_2
ui->forget_label
ui->user_edit
ui->pass_visible
```

都必须在 `setupUi(this)` 之后才能安全使用。

## 3. LoginDialog 构造函数

`LoginDialog` 的构造函数主要完成登录界面的初始化：

```cpp
LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);

    connect(ui->pushButton_2, &QPushButton::clicked,
            this, &LoginDialog::switchRegister);

    ui->forget_label->SetState("normal", "hover", "", "selected", "selected_hover", "");
    ui->forget_label->setCursor(Qt::PointingHandCursor);

    connect(ui->forget_label, &ClickedLabel::clicked,
            this, &LoginDialog::slot_forget_pwd);

    initHttpHandlers();

    connect(HttpMgr::GetInstance().get(),
            &HttpMgr::sig_login_mod_finish,
            this,
            &LoginDialog::slot_login_mod_finish);
}
```

它的执行流程可以图解为：

```text
创建 LoginDialog
      |
      v
QDialog(parent), ui(new Ui::LoginDialog)
      |
      v
ui->setupUi(this)
      |
      v
连接“注册”按钮 clicked -> switchRegister
      |
      v
初始化 forget_label 的状态和鼠标样式
      |
      v
连接 forget_label clicked -> slot_forget_pwd
      |
      v
initHttpHandlers()
      |
      v
连接 HttpMgr::sig_login_mod_finish -> slot_login_mod_finish
      |
      v
登录窗口可以响应用户操作和 HTTP 登录回包
```

这里可以分成两类信号：

```text
本地控件信号：
pushButton_2 clicked
forget_label clicked

全局模块信号：
HttpMgr::sig_login_mod_finish
```

本地控件信号负责处理界面操作，全局模块信号负责处理网络请求返回后的结果。

## 4. RegisterDialog 构造函数

`RegisterDialog` 的构造函数更复杂，因为注册界面有输入校验、密码可见按钮、验证码、成功倒计时、HTTP 回包处理。

它的初始化顺序可以理解为：

```text
创建 RegisterDialog
      |
      v
初始化 QDialog、ui、_countdown_timer、_countdown
      |
      v
ui->setupUi(this)
      |
      v
初始化错误提示 QLabel 的 QSS 状态
      |
      v
设置 stackedWidget 默认页面
      |
      v
设置密码框 EchoMode 为 Password
      |
      v
连接多个 QLineEdit::editingFinished 做输入校验
      |
      v
初始化 pass_visible / confirm_visible 小眼睛状态
      |
      v
连接小眼睛 clicked，切换密码显示/隐藏
      |
      v
连接 QTimer::timeout，注册成功后倒计时回登录
      |
      v
initHttpHandlers()
      |
      v
连接 HttpMgr::sig_reg_mod_finish -> slot_reg_mod_finish
```

可以看到，`RegisterDialog` 构造函数里既有 UI 初始化，也有业务初始化。

比如输入框校验：

```cpp
connect(ui->user_edit, &QLineEdit::editingFinished, this, [this]() {
    checkUserValid();
});
```

这表示：

```text
用户编辑完用户名输入框
      |
      v
QLineEdit 发出 editingFinished
      |
      v
lambda 被调用
      |
      v
执行 checkUserValid()
      |
      v
更新错误提示 QLabel
```

密码小眼睛的逻辑也是同样的套路：

```text
用户点击 pass_visible
      |
      v
ClickedLabel 发出 clicked
      |
      v
lambda 被调用
      |
      v
读取 ClickedLabel 当前状态
      |
      v
切换 pass_edit 的 EchoMode
      |
      v
密码在隐藏/明文之间切换
```

## 5. ResetDialog 构造函数

`ResetDialog` 的构造函数和 `RegisterDialog` 很像，只是业务目标从“注册用户”变成了“重置密码”。

它做的事情主要是：

```text
创建 ResetDialog
      |
      v
ui->setupUi(this)
      |
      v
连接 user_Edit editingFinished -> checkUserValid
      |
      v
连接 email_Edit_3 editingFinished -> checkEmailValid
      |
      v
连接 verify_lineEdit_4 editingFinished -> checkPassValid
      |
      v
连接 pwd_Edit_5 editingFinished -> checkVarifyValid
      |
      v
initHandlers()
      |
      v
连接 HttpMgr::sig_reset_mod_finish -> slot_reset_mod_finish
```

这个类的构造函数说明了一个规律：

> 页面打开时不一定马上发送请求，但必须提前把“用户操作后应该调用谁”和“网络返回后应该调用谁”连接好。

真正发送 HTTP 请求的代码不在构造函数里，而是在按钮槽函数里：

```cpp
void ResetDialog::on_verify_btn_clicked_clicked()
void ResetDialog::on_sur_btn_clicked()
```

这样设计比较合理，因为构造函数只负责初始化页面，请求应该由用户动作触发。

## 6. MainWindow 构造函数

`MainWindow` 的构造函数和普通 Dialog 不太一样，它更像是一个页面管理器。

```cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , _login_dlg(nullptr)
    , _reg_dlg(nullptr)
{
    ui->setupUi(this);

    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);
    setCentralWidget(_login_dlg);

    connect(_login_dlg, &LoginDialog::switchRegister,
            this, &MainWindow::SlotSwitchReg);

    connect(_login_dlg, &LoginDialog::switchReset,
            this, &MainWindow::SlotSwitchReset);
}
```

它的职责是：

```text
创建 MainWindow
      |
      v
ui->setupUi(this)
      |
      v
创建 LoginDialog
      |
      v
把 LoginDialog 设置成中心窗口
      |
      v
连接 LoginDialog 发出的页面切换信号
      |
      v
当用户点击注册/忘记密码时，切换到对应页面
```

因此 `MainWindow` 不直接处理登录、注册、重置密码的业务细节。

它只负责页面切换：

```text
LoginDialog::switchRegister -> MainWindow::SlotSwitchReg
LoginDialog::switchReset    -> MainWindow::SlotSwitchReset
RegisterDialog::sigSwitchLogin -> MainWindow::SlotSwitchLogin
ResetDialog::switchLogin       -> MainWindow::SlotSwitchLogin2
```

这就是主窗口作为“页面调度中心”的作用。

## 7. HttpMgr 构造函数

`HttpMgr` 不是界面类，但它的构造函数也遵循同一个思想：对象创建时，把内部信号通路接好。

```cpp
HttpMgr::HttpMgr()
{
    connect(this,
            &HttpMgr::sig_http_finish,
            this,
            &HttpMgr::slot_http_finish);
}
```

它的作用是：

```text
任意模块调用 PostHttpReq
      |
      v
QNetworkAccessManager 发送请求
      |
      v
请求完成后发出 sig_http_finish
      |
      v
HttpMgr::slot_http_finish 接收
      |
      v
根据 Modules 分发给对应模块信号
```

分发关系是：

```text
Modules::REGISTERMOD -> sig_reg_mod_finish
Modules::RESETMOD    -> sig_reset_mod_finish
Modules::LOGINMOD    -> sig_login_mod_finish
```

所以界面类构造函数里连接的是模块信号：

```cpp
connect(HttpMgr::GetInstance().get(),
        &HttpMgr::sig_login_mod_finish,
        this,
        &LoginDialog::slot_login_mod_finish);
```

这里使用 `.get()` 是因为：

```cpp
HttpMgr::GetInstance()
```

返回的是：

```cpp
std::shared_ptr<HttpMgr>
```

而 Qt 的 `connect` 需要的是 `QObject*`，所以要通过 `.get()` 取出原始指针。

## 8. 构造函数中的四种初始化

结合当前项目，可以把 Qt 构造函数里的代码分为四类。

### 8.1 UI 加载

```cpp
ui->setupUi(this);
```

作用：

```text
读取 .ui 生成的代码
创建控件
设置对象名、布局、样式
把控件指针绑定到 ui->xxx
```

注意：所有 `ui->xxx` 都应该在 `setupUi(this)` 之后使用。

### 8.2 本地信号连接

例如：

```cpp
connect(ui->pushButton_2, &QPushButton::clicked,
        this, &LoginDialog::switchRegister);
```

```cpp
connect(ui->user_edit, &QLineEdit::editingFinished,
        this, [this]() {
            checkUserValid();
        });
```

这类连接处理的是用户直接操作界面产生的事件。

常见来源：

```text
QPushButton::clicked
QLineEdit::editingFinished
ClickedLabel::clicked
QTimer::timeout
```

### 8.3 初始状态设置

例如：

```cpp
ui->pass_edit->setEchoMode(QLineEdit::Password);
ui->confirm_edit->setEchoMode(QLineEdit::Password);
ui->stackedWidget->setCurrentIndex(0);
ui->forget_label->setCursor(Qt::PointingHandCursor);
```

这类代码负责保证窗口刚打开时是正确状态。

如果没有这些初始化，界面可能能显示，但状态不符合预期。

### 8.4 全局模块连接

例如：

```cpp
initHttpHandlers();

connect(HttpMgr::GetInstance().get(),
        &HttpMgr::sig_reg_mod_finish,
        this,
        &RegisterDialog::slot_reg_mod_finish);
```

这类代码负责让界面能收到后台模块的结果。

请求发出去以后，服务器返回结果，最终会走到界面的槽函数里更新 UI。

## 9. 构造函数和按钮槽函数的分工

构造函数负责“准备好”：

```text
创建控件
设置默认状态
连接信号槽
注册回包处理函数
连接全局模块
```

按钮槽函数负责“真正执行业务”：

```text
读取输入框内容
校验参数
组装 QJsonObject
调用 HttpMgr::PostHttpReq
等待回包
```

例如登录请求不是在构造函数里发，而是在：

```cpp
void LoginDialog::on_log_btn_clicked()
```

里面发：

```cpp
QJsonObject json_obj;
json_obj["user"] = user;
json_obj["passwd"] = xorString(pwd);

HttpMgr::GetInstance()->PostHttpReq(
    QUrl(gate_url_prefix + "/user_login"),
    json_obj,
    ReqId::ID_LOGIN_USER,
    Modules::LOGINMOD
);
```

这样可以保持职责清晰：

```text
构造函数：搭舞台
按钮槽函数：开始演出
HttpMgr：负责网络请求
slot_xxx_finish：处理服务器返回结果
```

## 10. 项目里的完整流程图

以登录为例：

```text
程序启动
  |
  v
main.cpp 创建 MainWindow
  |
  v
MainWindow 构造函数
  |
  v
创建 LoginDialog，并 setCentralWidget
  |
  v
LoginDialog 构造函数
  |
  |-- setupUi 创建登录界面控件
  |-- connect 注册按钮、忘记密码标签
  |-- initHttpHandlers 初始化登录回包处理函数
  |-- connect HttpMgr::sig_login_mod_finish
  |
  v
用户点击登录按钮
  |
  v
LoginDialog::on_log_btn_clicked
  |
  |-- 校验用户名
  |-- 校验密码
  |-- 组装 JSON
  |-- HttpMgr::GetInstance()->PostHttpReq
  |
  v
HttpMgr 发送 HTTP POST
  |
  v
服务器返回
  |
  v
HttpMgr 发出 sig_http_finish
  |
  v
HttpMgr::slot_http_finish 根据 LOGINMOD 分发
  |
  v
发出 sig_login_mod_finish
  |
  v
LoginDialog::slot_login_mod_finish
  |
  v
界面根据登录结果更新
```

## 11. 写 Qt 构造函数时的检查清单

以后写新的页面类时，可以按这个顺序检查：

1. 是否在初始化列表里调用了正确的父类构造？
2. 是否创建了 `ui(new Ui::Xxx)`？
3. 是否第一时间调用了 `ui->setupUi(this)`？
4. 是否设置了控件的默认状态？
5. 是否连接了按钮、输入框、标签等本地信号？
6. 是否需要初始化 `_handlers` 这类回包表？
7. 是否需要连接 `HttpMgr` 等全局模块信号？
8. 是否避免在构造函数里直接发送业务请求？
9. 析构函数里是否 `delete ui`？

## 12. 总结

当前项目里的 Qt 构造函数可以抽象成一个固定模型：

```text
构造函数
  =
  创建 UI
  + 初始化状态
  + 连接用户操作
  + 注册业务回调
  + 接入全局模块
```

所以你感觉“这些源文件的构造函数都类似”，本质原因是：

> Qt 页面类创建出来以后，必须先把界面、状态、信号槽、业务回调全部装配好，这个对象才能成为一个真正可用的窗口。

