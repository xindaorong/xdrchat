const grpc = require('@grpc/grpc-js');
const redis_module = require('./redis')
const message_proto = require('./proto');
const emailModule = require('./email');
const const_module = require('./const');
const config_module = require('./config');
const { v4: uuidv4 } = require('uuid');

// 调用 GetVerifyCode(call, callback)
//       │
//       ▼
//   await redis_module.GetRedis(code_prefix + email)  ← 查询 Redis 有无旧验证码
//       │
//       ├── 有旧验证码（query_res != null） → 直接用旧验证码发邮件
//       │
//       └── 无旧验证码（query_res == null）
//               │
//               ├── uuidv4() 生成 UUID → 截取前4位作为验证码
//               │
//               ├── await SetRedisExpire(...)  ← 存 Redis，600秒过期
//               │       │
//               │       ├── 成功 → 发邮件 → callback 返回 Success
//               │       │
//               │       └── 失败 → callback 返回 RedisError，return 退出
//               │
//               └── catch 异常 → callback 返回 Exception
async function GetVerifyCode(call, callback){
    console.log("email is ", call.request.email);
    try{
        //获取value,看验证码有没有旧的
        let query_res=await redis_module.GetRedis(const_module.code_prefix + call.request.email);
        console.log("query_res is ", query_res);
        let uniqueId = query_res;
        if(query_res ==null)//没有旧验证码，生成新的验证码，并存入Redis，设置过期时间为600秒（10分钟）
        {
            uniqueId = uuidv4();
        
          if(uniqueId.length>4)
          {
            uniqueId = uniqueId.substring(0, 4);
          }
          let bres=await redis_module.SetRedisExpire(const_module.code_prefix + call.request.email, uniqueId, 600);
          if(!bres)
          {
            callback(null, {email: call.request.email, 
                error:const_module.Errors.RedisError
            });
            return;
          }
        } 
        console.log("uniqueId is ", uniqueId);
        let text_str = "您收到的验证码是：" + uniqueId +'请三分钟完成注册'
        //发送邮件
        let mailOptions = {
            from: config_module.email_user, // 发送方邮箱地址
            to: call.request.email, // 接收方邮箱地址
            subject: '验证码', // 邮件主题
            text: text_str, // 邮件内容
        };
       let send_result = await emailModule.SendMail(mailOptions);
       console.log("send_result is ", send_result)

       callback(null, {email: call.request.email, 
        error:const_module.Errors.Success
       });

     }
catch(error){
    console.log("error is ", error)

    callback(null, {email: call.request.email, 
        error:const_module.Errors.Exception
    });
 }
}
function main() {
    var server = new grpc.Server()
    server.addService(message_proto.VerifyService.service, { GetVerifyCode: GetVerifyCode })
    server.bindAsync('127.0.0.1:50051', grpc.ServerCredentials.createInsecure(), () => {
        server.start()
        console.log('grpc server started')        
    })
}

main()
