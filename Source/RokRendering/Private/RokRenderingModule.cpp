#include "RokCityGammaCompositor.h"
#include "RokCityGammaRaster.h"

#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

class FRokRenderingModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// RokRendering is loaded at PostConfigInit, before global shaders are compiled.
		AddShaderSourceDirectoryMapping(TEXT("/Rok"), FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders")));
		// A scene view extension needs GEngine->ViewExtensions, which does not exist
		// at PostConfigInit. Keep only the shader mapping at this early phase.
		if (GEngine)
		{
			StartCompositor();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
				this, &FRokRenderingModule::StartCompositor);
		}
	}

	virtual void ShutdownModule() override
	{
		if (PostEngineInitHandle.IsValid())
		{
			FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
			PostEngineInitHandle.Reset();
		}
		FRokCityGammaCompositor::Shutdown();
		FRokCityGammaRaster::Shutdown();
	}

private:
	void StartCompositor()
	{
		if (GEngine && !bCompositorStarted)
		{
			FRokCityGammaCompositor::Startup();
			FRokCityGammaRaster::Startup();
			bCompositorStarted = true;
		}
	}

	FDelegateHandle PostEngineInitHandle;
	bool bCompositorStarted = false;
};

IMPLEMENT_MODULE(FRokRenderingModule, RokRendering)
