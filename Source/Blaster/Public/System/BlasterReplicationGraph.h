// Fill out your copyright notice in the Description page of Project Settings.

#pragma once // 确保此头文件在编译时只被包含一次，防止重复定义

#include "CoreMinimal.h" // 包含 UE 最核心的数据类型和宏 (如 FString, TArray 等)
#include "ReplicationGraph.h" // 包含引擎底层 Replication Graph 的基础类
#include "BlasterReplicationGraph.generated.h" // Unreal Header Tool 自动生成的文件，用于支持反射(UCLASS等)

// 2. 结合具体例子详细拆解
// 想象一个场景：一场 3v3 的团队死斗（Team Deathmatch）。
// 玩家分为红队 (Team 0) 和 蓝队 (Team 1)。
//
// 场景 1：游戏启动与全局信息广播 (InitGlobalGraphNodes)
// 当服务器启动地图时，会生成 GameState（记录当前比分、剩余时间等）。
// RouteAddNetworkActorToNodes 被调用，发现这是 GameState。它被放入 UBlasterReplicationGraphNode_AlwaysRelevant_WithPending 中。
// 结果：无论是红队还是蓝队，无论你在地图哪个角落，服务器每一帧都会把比分和时间更新发给所有人。
//
// 场景 2：玩家加入与拓扑结构建立 (InitConnectionGraphNodes & UTeamReplicationConnectionManager)
// 玩家 A 连接到服务器。
// 引擎会通过 InitConnectionGraphNodes 为玩家 A 创建一个 UTeamReplicationConnectionManager（这是他在服务器里的网络代理人档案）。
// 同时，为他创建了两个专属节点：
//
// AlwaysRelevantForConnectionNode (私人节点)：如果玩家 A 捡起了一把特殊的任务钥匙，只有他自己能看到物品栏信息。钥匙的 Actor 会被扔进这个节点，服务器绝不会把这把钥匙的数据发给其他 5 个人。
//
// TeamConnectionNode (对外社交节点)：用于处理他该看到哪些队友和敌人。
//
// 场景 3：挂起状态 (Pending 相关逻辑)
// 玩家 A 刚连接进来，网速很慢，底层的 PlayerController 还没彻底和网络通道绑定好，但此时他踩到了一个陷阱，触发了某种状态 Actor。
// 如果在普通的 UE 逻辑里，系统找不到对应的 NetworkConnection，可能会报错或丢失同步。
// 在这里，这个 Actor 会被塞进 PendingConnectionActors。
// 每一帧的 PrepareForReplication 都会去调用 HandlePendingActorsAndTeamRequests，不断尝试：“玩家 A 连好没有？连好了？行，把这个 Actor 正式挂到他的树上！”
//
// 场景 4：高阶剔除与视野同步 (GatherActorListsForConnection & FTeamConnectionListMap)
// 玩家 A 被分配到了红队 (Team 0)。
// 当前战况：
//
// 红队：玩家 A，玩家 B，玩家 C。
//
// 蓝队：玩家 D，玩家 E，玩家 F。
//
// 位置分布：玩家 A 在中门；红队队友 B、C 在基地；蓝队 D 躲在门后的掩体里（墙后）；蓝队 E、F 在门外广场。
//
// 当服务器在这一帧准备发数据包给玩家 A 时，UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam 开始工作：
//
// 查户口：找 FTeamConnectionListMap。玩家 A 在 Team 0。
//
// 找队友：Team 0 还有 B 和 C。服务器立刻把 B 和 C 的坐标、血量加入到要发给 A 的数据包中。（队友隔着墙也永远可见，你可以在 UI 上看到他们的透视轮廓）。
//
// 找敌人并做视线检测：Team 1 有 D、E、F。服务器调用 GetVisibleConnectionArrayForNonTeam。
//
// 向 D 射一条射线：被墙挡住了。（D 不可见，剔除 D 的网络同步包。这不仅节省了网络带宽，还从底层物理上防止了外挂玩家制作透视挂偷看 D 的位置，因为 A 的客户端根本没收到 D 的坐标）。
//
// 向 E、F 射一条射线：无遮挡。（E 和 F 可见，将坐标加入数据包发给 A）。
//
// 为什么会有 friend 声明？
// 在 UBlasterReplicationGraph 的末尾有这样一句：
// friend UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam;
// 因为 TeamConnectionListMap（那个保存所有队伍名单的花名册）是 Graph 的 private 私有成员（为了保护数据不被乱改）。但是，处理团队逻辑的 TeamNode 需要频繁读取这个花名册来找队友和敌人。friend 关键字就是 Graph 给 TeamNode 开的一个“后门”，允许它直接读取这个私有变量。


// ---------------------------------------------------------
// 1. 全局始终相关节点 (附带挂起处理功能)
// ---------------------------------------------------------
UCLASS() // 暴露给 UE 垃圾回收和反射系统
class UBlasterReplicationGraphNode_AlwaysRelevant_WithPending : public UReplicationGraphNode_ActorList
{
    GENERATED_BODY() // 自动生成必须的样板代码

public:
    UBlasterReplicationGraphNode_AlwaysRelevant_WithPending(); // 构造函数，设置需要 PrepareForReplication
    
    // 每帧在真正的网络同步前调用，用于清理/处理那些因为网络还没就绪而“挂起”的Actor
    virtual void PrepareForReplication() override;
};

// ---------------------------------------------------------
// 2. 基于团队和视线的专属同步节点
// ---------------------------------------------------------
UCLASS()
class UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam : public UReplicationGraphNode_ActorList
{
    GENERATED_BODY()

    // 核心函数：每帧收集要发送给该客户端的 Actor 列表 (在这里实现队友可见、敌人视线检测的逻辑)
    virtual void GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params) override;
    
    // 一个包装函数，用于调用父类默认的收集逻辑 (直接把列表里的东西发过去)
    virtual void GatherActorListsForConnectionDefault(const FConnectionGatherActorListParameters& Params);
};

// ---------------------------------------------------------
// 3. 自定义玩家网络连接管理器
// ---------------------------------------------------------
UCLASS()
class UTeamReplicationConnectionManager : public UNetReplicationGraphConnection
{
    GENERATED_BODY()

public:
    // 指向处理该玩家“私人专属”（OnlyRelevantToOwner）物品的节点（如他手里的枪膛内子弹数）
    UPROPERTY()
    UReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantForConnectionNode;
    
    // 指向处理该玩家“团队与视野”相关的节点（如他的队友和他能看见的敌人）
    UPROPERTY()
    UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam* TeamConnectionNode;

    // 记录该客户端属于哪个队伍，默认为 -1 (无队伍)
    int32 Team = -1;
    
    // 弱指针：记录该客户端当前控制的 Pawn（角色）。使用弱指针防止角色死亡销毁时引发崩溃或内存泄漏
    TWeakObjectPtr<APawn> Pawn;
};

// ---------------------------------------------------------
// 4. 队伍连接映射表 (数据结构)
// ---------------------------------------------------------
// 继承自 TMap，键(Key)是队伍ID(int32)，值(Value)是该队伍所有玩家连接管理器的数组
struct FTeamConnectionListMap: TMap<int32, TArray<UTeamReplicationConnectionManager*>>
{
    // 获取指定队伍的所有玩家连接数组
    TArray<UTeamReplicationConnectionManager*>* GetConnectionArrayForTeam(int32 Team);
    
    // 将一个玩家添加到指定队伍的数组中
    void AddConnectionToTeam(int32 Team, UTeamReplicationConnectionManager* ConnManager);
    
    // 将一个玩家从指定队伍的数组中移除
    void RemoveConnectionFromTeam(int32 Team, UTeamReplicationConnectionManager* ConnManager);
    
    // 核心优化函数：传入观察者，返回不在同一队伍且能被观察者“看见”的敌方连接
    TArray<UTeamReplicationConnectionManager*> GetVisibleConnectionArrayForNonTeam(const APawn* ViewerPawn, int32 ViewerTeam) const;
};

// ---------------------------------------------------------
// 5. 核心复制图管理类 (总控制器)
// ---------------------------------------------------------
UCLASS()
class BLASTER_API UBlasterReplicationGraph : public UReplicationGraph
{
    GENERATED_BODY()

public:
    UBlasterReplicationGraph();

    // 初始化全局图节点 (比如上方的 AlwaysRelevantNode)，服务器启动时调用
    virtual void InitGlobalGraphNodes() override;
    
    // 当有新玩家(Client)连入时调用，为其创建个人的 ConnectionNode 和 TeamNode
    virtual void InitConnectionGraphNodes(UNetReplicationGraphConnection* ConnectionManager) override;

    // 玩家断开连接时调用，用于清理内存和队伍列表中的残留记录
    virtual void RemoveClientConnection(UNetConnection* NetConnection) override;
    
    // 无缝漫游(换地图)时重置状态，清理缓存
    virtual void ResetGameWorldState() override;
    
    // 核心路由函数：当有新Actor产生(Spawn)时，决定把它扔给哪个节点去管理同步
    virtual void RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo) override;
    
    // 核心路由函数：Actor销毁或停止同步时，把它从管理节点中摘除
    virtual void RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo) override;

    // 给玩家控制器分配队伍 ID
    void SetTeamForPlayerController(APlayerController* PlayerController, int32 Team);
    
    // 处理那些因为网络尚未准备好而积压的 Actor 或 分配队伍请求
    void HandlePendingActorsAndTeamRequests();

private:
    // 保存全局始终相关节点的实例引用
    UPROPERTY()
    UBlasterReplicationGraphNode_AlwaysRelevant_WithPending* AlwaysRelevantNode;
    
    // 缓存队列：存放那些已经 Spawn 但是其 Owner 的网络连接还没完全建立的 Actor
    UPROPERTY()
    TArray<AActor*> PendingConnectionActors;
    
    // 缓存队列：存放那些网络没建立好就请求分配队伍的 PlayerController 和 队伍ID
    TArray<TTuple<int32, APlayerController*>> PendingTeamRequests;

    // 声明友元类：允许 Team 节点直接访问本类中的私有成员 TeamConnectionListMap
    friend UBlasterReplicationGraphNode_AlwaysRelevant_ForTeam;
    
    // 实例化的队伍数据结构，存放所有队伍和玩家的映射关系
    FTeamConnectionListMap TeamConnectionListMap;
    
    // 辅助函数：通过一个 Actor 找到它归属玩家的 TeamReplicationConnectionManager
    UTeamReplicationConnectionManager* GetTeamReplicationConnectionManagerFromActor(const AActor* Actor);
};