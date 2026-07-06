# Day18--QSS 动态属性与 setProperty 联动

## 1. 核心理解

Qt 的 QSS 可以根据控件的动态属性切换样式。

例如：

```css
#err_tip[state='normal'] {
    color: green;
}

#err_tip[state='err'] {
    color: red;
}
```

这里的意思是：

```text
objectName 是 err_tip
并且 state 属性等于 normal -> 文字变绿色

objectName 是 err_tip
并且 state 属性等于 err -> 文字变红色
```

## 2. C++ 代码如何触发 QSS

代码里通过 `setProperty` 修改动态属性：

```cpp
ui->err_tip->setProperty("state", "err");
repolish(ui->err_tip);
```

执行流程：

```text
setProperty("state", "err")
        |
        v
err_tip 控件拥有 state=err
        |
        v
repolish 重新刷新样式
        |
        v
QSS 命中 #err_tip[state='err']
        |
        v
错误提示文字变红
```

如果设置为：

```cpp
ui->err_tip->setProperty("state", "normal");
repolish(ui->err_tip);
```

就会命中：

```css
#err_tip[state='normal']
```

文字显示为绿色。

## 3. 项目中的典型用法

`showTip` 可以统一控制提示文字和样式：

```cpp
void RegisterDialog::showTip(const QString &str, bool b_ok)
{
    ui->err_tip->setText(str);
    ui->err_tip->setProperty("state", b_ok ? "normal" : "err");
    repolish(ui->err_tip);
}
```

总结：

```text
b_ok == true  -> state=normal -> 绿色提示
b_ok == false -> state=err    -> 红色提示
```

## 4. 为什么需要 repolish

`setProperty` 只是改了控件属性，Qt 不一定会自动重新匹配 QSS。

所以需要：

```cpp
repolish(ui->err_tip);
```

它的作用是让控件重新应用样式：

```text
取消旧样式
重新匹配 QSS
刷新控件显示
```

## 5. 一句话总结

```text
setProperty 改状态，QSS 根据状态选样式，repolish 负责刷新显示。
```

