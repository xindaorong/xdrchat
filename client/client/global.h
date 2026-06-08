#ifndef GLOBAL_H
#define GLOBAL_H

#include <QStyle>
#include <QWidget>
#include <functional>
#include <QMetaType>

// 全局样式刷新函数，在设置了动态属性后调用它来让 QSS 立刻生效
extern std::function<void(QWidget*)> repolish;

// 修正：这些枚举在代码里是按 `ReqId::xxx` / `ErrorCode::xxx` / `Modules::xxx` 使用的，
// 所以这里必须定义成 enum class，不然会编译报错。
enum ReqId {
    ID_GET_VERIFY_CODE = 1001, // 获取验证码
    ID_REG_USER = 1002         // 注册用户
};

enum  ErrorCode {
    Success = 0,  // 成功
    ERR_JSON = 1, // JSON 解析失败
    ERR_NETWORK = 2
};

enum  Modules {
    REGISTERMOD = 0
};
extern QString gate_url_prefix;
Q_DECLARE_METATYPE(ReqId)
Q_DECLARE_METATYPE(ErrorCode)
Q_DECLARE_METATYPE(Modules)

#endif // GLOBAL_H
