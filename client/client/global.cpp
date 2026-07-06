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
//进行加密操作
std::function<QString(QString)> xorString = [](QString input){
    QString result = input; // 复制原始字符串，以便进行修改
    int length = input.length(); // 获取字符串的长度
    ushort xor_code = length % 255;
    for (int i = 0; i < length; ++i) {
        // 对每个字符进行异或操作
        // 注意：这里假设字符都是ASCII，因此直接转换为QChar
        result[i] = QChar(static_cast<ushort>(input[i].unicode() ^ xor_code));
    }
    return result;
};

QString serverErrorToString(int error)
{
    switch (error) {
    case Success:
        return QObject::tr("成功");
    case ERR_JSON:
        return QObject::tr("验证码服务 Redis 错误或本地 JSON 错误");
    case ERR_NETWORK:
        return QObject::tr("验证码服务异常或本地网络错误");
    case SERVER_ERR_JSON:
        return QObject::tr("服务端 JSON 解析失败");
    case SERVER_RPC_FAILED:
        return QObject::tr("RPC 调用失败，请检查 VerifyServer/StatusServer 是否启动或 proto 是否一致");
    case SERVER_VERIFY_EXPIRED:
        return QObject::tr("验证码已过期");
    case SERVER_VERIFY_CODE_ERR:
        return QObject::tr("验证码错误");
    case SERVER_USER_EXIST:
        return QObject::tr("用户或邮箱已存在");
    case SERVER_PASSWD_ERR:
        return QObject::tr("密码错误");
    case SERVER_EMAIL_NOT_MATCH:
        return QObject::tr("用户名和邮箱不匹配");
    case SERVER_PASSWD_UPDATE_FAILED:
        return QObject::tr("密码更新失败");
    case SERVER_PASSWD_INVALID:
        return QObject::tr("用户名或密码错误");
    case SERVER_TOKEN_INVALID:
        return QObject::tr("Token 失效");
    case SERVER_UID_INVALID:
        return QObject::tr("UID 无效");
    case SERVER_SQL_ERROR:
        return QObject::tr("数据库错误");
    default:
        return QObject::tr("未知错误");
    }
}

QString formatServerError(int error)
{
    return QObject::tr("%1，错误码：%2").arg(serverErrorToString(error)).arg(error);
}

