# Qt 登录注册系统

一个基于 Qt6 开发的桌面登录注册应用，支持样式自定义、邮箱格式验证等功能。

## 功能特性

| 功能 | 说明 |
|------|------|
| 样式主题 | 支持 QSS 样式表，可自定义界面外观 |
| 登录窗口 | 独立的登录对话框 |
| 注册窗口 | 多步骤注册向导，支持邮箱验证和验证码交互 |
| 邮箱验证 | 实时验证邮箱格式，错误提示以红色显示 |
| 网络请求 | 通过 `HttpMgr` 统一管理 HTTP 请求和回包分发 |

## 快速开始

### 环境要求
- Qt 6.5.3 或更高版本
- MinGW 64-bit 编译器

### 构建运行
```bash
# 使用 Qt Creator 打开 helloworld.pro
# 或命令行构建
qmake helloworld.pro
make
```

## 项目结构

```
helloworld/
├── main.cpp          # 程序入口，加载 QSS 样式
├── mainwindow.*      # 主窗口
├── logindialog.*     # 登录对话框
├── registerdialog.*  # 注册对话框
├── global.*          # 全局工具函数
├── httpmgr.*         # HTTP 请求管理器
├── singleton.h       # 单例模板
├── rc.qrc            # 资源文件（图标、样式）
├── style/
│   └── stylesheet.qss    # QSS 样式文件
└── 开发日志/
    ├── Day1.md       # 图标集成
    └── Day2.md       # 邮箱验证功能
```

## 开发日志

### Day 1 - 项目基础
- 添加应用程序图标 (icon.ico)

### Day 2 - 邮箱验证功能
**新增功能：**
- 注册页面增加邮箱格式验证
- 点击"获取验证码"按钮时校验邮箱
- 格式错误时以红色字体提示"邮箱地址不合法"
- 格式正确时以绿色字体提示"邮箱地址合法"

**技术要点：**
```cpp
// 使用 QRegularExpression 验证邮箱格式
QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)")
```

**遇到的问题：**
- 槽函数命名问题：Qt 自动槽命名必须匹配 `on_<对象名>_<信号名>` 格式

### Day 3 - HTTP 请求管理与验证码回包处理
**新增功能：**
- 增加 `HttpMgr` 网络请求管理器，统一发送 POST 请求
- 通过单例模式获取 `HttpMgr` 实例，避免重复创建网络管理对象
- 增加注册模块的回包分发逻辑
- 点击"获取验证码"后，支持对服务器返回 JSON 的解析和结果提示
- 让注册页从“只做本地校验”升级为“可以和后端通信并处理结果”

**技术要点：**
```cpp
// 发送请求后，通过回包信号分发到注册模块
connect(HttpMgr::GetInstance().get(), &HttpMgr::sig_reg_mod_finish,
        this, &RegisterDialog::slot_reg_mod_finish);
```

**新增注意点：**
- `ReqId`、`ErrorCode`、`Modules` 这几个枚举在代码里使用了 `ReqId::xxx` 的写法，因此需要定义为 `enum class`
- 服务端返回内容需要先用 `QJsonDocument::fromJson()` 解析，再读取字段
- `reply->readAll()` 返回的是 `QByteArray`，需要转换成 `QString` 再继续处理

**这一版的作用：**
- 先检查邮箱格式是否正确，避免发送无效请求
- 发送验证码请求到服务器
- 根据服务器返回结果提示用户是否发送成功
- 为后续注册、登录、找回密码等网络功能预留统一入口

## 核心代码说明

### 邮箱验证逻辑
```cpp
void RegisterDialog::on_get_code_clicked()
{
    auto email = ui->email_edit->text();
    // 定义一个正则表达式
    QRegularExpression regex(R"((\w+)(\.|_)?(\w*)@(\w+)(\.(\w+))+)");
    // 看是否匹配成功
    bool is_match = regex.match(email).hasMatch();

    if (!is_match) {
        showTip(tr("请输入正确的邮箱格式"), false);  // 红色错误提示
        return;
    }

    // 绿色成功提示
    ui->err_tip->setText(tr("邮箱格式正确"));
    ui->err_tip->setProperty("state", "normal");
}
```

### 网络回包处理
```cpp
void RegisterDialog::slot_reg_mod_finish(ReqId id, QString res, ErrorCode err)
{
    if (err != ErrorCode::Success) {
        showTip(tr("获取验证码失败，请检查网络"), false);
        return;
    }

    QJsonDocument jsonDoc = QJsonDocument::fromJson(res.toUtf8());
    if (!jsonDoc.isObject()) {
        showTip(tr("JSON 解析错误"), false);
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    if (_handlers.contains(id)) {
        _handlers[id](jsonObj);
    }
}
```

### QSS 样式加载
```cpp
QFile qss(":/style/stylesheet.qss");
if (qss.open(QFile::ReadOnly)) {
    QString styleSheet = QLatin1String(qss.readAll());
    a.setStyleSheet(styleSheet);
}
```

## 待办事项

- [ ] 连接后端 API 实现真实注册登录
- [ ] 添加密码强度检测
- [ ] 实现验证码发送功能
- [ ] 添加记住密码功能

## 截图

见 `开发日志/img/` 目录。
