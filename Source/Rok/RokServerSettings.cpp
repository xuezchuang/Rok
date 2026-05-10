#include "RokServerSettings.h"

URokServerSettings::URokServerSettings()
	: ServerBaseUrl(TEXT("http://home.snowsome.com:35818"))
	, DefaultPlayerId(TEXT("ue-editor"))
	, bAutoConnectOnGameInstanceStart(true)
{
}
