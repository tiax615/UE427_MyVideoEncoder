// caidu 20220512

#include "ActorVideoEncoder.h"

#include "ActorWebSocketServer.h"
#include "VideoEncoderFactory.h"
#include "ScreenRendering.h"
#include "CommonRenderResources.h"

AActorVideoEncoder::AActorVideoEncoder()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AActorVideoEncoder::BeginPlay()
{
	Super::BeginPlay();

	// Create VideoEncoder in AsyncTask
	// Important: UE4.27 cant create VideoEncoder in main thread. WTF
	// but UE5 can.
	AsyncTask(ENamedThreads::AnyHiPriThreadHiPriTask,
		[this] ()
		{
			CreateEncoder();
		});

	// Get Screen Render Result
	// Callback: OnFrameBufferReady
	// https://www.freesion.com/article/68001472921/
	FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().AddUObject(this, &AActorVideoEncoder::OnFrameBufferReady);

	// For Send Msg
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	WebSocketServer = GetWorld()->SpawnActor<AActorWebSocketServer>(AActorWebSocketServer::StaticClass(), SpawnInfo);
	if (WebSocketServer)
	{
		WebSocketServer->Start(8081);
	}
}

void AActorVideoEncoder::BeginDestroy()
{
	Super::BeginDestroy();
	VideoEncoder = nullptr;
}

void AActorVideoEncoder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AActorVideoEncoder::CreateEncoder()
{
	if (!VideoEncoder) // need create video encoder.
	{
		UE_LOG(LogTemp, Warning, TEXT("%s start"), *FString(__FUNCTION__));
		StartTime = FTimespan::FromSeconds(FPlatformTime::Seconds());

		// Create VideoEncoder Config
		// TODO: can set?
		VideoConfig.Width = 1280;
		VideoConfig.Height = 720;
		VideoConfig.TargetBitrate = 5000000;
		VideoConfig.MaxBitrate = 20000000;
		VideoConfig.MaxFramerate = 60;
		// VideoConfig.H264Profile = AVEncoder::FVideoEncoder::H264Profile::BASELINE; // Default profile is ok.

		// Create VideoEncoderInput (Windows Only)
		if (GDynamicRHI)
		{
			FString RHIName = GDynamicRHI->GetName();
			UE_LOG(LogTemp, Warning, TEXT("RHIName: %s"), *RHIName);
			if (RHIName == TEXT("D3D11")) // UE4 Default
			{
				VideoEncoderInput = AVEncoder::FVideoEncoderInput::CreateForD3D11(
					GDynamicRHI->RHIGetNativeDevice(), VideoConfig.Width, VideoConfig.Height, true, IsRHIDeviceAMD());
			}
			else if (RHIName == TEXT("D3D12")) // UE5 Default
			{
				VideoEncoderInput = AVEncoder::FVideoEncoderInput::CreateForD3D12(
					GDynamicRHI->RHIGetNativeDevice(), VideoConfig.Width, VideoConfig.Height, true, IsRHIDeviceNVIDIA());
			}
		}

		// Create VideoEncoder
		const TArray<AVEncoder::FVideoEncoderInfo>& Available = AVEncoder::FVideoEncoderFactory::Get().GetAvailable();
		VideoEncoder = AVEncoder::FVideoEncoderFactory::Get().Create(Available[0].ID, VideoEncoderInput, VideoConfig);

		// Callback: OnEncodedPacket
		if (VideoEncoder)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s success"), *FString(__FUNCTION__));

			// OnEncodedVideoFrame
			VideoEncoder->SetOnEncodedPacket(
			[this](uint32 LayerIndex, const AVEncoder::FVideoEncoderInputFrame* Frame, const AVEncoder::FCodecPacket& Packet)
			{
				OnEncodedVideoFrame(LayerIndex, Frame, Packet);
			});
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("%s fail"), *FString(__FUNCTION__));
		}
	}
}

void AActorVideoEncoder::OnEncodedVideoFrame(uint32 LayerIndex, const AVEncoder::FVideoEncoderInputFrame* Frame, const AVEncoder::FCodecPacket& Packet)
{
	UE_LOG(LogTemp, Log, TEXT("%s FrameID:%d"), *FString(__FUNCTION__), Frame->GetFrameID());

	// TODO: Do Something! ASS U CAN.
	// For Send Msg
	WebSocketServer->SendBytes(TArray<uint8>(Packet.Data, Packet.DataSize));
	
	Frame->Release(); // important! memory overflow
}

void AActorVideoEncoder::OnFrameBufferReady(SWindow& SlateWindow, const FTexture2DRHIRef& FrameBuffer)
{
	if (VideoEncoder)
	{
		ProcessVideoFrame(FrameBuffer);
	}
}

void AActorVideoEncoder::ProcessVideoFrame(const FTexture2DRHIRef& FrameBuffer)
{
	FTimespan Now = GetMediaTimestamp();

	AVEncoder::FVideoEncoderInputFrame* InputFrame = ObtainInputFrame();
	const int32 FrameId = InputFrame->GetFrameID();
	InputFrame->SetTimestampUs(Now.GetTicks());
	UE_LOG(LogTemp, Log, TEXT("%s FrameID:%d"), *FString(__FUNCTION__), FrameId);
    
	CopyTexture(FrameBuffer, InputFrameTextureMap[InputFrame]);
	// InputFrameTextureMap[InputFrame] = FrameBuffer;

	// Encode
	AVEncoder::FVideoEncoder::FEncodeOptions EncodeOptions;
	VideoEncoder->Encode(InputFrame, EncodeOptions);
}

AVEncoder::FVideoEncoderInputFrame* AActorVideoEncoder::ObtainInputFrame()
{
	AVEncoder::FVideoEncoderInputFrame* InputFrame = VideoEncoderInput->ObtainInputFrame();

	if (!InputFrameTextureMap.Contains(InputFrame))
	{
		FString RHIName = GDynamicRHI->GetName();
		if (RHIName == TEXT("D3D11"))
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("VideoCapturerBackBuffer"));
			FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture2D(VideoConfig.Width, VideoConfig.Height,
				EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_Shared | TexCreate_RenderTargetable | TexCreate_UAV,
				ERHIAccess::CopyDest, CreateInfo);
			InputFrame->SetTexture((ID3D11Texture2D*)Texture->GetNativeResource(),
				[&, InputFrame](ID3D11Texture2D* NativeTexture)
				{
					InputFrameTextureMap.Remove(InputFrame);
				});
			InputFrameTextureMap.Add(InputFrame, Texture);
		}
		else if(RHIName == TEXT("D3D12"))
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("VideoCapturerBackBuffer"));
			FTexture2DRHIRef Texture = GDynamicRHI->RHICreateTexture2D(VideoConfig.Width, VideoConfig.Height,
				EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_Shared | TexCreate_RenderTargetable | TexCreate_UAV,
				ERHIAccess::CopyDest, CreateInfo);
			InputFrame->SetTexture((ID3D12Resource*)Texture->GetNativeResource(),
				[&, InputFrame](ID3D12Resource* NativeTexture)
				{
					InputFrameTextureMap.Remove(InputFrame);
				});
			InputFrameTextureMap.Add(InputFrame, Texture);
		}
        
		UE_LOG(LogClass, Log, TEXT("%d input frame buffer currently allocated."), InputFrameTextureMap.Num());
	}

	return InputFrame;
}

FTimespan AActorVideoEncoder::GetMediaTimestamp() const
{
	return FTimespan::FromSeconds(FPlatformTime::Seconds()) - StartTime;
}

// from GameplayMediaEncoder.cpp
void AActorVideoEncoder::CopyTexture(const FTexture2DRHIRef& SourceTexture, FTexture2DRHIRef& DestinationTexture) const
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	if(SourceTexture->GetFormat() == DestinationTexture->GetFormat() && SourceTexture->GetSizeXY() == DestinationTexture->GetSizeXY())
	{
		RHICmdList.CopyToResolveTarget(SourceTexture, DestinationTexture, FResolveParams{});
	}
	else // Texture format mismatch, use a shader to do the copy.
	{
		IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>("Renderer");

		// #todo-renderpasses there's no explicit resolve here? Do we need one?
		FRHIRenderPassInfo RPInfo(DestinationTexture, ERenderTargetActions::Load_Store);

		RHICmdList.BeginRenderPass(RPInfo, TEXT("CopyBackbuffer"));

		{
			RHICmdList.SetViewport(0, 0, 0.0f, DestinationTexture->GetSizeX(), DestinationTexture->GetSizeY(), 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

			// New engine version...
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
			TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			if(DestinationTexture->GetSizeX() != SourceTexture->GetSizeX() || DestinationTexture->GetSizeY() != SourceTexture->GetSizeY())
			{
				PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SourceTexture);
			}
			else
			{
				PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SourceTexture);
			}

			RendererModule->DrawRectangle(RHICmdList, 0, 0,                // Dest X, Y
			                              DestinationTexture->GetSizeX(),  // Dest Width
			                              DestinationTexture->GetSizeY(),  // Dest Height
			                              0, 0,                            // Source U, V
			                              1, 1,                            // Source USize, VSize
			                              DestinationTexture->GetSizeXY(), // Target buffer size
			                              FIntPoint(1, 1),                 // Source texture size
			                              VertexShader, EDRF_Default);
		}

		RHICmdList.EndRenderPass();
	}
}
