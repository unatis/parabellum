#include "Player/PBLPlayerState.h"

#include "Net/UnrealNetwork.h"

void APBLPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APBLPlayerState, Hits);
	DOREPLIFETIME(APBLPlayerState, Headshots);
	DOREPLIFETIME(APBLPlayerState, Kills);
}
