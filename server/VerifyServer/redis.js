// 导入项目配置模块（注意：moudle 应为 module，拼写有误）
const config_module = require('./config');

// 导入 ioredis —— Node.js 的 Redis 客户端库，用于连接和操作 Redis 数据库
const Redis = require('ioredis');

// 创建一个 Redis 客户端实例，连接到 Redis 服务器
const RedisCli = new Redis(
    {
        host:config_module.redis_host, // Redis 服务器的主机名
        port:config_module.redis_port, // Redis 服务器的端口号
        password:config_module.redis_passwd, // Redis 服务器的密码
    }
);

//监听错误信息，on是核心方法，用于注册事件监听器
RedisCli.on('error', function(err) { 

    //function(err){}
    console.log('Redis Client connect Error', err); //打印错误信息
    
    RedisCli.quit(); //断开与Redis服务器的连接,优雅关闭连接
}); 
// 调用 GetRedis('some_key')
//       │
//       ▼
//   await RedisCli.get('some_key')  ← 发送 GET 命令到 Redis
//       │
//       ├── 成功，key存在 → 返回 value
//       │
//       ├── 成功，key不存在 → 返回 null，打印 "This key cannot be find..."
//       │
//       └── 网络/连接异常 → catch 捕获 → 打印错误 → 返回 null


//async function GetRedis(key) 是一个异步函数，用于从 Redis 数据库中获取指定 key 的值。
// 它使用 await 关键字等待 RedisCli.get(key) 的结果，并根据结果进行处理。
// 如果 key 存在，返回对应的 value；如果 key 不存在，返回 null 并打印提示信息；如果发生网络或连接异常，捕获错误并打印相关信息，然后返回 null。
async function GetRedis(key) {
    
    try{
        //const表明声明一个常量
        const result = await RedisCli.get(key)
        if(result === null){
          console.log('result:','<'+result+'>', 'This key cannot be find...')
          return null
        }
        console.log('Result:','<'+result+'>','Get key success!...');
        return result
    }catch(error){
        console.log('GetRedis error is', error);
        return null
    }

  }

  /**
 * 根据key查询redis中是否存在key
 * @param {*} key 
 * @returns 
 */
async function QueryRedis(key) {
    try{
        const result = await RedisCli.exists(key)
        //判断该值是否为空 如果为空返回null,
        if(result === 0){//注意是是三个等号判断是否严格等于
          consoel.log('result:','<'+result+'>', 'This key is  null...');
          return null
        }
        console.log('Result:','<'+result+'>','With this value!...');
        return result
    }catch(error){
        console.log('QueryRedis error is', error);
        return null
    }
}
/**
 * 设置key和value，并过期时间
 * @param {*} key 
 * @param {*} value 
 * @param {*} exptime 
 * @returns 
 */

//这是写操作，封装一般返回true 和false
async function SetRedisExpire(key, value, exptime) {
    try{
        //设置键和值
        await RedisCli.set(key, value)
        //设置过期时间
        await RedisCli.expire(key, exptime)
        
        return true
    }catch(error){
        console.log('SetRedisExpire error is', error);
        return false
    }
}
/**
* 退出函数
*/

function Quit() {
    RedisCli.quit();
}
// 导出函数，使其在其他模块中可用
module.exports = {GetRedis, QueryRedis, SetRedisExpire, Quit}; 


