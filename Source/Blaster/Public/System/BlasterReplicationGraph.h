// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ReplicationGraph.h"
#include "BlasterReplicationGraph.generated.h"

UCLASS()
class UBlasterReplicationGraphNode_AlwaysRelevant_WithPending : public UReplicationGraphNode_ActorList
{
	GENERATED_BODY()

public:
	UBlasterReplicationGraphNode_AlwaysRelevant_WithPending();
	virtual void PrepareForReplication() override;
};

UCLASS()
class UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam : public UReplicationGraphNode_ActorList
{
	GENERATED_BODY()

	virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;
	virtual void GatherActorListsForConnectionDefault(const FConnectionGatherActorListParameters& Params);
};

UCLASS()
class UTeamReplicationConnectionManager : public UNetReplicationGraphConnection
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantForConnectionNode;
	
	UPROPERTY()
	UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam* TeamConnectionNode;

	int32 Team = -1;
	
	TWeakObjectPtr<APawn> Pawn;
};

struct FTeamConnectionListMap: TMap<int32, TArray<UTeamReplicationConnectionManager*>>
{
	TArray<UTeamReplicationConnectionManager*>* GetConnectionArrayForTeam(int32 Team);
	
	void AddConnectionToTeam(int32 Team, UTeamReplicationConnectionManager* ConnManager);
	void RemoveConnectionFromTeam(int32 Team, UTeamReplicationConnectionManager* ConnManager);
	
	TArray<UTeamReplicationConnectionManager*> GetVisibleConnectionArrayForNonTeam(const APawn* ViewerPawn, int32 ViewerTeam) const;
};

UCLASS()
class BLASTER_API UBlasterReplicationGraph : public UReplicationGraph
{
	GENERATED_BODY()

public:
	UBlasterReplicationGraph();

	virtual void InitGlobalGraphNodes() override;
	virtual void InitConnectionGraphNodes(UNetReplicationGraphConnection* ConnectionManager) override;

	virtual void RemoveClientConnection(UNetConnection* NetConnection) override;
	virtual void ResetGameWorldState() override;
	
	virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo) override;
	virtual void RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo) override;

	void SetTeamForPlayerController(APlayerController* PlayerController, int32 Team);
	void HandlePendingActorsAndTeamRequests();

private:
	UPROPERTY()
	UBlasterReplicationGraphNode_AlwaysRelevant_WithPending* AlwaysRelevantNode;
	
	UPROPERTY()
	TArray<AActor*> PendingConnectionActors;
	TArray<TTuple<int32, APlayerController*>> PendingTeamRequests;

	friend UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam;
	FTeamConnectionListMap TeamConnectionListMap;
	
	UTeamReplicationConnectionManager* GetTeamReplicationConnectionManagerFromActor(const AActor* Actor);
};
