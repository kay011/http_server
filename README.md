# 简单的HTTP SERVER
支持GET 和 POST 请求
采用单线程Reactor网络模型
业务处理放入线程池中处理

定时器：清除处理长时间非活跃HTTP连接
## 编译
`sh build.sh`
## 启动
`./bin/http_server`

## 客户端测试
采用curl命令
`curl http://127.0.0.1:3490/hello`

响应:
`{"code":0,"msg":"hello world!"}`


## TODO

