// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

UENUM(BlueprintType)
enum class ECombatState : uint8
{
	ECS_NotCombat UMETA(DisplayName = "NotCombat"),
	ECS_Unoccupied UMETA(DisplayName = "Unoccupied"),
	ECS_FireTimerInProgress UMETA(DisplayName = "FireTimerInProgress"),
	ECS_Reloading UMETA(DisplayName = "Reloading"),
	ECS_Equipping UMETA(DisplayName = "Equipping"),
	ECS_ThrowingGrenade UMETA(DisplayName = "ThrowingGrenade"),

	ECS_MAX UMETA(DisplayName = "DefaultMAX")
	
};