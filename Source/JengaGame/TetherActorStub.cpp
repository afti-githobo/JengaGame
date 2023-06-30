// Fill out your copyright notice in the Description page of Project Settings.


#include "TetherActorStub.h"


// Sets default values
ATetherActorStub::ATetherActorStub()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
}

// Called when the game starts or when spawned
void ATetherActorStub::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATetherActorStub::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

