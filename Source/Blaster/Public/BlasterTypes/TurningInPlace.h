// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

UENUM(BlueprintType)
enum class ETurningInPlace : uint8
{
	ETIP_Right UMETA(DisplayName = "Turning Right"),
	ETIP_Left UMETA(DisplayName = "Turning Left"),
	ETIP_NotTurning UMETA(DisplayName = "Not Turning"),
	
	TIP_MAX UMETA(DisplayName = "DefaultMAX")
};