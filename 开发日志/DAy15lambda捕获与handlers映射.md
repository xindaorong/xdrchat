## lambda 捕获与 handlers 映射

这份笔记用 [resetdialog.cpp](resetdialog.cpp) 里的 `ResetDialog::initHandlers()` 来理解两个问题：

1. `[this]` 到底捕获了什么？
2. `_handlers.insert(...)` 是什么方法，为什么要这样写？

---

### 一、先看你的代码结构

你想写的逻辑大概是这样：

```cpp
void ResetDialog::initHandlers()
{
    _handlers.insert(ReqId::ID_GET_VERIFY_CODE, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCode::Success){
            showTip(tr("参数错误"), false);
            return;
        }

        auto email = jsonObj["email"].toString();
        showTip(tr("验证码已发送到邮箱，注意查收"), true);
        qDebug() << "email is " << email;
    });

    _handlers.insert(ReqId::ID_RESET_PWD, [this](QJsonObject jsonObj){
        int error = jsonObj["error"].toInt();
        if(error != ErrorCode::Success){
            showTip(tr("参数错误"), false);
            return;
        }

        auto email = jsonObj["email"].toString();
        showTip(tr("重置成功,点击返回登录"), true);
        qDebug() << "email is " << email;
        qDebug() << "user uuid is " << jsonObj["uuid"].toString();
    });
}
```

这段代码不是马上发送请求，也不是马上处理服务器返回。

它的作用是：

```text
提前登记：
如果以后回来的是 ID_GET_VERIFY_CODE，就执行第一个 lambda。
如果以后回来的是 ID_RESET_PWD，就执行第二个 lambda。
```

---

### 二、[this] 捕获的是什么

```cpp
[this](QJsonObject jsonObj) {
    showTip(...);
}
```

这里的 `[this]` 捕获的是当前 `ResetDialog` 对象的地址。

它不是复制一份完整的 `ResetDialog` 对象。

可以理解成 lambda 里面保存了一个指针：

```cpp
ResetDialog* this;
```

所以 lambda 里面写：

```cpp
showTip(tr("参数错误"), false);
```

本质上等价于：

```cpp
this->showTip(tr("参数错误"), false);
```

访问 `ui` 也是一样：

```cpp
ui->err_tip->setText(...);
```

本质上等价于：

```cpp
this->ui->err_tip->setText(...);
```

---

### 三、为什么这里必须捕获 this

因为 `showTip()` 是 `ResetDialog` 的成员函数。

`ui`、`_handlers`、`_tip_errs` 也是 `ResetDialog` 的成员。

如果不写 `[this]`，lambda 里面就不能直接访问这些成员：

```cpp
// 错误理解示例：不捕获 this
[](QJsonObject jsonObj){
    showTip(tr("参数错误"), false);  // 无法直接访问 ResetDialog 成员函数
}
```

正确写法：

```cpp
[this](QJsonObject jsonObj){
    showTip(tr("参数错误"), false);
}
```

---

### 四、[this] 是值传递吗

可以说：

```text
[this] 是把 this 指针按值捕获。
```

但是不能说：

```text
[this] 复制了一份 ResetDialog 对象。
```

更准确地说：

```text
复制的是指针值，也就是对象地址。
没有复制整个对象。
```

比如当前 `ResetDialog` 对象在内存中的地址是：

```text
0x12345678
```

那么 `[this]` 捕获进去的是：

```text
this = 0x12345678
```

lambda 后面执行时，仍然通过这个地址去操作原来的 `ResetDialog` 对象。

---

### 五、insert 是什么

你的 `_handlers` 在 [resetdialog.h](resetdialog.h) 里类似这样声明：

```cpp
QMap<ReqId, std::function<void(const QJsonObject&)>> _handlers;
```

它是一个映射表：

```text
key   = ReqId，请求编号
value = 处理这个请求回包的函数
```

所以：

```cpp
_handlers.insert(ReqId::ID_GET_VERIFY_CODE, lambda);
```

意思是：

```text
往 _handlers 这张表里插入一条规则：
ID_GET_VERIFY_CODE 这个请求回来时，执行这个 lambda。
```

再比如：

```cpp
_handlers.insert(ReqId::ID_RESET_PWD, lambda);
```

意思是：

```text
ID_RESET_PWD 这个请求回来时，执行这个 lambda。
```

---

### 六、为什么要用 _handlers

因为 `ResetDialog` 会发送不止一种 HTTP 请求。

比如：

```text
获取验证码请求       ID_GET_VERIFY_CODE
重置密码请求         ID_RESET_PWD
```

它们都是通过 `HttpMgr` 发出去的，也都会通过类似的信号回来。

如果不用 `_handlers`，你可能要写很多 `if else`：

```cpp
if(id == ReqId::ID_GET_VERIFY_CODE){
    // 处理验证码回包
}
else if(id == ReqId::ID_RESET_PWD){
    // 处理重置密码回包
}
```

用了 `_handlers` 后，就可以提前把处理逻辑登记进去：

```cpp
_handlers.insert(ReqId::ID_GET_VERIFY_CODE, 处理验证码的函数);
_handlers.insert(ReqId::ID_RESET_PWD, 处理重置密码的函数);
```

服务器回包来了以后，只要按 `id` 查表执行：

```cpp
if(_handlers.contains(id)){
    _handlers[id](jsonObj);
}
```

---

### 七、完整调用链

以获取验证码为例：

```text
用户点击获取验证码
        ↓
ResetDialog::on_verify_btn_clicked_clicked()
        ↓
HttpMgr::PostHttpReq(..., ReqId::ID_GET_VERIFY_CODE, Modules::RESETMOD)
        ↓
服务器返回 JSON
        ↓
ResetDialog::slot_reset_mod_finish(id, res, err)
        ↓
解析 res 得到 jsonObj
        ↓
_handlers[id](jsonObj)
        ↓
执行 ID_GET_VERIFY_CODE 对应的 lambda
        ↓
showTip("验证码已发送到邮箱，注意查收", true)
```

以重置密码为例：

```text
用户点击确认重置
        ↓
发送 ID_RESET_PWD 请求
        ↓
服务器返回 JSON
        ↓
slot_reset_mod_finish(...)
        ↓
_handlers[ReqId::ID_RESET_PWD](jsonObj)
        ↓
执行重置密码对应的 lambda
        ↓
showTip("重置成功,点击返回登录", true)
```

---

### 八、一个重要注意点

因为 `[this]` 捕获的是指针，所以它要求：

```text
lambda 执行的时候，ResetDialog 对象还活着。
```

在你这里一般是合理的，因为 lambda 存在 `_handlers` 这个成员变量里：

```cpp
ResetDialog 对象存在
        ↓
_handlers 存在
        ↓
lambda 存在
```

当 `ResetDialog` 销毁时，`_handlers` 也会一起销毁，lambda 也就没了。

但是如果你把这个 lambda 存到全局变量、线程里、或者别的生命周期更长的对象里，就要小心野指针问题。

---

### 九、最核心的记忆

```cpp
[this]
```

记住：

```text
捕获当前对象的指针，不复制整个对象。
```

```cpp
_handlers.insert(id, lambda)
```

记住：

```text
把“请求编号”和“这个请求回来以后怎么处理”绑定在一起。
```

这两个东西合起来，就是：

```text
ResetDialog 先登记各种回包处理规则；
服务器回包时，根据 ReqId 找到对应 lambda；
lambda 通过 this 操作当前 ResetDialog 的界面和成员函数。
```

