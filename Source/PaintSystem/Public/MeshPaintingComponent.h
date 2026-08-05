// Copyright (c) Victor Rivas Perez. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "MeshPaintingComponent.generated.h"

class UPrimitiveComponent;
class UStaticMeshComponent;

/** Vertex colour channel a paint stroke writes into. */
UENUM(BlueprintType)
enum class EMaterialChannel : uint8
{
	Red,
	Green,
	Blue,
	Alpha,

	Count UMETA(Hidden)
};

/** Number of paintable channels, derived from the enum. */
constexpr int32 PaintChannelCount = static_cast<int32>(EMaterialChannel::Count);

/**
 * One paint deposit on one vertex.
 *
 * Vertices accumulate deposits rather than being overwritten, so overlapping strokes build up
 * and each fades on its own schedule.
 */
USTRUCT()
struct FVertexPaintContribution
{
	GENERATED_BODY()

	/** Strength of this deposit at the moment it landed, in [0, 1]. */
	UPROPERTY()
	float InitialIntensity = 0.0f;

	/** World time this deposit was made. */
	UPROPERTY()
	float TimePainted = 0.0f;

	/** How long the deposit holds at full strength before it starts to fade. */
	UPROPERTY()
	float EraseAfterSeconds = 0.0f;

	/** Seconds the fade itself takes, once it begins. */
	UPROPERTY()
	float FadeSpeed = 1.0f;

	/**
	 * Channel this deposit was painted into.
	 *
	 * Stored per contribution rather than read from a component-wide setting. Sharing one
	 * channel across every deposit meant that changing channel mid-session re-wrote earlier
	 * strokes into the new channel as they faded.
	 */
	UPROPERTY()
	EMaterialChannel Channel = EMaterialChannel::Red;
};

/**
 * Runtime vertex-colour painting for static meshes.
 *
 * Call PaintMaterial() with a world-space hit location to deposit paint on the vertices within
 * the brush radius. Deposits accumulate per channel and fade out on their own timers; the
 * component stops doing per-frame work once nothing is left to fade.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PAINTSYSTEM_API UMeshPaintingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeshPaintingComponent();

	//~ Begin UActorComponent interface
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent interface

	/**
	 * Deposits paint on every vertex of MeshComp within PaintRadius of HitLocation.
	 *
	 * Costs one pass over the mesh's vertex buffer, so brush size does not affect its cost -
	 * mesh density does. The component's bounds are tested first, so a stroke that misses is
	 * cheap.
	 */
	UFUNCTION(BlueprintCallable, Category = "Painting", meta = (DefaultToSelf = "MeshComp"))
	void PaintMaterial(
		UPrimitiveComponent* MeshComp,
		FVector HitLocation,
		float PaintStrength = 1.0f,
		float PaintRadius = 100.0f,
		EMaterialChannel InChannel = EMaterialChannel::Red,
		int32 LOD = 0,
		float PaintFalloff = 1.0f,
		float EraseAfterSeconds = 5.0f,
		bool bShouldFade = true,
		float FadeSpeed = 1.0f);

	/** Drops all paint from every mesh this component has touched. */
	UFUNCTION(BlueprintCallable, Category = "Painting")
	void ClearAllPaint();

protected:
	/** Brush radius used when a caller does not supply one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Painting",
		meta = (ClampMin = "0.1", Units = "cm"))
	float DefaultPaintRadius;

	/** Brush strength used when a caller does not supply one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Painting",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultPaintStrength;

	/** Hold time applied when a caller passes a non-positive EraseAfterSeconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Painting",
		meta = (ClampMin = "0.0", Units = "s"))
	float FadeDuration;

	/**
	 * Seconds between fade passes.
	 *
	 * Each pass rebuilds and re-uploads a mesh's vertex colour buffer, which is far too
	 * expensive to do every frame. The previous version declared this interval and an
	 * accompanying timer but never used either, so the work ran on every tick.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Painting",
		meta = (ClampMin = "0.01", Units = "s"))
	float FadeUpdateInterval;

private:
	/** Accumulated paint on a single vertex. */
	struct FVertexPaintData
	{
		TArray<FVertexPaintContribution> Contributions;

		/**
		 * Bitmask of channels this vertex has ever been painted in.
		 *
		 * A channel that has faded to nothing still has to be written back to zero once, and
		 * channels never painted must be left alone - the buffer's initial alpha is opaque,
		 * so blindly zeroing every channel would erase it.
		 */
		uint8 TouchedChannelMask = 0;
	};

	/** Paint state for one mesh component. */
	struct FMeshPaintState
	{
		TMap<uint32, FVertexPaintData> Vertices;

		/** LOD the paint was applied to; fades must target the same one. */
		int32 LODIndex = 0;
	};

	/** Advances every fade and re-uploads the meshes whose colours actually changed. */
	void UpdateVertexGroupFades();

	/**
	 * Recomputes a vertex's colour from its live contributions.
	 * Returns true if OutColor changed. Drops contributions that have fully faded.
	 */
	bool RecomputeVertexColor(FVertexPaintData& VertexData, float CurrentTime, FColor& OutColor) const;

	/** Releases, rewrites and re-initialises a mesh's override colour buffer. */
	static void UploadVertexColors(UStaticMeshComponent& MeshComp, int32 LODIndex, const TArray<FColor>& Colors);

	/** Enables ticking only while there is paint left to fade. */
	void UpdateTickEnabled();

	/**
	 * Painted meshes, keyed weakly.
	 *
	 * The previous version keyed this map on raw UStaticMeshComponent pointers held outside the
	 * reflection system, then dereferenced them on later frames - so a component destroyed
	 * between strokes left a dangling key that the fade pass would read through.
	 */
	TMap<TWeakObjectPtr<UStaticMeshComponent>, FMeshPaintState> MeshPaintStates;

	/** Time accumulated toward the next fade pass. */
	float TimeSinceLastFadeUpdate;
};
