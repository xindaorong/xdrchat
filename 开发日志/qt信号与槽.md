## 十分钟入门 Qt 信号与槽

用项目里的代码讲，不讲虚的。

---

### 一、是什么

信号与槽就是 Qt 的**打电话机制**：

```
对象A发出信号      对象B的槽函数被调用
    📞 喂！我被人点了  ──────────▶  📱 收到，我来处理
```

就是 **"发生了什么事 → 谁来处理"** 的一套连线。

---

### 二、三步走

#### 第 1 步：声明信号

在 `.h` 文件里用 `signals:` 声明：

[clickedlabel.h:34-35](clickedlabel.h#L34-L35)
```cpp
signals:
    void clicked(void);
```

信号就是一个**函数声明**，你不需要实现它。Qt 的 moc 工具会自动生成实现代码。

#### 第 2 步：发射信号

在 `.cpp` 里用 `emit` 发射：

[clickedlabel.cpp:34](clickedlabel.cpp#L34)
```cpp
emit clicked();   // 相当于群发广播："我被人点了！"
```

#### 第 3 步：连接信号和槽

在某个地方用 `connect` 连线：

[registerdialog.cpp:68](registerdialog.cpp#L68)
```cpp
connect(ui->pass_visible,     // 谁发出信号
        &ClickedLabel::clicked, // 哪个信号
        this,                   // 谁处理
        [this]() { ... });      // 怎么处理（槽函数）
```

---

### 三、四种槽函数的写法

项目里都有：

**① Lambda（最常用）**

```cpp
connect(ui->pass_visible, &ClickedLabel::clicked, this, [this]() {
    // 直接在 connect 里写处理逻辑
    ui->pass_edit->setEchoMode(QLineEdit::Password);
});
```

**② 成员函数**

```cpp
connect(ui->btn, &QPushButton::clicked, this, &RegisterDialog::switchRegister);
//                                                      ↑ 直接传函数指针
```

**③ 信号连信号（转发）**

[httpmgr.cpp:8](httpmgr.cpp#L8)
```cpp
connect(this, &HttpMgr::sig_http_finish, this, &HttpMgr::slot_http_finish);
//         信号A                           →   信号A转发给槽
```

**④ Lambda 带额外参数**

[httpmgr.cpp:26](httpmgr.cpp#L26)
```cpp
QObject::connect(reply, &QNetworkReply::finished, [reply, self, req_id, mod]() {
    // 捕获了 reply, self, req_id, mod 四个变量进来用
    emit self->sig_http_finish(req_id, res, ErrorCode::Success, mod);
});
```

---

### 四、核心规则

| 规则 | 说明 |
|---|---|
| **参数要匹配** | 信号有 `(int, QString)`，槽也得接 `(int, QString)`。槽可以比信号少参数（不要尾巴），但不能类型不对 |
| **信号不能有实现** | 写了信号声明就够了，别在 `.cpp` 里写函数体，编译报错 |
| **connect 可以随便连** | 一个信号连多个槽，多个信号连一个槽，都行 |
| **Q_OBJECT 宏** | 有信号/槽的类必须在 `.h` 类声明第一行写 `Q_OBJECT` |
| **跨线程自动排队** | 信号和槽在不同线程时，Qt 自动把槽放入事件队列，不用手动锁 |

---

### 五、项目里的实际数据流

```
用户点眼睛图标（ClickedLabel）
        │
        ▼
emit clicked()                          ← 信号
        │
        │ connect(ui->pass_visible, &ClickedLabel::clicked, ...)
        ▼
lambda 执行 setEchoMode(Password)       ← 槽
        │
        ▼
密码框变成 ●●●●●
```

```
网络回包到达（QNetworkReply）
        │
emit finished()                         ← 信号
        │
emit sig_http_finish(...)               ← 信号转发
        │
emit sig_reg_mod_finish(...)            ← 再转发
        │
slot_reg_mod_finish(...)                ← 槽：解析JSON
        │
_handlers[id](jsonObj)                  ← 手动调用lambda（不是信号槽，是查表直接调）
```

---

### 六、常见写错

```cpp
// ❌ 错误：connect 没写第三个参数 this
connect(btn, &QPushButton::clicked, [this](){ ... });

// ✅ 正确
connect(btn, &QPushButton::clicked, this, [this](){ ... });
```

```cpp
// ❌ 错误：信号和槽参数不匹配
// 信号是 clicked()  槽是 onClick(int x)   ← 不行，参数数量不对

// ✅ 正确：参数类型和顺序要对上
```

---

**总结：声明信号 → emit 发出 → connect 连好 → 槽自动被调。就这四步。**
