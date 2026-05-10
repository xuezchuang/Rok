#include "RokGameModeBase.h"

#include "RokPlayerController.h"
#include "RokStrategyCameraPawn.h"

ARokGameModeBase::ARokGameModeBase()
{
	PlayerControllerClass = ARokPlayerController::StaticClass();
	DefaultPawnClass = ARokStrategyCameraPawn::StaticClass();
}
