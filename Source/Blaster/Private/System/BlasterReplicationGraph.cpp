// Fill out your copyright notice in the Description page of Project Settings.


#include "System/BlasterReplicationGraph.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Blaster/Blaster.h"

UBlasterReplicationGraphNode_AlwaysRelevant_WithPending::UBlasterReplicationGraphNode_AlwaysRelevant_WithPending()
{
	// Call PrepareForReplication before replication once per frame
	bRequiresPrepareForReplicationCall = true;
}

//Prepare会一直执行
void UBlasterReplicationGraphNode_AlwaysRelevant_WithPending::PrepareForReplication()
{
	UBlasterReplicationGraph* ReplicationGraph = Cast<UBlasterReplicationGraph>(GetOuter());
	UE_LOG(LogTemp, Warning, TEXT("RepG _AlwaysRelevant_WithPending Prepare"));
	ReplicationGraph->HandlePendingActorsAndTeamRequests();
}

//这个节点要导出哪些Actor可以同步
//也会一直执行
void UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam::GatherActorListsForConnection(
	const FConnectionGatherActorListParameters& Params)
{
	 // Get all other team members with the same team ID from ReplicationGraph->TeamConnectionListMap
	UE_LOG(LogTemp, Warning, TEXT("RepG AlwaysRelevant_ForTeam GetActors"));
	 UBlasterReplicationGraph* ReplicationGraph = Cast<UBlasterReplicationGraph>(GetOuter());
	 const UTeamReplicationConnectionManager* ConnectionManager = Cast<UTeamReplicationConnectionManager>(&Params.ConnectionManager);
	 if (ReplicationGraph && ConnectionManager && ConnectionManager->Team != -1)
	 {
	 	//UE_LOG(LogTemp, Warning, TEXT("RepG AlwaysRelevant_ForTeam GetActors CurrentTeam:%d"), ConnectionManager->Team);
	 	if (TArray<UTeamReplicationConnectionManager*>* TeamConnections = ReplicationGraph->TeamConnectionListMap.
	 		GetConnectionArrayForTeam(ConnectionManager->Team))
	 	{
	 		TArray<UTeamReplicationConnectionManager*> TeamConnectionsRef = *TeamConnections;
	 		for (const UTeamReplicationConnectionManager* TeamMember : *TeamConnections)
	 		{
	 			TeamMember->TeamConnectionNode->GatherActorListsForConnectionDefault(Params);
	 		}
	 		
	 		TArray<UTeamReplicationConnectionManager*> VisibleNonTeamConnections = ReplicationGraph->TeamConnectionListMap.GetVisibleConnectionArrayForNonTeam(ConnectionManager->Pawn.Get(), ConnectionManager->Team);
	 		for (const UTeamReplicationConnectionManager* NonTeamMember : VisibleNonTeamConnections)
	 		{
	 			NonTeamMember->TeamConnectionNode->GatherActorListsForConnectionDefault(Params);
	 		}
	 	}
	 	
	 	else
	 	{
	 		UE_LOG(LogTemp, Warning, TEXT("RepG AlwaysRelevant_ForTeam GetActors CurrentTeam:%d NotSameTeamCount"),
	 		       ConnectionManager->Team);
	 	}
	 }
	 else
	 {
	 	UE_LOG(LogTemp, Warning, TEXT("RepG AlwaysRelevant_ForTeam GetActors Default"));
	 	Super::GatherActorListsForConnection(Params);
	 }
}

void UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam::GatherActorListsForConnectionDefault(const FConnectionGatherActorListParameters& Params)
{
	Super::GatherActorListsForConnection(Params);
}

TArray<UTeamReplicationConnectionManager*>* FTeamConnectionListMap::GetConnectionArrayForTeam(int32 Team)
{
	return Find(Team);
}

void FTeamConnectionListMap::AddConnectionToTeam(int32 Team, UTeamReplicationConnectionManager* ConnManager)
{
	TArray<UTeamReplicationConnectionManager*>& TeamList = FindOrAdd(Team);
	TeamList.Add(ConnManager);
}

void FTeamConnectionListMap::RemoveConnectionFromTeam(int32 Team, UTeamReplicationConnectionManager* ConnManager)
{
	if (TArray<UTeamReplicationConnectionManager*>* TeamList = Find(Team))
	{
		TeamList->RemoveSwap(ConnManager);

		// Remove the team from the map if there are no more connections
		if (TeamList->Num() == 0)
		{
			Remove(Team);
		}
	}
}

// 函数定义：返回一个 TArray, 其中包含对观察者可见的非本队玩家连接管理器指针
TArray<UTeamReplicationConnectionManager*> FTeamConnectionListMap::GetVisibleConnectionArrayForNonTeam(const APawn* ViewerPawn, int32 ViewerTeam) const
{
	// 1. 创建一个空数组，用于存放最终可见的连接管理器对象
    TArray<UTeamReplicationConnectionManager*> VisibleConnections;


	// 2. 检查观察者 Pawn 是否有效 （未销毁且非空）
    if (!IsValid(ViewerPawn))
    {
    	// 无效则直接返回空数组
        return VisibleConnections;
    }

	// 3. 检查观察者 Pawn 获取其所属的世界 （World），用于后续射线检测
    const UWorld* World = ViewerPawn->GetWorld();
    if (!World)
    {
    	// 获取不到世界也返回空
        return VisibleConnections;
    }

    // 4. 获取当前映射中所有的团队 ID （键值），存入 TeamIDs 数组
    TArray<int32> TeamIDs;
    GetKeys(TeamIDs);

    // 设置碰撞忽略参数：忽略观察者自身及其队友的 Pawn
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(VisibilityTrace), false, ViewerPawn);
    if (const TArray<UTeamReplicationConnectionManager*>* TeamMembers = Find(ViewerTeam))
    {
        for (const UTeamReplicationConnectionManager* Member : *TeamMembers)
        {
            if (Member->Pawn.IsValid())
            {
                TraceParams.AddIgnoredActor(Member->Pawn.Get());
            }
        }
    }

    // 射线起点：观察者 Pawn 的位置加上一个高度偏移（可根据需要调整）
    const FVector TraceOffset(0.f, 0.f, 180.f);
    const FVector TraceStart = ViewerPawn->GetActorLocation() + TraceOffset;

    // 遍历所有非观察者团队的连接
    for (int32 TeamID : TeamIDs)
    {
        if (TeamID == ViewerTeam) continue;

        if (const TArray<UTeamReplicationConnectionManager*>* OtherTeamMembers = Find(TeamID))
        {
            for (UTeamReplicationConnectionManager* OtherMember : *OtherTeamMembers)
            {
                if (!OtherMember->Pawn.IsValid())
                {
                    continue;
                }

                APawn* OtherPawn = OtherMember->Pawn.Get();
                const FVector TraceEnd = OtherPawn->GetActorLocation() + TraceOffset;

                FHitResult Hit;
                // 使用自定义碰撞通道（请确保已在项目设置中定义该通道）
            	// ... 使用自定义通道
            	if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, COLLISION_VISIBILITY_CHECK, TraceParams))
            	{
            		// 未命中任何障碍物 → 可见
            		VisibleConnections.Add(OtherMember);
            	}
            }
        }
    }
    return VisibleConnections;
}

UBlasterReplicationGraph::UBlasterReplicationGraph()
{
	// Specify the connection graph class to use
	ReplicationConnectionManagerClass = UTeamReplicationConnectionManager::StaticClass();
}

void UBlasterReplicationGraph::InitGlobalGraphNodes()
{
	Super::InitGlobalGraphNodes();

	// Create the always relevant node
	AlwaysRelevantNode = CreateNewNode<UBlasterReplicationGraphNode_AlwaysRelevant_WithPending>();
	AddGlobalGraphNode(AlwaysRelevantNode);
}

void UBlasterReplicationGraph::InitConnectionGraphNodes(UNetReplicationGraphConnection* ConnectionManager)
{
	Super::InitConnectionGraphNodes(ConnectionManager);

	// Create the connection graph for the incoming connection
	UTeamReplicationConnectionManager* TeamRepConnection = Cast<UTeamReplicationConnectionManager>(ConnectionManager);

	if (ensure(TeamRepConnection))
	{
		TeamRepConnection->AlwaysRelevantForConnectionNode = CreateNewNode<UReplicationGraphNode_AlwaysRelevant_ForConnection>();
		AddConnectionGraphNode(TeamRepConnection->AlwaysRelevantForConnectionNode, ConnectionManager);
		
		TeamRepConnection->TeamConnectionNode = CreateNewNode<UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam>();
		AddConnectionGraphNode(TeamRepConnection->TeamConnectionNode, ConnectionManager);
	}
}

void UBlasterReplicationGraph::RemoveClientConnection(UNetConnection* NetConnection)
{
	int32 ConnectionId = 0;
	bool bFound = false;
	
	auto UpdateList = [&](TArray<TObjectPtr<UNetReplicationGraphConnection>>& List)
	{
		for (int32 idx = 0; idx < List.Num(); ++idx)
		{
			UTeamReplicationConnectionManager* ConnectionManager = Cast<UTeamReplicationConnectionManager>(Connections[idx]);
			repCheck(ConnectionManager);

			if (ConnectionManager->NetConnection == NetConnection)
			{
				ensure(!bFound);

				// Remove the connection from the team node if the team is valid
				if (ConnectionManager->Team != -1)
				{
					TeamConnectionListMap.RemoveConnectionFromTeam(ConnectionManager->Team, ConnectionManager);
				}

				// Also remove it from the input list
				List.RemoveAtSwap(idx, 1, EAllowShrinking::No);
				bFound = true;
			}
			else
			{
				ConnectionManager->ConnectionOrderNum = ConnectionId++;
			}
		}
	};

	UpdateList(Connections);
	UpdateList(PendingConnections);
}

void UBlasterReplicationGraph::ResetGameWorldState()
{
	Super::ResetGameWorldState();

	PendingConnectionActors.Reset();
	PendingTeamRequests.Reset();

	auto EmptyConnectionNode = [](TArray<TObjectPtr<UNetReplicationGraphConnection>>& GraphConnections)
	{
		for (UNetReplicationGraphConnection* GraphConnection : GraphConnections)
		{
			if (const UTeamReplicationConnectionManager* TeamRepConnection = Cast<UTeamReplicationConnectionManager>(GraphConnection))
			{
				// Clear out all always relevant actors
				// Seamless travel means that the team connections will still be relevant due to the controllers not being destroyed
				TeamRepConnection->AlwaysRelevantForConnectionNode->NotifyResetAllNetworkActors();
			}
		}
	};

	EmptyConnectionNode(PendingConnections);
	EmptyConnectionNode(Connections);
}

void UBlasterReplicationGraph::RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo,
                                                    FGlobalActorReplicationInfo& GlobalInfo)
{
	auto ActorClass = ActorInfo.Class;
	auto ActorInInfo = ActorInfo.GetActor();
	// UE_LOG(LogTemp, Warning, TEXT("RepG RouteAddNetworkActorToNodes Class:%s, HaveActor:%d"), *ActorClass->GetName(),
	//        ActorInInfo!=nullptr);
	// All clients must receive game states and player states
	if (ActorInfo.Class->IsChildOf(AGameStateBase::StaticClass()) || ActorInfo.Class->IsChildOf(APlayerState::StaticClass()))
	{
		UE_LOG(LogTemp, Warning, TEXT("RepG RouteAddNetworkActorToNodes Class:%s, AddToAlways"), *ActorClass->GetName());
		AlwaysRelevantNode->NotifyAddNetworkActor(ActorInfo);
	}
	// If not we see if it belongs to a connection
	else if (UTeamReplicationConnectionManager* ConnectionManager = GetTeamReplicationConnectionManagerFromActor(ActorInfo.GetActor()))
	{
		if (ActorInfo.Actor->bOnlyRelevantToOwner)
		{
			UE_LOG(LogTemp, Warning, TEXT("RepG RouteAddNetworkActorToNodes Class:%s, AddToAlwaysForConnection"), *ActorClass->GetName());
			ConnectionManager->AlwaysRelevantForConnectionNode->NotifyAddNetworkActor(ActorInfo);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RepG RouteAddNetworkActorToNodes Class:%s, AddToTeam"), *ActorClass->GetName());
			ConnectionManager->TeamConnectionNode->NotifyAddNetworkActor(ActorInfo);
			
			if (APawn* Pawn = Cast<APawn>(ActorInfo.GetActor()))
			{
				ConnectionManager->Pawn = Pawn;
			}
		}
	}
	else if(ActorInfo.Actor->GetNetOwner())
	{
		//UE_LOG(LogTemp, Warning, TEXT("RepG RouteAddNetworkActorToNodes Class:%s, Have Not RepConnection"), *ActorClass->GetName());
		// Add to PendingConnectionActors if the net connection is not ready yet
		PendingConnectionActors.Add(ActorInfo.GetActor());
	}
}

void UBlasterReplicationGraph::RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo)
{
	if (ActorInfo.Class->IsChildOf(AGameStateBase::StaticClass()) || ActorInfo.Class->IsChildOf(APlayerState::StaticClass()))
	{
		AlwaysRelevantNode->NotifyRemoveNetworkActor(ActorInfo);
	}
	else if (const UTeamReplicationConnectionManager* ConnectionManager = GetTeamReplicationConnectionManagerFromActor(ActorInfo.GetActor()))
	{
		if (ActorInfo.Actor->bOnlyRelevantToOwner)
		{
			ConnectionManager->AlwaysRelevantForConnectionNode->NotifyRemoveNetworkActor(ActorInfo);
		}
		else
		{
			ConnectionManager->TeamConnectionNode->NotifyRemoveNetworkActor(ActorInfo);
		}
	}
	else if (ActorInfo.Actor->GetNetOwner())
	{
		PendingConnectionActors.Remove(ActorInfo.GetActor());
	}
}

void UBlasterReplicationGraph::SetTeamForPlayerController(APlayerController* PlayerController, int32 Team)
{
	if (PlayerController)
	{
		if (UTeamReplicationConnectionManager* ConnectionManager = GetTeamReplicationConnectionManagerFromActor(PlayerController))
		{
			const int32 CurrentTeam = ConnectionManager->Team;
			if (CurrentTeam != Team)
			{
				// Remove the connection to the old team list
				if (CurrentTeam != -1)
				{
					UE_LOG(LogTemp, Warning, TEXT("RepG RemoveConnectionFromTeam Controller:%s"),*PlayerController->GetName());
					TeamConnectionListMap.RemoveConnectionFromTeam(CurrentTeam, ConnectionManager);
				}

				// Add the graph to the new team list
				if (Team != -1)
				{
					UE_LOG(LogTemp, Warning, TEXT("RepG AddConnectionToTeam Controller:%s"),*PlayerController->GetName());
					TeamConnectionListMap.AddConnectionToTeam(Team, ConnectionManager);
				}
				ConnectionManager->Team = Team;
			}
		}
		else
		{
			// Add to PendingTeamRequests if the net connection is not ready yet
			PendingTeamRequests.Emplace(Team, PlayerController);
		}
	}
}

void UBlasterReplicationGraph::HandlePendingActorsAndTeamRequests()
{
	// Setup all pending team requests
	if(PendingTeamRequests.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RepG HandleTeamRequests"));
		TArray<TTuple<int32, APlayerController*>> TempRequests = MoveTemp(PendingTeamRequests);

		for (const TTuple<int32, APlayerController*>& Request : TempRequests)
		{
			if (IsValid(Request.Value))
			{
				SetTeamForPlayerController(Request.Value, Request.Key);
			}
		}
	}

	// Set up all pending connections
	if (PendingConnectionActors.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RepG HandlePendingActors"));
		TArray<AActor*> PendingActors = MoveTemp(PendingConnectionActors);

		for (AActor* Actor : PendingActors)
		{
			if (IsValid(Actor))
			{
				FGlobalActorReplicationInfo& GlobalInfo = GlobalActorReplicationInfoMap.Get(Actor);
				RouteAddNetworkActorToNodes(FNewReplicatedActorInfo(Actor), GlobalInfo);
			}
		}
	}
}

UTeamReplicationConnectionManager* UBlasterReplicationGraph::GetTeamReplicationConnectionManagerFromActor(const AActor* Actor)
{
	if (Actor)
	{
		if (UNetConnection* NetConnection = Actor->GetNetConnection())
		{
			if (UTeamReplicationConnectionManager* ConnectionManager = Cast<UTeamReplicationConnectionManager>(FindOrAddConnectionManager(NetConnection)))
			{
				return ConnectionManager;
			}
		}
	}
	
	return nullptr;
}

