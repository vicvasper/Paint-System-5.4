#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "StaticMeshComponentLODInfo.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Rendering/ColorVertexBuffer.h"
#include "MeshPaintingComponent.generated.h"

UENUM(BlueprintType)
enum class EMaterialChannel : uint8
{
    Red,
    Green,
    Blue,
    Alpha
};

USTRUCT(BlueprintType)
struct FPaintStroke
{
    GENERATED_BODY()

    UPROPERTY()
    FVector StartPosition;

    UPROPERTY()
    FVector EndPosition;

    UPROPERTY()
    TArray<FVector> PaintPositions;

    // Propiedad para almacenar los v�rtices asociados a cada trazo
    UPROPERTY()
    TArray<uint32> Vertices;

    FPaintStroke()
        : StartPosition(FVector::ZeroVector), EndPosition(FVector::ZeroVector)
    {
    }
};

USTRUCT()
struct FVertexPaintContribution
{
    GENERATED_BODY()

    float InitialIntensity;    // Intensidad inicial de la pintura (entre 0.0f y 1.0f)
    float TimePainted;         // Momento en que se pint�
    float EraseAfterSeconds;   // Tiempo despu�s del cual comienza a desvanecerse
    float FadeSpeed;           // Duraci�n del fade

    FVertexPaintContribution()
        : InitialIntensity(0.0f), TimePainted(0.0f), EraseAfterSeconds(5.0f), FadeSpeed(1.0f) {}
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PAINTSYSTEM_API UMeshPaintingComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMeshPaintingComponent();

    // Mueve la propiedad Channel a la secci�n p�blica
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Painting")
    EMaterialChannel Channel = EMaterialChannel::Red;

    // Valores predeterminados para la pintura
    UPROPERTY(EditAnywhere, Category = "Painting")
    float DefaultPaintRadius;

    UPROPERTY(EditAnywhere, Category = "Painting")
    float DefaultPaintStrength;

    // Duraci�n total del desvanecimiento (en segundos)
    UPROPERTY(EditAnywhere, Category = "Painting")
    float FadeDuration = 5.0f; // Puedes ajustar este valor seg�n tus necesidades

protected:
    virtual void BeginPlay() override;

    // Estado de pintado
    bool bIsPainting;

    // Variable para controlar si estamos actualizando los colores
    bool bIsUpdatingColors;

    // Variables necesarias para optimizar la actualizaci�n del fade
    float TimeSinceLastUpdate;  // Controla el tiempo desde la �ltima actualizaci�n
    float UpdateInterval;  // Controla el intervalo entre actualizaciones del fade (en segundos)

    // Lista de trazos de pintura activos
    TArray<FPaintStroke> ActivePaintStrokes;

    // Mapa que asocia componentes de malla a sus contribuciones de pintura por v�rtice
    TMap<UStaticMeshComponent*, TMap<uint32, TArray<FVertexPaintContribution>>> MeshVertexContributions;

    // Almacena los v�rtices pendientes de actualizar
    TArray<uint32> PendingVertexUpdates;

    // Define el umbral para enviar actualizaciones a la GPU
    int32 UpdateThreshold = 50; // Valor ajustable seg�n el tama�o del batch que desees

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

    // Iniciar y finalizar el proceso de pintura
    void StartPaintingIfNeeded(FVector StartPosition);
    void EndPainting(FVector EndPosition);

private:
    // Actualizar el fade de las contribuciones de pintura
    void UpdateVertexGroupFades(float DeltaTime);

};



