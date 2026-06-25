#ifndef TIMERBTN_H
#define TIMERBTN_H
#include <QPushButton>
#include <QTimer>
//1首先先声明这个类
class QMouseEvent;

class TimerBtn : public QPushButton
{
public:
    //将构造函数和析构函数声明为公有
    TimerBtn(QWidget *parent = nullptr);
    ~ TimerBtn();

    // 重写mouseReleaseEvent，为什么要进行重写？
    virtual void mouseReleaseEvent(QMouseEvent *e) override;
protected:

private:
    //定义俩个私有变量
    QTimer  *_timer;
    int _counter;
};

#endif // TIMERBTN_H
