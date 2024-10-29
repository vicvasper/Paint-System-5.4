#include "MeshPaintingComponent.h"

UMeshPaintingComponent::UMeshPaintingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    DefaultPaintRadius = 100.0f;
    DefaultPaintStrength = 1.0f;
    TimeSinceLastUpdate = 0.0f; // Para controlar la frecuencia de actualización del tick
    UpdateInterval = 0.1f; // Actualizar cada 0.1 segundos
    bIsUpdatingColors = false;
}

void UMeshPaintingComponent::BeginPlay()
{
    Super::BeginPlay();

    // No necesitamos iniciar ningún temporizador aquí, ya que manejaremos el fade en TickComponent
}

void UMeshPaintingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateVertexGroupFades(DeltaTime);
}

void UMeshPaintingComponent::StartPaintingIfNeeded(FVector StartPosition)
{
    if (!bIsPainting) // Solo crear un nuevo trazo si no estamos pintando
    {
        FPaintStroke NewStroke;
        NewStroke.StartPosition = StartPosition;
        NewStroke.PaintPositions.Add(StartPosition);  // Añadir la primera posición
        ActivePaintStrokes.Add(NewStroke);  // Añadir el nuevo trazo a la lista de trazos activos
        bIsPainting = true;  // Iniciar el estado de pintado
    }
    else
    {
        // Si ya estamos pintando, añadimos nuevas posiciones al último trazo
        if (ActivePaintStrokes.Num() > 0)
        {
            FPaintStroke& CurrentStroke = ActivePaintStrokes.Last();
            CurrentStroke.PaintPositions.Add(StartPosition);  // Añadir la nueva posición
        }
    }
}

void UMeshPaintingComponent::EndPainting(FVector EndPosition)
{
    if (bIsPainting && ActivePaintStrokes.Num() > 0)
    {
        FPaintStroke& CurrentStroke = ActivePaintStrokes.Last();
        CurrentStroke.EndPosition = EndPosition;
        bIsPainting = false;  // Finaliza el estado de pintado
    }
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
    // Actualizar el canal de pintura actual
    Channel = InChannel;

    // Verificar que MeshComp es válido
    if (!MeshComp)
    {
        UE_LOG(LogTemp, Error, TEXT("MeshComp es nulo."));
        return;
    }

    UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(MeshComp);
    if (!StaticMeshComp || !StaticMeshComp->GetStaticMesh())
    {
        UE_LOG(LogTemp, Error, TEXT("StaticMeshComp o StaticMesh no son válidos."));
        return;
    }

    if (!StaticMeshComp->GetStaticMesh()->GetRenderData())
    {
        UE_LOG(LogTemp, Error, TEXT("StaticMesh no tiene RenderData."));
        return;
    }

    if (StaticMeshComp->LODData.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("LODData es cero, configurando LODData..."));
        StaticMeshComp->SetLODDataCount(1, StaticMeshComp->GetStaticMesh()->GetNumLODs());
        if (StaticMeshComp->LODData.Num() == 0)
        {
            UE_LOG(LogTemp, Error, TEXT("No se pudo configurar LODData."));
            return;
        }
    }

    const FStaticMeshLODResources& LODModel = StaticMeshComp->GetStaticMesh()->GetRenderData()->LODResources[LOD];
    const FPositionVertexBuffer* PositionVertexBuffer = &LODModel.VertexBuffers.PositionVertexBuffer;

    const uint32 NumVertices = PositionVertexBuffer->GetNumVertices();
    if (NumVertices == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Número de vértices es 0."));
        return;
    }

    FStaticMeshComponentLODInfo& LODInfo = StaticMeshComp->LODData[LOD];

    if (!LODInfo.OverrideVertexColors)
    {
        UE_LOG(LogTemp, Warning, TEXT("OverrideVertexColors no es válido, inicializando con color negro."));
        LODInfo.OverrideVertexColors = new FColorVertexBuffer;
        LODInfo.OverrideVertexColors->InitFromSingleColor(FColor::Black, NumVertices);
    }

    TArray<FColor> CurrentColors;
    LODInfo.OverrideVertexColors->GetVertexColors(CurrentColors);

    if (CurrentColors.Num() != NumVertices)
    {
        UE_LOG(LogTemp, Error, TEXT("Número de colores en CurrentColors no coincide con el número de vértices."));
        return;
    }

    // Array temporal para almacenar los vértices pintados
    TArray<uint32> PaintedVertexGroup;

    // Obtener o crear el mapa de contribuciones para este MeshComp
    TMap<uint32, TArray<FVertexPaintContribution>>& VertexContributions = MeshVertexContributions.FindOrAdd(StaticMeshComp);

    for (int32 VertexIndex = 0; VertexIndex < static_cast<int32>(NumVertices); ++VertexIndex)
    {
        // Obtener la posición local del vértice como FVector3f
        FVector3f VertexPositionLocal = PositionVertexBuffer->VertexPosition(VertexIndex);

        // Convertir a FVector (double) para usar en TransformPosition
        FVector VertexPosition = FVector(VertexPositionLocal);

        // Transformar la posición del vértice a espacio mundial
        VertexPosition = StaticMeshComp->GetComponentTransform().TransformPosition(VertexPosition);

        float Distance = FVector::Dist(VertexPosition, HitLocation);

        float FalloffFactor = FMath::Clamp(1.0f - (Distance / PaintRadius), 0.0f, 1.0f);
        float FinalPaintStrength = PaintStrength * FMath::Pow(FalloffFactor, PaintFalloff);

        if (Distance <= PaintRadius && FinalPaintStrength > 0.0f)
        {
            PaintedVertexGroup.Add(VertexIndex);

            // Crear una nueva contribución de pintura
            FVertexPaintContribution NewContribution;
            NewContribution.InitialIntensity = FinalPaintStrength; // Valor entre 0.0f y 1.0f
            NewContribution.TimePainted = GetWorld()->GetTimeSeconds();
            NewContribution.EraseAfterSeconds = EraseAfterSeconds > 0.0f ? EraseAfterSeconds : FadeDuration;
            NewContribution.FadeSpeed = FadeSpeed > 0.0f ? FadeSpeed : 1.0f;

            // Agregar la contribución al vértice
            VertexContributions.FindOrAdd(VertexIndex).Add(NewContribution);

            // Calcular la intensidad total actual del vértice sumando todas las contribuciones
            float TotalIntensity = 0.0f;
            for (const FVertexPaintContribution& Contribution : VertexContributions[VertexIndex])
            {
                TotalIntensity += Contribution.InitialIntensity;
            }

            // Clampear la intensidad total
            TotalIntensity = FMath::Clamp(TotalIntensity, 0.0f, 1.0f);

            // Actualizar el color del vértice
            FColor& VertexColor = CurrentColors[VertexIndex];
            uint8 ColorValue = static_cast<uint8>(TotalIntensity * 255.0f);

            switch (Channel)
            {
            case EMaterialChannel::Red:
                VertexColor.R = ColorValue;
                break;
            case EMaterialChannel::Green:
                VertexColor.G = ColorValue;
                break;
            case EMaterialChannel::Blue:
                VertexColor.B = ColorValue;
                break;
            case EMaterialChannel::Alpha:
                VertexColor.A = ColorValue;
                break;
            }
        }
    }

    if (PaintedVertexGroup.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("No se pintaron vértices en este ciclo."));
        return;
    }

    // Aplicar los cambios a los colores de los vértices
    LODInfo.OverrideVertexColors->InitFromColorArray(CurrentColors);
    BeginReleaseResource(LODInfo.OverrideVertexColors);
    FlushRenderingCommands();
    BeginInitResource(LODInfo.OverrideVertexColors);
    StaticMeshComp->MarkRenderStateDirty();
}

void UMeshPaintingComponent::UpdateVertexGroupFades(float DeltaTime)
{
    if (MeshVertexContributions.Num() == 0)
    {
        return;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();

    // Lista para almacenar MeshComps que deben ser eliminados
    TArray<UStaticMeshComponent*> MeshCompsToRemove;

    for (auto& MeshPair : MeshVertexContributions)
    {
        UStaticMeshComponent* MeshComp = MeshPair.Key;
        TMap<uint32, TArray<FVertexPaintContribution>>& VertexContributions = MeshPair.Value;

        if (!MeshComp || MeshComp->LODData.Num() == 0)
        {
            MeshCompsToRemove.Add(MeshComp);
            continue;
        }

        FStaticMeshComponentLODInfo& LODInfo = MeshComp->LODData[0];
        if (!LODInfo.OverrideVertexColors || !LODInfo.OverrideVertexColors->IsInitialized())
        {
            MeshCompsToRemove.Add(MeshComp);
            continue;
        }

        TArray<FColor> CurrentColors;
        LODInfo.OverrideVertexColors->GetVertexColors(CurrentColors);

        bool bModifiedColors = false;

        // Lista para almacenar índices de vértices que deben ser eliminados
        TArray<uint32> VerticesToRemove;

        for (auto& VertexPair : VertexContributions)
        {
            uint32 VertexIndex = VertexPair.Key;
            TArray<FVertexPaintContribution>& Contributions = VertexPair.Value;

            if (VertexIndex >= static_cast<uint32>(CurrentColors.Num()))
            {
                VerticesToRemove.Add(VertexIndex);
                continue;
            }

            float TotalIntensity = 0.0f;

            // Lista para almacenar índices de contribuciones que deben ser eliminadas
            TArray<int32> ContributionsToRemove;

            for (int32 i = Contributions.Num() - 1; i >= 0; --i)
            {
                FVertexPaintContribution& Contribution = Contributions[i];

                float TimeSincePainted = CurrentTime - Contribution.TimePainted;
                float FadeProgress = 0.0f;

                if (TimeSincePainted >= Contribution.EraseAfterSeconds)
                {
                    FadeProgress = (TimeSincePainted - Contribution.EraseAfterSeconds) / Contribution.FadeSpeed;
                    FadeProgress = FMath::Clamp(FadeProgress, 0.0f, 1.0f);
                }

                float RemainingIntensity = Contribution.InitialIntensity * (1.0f - FadeProgress);

                // Si la intensidad restante es muy pequeña, marcar para eliminar
                if (RemainingIntensity <= KINDA_SMALL_NUMBER)
                {
                    Contributions.RemoveAt(i);
                    continue;
                }
                TotalIntensity += RemainingIntensity;
            }

            // Eliminar las contribuciones marcadas
            for (int32 IndexToRemove : ContributionsToRemove)
            {
                Contributions.RemoveAt(IndexToRemove);
            }

            // Si ya no hay contribuciones, marcar el vértice para eliminar
            if (Contributions.Num() == 0)
            {
                VerticesToRemove.Add(VertexIndex);
            }

            // Clampear la intensidad total
            TotalIntensity = FMath::Clamp(TotalIntensity, 0.0f, 1.0f);

            // Si la intensidad total es muy pequeña, establecerla en cero
            if (TotalIntensity <= KINDA_SMALL_NUMBER)
            {
                TotalIntensity = 0.0f;
            }

            // Actualizar el color del vértice
            FColor& VertexColor = CurrentColors[VertexIndex];
            uint8 ColorValue = static_cast<uint8>(TotalIntensity * 255.0f);

            switch (Channel)
            {
            case EMaterialChannel::Red:
                VertexColor.R = ColorValue;
                break;
            case EMaterialChannel::Green:
                VertexColor.G = ColorValue;
                break;
            case EMaterialChannel::Blue:
                VertexColor.B = ColorValue;
                break;
            case EMaterialChannel::Alpha:
                VertexColor.A = ColorValue;
                break;
            }

            bModifiedColors = true;
        }

        // Eliminar los vértices marcados
        for (uint32 VertexIndexToRemove : VerticesToRemove)
        {
            VertexContributions.Remove(VertexIndexToRemove);
        }

        if (bModifiedColors)
        {
            // Aplicar los cambios a los colores de los vértices
            LODInfo.OverrideVertexColors->InitFromColorArray(CurrentColors);
            BeginReleaseResource(LODInfo.OverrideVertexColors);
            FlushRenderingCommands();
            BeginInitResource(LODInfo.OverrideVertexColors);
            MeshComp->MarkRenderStateDirty();
        }

        // Si ya no hay vértices con contribuciones, marcar el MeshComp para eliminar
        if (VertexContributions.Num() == 0)
        {
            MeshCompsToRemove.Add(MeshComp);
        }
    }

    // Eliminar los MeshComps marcados
    for (UStaticMeshComponent* MeshCompToRemove : MeshCompsToRemove)
    {
        MeshVertexContributions.Remove(MeshCompToRemove);
    }
}













