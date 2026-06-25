#include "timerbtn.h"
#include <QMouseEvent>
#include <QDebug>

TimerBtn::TimerBtn(QWidget *parent):QPushButton(parent),_counter(10)
{

    //初始化定时器，自己作为父对象，按钮销毁时，定时器自动跟着销毁，不会泄露内存
    //创建一个对象
    _timer = new QTimer(this);

    //连接信号与槽--把定时器的timeout信号连接到一个lambda匿名函数
    connect(_timer, &QTimer::timeout, [this](){
        _counter--;
        if(_counter <= 0){
            _timer->stop();
            _counter = 10;
            this->setText("获取");//重置为获取
            this->setEnabled(true);//按钮可点击
            return;
        }
        this->setText(QString::number(_counter));
    });
}

//停止计时
TimerBtn::~TimerBtn()
{
    _timer->stop();//停止计时
}

//处理鼠标释放事件就会触发下面的函数
void TimerBtn::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        // 在这里处理鼠标左键释放事件
        qDebug() << "MyButton was released!";
        this->setEnabled(false);//不可点击按钮
        this->setText(QString::number(_counter));//开始展示这个_counter的值
        _timer->start(1000);
        emit clicked(); // 发出clicked信号
    }
    // 调用基类的mouseReleaseEvent以确保正常的事件处理（如点击效果）
    QPushButton::mouseReleaseEvent(e);
}
