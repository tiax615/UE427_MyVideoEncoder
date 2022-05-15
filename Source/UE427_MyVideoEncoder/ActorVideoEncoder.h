// caidu 20220512

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VideoEncoder.h"
#include "ActorVideoEncoder.generated.h"

UCLASS()
class UE427_MYVIDEOENCODER_API AActorVideoEncoder : public AActor
{
	GENERATED_BODY()
	
public:	
	AActorVideoEncoder();

	// For Send Msg
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	class AActorWebSocketServer* WebSocketServer;

protected:
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

private:
	FTimespan StartTime = 0;
	AVEncoder::FVideoEncoder::FLayerConfig VideoConfig;
	TUniquePtr<AVEncoder::FVideoEncoder> VideoEncoder;
	TSharedPtr<AVEncoder::FVideoEncoderInput> VideoEncoderInput;
	TMap<AVEncoder::FVideoEncoderInputFrame*, FTexture2DRHIRef> InputFrameTextureMap;

public:	
	virtual void Tick(float DeltaTime) override;

private:
	void CreateEncoder();
	void OnEncodedVideoFrame(uint32 LayerIndex, const AVEncoder::FVideoEncoderInputFrame* Frame, const AVEncoder::FCodecPacket& Packet);
	void OnFrameBufferReady(SWindow& SlateWindow, const FTexture2DRHIRef& FrameBuffer);
	void ProcessVideoFrame(const FTexture2DRHIRef& FrameBuffer);
	AVEncoder::FVideoEncoderInputFrame* ObtainInputFrame();
	FTimespan GetMediaTimestamp() const;
	void CopyTexture(const FTexture2DRHIRef& SourceTexture, FTexture2DRHIRef& DestinationTexture) const;
};
