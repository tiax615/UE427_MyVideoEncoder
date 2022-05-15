// caidu 20220511
// https://github.com/ljason1993/WebSocketServer-unreal

#include "ActorWebSocketServer.h"
#include "IWebSocketNetworkingModule.h"
#include <string>

// Sets default values
AActorWebSocketServer::AActorWebSocketServer()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AActorWebSocketServer::BeginPlay()
{
	Super::BeginPlay();
}

void AActorWebSocketServer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	WebSocketServerTick(DeltaTime);
}

void AActorWebSocketServer::BeginDestroy()
{
	Super::BeginDestroy();
	Stop();
}

void AActorWebSocketServer::Stop()
{
	if (IsRunning())
	{
		Server.Reset();
	}
}

bool AActorWebSocketServer::Start(int Port)
{
	FWebSocketClientConnectedCallBack CallBack;
	CallBack.BindUObject(this, &AActorWebSocketServer::OnWebSocketClientConnected);

	Server = FModuleManager::Get().LoadModuleChecked<IWebSocketNetworkingModule>(TEXT("WebSocketNetworking")).CreateServer();

	if (!Server || !Server->Init(Port, CallBack))
	{
		Server.Reset();
		return false;
	}

	return true;
}

bool AActorWebSocketServer::WebSocketServerTick(float DeltaTime)
{
	if (IsRunning())
	{
		Server->Tick();
		return true;
	}
	else
	{
		return false;
	}
}

void AActorWebSocketServer::Send(const FGuid& InTargetClientId, const TArray<uint8>& InUTF8Payload)
{
	if (FWebSocketConnection* Connection = Connections.FindByPredicate([&InTargetClientId](const FWebSocketConnection& InConnection)
	{ return InConnection.Id == InTargetClientId; }))
	{
		Connection->Socket->Send(InUTF8Payload.GetData(), InUTF8Payload.Num(), /*PrependSize=*/false);
	}
}

void AActorWebSocketServer::Send(const FString msg)
{
	FTCHARToUTF8 utf8Str(*msg);
	int32 utf8StrLen = utf8Str.Length();

	TArray<uint8> uint8Array;
	uint8Array.SetNum(utf8StrLen);
	memcpy(uint8Array.GetData(), utf8Str.Get(), utf8StrLen);

	for (auto& ws : Connections)
	{
		ws.Socket->Send(uint8Array.GetData(), uint8Array.Num(), /*PrependSize=*/false);
	}
}

void AActorWebSocketServer::SendBytes(const TArray<uint8>& MsgBytes)
{
	for (auto& ws : Connections) {
		ws.Socket->Send(MsgBytes.GetData(), MsgBytes.Num(), /*PrependSize=*/false);
	}
}

bool AActorWebSocketServer::IsRunning() const
{
	return !!Server;
}

void AActorWebSocketServer::_DebugLog(FString msg, float delayTime, FColor color)
{
	GEngine->AddOnScreenDebugMessage(-1, delayTime, color, " >LJason< " + msg);// 打印到屏幕
	UE_LOG(LogTemp, Log, TEXT(" >LJason<  %s"), *msg);// 打印到outputlog
}

void AActorWebSocketServer::OnWebSocketClientConnected(INetworkingWebSocket* Socket)
{
	_DebugLog("----OnWebSocketClientConnected ", 10, FColor::Red);
	if (ensureMsgf(Socket, TEXT("Socket was null while creating a new websocket connection.")))
	{
		FWebSocketConnection Connection = FWebSocketConnection{ Socket };

		FWebSocketPacketReceivedCallBack ReceiveCallBack;
		ReceiveCallBack.BindUObject(this, &AActorWebSocketServer::ReceivedRawPacket, Connection.Id);
		Socket->SetReceiveCallBack(ReceiveCallBack);

		FWebSocketInfoCallBack CloseCallback;
		CloseCallback.BindUObject(this, &AActorWebSocketServer::OnSocketClose, Socket);
		Socket->SetSocketClosedCallBack(CloseCallback);

		Connections.Add(MoveTemp(Connection));
	}
}

void AActorWebSocketServer::ReceivedRawPacket(void* Data, int32 Size, FGuid ClientId)
{
	TArrayView<uint8> dataArrayView = MakeArrayView(static_cast<uint8*>(Data), Size);
	const std::string cstr(reinterpret_cast<const char*>(
		dataArrayView.GetData()),
		dataArrayView.Num());
	FString frameAsFString = UTF8_TO_TCHAR(cstr.c_str());

	OnH5MsgCallback(frameAsFString);
}

void AActorWebSocketServer::OnSocketClose(INetworkingWebSocket* Socket)
{
	int32 Index = Connections.IndexOfByPredicate([Socket](const FWebSocketConnection& Connection) { return Connection.Socket == Socket; });

	_DebugLog("----OnSocketClose " + FString::FromInt(Index), 10, FColor::Red);
	if (Index != INDEX_NONE)
	{
		OnConnectionClosed().Broadcast(Connections[Index].Id);
		Connections.RemoveAtSwap(Index);
	}
}