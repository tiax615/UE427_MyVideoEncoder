// caidu 20220511
// https://github.com/ljason1993/WebSocketServer-unreal

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "INetworkingWebSocket.h"
#include "IWebSocketServer.h"

#include "ActorWebSocketServer.generated.h"

UCLASS()
class UE427_MYVIDEOENCODER_API AActorWebSocketServer : public AActor
{
	GENERATED_BODY()
	
public:	
	AActorWebSocketServer();

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

public:	
	virtual void Tick(float DeltaTime) override;

	// web client message callback
	UFUNCTION(BlueprintImplementableEvent, Category = "WebSocketServer")
	void OnH5MsgCallback(const FString& msg);

	// Open WebSocket Server
	UFUNCTION(BlueprintCallable, Category = "WebSocketServer")
	bool Start(int Port);

	// Close WebSocket Server （The automatic call this func when BeginDestroy）
	UFUNCTION(BlueprintCallable, Category = "WebSocketServer")
	void Stop();

	// It is automatically called in actor tick to maintain the connection of websocket
	bool WebSocketServerTick(float DeltaTime);

	// TODO: Send information by client ID
	void Send(const FGuid& InTargetClientId, const TArray<uint8>& InUTF8Payload);

	// send message to all web client
	UFUNCTION(BlueprintCallable, Category = "WebSocketServer")
	void Send(const FString msg);

	// caidu 20220511: Send Bytes
	UFUNCTION(BlueprintCallable, Category = "WebSocketServer")
	void SendBytes(const TArray<uint8>& MsgBytes);

	// Returns whether the server is currently running
	UFUNCTION(BlueprintCallable, Category = "WebSocketServer")
	bool IsRunning() const;

	// Callback when a socket is closed
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnConnectionClosed, FGuid /*ClientId*/);
	FOnConnectionClosed& OnConnectionClosed() { return OnConnectionClosedDelegate; }

	void _DebugLog(FString msg, float delayTime, FColor color);

private:
	// Handles a new client connecting
	void OnWebSocketClientConnected(INetworkingWebSocket* Socket);

	// Handles sending the received packet to the message router.
	void ReceivedRawPacket(void* Data, int32 Size, FGuid ClientId);

	// Handles a client close
	void OnSocketClose(INetworkingWebSocket* Socket);

private:
	/** Holds a web socket connection to a client. */
	class FWebSocketConnection
	{
	public:

		explicit FWebSocketConnection(INetworkingWebSocket* InSocket)
			: Socket(InSocket)
			, Id(FGuid::NewGuid())
		{
		}

		FWebSocketConnection(FWebSocketConnection&& WebSocketConnection)
			: Id(WebSocketConnection.Id)
		{
			Socket = WebSocketConnection.Socket;
			WebSocketConnection.Socket = nullptr;
		}

		~FWebSocketConnection()
		{
			if (Socket)
			{
				delete Socket;
				Socket = nullptr;
			}
		}

		FWebSocketConnection(const FWebSocketConnection&) = delete;
		FWebSocketConnection& operator=(const FWebSocketConnection&) = delete;
		FWebSocketConnection& operator=(FWebSocketConnection&&) = delete;

		/** Underlying WebSocket. */
		INetworkingWebSocket* Socket = nullptr;

		/** Generated ID for this client. */
		FGuid Id;
	};
	
	/** Holds the LibWebSocket wrapper. */
	TUniquePtr<IWebSocketServer> Server;

	/** Holds all active connections. */
	TArray<FWebSocketConnection> Connections;

	/** Delegate triggered when a connection is closed */
	FOnConnectionClosed OnConnectionClosedDelegate;
};
