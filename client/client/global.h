#ifndef GLOBAL_H
#define GLOBAL_H

#include <QStyle>
#include <QString>
#include <QWidget>
#include <functional>
#include <QMetaType>

// 全局样式刷新函数，在设置了动态属性后调用它来让 QSS 立刻生效
extern std::function<void(QWidget*)> repolish;
extern std::function<QString(QString)> xorString;
QString serverErrorToString(int error);
QString formatServerError(int error);
// 修正：这些枚举在代码里是按 `ReqId::xxx` / `ErrorCode::xxx` / `Modules::xxx` 使用的，
// 所以这里必须定义成 enum class，不然会编译报错。
enum ReqId {
    ID_GET_VERIFY_CODE = 1001, // 获取验证码
    ID_REG_USER = 1002,         // 注册用户
    ID_RESET_PWD=1003,   //重置密码
    ID_LOGIN_USER = 1004, //用户登录
    ID_CHAT_LOGIN = 1005, //登陆聊天服务器
    ID_CHAT_LOGIN_RSP= 1006, //登陆聊天服务器回包
    // ID_SEARCH_USER_REQ = 1007, //用户搜索请求
    // ID_SEARCH_USER_RSP = 1008, //搜索用户回包
    // ID_ADD_FRIEND_REQ = 1009,  //添加好友申请
    // ID_ADD_FRIEND_RSP = 1010, //申请添加好友回复
};

enum  ErrorCode {
    Success = 0,  // 成功
    ERR_JSON = 1, // JSON 解析失败
    ERR_NETWORK = 2,
    SERVER_ERR_JSON = 1001,
    SERVER_RPC_FAILED = 1002,
    SERVER_VERIFY_EXPIRED = 1003,
    SERVER_VERIFY_CODE_ERR = 1004,
    SERVER_USER_EXIST = 1005,
    SERVER_PASSWD_ERR = 1006,
    SERVER_EMAIL_NOT_MATCH = 1007,
    SERVER_PASSWD_UPDATE_FAILED = 1008,
    SERVER_PASSWD_INVALID = 1009,
    SERVER_TOKEN_INVALID = 1010,
    SERVER_UID_INVALID = 1011,
    SERVER_SQL_ERROR = 1012
};


enum Modules{
    REGISTERMOD = 0,
    RESETMOD = 1,
    LOGINMOD = 2,
};

enum TipErr {
    TIP_SUCCESS = 0,
    TIP_EMAIL_ERR = 1,
    TIP_PWD_ERR = 2,
    TIP_CONFIRM_ERR = 3,
    TIP_PWD_CONFIRM = 4,
    TIP_VARIFY_ERR = 5,
    TIP_USER_ERR = 6
};

enum ClickLbState {
    Normal = 0,//正常状态
    Selected = 1//选中状态
};
struct ServerInfo{
    QString Host;
    QString Port;
    QString Token;
    int Uid;
};
extern QString gate_url_prefix;
Q_DECLARE_METATYPE(ReqId)
Q_DECLARE_METATYPE(ErrorCode)
Q_DECLARE_METATYPE(Modules)

#endif // GLOBAL_H
