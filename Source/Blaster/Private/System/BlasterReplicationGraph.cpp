// Fill out your copyright notice in the Description page of Project Settings.

// 包含必要的基础、框架以及项目内的头文件
#include "System/BlasterReplicationGraph.h" // 当前类的头文件声明
#include "GameFramework/GameStateBase.h"    // GameState，用于判断全局同步Actor
#include "GameFramework/PlayerState.h"      // PlayerState，用于判断全局同步Actor
#include "Engine/World.h"                   // 世界上下文，射线检测等操作需要
#include "CollisionQueryParams.h"           // 碰撞查询参数配置，用于视线检测
#include "Blaster/Blaster.h"                // 项目主头文件

// 构造函数：初始化自定义的全局“始终相关”图节点
UBlasterReplicationGraphNode_AlwaysRelevant_WithPending::UBlasterReplicationGraphNode_AlwaysRelevant_WithPending()
{
    // 标记为 true 后，引擎会在每帧执行网络复制前自动调用 PrepareForReplication()
    bRequiresPrepareForReplicationCall = true;
}

// 每帧执行，用于在正式分发网络数据前处理挂起的任务
void UBlasterReplicationGraphNode_AlwaysRelevant_WithPending::PrepareForReplication()
{
    // 获取当前节点所属的 ReplicationGraph 实例
    UBlasterReplicationGraph* ReplicationGraph = Cast<UBlasterReplicationGraph>(GetOuter());
    
    // 打印调试信息，标记 Prepare 执行
    UE_LOG(LogTemp, Warning, TEXT("RepG _AlwaysRelevant_WithPending Prepare"));
    
    // 通知 ReplicationGraph 处理那些因网络连接未就绪而被挂起的 Actor 与组队请求
    ReplicationGraph->HandlePendingActorsAndTeamRequests();
}

// 核心节点方法（每帧执行）：为特定的玩家（Connection）收集需要进行网络同步的 Actor 列表
void UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam::GatherActorListsForConnection(
    const FConnectionGatherActorListParameters& Params)
{
     UE_LOG(LogTemp, Warning, TEXT("RepG AlwaysRelevant_ForTeam GetActors"));
     
     // 获取 ReplicationGraph 实例以及转换为自定义的连接管理器
     UBlasterReplicationGraph* ReplicationGraph = Cast<UBlasterReplicationGraph>(GetOuter());
     const UTeamReplicationConnectionManager* ConnectionManager = Cast<UTeamReplicationConnectionManager>(&Params.ConnectionManager);
     
     // 如果 Graph、连接管理器有效，且玩家已经被分配了合法的队伍 (Team != -1)
     if (ReplicationGraph && ConnectionManager && ConnectionManager->Team != -1)
     {
       // 尝试获取当前玩家所属队伍的完整连接列表 (所有的队友)
       if (TArray<UTeamReplicationConnectionManager*>* TeamConnections = ReplicationGraph->TeamConnectionListMap.
          GetConnectionArrayForTeam(ConnectionManager->Team))
       {
          // 拷贝一份数组的引用（此处原本声明未使用，可能是用于其他安全检查保留）
          TArray<UTeamReplicationConnectionManager*> TeamConnectionsRef = *TeamConnections;
          
          // 1. 遍历当前队伍的所有成员 (队友逻辑)
          for (const UTeamReplicationConnectionManager* TeamMember : *TeamConnections)
          {
             // 使用默认方法，将队友的同步数据无条件加入到当前玩家的接收列表中（队友永远可见）
             TeamMember->TeamConnectionNode->GatherActorListsForConnectionDefault(Params);
          }
          
          // 2. 检查非队友连接（敌人逻辑）：执行基于视线的遮挡剔除，返回可以被当前玩家看见的敌人列表
          TArray<UTeamReplicationConnectionManager*> VisibleNonTeamConnections = ReplicationGraph->TeamConnectionListMap.GetVisibleConnectionArrayForNonTeam(ConnectionManager->Pawn.Get(), ConnectionManager->Team);
          
          // 遍历所有可见的敌人
          for (const UTeamReplicationConnectionManager* NonTeamMember : VisibleNonTeamConnections)
          {
             // 将可见的敌人加入当前玩家的接收列表中，不可见的敌人将被忽略，从而节省带宽
             NonTeamMember->TeamConnectionNode->GatherActorListsForConnectionDefault(Params);
          }
       }
       else
       {
          // 如果根据队伍ID找不到任何成员列表，抛出警告（异常状态）
          UE_LOG(LogTemp, Warning, TEXT("RepG AlwaysRelevant_ForTeam GetActors CurrentTeam:%d NotSameTeamCount"),
                 ConnectionManager->Team);
       }
     }
     else
     {
       // 如果玩家未分配队伍或未完全就绪，降级使用基类的默认收集逻辑
       UE_LOG(LogTemp, Warning, TEXT("RepG AlwaysRelevant_ForTeam GetActors Default"));
       Super::GatherActorListsForConnection(Params);
     }
}

// 封装的默认调用：直接调用父类的 GatherActorListsForConnection
void UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam::GatherActorListsForConnectionDefault(const FConnectionGatherActorListParameters& Params)
{
    Super::GatherActorListsForConnection(Params);
}

// 结构体方法：根据给定的队伍ID，返回存储该队伍所有玩家连接管理器的数组
TArray<UTeamReplicationConnectionManager*>* FTeamConnectionListMap::GetConnectionArrayForTeam(int32 Team)
{
    return Find(Team); // TMap 的查找操作
}

// 结构体方法：将一个玩家的网络连接管理器添加到指定的队伍列表中
void FTeamConnectionListMap::AddConnectionToTeam(int32 Team, UTeamReplicationConnectionManager* ConnManager)
{
    // 如果队伍存在则获取，不存在则创建
    TArray<UTeamReplicationConnectionManager*>& TeamList = FindOrAdd(Team);
    // 将连接管理器加入队伍数组
    TeamList.Add(ConnManager);
}

// 结构体方法：从指定的队伍列表中移除某个网络连接管理器
void FTeamConnectionListMap::RemoveConnectionFromTeam(int32 Team, UTeamReplicationConnectionManager* ConnManager)
{
    // 查找指定队伍
    if (TArray<UTeamReplicationConnectionManager*>* TeamList = Find(Team))
    {
       // 从数组中快速移除并交换（不保持原数组顺序，但性能更高）
       TeamList->RemoveSwap(ConnManager);

       // 如果该队伍已经没有成员了，清理内存占用，将其从 Map 中移除
       if (TeamList->Num() == 0)
       {
          Remove(Team);
       }
    }
}

// 核心优化函数：根据观察者的 Pawn 获取当前在视野内且非同队伍的敌方连接（遮挡剔除）
TArray<UTeamReplicationConnectionManager*> FTeamConnectionListMap::GetVisibleConnectionArrayForNonTeam(const APawn* ViewerPawn, int32 ViewerTeam) const
{
    // 1. 初始化空数组，用于存储可见的敌人
    TArray<UTeamReplicationConnectionManager*> VisibleConnections;

    // 2. 如果观察者的 Pawn 已经销毁或为空，直接返回空数组
    if (!IsValid(ViewerPawn))
    {
        return VisibleConnections;
    }

    // 3. 获取世界实例以进行射线检测，获取失败则返回空
    const UWorld* World = ViewerPawn->GetWorld();
    if (!World)
    {
        return VisibleConnections;
    }

    // 4. 获取当前游戏中所有存在的队伍 ID
    TArray<int32> TeamIDs;
    GetKeys(TeamIDs);

    // 5. 配置射线碰撞参数
    // SCENE_QUERY_STAT(VisibilityTrace) 用于性能分析统计
    // false 表示不使用复杂碰撞，ViewerPawn 表示在射线检测中忽略观察者自己
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(VisibilityTrace), false, ViewerPawn);
    
    // 如果观察者有队伍，将其队友也加入射线的忽略列表（视线穿过队友时不算被挡住）
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

    // 6. 定义射线的起点，基于观察者位置上移 180 单位（模拟眼部/头部高度）
    const FVector TraceOffset(0.f, 0.f, 180.f);
    const FVector TraceStart = ViewerPawn->GetActorLocation() + TraceOffset;

    // 7. 遍历全场所有队伍
    for (int32 TeamID : TeamIDs)
    {
        // 跳过观察者自己所在的队伍
        if (TeamID == ViewerTeam) continue;

        // 获取敌方队伍的成员列表
        if (const TArray<UTeamReplicationConnectionManager*>* OtherTeamMembers = Find(TeamID))
        {
            // 遍历每个敌对玩家
            for (UTeamReplicationConnectionManager* OtherMember : *OtherTeamMembers)
            {
                // 如果敌人的 Pawn 已销毁则跳过
                if (!OtherMember->Pawn.IsValid())
                {
                    continue;
                }

                // 确定射线检测的终点（敌人的位置上移 180 单位）
                APawn* OtherPawn = OtherMember->Pawn.Get();
                const FVector TraceEnd = OtherPawn->GetActorLocation() + TraceOffset;

                FHitResult Hit;
                // 发射一条射线，检测从观察者到敌人之间是否有障碍物
                // COLLISION_VISIBILITY_CHECK 需要在工程的 DefaultEngine.ini 中预先定义
                if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, COLLISION_VISIBILITY_CHECK, TraceParams))
                {
                   // 如果 LineTrace 返回 false，说明射线未击中任何障碍物，即该敌人可见
                   VisibleConnections.Add(OtherMember);
                }
            }
        }
    }
    // 返回所有对该观察者可见的敌人连接
    return VisibleConnections;
}

// 自定义 ReplicationGraph 的构造函数
UBlasterReplicationGraph::UBlasterReplicationGraph()
{
    // 指定后续生成网络连接管理器时，使用我们自定义的 UTeamReplicationConnectionManager 类
    ReplicationConnectionManagerClass = UTeamReplicationConnectionManager::StaticClass();
}

// 重写全局图节点初始化函数（服务器启动阶段调用）
void UBlasterReplicationGraph::InitGlobalGraphNodes()
{
    Super::InitGlobalGraphNodes(); // 先执行引擎底层初始化逻辑

    // 实例化自定义带有 Pending 逻辑的全局相关节点
    AlwaysRelevantNode = CreateNewNode<UBlasterReplicationGraphNode_AlwaysRelevant_WithPending>();
    // 将其加入到全局图节点序列中
    AddGlobalGraphNode(AlwaysRelevantNode);
}

// 当新的玩家客户端连接到服务器时调用，初始化其专属的图节点
void UBlasterReplicationGraph::InitConnectionGraphNodes(UNetReplicationGraphConnection* ConnectionManager)
{
    Super::InitConnectionGraphNodes(ConnectionManager);

    // 将引擎生成的默认管理器转为我们的团队管理器类
    UTeamReplicationConnectionManager* TeamRepConnection = Cast<UTeamReplicationConnectionManager>(ConnectionManager);

    // 确保转换成功
    if (ensure(TeamRepConnection))
    {
       // 为当前玩家连接创建一个私有的“始终相关”节点（用于同步只给该玩家本人看的Actor，如其手持武器详情等）
       TeamRepConnection->AlwaysRelevantForConnectionNode = CreateNewNode<UReplicationGraphNode_AlwaysRelevant_ForConnection>();
       AddConnectionGraphNode(TeamRepConnection->AlwaysRelevantForConnectionNode, ConnectionManager);
       
       // 为当前玩家创建一个处理团队和可见性同步逻辑的专属节点
       TeamRepConnection->TeamConnectionNode = CreateNewNode<UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam>();
       AddConnectionGraphNode(TeamRepConnection->TeamConnectionNode, ConnectionManager);
    }
}

// 当客户端断开连接时清理其残留数据
void UBlasterReplicationGraph::RemoveClientConnection(UNetConnection* NetConnection)
{
    int32 ConnectionId = 0; // 用于重新排布后续元素的索引ID
    bool bFound = false;    // 标识是否找到了对应断连的客户端
    
    // 定义局部 Lambda：遍历一个指定的连接列表，寻找目标并删除
    auto UpdateList = [&](TArray<TObjectPtr<UNetReplicationGraphConnection>>& List)
    {
       for (int32 idx = 0; idx < List.Num(); ++idx)
       {
          // 转换出对应的管理类对象
          UTeamReplicationConnectionManager* ConnectionManager = Cast<UTeamReplicationConnectionManager>(Connections[idx]);
          repCheck(ConnectionManager); // 确保指针有效（宏类似 check()）

          // 匹配到了断开的 NetConnection
          if (ConnectionManager->NetConnection == NetConnection)
          {
             ensure(!bFound); // 确保只被清理了一次，不发生逻辑错误

             // 如果该玩家已经加入了队伍，将其从自定义的团队映射表中除名
             if (ConnectionManager->Team != -1)
             {
                TeamConnectionListMap.RemoveConnectionFromTeam(ConnectionManager->Team, ConnectionManager);
             }

             // 从维护的网络大名单中移除该对象
             List.RemoveAtSwap(idx, 1, EAllowShrinking::No);
             bFound = true; // 标记已处理
          }
          else
          {
             // 对于没有断连的其他玩家，重新整理并修正他们的内部排序索引
             ConnectionManager->ConnectionOrderNum = ConnectionId++;
          }
       }
    };

    // 分别在正式网络连接列表和未决（挂起）连接列表中执行上述清理逻辑
    UpdateList(Connections);
    UpdateList(PendingConnections);
}

// 在世界切换（例如无缝漫游换地图）时重置游戏状态
void UBlasterReplicationGraph::ResetGameWorldState()
{
    Super::ResetGameWorldState(); // 父类基础清理

    // 清空缓存队列（上一张地图未处理完的遗留数据清空）
    PendingConnectionActors.Reset();
    PendingTeamRequests.Reset();

    // 定义局部 Lambda：清理每个玩家私有节点上的数据
    auto EmptyConnectionNode = [](TArray<TObjectPtr<UNetReplicationGraphConnection>>& GraphConnections)
    {
       for (UNetReplicationGraphConnection* GraphConnection : GraphConnections)
       {
          if (const UTeamReplicationConnectionManager* TeamRepConnection = Cast<UTeamReplicationConnectionManager>(GraphConnection))
          {
             // 因为无缝漫游不会销毁 Controller 及其 Connection节点，所以只需重置它内部追踪的 Actor
             TeamRepConnection->AlwaysRelevantForConnectionNode->NotifyResetAllNetworkActors();
          }
       }
    };

    // 执行私有节点内数据重置
    EmptyConnectionNode(PendingConnections);
    EmptyConnectionNode(Connections);
}

// 网络拓扑路由核心机制：当有一个新的 Actor 产生且需同步时，决定它该挂载到哪个同步节点上
void UBlasterReplicationGraph::RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo,
                                                    FGlobalActorReplicationInfo& GlobalInfo)
{
    auto ActorClass = ActorInfo.Class;
    auto ActorInInfo = ActorInfo.GetActor();
    
    // 如果是游戏状态或玩家状态基类，它们应被分发到全局始终相关节点（所有人都必须同步这些宏观数据）
    if (ActorInfo.Class->IsChildOf(AGameStateBase::StaticClass()) || ActorInfo.Class->IsChildOf(APlayerState::StaticClass()))
    {
       UE_LOG(LogTemp, Warning, TEXT("RepG RouteAddNetworkActorToNodes Class:%s, AddToAlways"), *ActorClass->GetName());
       AlwaysRelevantNode->NotifyAddNetworkActor(ActorInfo);
    }
    // 否则，如果该 Actor 归属于某个特定的客户端/玩家 (能获取到该玩家的网络连接器)
    else if (UTeamReplicationConnectionManager* ConnectionManager = GetTeamReplicationConnectionManagerFromActor(ActorInfo.GetActor()))
    {
       // 如果该 Actor 的属性 bOnlyRelevantToOwner 被勾选了（意味着只同步给拥有者自身看）
       if (ActorInfo.Actor->bOnlyRelevantToOwner)
       {
          UE_LOG(LogTemp, Warning, TEXT("RepG RouteAddNetworkActorToNodes Class:%s, AddToAlwaysForConnection"), *ActorClass->GetName());
          // 挂载到该玩家独有的私有节点上
          ConnectionManager->AlwaysRelevantForConnectionNode->NotifyAddNetworkActor(ActorInfo);
       }
       else
       {
          // 否则（例如玩家控制的化身 Pawn 本身），挂载到该玩家的队伍计算节点上，受前文的队伍逻辑及射线遮挡判断约束
          UE_LOG(LogTemp, Warning, TEXT("RepG RouteAddNetworkActorToNodes Class:%s, AddToTeam"), *ActorClass->GetName());
          ConnectionManager->TeamConnectionNode->NotifyAddNetworkActor(ActorInfo);
          
          // 如果这个 Actor 正是 Pawn，顺便把连接管理器里的 Pawn 引用设置好（给前面的射线检测用）
          if (APawn* Pawn = Cast<APawn>(ActorInfo.GetActor()))
          {
             ConnectionManager->Pawn = Pawn;
          }
       }
    }
    // 如果没有找到对应的连接管理器，但该 Actor 又明确拥有 NetOwner (网络属主)
    // 这表明该网络连接处于不稳定或尚未完全建立的临时状态
    else if(ActorInfo.Actor->GetNetOwner())
    {
       // 放入延后处理队列，等网络链接和身份准备就绪后，由 PrepareForReplication 来处理它
       PendingConnectionActors.Add(ActorInfo.GetActor());
    }
}

// 移除路由逻辑：与 RouteAddNetworkActorToNodes 逆向操作，当销毁或者停止同步时剥离节点
void UBlasterReplicationGraph::RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo)
{
    // 如果是 GameState 或 PlayerState，从全局节点摘除
    if (ActorInfo.Class->IsChildOf(AGameStateBase::StaticClass()) || ActorInfo.Class->IsChildOf(APlayerState::StaticClass()))
    {
       AlwaysRelevantNode->NotifyRemoveNetworkActor(ActorInfo);
    }
    // 如果归属玩家
    else if (const UTeamReplicationConnectionManager* ConnectionManager = GetTeamReplicationConnectionManagerFromActor(ActorInfo.GetActor()))
    {
       // 从私有节点摘除
       if (ActorInfo.Actor->bOnlyRelevantToOwner)
       {
          ConnectionManager->AlwaysRelevantForConnectionNode->NotifyRemoveNetworkActor(ActorInfo);
       }
       // 或者从队伍节点摘除
       else
       {
          ConnectionManager->TeamConnectionNode->NotifyRemoveNetworkActor(ActorInfo);
       }
    }
    // 如果仍在等待队列中，直接从缓存队列中抛弃
    else if (ActorInfo.Actor->GetNetOwner())
    {
       PendingConnectionActors.Remove(ActorInfo.GetActor());
    }
}

// 给某个玩家分配/更改队伍
void UBlasterReplicationGraph::SetTeamForPlayerController(APlayerController* PlayerController, int32 Team)
{
    if (PlayerController)
    {
       // 拿到该控制器底层的 ConnectionManager
       if (UTeamReplicationConnectionManager* ConnectionManager = GetTeamReplicationConnectionManagerFromActor(PlayerController))
       {
          const int32 CurrentTeam = ConnectionManager->Team;
          // 如果当前队伍和想指定的队伍不一样，则执行移交
          if (CurrentTeam != Team)
          {
             // 如果原本已经存在于某个队伍里了
             if (CurrentTeam != -1)
             {
                UE_LOG(LogTemp, Warning, TEXT("RepG RemoveConnectionFromTeam Controller:%s"),*PlayerController->GetName());
                // 先从老队伍将其除名
                TeamConnectionListMap.RemoveConnectionFromTeam(CurrentTeam, ConnectionManager);
             }

             // 如果要加入的目标队伍有效
             if (Team != -1)
             {
                UE_LOG(LogTemp, Warning, TEXT("RepG AddConnectionToTeam Controller:%s"),*PlayerController->GetName());
                // 加入新队伍的列表
                TeamConnectionListMap.AddConnectionToTeam(Team, ConnectionManager);
             }
             // 更新自身属性
             ConnectionManager->Team = Team;
          }
       }
       else
       {
          // 若底层网络暂未连接成功，把此改队请求缓存，稍后再试
          PendingTeamRequests.Emplace(Team, PlayerController);
       }
    }
}

// 该函数由 PrepareForReplication 驱动，清空积压处理
void UBlasterReplicationGraph::HandlePendingActorsAndTeamRequests()
{
    // 1. 处理所有挂起的改组队请求
    if(PendingTeamRequests.Num() > 0)
    {
       UE_LOG(LogTemp, Warning, TEXT("RepG HandleTeamRequests"));
       
       // 用 MoveTemp 将缓存数据快速转移至临时数组，同时自动清空了原有 PendingTeamRequests 队列
       TArray<TTuple<int32, APlayerController*>> TempRequests = MoveTemp(PendingTeamRequests);

       // 重新挨个尝试分配队伍
       for (const TTuple<int32, APlayerController*>& Request : TempRequests)
       {
          if (IsValid(Request.Value))
          {
             SetTeamForPlayerController(Request.Value, Request.Key);
          }
       }
    }

    // 2. 处理挂起的等待挂载树节点的 Actor 路由请求
    if (PendingConnectionActors.Num() > 0)
    {
       UE_LOG(LogTemp, Warning, TEXT("RepG HandlePendingActors"));
       
       // 同样转移所有等待加入图节点的 Actor 数组
       TArray<AActor*> PendingActors = MoveTemp(PendingConnectionActors);

       // 重新挨个尝试挂载进入同步拓扑网络
       for (AActor* Actor : PendingActors)
       {
          if (IsValid(Actor))
          {
             // 获取引擎为该Actor分配的全局复制信息对象
             FGlobalActorReplicationInfo& GlobalInfo = GlobalActorReplicationInfoMap.Get(Actor);
             // 再次调用路由机制
             RouteAddNetworkActorToNodes(FNewReplicatedActorInfo(Actor), GlobalInfo);
          }
       }
    }
}

// 工具函数：根据给定的任意受控 Actor，找到对应其客户端的 TeamReplicationConnectionManager
UTeamReplicationConnectionManager* UBlasterReplicationGraph::GetTeamReplicationConnectionManagerFromActor(const AActor* Actor)
{
    if (Actor)
    {
       // 获取 Actor 的 NetConnection（普通物品没有，只有具有 Controller 或者 Pawn 一般才有所有权）
       if (UNetConnection* NetConnection = Actor->GetNetConnection())
       {
          // 利用基类的 FindOrAddConnectionManager 取出关联的 Manager，并强转为我们自定义的业务类
          if (UTeamReplicationConnectionManager* ConnectionManager = Cast<UTeamReplicationConnectionManager>(FindOrAddConnectionManager(NetConnection)))
          {
             return ConnectionManager;
          }
       }
    }
    
    return nullptr;
}