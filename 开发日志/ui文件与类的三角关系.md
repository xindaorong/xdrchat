## .ui / .h / .cpp 三角关系

一般就是在QT---->添加新文件----->qt--->qt wigits for class后就会得到一个.ui----.h----.cpp文件，就能得到一个qt为我们提供好的模板文件

用项目里的 [ResetDialog](resetdialog.ui) 和 [RegisterDialog](registerdialog.ui) 讲。

---

### 一、三个文件各自是什么

| 文件 | 是什么 | 谁来改 | 例子 |
|---|---|---|---|
| `.ui` | 界面布局（XML） | Qt Designer 拖控件 | 按钮长什么样、放哪、叫什么名 |
| `.h` | 类的声明 | 手写 | 有哪些信号、槽、成员变量 |
| `.cpp` | 类的实现 | 手写 | 按钮点了之后干嘛、网络回调处理 |

---

### 二、它们怎么连在一起

以刚创建的 ResetDialog 为例：

#### .ui 文件

[resetdialog.ui](resetdialog.ui) 第一行就定死了：

```xml
<class>ResetDialog</class>
<widget class="QDialog" name="ResetDialog">
```

`<class>ResetDialog</class>` 告诉 Qt 的 uic 工具：生成的类叫 `Ui::ResetDialog`。

在 Designer 里拖了这些控件：

| objectName | 类型 | 在界面上的位置 |
|---|---|---|
| `err_tip` | QLabel | 错误提示 |
| `lineEdit` | QLineEdit | 用户名输入框 |
| `lineEdit_3` | QLineEdit | 邮箱输入框 |
| `lineEdit_4` | QLineEdit | 验证码输入框 |
| `pushButton_3` | QPushButton | 获取验证码按钮 |
| `lineEdit_5` | QLineEdit | 新密码输入框 |
| `pushButton` | QPushButton | 确认按钮 |
| `pushButton_2` | QPushButton | 返回按钮 |

#### .h 文件

现在的 [resetdialog.h](resetdialog.h) 是骨架，对照成熟的 [registerdialog.h](registerdialog.h) 来看需要补什么：

**现在（骨架）：**
```cpp
#ifndef RESETDIALOG_H
#define RESETDIALOG_H

class ResetDialog
{
public:
    ResetDialog();
};

#endif // RESETDIALOG_H
```

**需要补成（对照 RegisterDialog）：**
```cpp
#ifndef RESETDIALOG_H
#define RESETDIALOG_H

#include <QDialog>                                    // ① 继承 QDialog

namespace Ui {
class ResetDialog;                                    // ② 前向声明 Ui::ResetDialog
}

class ResetDialog : public QDialog                    // ③ 继承 QDialog
{
    Q_OBJECT                                          // ④ 必须加

public:
    explicit ResetDialog(QWidget *parent = nullptr);
    ~ResetDialog();

private:
    Ui::ResetDialog *ui;                              // ⑤ 指向 UI 生成的对象的指针

signals:                                              // ⑥ 要发出的信号
    void sigSwitchLogin();

private slots:                                        // ⑦ 槽函数
    void on_pushButton_clicked();                     // 点"确认"按钮
    void on_pushButton_2_clicked();                   // 点"返回"按钮
    void on_pushButton_3_clicked();                   // 点"获取验证码"
};

#endif // RESETDIALOG_H
```

#### .cpp 文件

现在的 [resetdialog.cpp](resetdialog.cpp) 也是骨架：

```cpp
#include "resetdialog.h"
ResetDialog::ResetDialog() {}
```

需要补成：

```cpp
#include "resetdialog.h"
#include "ui_resetdialog.h"                // ① 包含 UI 生成的头文件

ResetDialog::ResetDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ResetDialog)                // ② 初始化 ui 对象
{
    ui->setupUi(this);                     // ③ 根据 .ui 搭建所有控件

    // ④ 手动连接信号
    connect(ui->pushButton_2, &QPushButton::clicked, this, &ResetDialog::sigSwitchLogin);
}

ResetDialog::~ResetDialog()
{
    delete ui;                             // ⑤ 析构时删 ui
}

// ⑥ 槽函数：点"确认"按钮
void ResetDialog::on_pushButton_clicked()
{
    // 校验输入 → 发 HTTP 请求
}

// ⑦ 槽函数：点"返回"按钮（也可以用自动连接，也可以手动 connect）
void ResetDialog::on_pushButton_2_clicked()
{
    emit sigSwitchLogin();
}

// ⑧ 槽函数：点"获取验证码"
void ResetDialog::on_pushButton_3_clicked()
{
    // 发送获取验证码请求
}
```

---

### 三、关键连接点

```
resetdialog.ui          resetdialog.h           resetdialog.cpp
─────────────          ──────────────          ──────────────
<class>ResetDialog</class>
                            ↓
                namespace Ui {
                class ResetDialog;  ← 前向声明
                  ↓                   }
         ── uic 工具编译 .ui 文件时 ──
         生成 ui_resetdialog.h
         里面有 Ui::ResetDialog 类
                  ↓                                       ↓
                            Ui::ResetDialog *ui;    #include "ui_resetdialog.h"
                                                    ui = new Ui::ResetDialog
                                                    ui->setupUi(this);
```

**四个必须对上的名字：**

| 地方 | 名字 |
|---|---|
| `.ui` 的 `<class>` | `ResetDialog` |
| `.h` 的类名 | `ResetDialog` |
| `.h` 里面 `Ui::` 后面 | `Ui::ResetDialog *ui` |
| `.cpp` 里面 `#include` | `"ui_resetdialog.h"` |

---

### 四、setupUi 干了什么

```cpp
ui->setupUi(this);
```

这一行调用后，`.ui` 里拖的所有控件全部被 new 出来、按布局摆好、属性设好。之后你就可以通过 `ui->控件名` 访问它们：

```cpp
ui->lineEdit->setText("hello");              // 读/写输入框
ui->pushButton->setEnabled(false);           // 禁用按钮
ui->err_tip->setProperty("state", "err");   // 改属性
```

---

### 五、两种连接按钮的方式

**方式 1：自动连接（靠命名规则）**

只要槽名叫 `on_<objectName>_<signal>()`，Qt 自动连，不用手写 connect：

```cpp
// .h
private slots:
    void on_pushButton_clicked();   // 自动匹配 pushButton 的 clicked 信号

// .cpp
void ResetDialog::on_pushButton_clicked() {
    // 用户点了"确认"按钮，自动走到这儿
}
```

**方式 2：手动 connect**

```cpp
connect(ui->pushButton_2, &QPushButton::clicked,
        this, &ResetDialog::sigSwitchLogin);
//         ↑ 点"返回"按钮 → 直接 emit 信号 → MainWindow 收到切回登录页
```

两种都可以，自动连接省代码，手动连接可控性强。

---

### 六、.pro 文件里的注册

[helloworld.pro:32-36](helloworld.pro#L32-L36)

```qmake
FORMS += \
    resetdialog.ui        # ← .ui 文件放这里

HEADERS += \
    resetdialog.h         # ← .h 文件放这里

SOURCES += \
    resetdialog.cpp       # ← .cpp 文件放这里
```

新加了 `.ui` 文件后，必须同时把 `.h` 和 `.cpp` 加到 `.pro` 里并写对类名，否则链接报错。

---

### 七、完整开发流程

```
1. 创建 resetdialog.ui → Qt Designer 拖控件、设 objectName、布局
        │
2. 创建 resetdialog.h   → 声明类、Q_OBJECT、Ui::ResetDialog *ui、信号、槽
        │
3. 创建 resetdialog.cpp → #include "ui_resetdialog.h"、写构造析构、写槽函数逻辑
        │
4. 检查 .pro 文件       → FORMS 有 .ui、HEADERS 有 .h、SOURCES 有 .cpp
        │
5. 编译                 → uic 处理 .ui → 生成 ui_resetdialog.h → 一起编译链接
```

---

**总结一句话：.ui 负责"长什么样"，.h 负责"有哪些东西"，.cpp 负责"点了之后干嘛"。三者通过对齐的类名和 `Ui::` 命名空间连在一起。**
