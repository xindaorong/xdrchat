#include"global.h"
/**
 * @brief Lambda function that repolishes a QWidget to refresh its appearance.
 * 
 * This function unpolishes and re-polishes a QWidget, then triggers a repaint.
 * It's useful for applying style changes dynamically without recreating the widget.
 * 
 * @param w Pointer to the QWidget to be repolished. Must not be nullptr.
 * 
 * @note This operation:
 *       - Removes the widget's current style properties (unpolish)
 *       - Reapplies the style properties (polish)
 *       - Triggers a visual update to reflect style changes
 * 
 * @example
 *       repolish(myButton);  // Refresh button appearance
 * 
 * @see QWidget::style(), QStyle::unpolish(), QStyle::polish(), QWidget::update()
 */
std::function<void(QWidget*)> repolish=[](QWidget* w){
    w->style()->unpolish(w);//撤销旧样式
    w->style()->polish(w);//应用新的样式
    w->update();//尽快更新新的样式
};
QString gate_url_prefix = "";
//这是一个lambda表达式，用于刷新一个QWidget的外观。

