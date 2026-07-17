// Automation tests for AMeltableActor material retention on multi-material meshes.
//
// Reproduces the report that complex imports (e.g. Content/LevelAssets/circuitbox.glb:
// an open box shell with a separate zero-thickness interior "Inside" quad) lose
// material sections after the surface-nets conversion.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeltableActor.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"

namespace MeltableActorTests
{
/** Minimal game world that exists for the duration of one test. */
struct FScopedTestWorld
{
	UWorld* World = nullptr;

	FScopedTestWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MeltableActorTestWorld"));
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(World);

		World->InitializeActorsForPlay(FURL());
		// Test worlds have no game mode, so route begin-play through world settings
		// (UWorld::BeginPlay alone would never dispatch actor BeginPlay).
		World->GetWorldSettings()->NotifyBeginPlay();
		World->GetWorldSettings()->NotifyMatchStarted();
		World->BeginPlay();
	}

	~FScopedTestWorld()
	{
		if (World)
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	}
};

/** Helper that accumulates quads into polygon groups and builds a transient static mesh. */
struct FTestMeshBuilder
{
	FMeshDescription MeshDescription;
	FStaticMeshAttributes Attributes;
	FMeshDescriptionBuilder Builder;
	TArray<FPolygonGroupID> PolygonGroups;
	TArray<FName> SlotNames;

	explicit FTestMeshBuilder(int32 MaterialCount)
		: Attributes(MeshDescription)
	{
		Attributes.Register();
		Builder.SetMeshDescription(&MeshDescription);
		Builder.EnablePolyGroups();
		Builder.SetNumUVLayers(1);

		TPolygonGroupAttributesRef<FName> GroupSlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			const FPolygonGroupID GroupID = Builder.AppendPolygonGroup();
			const FName SlotName(*FString::Printf(TEXT("TestSlot%d"), MaterialIndex));
			GroupSlotNames[GroupID] = SlotName;
			PolygonGroups.Add(GroupID);
			SlotNames.Add(SlotName);
		}
	}

	void AppendQuad(int32 MaterialIndex, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		const FVector Normal = FVector::CrossProduct(B - A, D - A).GetSafeNormal();
		const FVector Corners[4] = {A, B, C, D};
		const FVector2D UVs[4] = {FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1)};

		FVertexInstanceID Instances[4];
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			const FVertexID VertexID = Builder.AppendVertex(Corners[Corner]);
			Instances[Corner] = Builder.AppendInstance(VertexID);
			Builder.SetInstanceUV(Instances[Corner], UVs[Corner], 0);
			Builder.SetInstanceNormal(Instances[Corner], Normal);
		}

		const FPolygonGroupID GroupID = PolygonGroups[MaterialIndex];
		Builder.AppendTriangle(Instances[0], Instances[1], Instances[2], GroupID);
		Builder.AppendTriangle(Instances[0], Instances[2], Instances[3], GroupID);
	}

	/** Appends an axis-aligned box. Face order: -X, +X, -Y, +Y, -Z, +Z. SkipFace omits one face to make an open shell. */
	void AppendBox(const FVector& Min, const FVector& Max, const int32 FaceMaterials[6], int32 SkipFace = INDEX_NONE)
	{
		const FVector V000(Min.X, Min.Y, Min.Z);
		const FVector V100(Max.X, Min.Y, Min.Z);
		const FVector V010(Min.X, Max.Y, Min.Z);
		const FVector V110(Max.X, Max.Y, Min.Z);
		const FVector V001(Min.X, Min.Y, Max.Z);
		const FVector V101(Max.X, Min.Y, Max.Z);
		const FVector V011(Min.X, Max.Y, Max.Z);
		const FVector V111(Max.X, Max.Y, Max.Z);

		struct FFace
		{
			FVector A, B, C, D;
		};
		const FFace Faces[6] = {
			{V000, V001, V011, V010}, // -X
			{V100, V110, V111, V101}, // +X
			{V000, V100, V101, V001}, // -Y
			{V010, V011, V111, V110}, // +Y
			{V000, V010, V110, V100}, // -Z
			{V001, V101, V111, V011}, // +Z
		};

		for (int32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
		{
			if (FaceIndex == SkipFace)
			{
				continue;
			}
			AppendQuad(FaceMaterials[FaceIndex], Faces[FaceIndex].A, Faces[FaceIndex].B, Faces[FaceIndex].C, Faces[FaceIndex].D);
		}
	}

	UStaticMesh* Build(const TArray<UMaterialInterface*>& Materials)
	{
		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient);
		for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
		{
			FStaticMaterial StaticMaterial(Materials[MaterialIndex], SlotNames[MaterialIndex], SlotNames[MaterialIndex]);
			StaticMesh->GetStaticMaterials().Add(StaticMaterial);
		}

		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Params.bAllowCpuAccess = true;

		TArray<const FMeshDescription*> MeshDescriptions;
		MeshDescriptions.Add(&MeshDescription);
		if (!StaticMesh->BuildFromMeshDescriptions(MeshDescriptions, Params))
		{
			return nullptr;
		}
		return StaticMesh;
	}
};

TArray<UMaterialInterface*> MakeTestMaterials(int32 Count)
{
	TArray<UMaterialInterface*> Materials;
	UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	for (int32 MaterialIndex = 0; MaterialIndex < Count; ++MaterialIndex)
	{
		Materials.Add(UMaterialInstanceDynamic::Create(DefaultMaterial, GetTransientPackage()));
	}
	return Materials;
}

struct FConversionResult
{
	bool bSpawned = false;
	int32 SectionCount = 0;
	TArray<int32> TriangleCountPerSection;
	TArray<UMaterialInterface*> SectionMaterials;
};

/** Spawns a MeltableActor around the mesh, lets BeginPlay run the surface-nets conversion, and reports the generated sections. */
FConversionResult RunMeltableConversion(FAutomationTestBase& Test, UStaticMesh* StaticMesh, const TArray<UMaterialInterface*>& Materials)
{
	FConversionResult Result;

	FScopedTestWorld ScopedWorld;
	AMeltableActor* Actor = ScopedWorld.World->SpawnActorDeferred<AMeltableActor>(AMeltableActor::StaticClass(), FTransform::Identity);
	if (!Actor)
	{
		Test.AddError(TEXT("Failed to spawn AMeltableActor."));
		return Result;
	}

	UStaticMeshComponent* SourceMesh = Actor->FindComponentByClass<UStaticMeshComponent>();
	if (!SourceMesh)
	{
		Test.AddError(TEXT("MeltableActor has no source static mesh component."));
		return Result;
	}

	SourceMesh->SetStaticMesh(StaticMesh);
	for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
	{
		SourceMesh->SetMaterial(MaterialIndex, Materials[MaterialIndex]);
	}

	Actor->FinishSpawning(FTransform::Identity);
	Result.bSpawned = true;

	UProceduralMeshComponent* GeneratedMesh = Actor->FindComponentByClass<UProceduralMeshComponent>();
	if (!GeneratedMesh)
	{
		Test.AddError(TEXT("MeltableActor has no generated procedural mesh component."));
		return Result;
	}

	Result.SectionCount = GeneratedMesh->GetNumSections();
	for (int32 SectionIndex = 0; SectionIndex < Result.SectionCount; ++SectionIndex)
	{
		const FProcMeshSection* Section = GeneratedMesh->GetProcMeshSection(SectionIndex);
		Result.TriangleCountPerSection.Add(Section ? Section->ProcIndexBuffer.Num() / 3 : 0);
		Result.SectionMaterials.Add(GeneratedMesh->GetMaterial(SectionIndex));
		Test.AddInfo(FString::Printf(
			TEXT("Generated section %d: %d triangles, material %s"),
			SectionIndex,
			Result.TriangleCountPerSection.Last(),
			*GetNameSafe(Result.SectionMaterials.Last())
		));
	}

	return Result;
}

void ExpectMaterialRetained(
	FAutomationTestBase& Test,
	const FConversionResult& Result,
	int32 MaterialIndex,
	UMaterialInterface* ExpectedMaterial)
{
	const bool bHasSection =
		Result.TriangleCountPerSection.IsValidIndex(MaterialIndex) &&
		Result.TriangleCountPerSection[MaterialIndex] > 0;

	Test.TestTrue(
		FString::Printf(TEXT("Material %d should still drive a non-empty mesh section after conversion"), MaterialIndex),
		bHasSection
	);

	if (bHasSection)
	{
		Test.TestEqual(
			FString::Printf(TEXT("Section %d should use the source material"), MaterialIndex),
			Result.SectionMaterials[MaterialIndex],
			ExpectedMaterial
		);
	}
}
} // namespace MeltableActorTests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMeltableActorClosedTwoMaterialBoxTest,
	"RabbitLab.MeltableActor.MaterialRetention.ClosedTwoMaterialBox",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMeltableActorClosedTwoMaterialBoxTest::RunTest(const FString& Parameters)
{
	using namespace MeltableActorTests;

	// Closed 200 cm box: five faces material 0, top face material 1.
	FTestMeshBuilder MeshBuilder(2);
	const int32 FaceMaterials[6] = {0, 0, 0, 0, 0, 1};
	MeshBuilder.AppendBox(FVector(-100), FVector(100), FaceMaterials);

	const TArray<UMaterialInterface*> Materials = MakeTestMaterials(2);
	UStaticMesh* StaticMesh = MeshBuilder.Build(Materials);
	if (!TestNotNull(TEXT("Static mesh built"), StaticMesh))
	{
		return false;
	}

	const FConversionResult Result = RunMeltableConversion(*this, StaticMesh, Materials);
	if (!Result.bSpawned)
	{
		return false;
	}

	ExpectMaterialRetained(*this, Result, 0, Materials[0]);
	ExpectMaterialRetained(*this, Result, 1, Materials[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMeltableActorInteriorPanelTest,
	"RabbitLab.MeltableActor.MaterialRetention.ClosedShellWithInteriorPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMeltableActorInteriorPanelTest::RunTest(const FString& Parameters)
{
	using namespace MeltableActorTests;

	// Closed shell with a separate zero-thickness interior quad using a second
	// material. Mirrors circuitbox.glb's "Inside" panel (dimensions in cm match
	// the glb scaled x100).
	FTestMeshBuilder MeshBuilder(2);
	const int32 FaceMaterials[6] = {0, 0, 0, 0, 0, 0};
	MeshBuilder.AppendBox(FVector(-114.0, -484.7, -535.9), FVector(483.4, 484.7, 293.4), FaceMaterials);

	// Interior flat panel at X=-78.4 (about 36 cm inside the -X wall).
	MeshBuilder.AppendQuad(
		1,
		FVector(-78.4, -423.8, -232.5),
		FVector(-78.4, 423.8, -232.5),
		FVector(-78.4, 423.8, 232.5),
		FVector(-78.4, -423.8, 232.5)
	);

	const TArray<UMaterialInterface*> Materials = MakeTestMaterials(2);
	UStaticMesh* StaticMesh = MeshBuilder.Build(Materials);
	if (!TestNotNull(TEXT("Static mesh built"), StaticMesh))
	{
		return false;
	}

	const FConversionResult Result = RunMeltableConversion(*this, StaticMesh, Materials);
	if (!Result.bSpawned)
	{
		return false;
	}

	ExpectMaterialRetained(*this, Result, 0, Materials[0]);
	ExpectMaterialRetained(*this, Result, 1, Materials[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMeltableActorOpenShellInteriorPanelTest,
	"RabbitLab.MeltableActor.MaterialRetention.OpenShellWithInteriorPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMeltableActorOpenShellInteriorPanelTest::RunTest(const FString& Parameters)
{
	using namespace MeltableActorTests;

	// Circuitbox-like: the shell is NOT watertight (one face missing, matching the
	// glb's 4 boundary edges) plus the interior "Inside" quad. The signed-distance
	// parity test assumes closed meshes, so this is the worst case from the report.
	FTestMeshBuilder MeshBuilder(2);
	const int32 FaceMaterials[6] = {0, 0, 0, 0, 0, 0};
	constexpr int32 SkipPositiveXFace = 1;
	MeshBuilder.AppendBox(FVector(-114.0, -484.7, -535.9), FVector(483.4, 484.7, 293.4), FaceMaterials, SkipPositiveXFace);

	MeshBuilder.AppendQuad(
		1,
		FVector(-78.4, -423.8, -232.5),
		FVector(-78.4, 423.8, -232.5),
		FVector(-78.4, 423.8, 232.5),
		FVector(-78.4, -423.8, 232.5)
	);

	const TArray<UMaterialInterface*> Materials = MakeTestMaterials(2);
	UStaticMesh* StaticMesh = MeshBuilder.Build(Materials);
	if (!TestNotNull(TEXT("Static mesh built"), StaticMesh))
	{
		return false;
	}

	const FConversionResult Result = RunMeltableConversion(*this, StaticMesh, Materials);
	if (!Result.bSpawned)
	{
		return false;
	}

	ExpectMaterialRetained(*this, Result, 0, Materials[0]);
	ExpectMaterialRetained(*this, Result, 1, Materials[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMeltableActorImportedCircuitBoxTest,
	"RabbitLab.MeltableActor.MaterialRetention.ImportedCircuitBoxAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMeltableActorImportedCircuitBoxTest::RunTest(const FString& Parameters)
{
	using namespace MeltableActorTests;

	// Runs the conversion against the actual imported asset the bug was reported on.
	UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Game/LevelAssets/circuitbox/StaticMeshes/circuitbox.circuitbox"));
	if (!StaticMesh)
	{
		AddWarning(TEXT("circuitbox static mesh not found; skipping."));
		return true;
	}

	AddInfo(FString::Printf(TEXT("Source material slots: %d"), StaticMesh->GetStaticMaterials().Num()));
	AddInfo(FString::Printf(TEXT("Nanite enabled: %s, has Nanite data: %s"),
		StaticMesh->IsNaniteEnabled() ? TEXT("yes") : TEXT("no"),
		StaticMesh->HasValidNaniteData() ? TEXT("yes") : TEXT("no")));

	if (const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData())
	{
		if (!RenderData->LODResources.IsEmpty())
		{
			const FStaticMeshLODResources& LOD0 = RenderData->LODResources[0];
			AddInfo(FString::Printf(TEXT("LOD0 (what MeltableActor reads): %d sections, %d triangles total"),
				LOD0.Sections.Num(), static_cast<int32>(LOD0.IndexBuffer.GetNumIndices() / 3)));
			for (int32 SectionIndex = 0; SectionIndex < LOD0.Sections.Num(); ++SectionIndex)
			{
				AddInfo(FString::Printf(TEXT("  LOD0 section %d: material index %d, %d triangles"),
					SectionIndex,
					LOD0.Sections[SectionIndex].MaterialIndex,
					static_cast<int32>(LOD0.Sections[SectionIndex].NumTriangles)));
			}
		}
		else
		{
			AddInfo(TEXT("LOD0: no LODResources (render data empty)"));
		}
	}

	TArray<UMaterialInterface*> Materials;
	for (const FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials())
	{
		Materials.Add(StaticMaterial.MaterialInterface);
	}

	const FConversionResult Result = RunMeltableConversion(*this, StaticMesh, Materials);
	if (!Result.bSpawned)
	{
		return false;
	}

	for (int32 MaterialIndex = 0; MaterialIndex < Materials.Num(); ++MaterialIndex)
	{
		ExpectMaterialRetained(*this, Result, MaterialIndex, Materials[MaterialIndex]);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
