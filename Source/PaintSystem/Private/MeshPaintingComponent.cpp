// Copyright (c) Victor Rivas Perez. All Rights Reserved.

#include "MeshPaintingComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PaintSystem.h"
#include "Rendering/ColorVertexBuffer.h"
#include "StaticMeshComponentLODInfo.h"
#include "StaticMeshResources.h"

namespace
{
	constexpr float DefaultPaintRadiusValue = 100.0f;
	constexpr float DefaultPaintStrengthValue = 1.0f;
	constexpr float DefaultFadeDuration = 5.0f;

	/** Ten fade passes a second. Each rebuilds a vertex colour buffer, so this is not free. */
	constexpr float DefaultFadeUpdateInterval = 0.1f;

	/** Full intensity as an 8-bit colour component. */
	constexpr float MaxChannelValue = 255.0f;

	/** Maps a channel onto the FColor component holding it, without depending on byte order. */
	constexpr uint8 FColor::* ChannelMembers[PaintChannelCount] =
	{
		&FColor::R,
		&FColor::G,
		&FColor::B,
		&FColor::A,
	};
}

UMeshPaintingComponent::UMeshPaintingComponent()
	: DefaultPaintRadius(DefaultPaintRadiusValue)
	, DefaultPaintStrength(DefaultPaintStrengthValue)
	, FadeDuration(DefaultFadeDuration)
	, FadeUpdateInterval(DefaultFadeUpdateInterval)
	, TimeSinceLastFadeUpdate(0.0f)
{
	PrimaryComponentTick.bCanEverTick = true;

	// Nothing to fade until something is painted. Ticking is switched on by the first stroke
	// and off again once the last contribution expires.
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMeshPaintingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceLastFadeUpdate += DeltaTime;
	if (TimeSinceLastFadeUpdate < FadeUpdateInterval)
	{
		return;
	}

	TimeSinceLastFadeUpdate = 0.0f;
	UpdateVertexGroupFades();
}

void UMeshPaintingComponent::PaintMaterial(
	UPrimitiveComponent* MeshComp,
	FVector HitLocation,
	float PaintStrength,
	float PaintRadius,
	EMaterialChannel InChannel,
	int32 LOD,
	float PaintFalloff,
	float EraseAfterSeconds,
	bool bShouldFade,
	float FadeSpeed)
{
	UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(MeshComp);
	if (!StaticMeshComp)
	{
		UE_LOG(LogPaintSystem, Error, TEXT("PaintMaterial needs a UStaticMeshComponent."));
		return;
	}

	UStaticMesh* Mesh = StaticMeshComp->GetStaticMesh();
	if (!Mesh || !Mesh->GetRenderData())
	{
		UE_LOG(LogPaintSystem, Error, TEXT("'%s' has no mesh or no render data."), *StaticMeshComp->GetName());
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (PaintRadius <= 0.0f)
	{
		PaintRadius = DefaultPaintRadius;
	}

	// Cheap rejection before touching the vertex buffer: a stroke that misses the mesh
	// entirely should not cost a pass over its vertices.
	const FBoxSphereBounds& MeshBounds = StaticMeshComp->Bounds;
	if (FVector::DistSquared(MeshBounds.Origin, HitLocation) >
		FMath::Square(MeshBounds.SphereRadius + PaintRadius))
	{
		return;
	}

	const TIndirectArray<FStaticMeshLODResources>& LODResources = Mesh->GetRenderData()->LODResources;
	if (!LODResources.IsValidIndex(LOD))
	{
		UE_LOG(LogPaintSystem, Error, TEXT("LOD %d is out of range on '%s' (%d LODs)."),
			LOD, *Mesh->GetName(), LODResources.Num());
		return;
	}

	const FPositionVertexBuffer& PositionBuffer = LODResources[LOD].VertexBuffers.PositionVertexBuffer;
	const int32 NumVertices = static_cast<int32>(PositionBuffer.GetNumVertices());
	if (NumVertices == 0)
	{
		return;
	}

	if (StaticMeshComp->LODData.Num() <= LOD)
	{
		StaticMeshComp->SetLODDataCount(LOD + 1, Mesh->GetNumLODs());
		if (StaticMeshComp->LODData.Num() <= LOD)
		{
			UE_LOG(LogPaintSystem, Error, TEXT("Could not allocate LOD data for LOD %d on '%s'."),
				LOD, *StaticMeshComp->GetName());
			return;
		}
	}

	FStaticMeshComponentLODInfo& LODInfo = StaticMeshComp->LODData[LOD];
	if (!LODInfo.OverrideVertexColors)
	{
		LODInfo.OverrideVertexColors = new FColorVertexBuffer();
		LODInfo.OverrideVertexColors->InitFromSingleColor(FColor::Black, NumVertices);
		BeginInitResource(LODInfo.OverrideVertexColors);
	}

	TArray<FColor> Colors;
	LODInfo.OverrideVertexColors->GetVertexColors(Colors);
	if (Colors.Num() != NumVertices)
	{
		UE_LOG(LogPaintSystem, Error,
			TEXT("Override colour count (%d) does not match the vertex count (%d) on '%s'."),
			Colors.Num(), NumVertices, *StaticMeshComp->GetName());
		return;
	}

	FMeshPaintState& PaintState = MeshPaintStates.FindOrAdd(StaticMeshComp);
	PaintState.LODIndex = LOD;

	// Hoisted: the transform was fetched once per vertex.
	const FTransform ComponentTransform = StaticMeshComp->GetComponentTransform();
	const float PaintRadiusSquared = FMath::Square(PaintRadius);
	const float CurrentTime = World->GetTimeSeconds();

	const int32 ChannelIndex = static_cast<int32>(InChannel);
	if (!ChannelMembers[ChannelIndex])
	{
		return;
	}

	int32 PaintedVertexCount = 0;
	bool bColorsChanged = false;

	for (int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
	{
		const FVector VertexWorldPosition =
			ComponentTransform.TransformPosition(FVector(PositionBuffer.VertexPosition(VertexIndex)));

		// Squared comparison: the exact distance is only needed for the falloff, which is
		// skipped for the vertices that fail this test - the majority, for a typical brush.
		const float DistanceSquared = FVector::DistSquared(VertexWorldPosition, HitLocation);
		if (DistanceSquared > PaintRadiusSquared)
		{
			continue;
		}

		const float FalloffFactor = FMath::Clamp(1.0f - (FMath::Sqrt(DistanceSquared) / PaintRadius), 0.0f, 1.0f);
		const float FinalPaintStrength = PaintStrength * FMath::Pow(FalloffFactor, PaintFalloff);
		if (FinalPaintStrength <= 0.0f)
		{
			continue;
		}

		FVertexPaintContribution Contribution;
		Contribution.InitialIntensity = FinalPaintStrength;
		Contribution.TimePainted = CurrentTime;
		Contribution.EraseAfterSeconds = EraseAfterSeconds > 0.0f ? EraseAfterSeconds : FadeDuration;
		Contribution.Channel = InChannel;

		// bShouldFade was accepted and ignored. A non-positive fade speed now means the
		// deposit is permanent, which is what the flag was presumably for.
		Contribution.FadeSpeed = (bShouldFade && FadeSpeed > 0.0f) ? FadeSpeed : 0.0f;

		FVertexPaintData& VertexData = PaintState.Vertices.FindOrAdd(VertexIndex);
		VertexData.Contributions.Add(Contribution);
		VertexData.TouchedChannelMask |= static_cast<uint8>(1 << ChannelIndex);

		bColorsChanged |= RecomputeVertexColor(VertexData, CurrentTime, Colors[VertexIndex]);
		++PaintedVertexCount;
	}

	if (PaintedVertexCount == 0)
	{
		return;
	}

	if (bColorsChanged)
	{
		UploadVertexColors(*StaticMeshComp, LOD, Colors);
	}

	UpdateTickEnabled();

	UE_LOG(LogPaintSystem, Verbose, TEXT("Painted %d of %d vertices on '%s'."),
		PaintedVertexCount, NumVertices, *StaticMeshComp->GetName());
}

void UMeshPaintingComponent::UpdateVertexGroupFades()
{
	const UWorld* World = GetWorld();
	if (!World || MeshPaintStates.Num() == 0)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();

	for (auto MeshIt = MeshPaintStates.CreateIterator(); MeshIt; ++MeshIt)
	{
		UStaticMeshComponent* MeshComp = MeshIt.Key().Get();
		FMeshPaintState& PaintState = MeshIt.Value();

		// A null weak pointer means the component is gone. The previous version stored raw
		// pointers here and read through them.
		if (!MeshComp || !MeshComp->LODData.IsValidIndex(PaintState.LODIndex))
		{
			MeshIt.RemoveCurrent();
			continue;
		}

		FStaticMeshComponentLODInfo& LODInfo = MeshComp->LODData[PaintState.LODIndex];
		if (!LODInfo.OverrideVertexColors || !LODInfo.OverrideVertexColors->IsInitialized())
		{
			MeshIt.RemoveCurrent();
			continue;
		}

		TArray<FColor> Colors;
		LODInfo.OverrideVertexColors->GetVertexColors(Colors);

		bool bColorsChanged = false;

		for (auto VertexIt = PaintState.Vertices.CreateIterator(); VertexIt; ++VertexIt)
		{
			const uint32 VertexIndex = VertexIt.Key();
			if (!Colors.IsValidIndex(static_cast<int32>(VertexIndex)))
			{
				VertexIt.RemoveCurrent();
				continue;
			}

			FVertexPaintData& VertexData = VertexIt.Value();
			bColorsChanged |= RecomputeVertexColor(VertexData, CurrentTime, Colors[VertexIndex]);

			// Dropped only after the recompute has written its channels back to zero.
			if (VertexData.Contributions.Num() == 0)
			{
				VertexIt.RemoveCurrent();
			}
		}

		// Re-uploading is the expensive part, so it happens only when a colour actually
		// changed - not, as before, whenever any vertex still had contributions.
		if (bColorsChanged)
		{
			UploadVertexColors(*MeshComp, PaintState.LODIndex, Colors);
		}

		if (PaintState.Vertices.Num() == 0)
		{
			MeshIt.RemoveCurrent();
		}
	}

	UpdateTickEnabled();
}

bool UMeshPaintingComponent::RecomputeVertexColor(FVertexPaintData& VertexData, float CurrentTime, FColor& OutColor) const
{
	float ChannelIntensity[PaintChannelCount] = {};

	// Reverse iteration so expired contributions can be dropped in place.
	for (int32 Index = VertexData.Contributions.Num() - 1; Index >= 0; --Index)
	{
		const FVertexPaintContribution& Contribution = VertexData.Contributions[Index];

		float FadeProgress = 0.0f;
		if (Contribution.FadeSpeed > 0.0f)
		{
			const float TimeSincePainted = CurrentTime - Contribution.TimePainted;
			if (TimeSincePainted >= Contribution.EraseAfterSeconds)
			{
				FadeProgress = FMath::Clamp(
					(TimeSincePainted - Contribution.EraseAfterSeconds) / Contribution.FadeSpeed, 0.0f, 1.0f);
			}
		}

		const float RemainingIntensity = Contribution.InitialIntensity * (1.0f - FadeProgress);
		if (RemainingIntensity <= KINDA_SMALL_NUMBER)
		{
			// Swap-remove is safe here: the element moved into this slot comes from the tail,
			// which the reverse walk has already visited.
			VertexData.Contributions.RemoveAtSwap(Index);
			continue;
		}

		ChannelIntensity[static_cast<int32>(Contribution.Channel)] += RemainingIntensity;
	}

	bool bChanged = false;

	for (int32 ChannelIndex = 0; ChannelIndex < PaintChannelCount; ++ChannelIndex)
	{
		// Channels this vertex was never painted in are left untouched. Zeroing them all would
		// clear the buffer's initial opaque alpha.
		if ((VertexData.TouchedChannelMask & (1 << ChannelIndex)) == 0)
		{
			continue;
		}

		const float Intensity = FMath::Clamp(ChannelIntensity[ChannelIndex], 0.0f, 1.0f);
		const uint8 NewValue = static_cast<uint8>(Intensity * MaxChannelValue);

		uint8 FColor::* const Member = ChannelMembers[ChannelIndex];
		if (OutColor.*Member != NewValue)
		{
			OutColor.*Member = NewValue;
			bChanged = true;
		}
	}

	return bChanged;
}

void UMeshPaintingComponent::UploadVertexColors(UStaticMeshComponent& MeshComp, int32 LODIndex, const TArray<FColor>& Colors)
{
	FStaticMeshComponentLODInfo& LODInfo = MeshComp.LODData[LODIndex];
	if (!LODInfo.OverrideVertexColors)
	{
		return;
	}

	// Release, flush, rewrite, re-init - in that order. The previous version rewrote the
	// buffer's contents first and released it afterwards, mutating memory the render thread
	// could still have been reading.
	BeginReleaseResource(LODInfo.OverrideVertexColors);
	FlushRenderingCommands();

	LODInfo.OverrideVertexColors->InitFromColorArray(Colors);

	BeginInitResource(LODInfo.OverrideVertexColors);
	MeshComp.MarkRenderStateDirty();
}

void UMeshPaintingComponent::ClearAllPaint()
{
	for (auto& MeshPair : MeshPaintStates)
	{
		UStaticMeshComponent* MeshComp = MeshPair.Key.Get();
		if (!MeshComp || !MeshComp->LODData.IsValidIndex(MeshPair.Value.LODIndex))
		{
			continue;
		}

		FStaticMeshComponentLODInfo& LODInfo = MeshComp->LODData[MeshPair.Value.LODIndex];
		if (!LODInfo.OverrideVertexColors)
		{
			continue;
		}

		TArray<FColor> Colors;
		LODInfo.OverrideVertexColors->GetVertexColors(Colors);

		for (const TPair<uint32, FVertexPaintData>& VertexPair : MeshPair.Value.Vertices)
		{
			if (!Colors.IsValidIndex(static_cast<int32>(VertexPair.Key)))
			{
				continue;
			}

			for (int32 ChannelIndex = 0; ChannelIndex < PaintChannelCount; ++ChannelIndex)
			{
				if (VertexPair.Value.TouchedChannelMask & (1 << ChannelIndex))
				{
					Colors[VertexPair.Key].*ChannelMembers[ChannelIndex] = 0;
				}
			}
		}

		UploadVertexColors(*MeshComp, MeshPair.Value.LODIndex, Colors);
	}

	MeshPaintStates.Empty();
	UpdateTickEnabled();
}

void UMeshPaintingComponent::UpdateTickEnabled()
{
	// Idle components cost nothing: with no paint left there is nothing to fade.
	SetComponentTickEnabled(MeshPaintStates.Num() > 0);
}
