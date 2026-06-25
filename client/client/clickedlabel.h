//重写这个类是因为 QLabel 默认只是显示文字/图片，不具备“按钮式交互能力”。
#ifndef CLICKEDLABEL_H
#define CLICKEDLABEL_H
#include <QLabel>
#include "global.h"

class QMouseEvent;
class QEnterEvent;

class ClickedLabel:public QLabel
{
    Q_OBJECT
public:
    ClickedLabel(QWidget* parent);
    virtual void mousePressEvent(QMouseEvent *ev) override;
    virtual void enterEvent(QEnterEvent* event) override;
    virtual void leaveEvent(QEvent* event) override;
    void SetState(QString normal="", QString hover="", QString press="",
                  QString select="", QString select_hover="", QString select_press="");

    ClickLbState GetCurState();
protected:

private:
    QString _normal;
    QString _normal_hover;
    QString _normal_press;

    QString _selected;
    QString _selected_hover;
    QString _selected_press;

    ClickLbState _curstate;
signals:
    //信号的声明
    void clicked(void);

};

#endif // CLICKEDLABEL_H
