// Fill out your copyright notice in the Description page of Project Settings.


#include "GameState/BlasterGameState.h"
#include "PlayerState/BlasterPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "PlayerController/BlasterPlayerController.h"

void ABlasterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ABlasterGameState, TopScoringPlayers);
	DOREPLIFETIME(ABlasterGameState, RedTeamScore);
	DOREPLIFETIME(ABlasterGameState, BlueTeamScore);
}

void ABlasterGameState::UpdateTopScore(class ABlasterPlayerState* ScoringPlayer)  // 定义更新最高分的方法，参数为得分玩家状态指针
{
	if (TopScoringPlayers.Num() == 0)  // 如果当前最高分玩家列表为空（还没有任何玩家达到过最高分）
	{
		TopScoringPlayers.Add(ScoringPlayer);  // 将该玩家加入最高分列表
		TopScore = ScoringPlayer->GetScore();  // 将当前最高分设置为该玩家的分数
	}
	else if (ScoringPlayer->GetScore() > TopScore)  // 否则，如果该玩家的分数大于当前记录的最高分
	{
		TopScoringPlayers.Empty();  // 清空原有的最高分玩家列表（因为有了新的唯一最高分）
		TopScoringPlayers.Add(ScoringPlayer);  // 将该玩家加入列表
		TopScore = ScoringPlayer->GetScore();  // 更新最高分数
	}
	else if (ScoringPlayer->GetScore() == TopScore)  // 否则，如果该玩家的分数等于当前最高分
	{
		TopScoringPlayers.AddUnique(ScoringPlayer);  // 将该玩家加入列表（使用AddUnique避免重复添加同一玩家）
	}
}

void ABlasterGameState::RedTeamScores()
{
	++RedTeamScore;
	if (ABlasterPlayerController* BlasterPlayerController = Cast<ABlasterPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		BlasterPlayerController->SetHUDRedTeamScore(RedTeamScore);
	}
}

void ABlasterGameState::BlueTeamScores()
{
	++BlueTeamScore;
	if (ABlasterPlayerController* BlasterPlayerController = Cast<ABlasterPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		BlasterPlayerController->SetHUDBlueTeamScore(BlueTeamScore);
	}
}

void ABlasterGameState::OnRep_RedTeamScore()
{
	if (ABlasterPlayerController* BlasterPlayerController = Cast<ABlasterPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		BlasterPlayerController->SetHUDRedTeamScore(RedTeamScore);
	}
}

void ABlasterGameState::OnRep_BlueTeamScore()
{
	if (ABlasterPlayerController* BlasterPlayerController = Cast<ABlasterPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		BlasterPlayerController->SetHUDBlueTeamScore(BlueTeamScore);
	}
}
