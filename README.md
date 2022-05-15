* UE4.27.2
* 依赖内置插件：
  * **HardwardEncoders**
  * Experimental WebSocket Networking Plugin
* AActorVideoEncoder：H264 编码
  * 创建 AActorWebSocketServer 实例
  * **捕获渲染结果**
  * **编码渲染结果**
  * 通过 AActorWebSocketServer 发送
* AActorWebSocketServer：WS Server

UE5.0.0 也可以使用，仅 packet.data 改为 packet.data.get()。