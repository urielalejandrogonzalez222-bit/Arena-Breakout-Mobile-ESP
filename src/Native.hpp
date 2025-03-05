#pragma once

#include <codecvt>
#include <imgui.h>
#include <locale>
#include <string>

namespace Colors {
    constexpr ImVec4 BLACK = ImVec4(0.06f, 0.06f, 0.06f, 1.f);
    constexpr ImVec4 WHITE = ImVec4(1.f, 1.f, 1.f, 1.f);
    constexpr ImVec4 RED = ImVec4(1.f, 0.f, 0.f, 1.f);
    constexpr ImVec4 DARK_RED = ImVec4(0.6f, 0.f, 0.f, 1.f);
    constexpr ImVec4 GREEN = ImVec4(0.f, 1.f, 0.f, 1.f);
    constexpr ImVec4 DARK_GREEN = ImVec4(0.f, 0.6f, 0.f, 1.f);
    constexpr ImVec4 YELLOW = ImVec4(1.f, 1.f, 0.f, 1.f);
    constexpr ImVec4 DARK_YELLOW = ImVec4(0.6f, 0.6f, 0.f, 1.f);
    constexpr ImVec4 CYAN = ImVec4(0.f, 1.f, 1.f, 1.f);
    constexpr ImVec4 PURPLE = ImVec4(1.f, 0.f, 1.f, 1.f);
    constexpr ImVec4 GRAY = ImVec4(0.5f, 0.5f, 0.5f, 1.f);
    constexpr ImVec4 ORANGE = ImVec4(1.f, 0.54f, 0.f, 1.f);
    constexpr ImVec4 BLUE = ImVec4(0.f, 0.f, 1.f, 1.f);
    constexpr ImVec4 BROWN = ImVec4(0.54f, 0.27f, 0.06f, 1.f);
    constexpr ImVec4 SPELL_NOT_READY = ImVec4(0.29f, 0.31f, 0.34f, 1.f);
} // namespace Colors

template <typename T>
std::string ToUTF8(const std::basic_string<T, std::char_traits<T>, std::allocator<T>> &source) {
    std::string result;
    std::wstring_convert<std::codecvt_utf8_utf16<T>, T> convertor;
    result = convertor.to_bytes(source);
    return result;
}

template <typename T>
void UTF8To(const std::string &source, std::basic_string<T, std::char_traits<T>, std::allocator<T>> &result) {
    std::wstring_convert<std::codecvt_utf8_utf16<T>, T> convertor;
    result = convertor.from_bytes(source);
}

namespace Core {
    enum Offset {
        GNames = 0xC52EB80,
        GObjects = 0xC54E2B0,
        GWorld = 0xC75A4E8,
        GNativeAndroidApp = 0xC091F40,
        ANativeActivity_onCreate = 0x448F904,
    };

    inline double GetTickCount() { // ms
        struct timespec now {};
        if (clock_gettime(CLOCK_MONOTONIC, &now)) {
            return 0;
        }
        return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
    }
} // namespace Core

template <typename T>
struct TArray {
    T *Data;
    int Count;
    int Max;
};

static constexpr uint32_t FNameMaxBlockBits = 13;
static constexpr uint32_t FNameBlockOffsetBits = 16;
static constexpr uint32_t FNameMaxBlocks = 1 << FNameMaxBlockBits;
static constexpr uint32_t FNameBlockOffsets = 1 << FNameBlockOffsetBits;

struct FPThreadsRWLock {
    pthread_rwlock_t Mutex;
};

typedef FPThreadsRWLock FRWLock;

struct FNameEntryId {
    uint32_t Value;
};

struct FNameEntryHeader {
    uint16_t bIsWide : 1;
    uint16_t LowercaseProbeHash : 5;
    uint16_t Len : 10;
};

struct FNameEntry {
    FNameEntryHeader Header;
    union {
        char AnsiName[1024];
        char16_t WideName[1024];
    };

    std::string ToString();
};

struct FName {
    FNameEntryId ComparisonIndex;
    uint32_t Number;

    FNameEntry *GetDisplayNameEntry() const;
    std::string GetName();
};

struct FNameEntryHandle {
    uint32_t Block = 0;
    uint32_t Offset = 0;

    FNameEntryHandle(FNameEntryId Id)
        : Block(Id.Value >> FNameBlockOffsetBits),
          Offset(Id.Value & (FNameBlockOffsets - 1)) {}
};

struct FNameEntryAllocator {
    mutable FRWLock Lock;
    uint32_t CurrentBlock = 0;
    uint32_t CurrentByteCursor = 0;
    uint8_t *Blocks[FNameMaxBlocks] = {};

    enum {
        Stride = alignof(FNameEntry)
    };

    FNameEntry &Resolve(FNameEntryHandle Handle) const {
        return *reinterpret_cast<FNameEntry *>(Blocks[Handle.Block] + Stride * Handle.Offset);
    }
};

struct FNamePool {
    FNameEntryAllocator Entries;

    FNameEntry &Resolve(FNameEntryHandle Handle) const {
        return Entries.Resolve(Handle);
    }
};

struct UObject {
    void **VTable;
    int32_t ObjectFlags;
    int32_t InternalIndex;
    struct UClass *ClassPrivate;
    FName NamePrivate;
    UObject *OuterPrivate;

    std::string GetName();
    std::string GetFullName();
};

struct FUObjectItem {
    struct UObject *Object;
    int32_t Flags;
    int32_t ClusterRootIndex;
    int32_t SerialNumber;
};

struct TUObjectArray {
    FUObjectItem **Objects;
    FUObjectItem *PreAllocatedObjects;
    int32_t MaxElements;
    int32_t NumElements;
    int32_t MaxChunks;
    int32_t NumChunks;

    enum {
        NumElementsPerChunk = 64 * 1024,
    };

    bool IsValidIndex(int32_t Index) const {
        return Index < NumElements && Index >= 0;
    }

    FUObjectItem *GetObjectPtr(int32_t Index) {
        const int32_t ChunkIndex = Index / NumElementsPerChunk;
        const int32_t WithinChunkIndex = Index % NumElementsPerChunk;
        if (!IsValidIndex(Index))
            return nullptr;
        FUObjectItem *Chunk = Objects[ChunkIndex];
        if (!Chunk)
            return nullptr;
        return Chunk + WithinChunkIndex;
    }

    template <typename T>
    T FindObject(const char *name) {
        for (int i = 0; i < NumElements; ++i) {
            auto objectItem = GetObjectPtr(i);
            if (objectItem && objectItem->Object && objectItem->Object->GetFullName() == name)
                return static_cast<T>(objectItem->Object);
        }

        return nullptr;
    }

    template <typename T>
    std::vector<T> FindObjects(const char *name) {
        std::vector<T> result{};
        for (int i = 0; i < NumElements; ++i) {
            auto objectItem = GetObjectPtr(i);
            if (objectItem && objectItem->Object && objectItem->Object->GetFullName().contains(name))
                result.push_back(static_cast<T>(objectItem->Object));
        }

        return result;
    }
};

struct FUObjectArray {
    int32_t ObjFirstGCIndex;
    int32_t ObjLastNonGCIndex;
    int32_t MaxObjectsNotConsideredByGC;
    bool OpenForDisregardForGC;
    TUObjectArray ObjObjects;
};

struct UField : UObject {
    // Fields
    char pad_0x28[0x8]; // Offset: 0x28 | Size: 0x8
};

struct UStruct : UField {
    // Fields
    char pad_0x30[0x80]; // Offset: 0x30 | Size: 0x80
};

struct UClass : UStruct {
    // Fields
    char pad_0xB0[0x1b0]; // Offset: 0xb0 | Size: 0x1b0
};

struct FVector2D {
    // Fields
    float X; // Offset: 0x0 | Size: 0x4
    float Y; // Offset: 0x4 | Size: 0x4
};

struct FColor {
    // Fields
    char B; // Offset: 0x0 | Size: 0x1
    char G; // Offset: 0x1 | Size: 0x1
    char R; // Offset: 0x2 | Size: 0x1
    char A; // Offset: 0x3 | Size: 0x1
};

struct FLinearColor {
    // Fields
    float R; // Offset: 0x0 | Size: 0x4
    float G; // Offset: 0x4 | Size: 0x4
    float B; // Offset: 0x8 | Size: 0x4
    float A; // Offset: 0xc | Size: 0x4
};

struct FString {
    TArray<char16_t> Data;

    FString() = default;

    FString(const char16_t *str) {
        Data.Max = Data.Count = (int)std::char_traits<char16_t>::length(str) + 1;
        Data.Data = const_cast<char16_t *>(str);
    }

    std::string ToString() {
        std::u16string uText(Data.Data, Data.Count);
        return ToUTF8(uText);
    }
};

struct UPlayer : UObject {
    // Fields
    char pad_0x28[0x8];                         // Offset: 0x28 | Size: 0x8
    struct APlayerController *PlayerController; // Offset: 0x30 | Size: 0x8
    int32_t CurrentNetSpeed;                    // Offset: 0x38 | Size: 0x4
    int32_t ConfiguredInternetSpeed;            // Offset: 0x3c | Size: 0x4
    int32_t ConfiguredLanSpeed;                 // Offset: 0x40 | Size: 0x4
    char pad_0x44[0x4];                         // Offset: 0x44 | Size: 0x4
};

struct ULocalPlayer : UPlayer {
    // Fields
    char pad_0x48[0x28];                                         // Offset: 0x48 | Size: 0x28
    struct UGameViewportClient *ViewportClient;                  // Offset: 0x70 | Size: 0x8
    char pad_0x78[0x1c];                                         // Offset: 0x78 | Size: 0x1c
    char AspectRatioAxisConstraint;                              // Offset: 0x94 | Size: 0x1
    char pad_0x95[0x3];                                          // Offset: 0x95 | Size: 0x3
    struct APlayerController *PendingLevelPlayerControllerClass; // Offset: 0x98 | Size: 0x8
    char bSentSplitJoin : 1;                                     // Offset: 0xa0 | Size: 0x1
    char pad_0xA0_1 : 7;                                         // Offset: 0xa0 | Size: 0x1
    char pad_0xA1[0x17];                                         // Offset: 0xa1 | Size: 0x17
    int32_t ControllerId;                                        // Offset: 0xb8 | Size: 0x4
    char pad_0xBC[0x19c];                                        // Offset: 0xbc | Size: 0x19c
};

struct UGameInstance : UObject {
    // Fields
    char pad_0x28[0x10];                               // Offset: 0x28 | Size: 0x10
    struct TArray<struct ULocalPlayer *> LocalPlayers; // Offset: 0x38 | Size: 0x10
    struct UOnlineSession *OnlineSession;              // Offset: 0x48 | Size: 0x8
    struct TArray<struct UObject *> ReferencedObjects; // Offset: 0x50 | Size: 0x10
    char pad_0x60[0x18];                               // Offset: 0x60 | Size: 0x18
    char OnPawnControllerChangedDelegates[0x10];       // Offset: 0x78 | Size: 0x10
    char pad_0x88[0x120];                              // Offset: 0x88 | Size: 0x120
};

struct ULevel : UObject {
    // Fields
    char pad_0x28[0x70];                                                                         // Offset: 0x28 | Size: 0x70
    struct TArray<struct AActor *> Actors;                                                       // Offset: 0x98 | Size: 0x10
    char pad_0xA8[0x10];                                                                         // Offset: 0xA8 | Size: 0x10
    struct UWorld *OwningWorld;                                                                  // Offset: 0xb8 | Size: 0x8
    struct UModel *Model;                                                                        // Offset: 0xc0 | Size: 0x8
    struct TArray<struct UModelComponent *> ModelComponents;                                     // Offset: 0xc8 | Size: 0x10
    struct ULevelActorContainer *ActorCluster;                                                   // Offset: 0xd8 | Size: 0x8
    int32_t NumTextureStreamingUnbuiltComponents;                                                // Offset: 0xe0 | Size: 0x4
    int32_t NumTextureStreamingDirtyResources;                                                   // Offset: 0xe4 | Size: 0x4
    struct ALevelScriptActor *LevelScriptActor;                                                  // Offset: 0xe8 | Size: 0x8
    struct ANavigationObjectBase *NavListStart;                                                  // Offset: 0xf0 | Size: 0x8
    struct ANavigationObjectBase *NavListEnd;                                                    // Offset: 0xf8 | Size: 0x8
    struct TArray<struct UNavigationDataChunk *> NavDataChunks;                                  // Offset: 0x100 | Size: 0x10
    float LightmapTotalSize;                                                                     // Offset: 0x110 | Size: 0x4
    float ShadowmapTotalSize;                                                                    // Offset: 0x114 | Size: 0x4
    struct TArray<struct FVector> StaticNavigableGeometry;                                       // Offset: 0x118 | Size: 0x10
    struct TArray<struct FGuid> StreamingTextureGuids;                                           // Offset: 0x128 | Size: 0x10
    char pad_0x138[0x270];                                                                       // Offset: 0x138 | Size: 0x270
    char LevelBuildDataId[0x10];                                                                 // Offset: 0x3a8 | Size: 0x10
    struct UMapBuildDataRegistry *MapBuildData;                                                  // Offset: 0x3b8 | Size: 0x8
    char LightBuildLevelOffset[0xc];                                                             // Offset: 0x3c0 | Size: 0xc
    char bIsLightingScenario : 1;                                                                // Offset: 0x3cc | Size: 0x1
    char pad_0x3CC_1 : 2;                                                                        // Offset: 0x3cc | Size: 0x1
    char bTextureStreamingRotationChanged : 1;                                                   // Offset: 0x3cc | Size: 0x1
    char bStaticComponentsRegisteredInStreamingManager : 1;                                      // Offset: 0x3cc | Size: 0x1
    char bIsVisible : 1;                                                                         // Offset: 0x3cc | Size: 0x1
    char pad_0x3CC_6 : 2;                                                                        // Offset: 0x3cc | Size: 0x1
    char pad_0x3CD[0x73];                                                                        // Offset: 0x3cd | Size: 0x73
    struct AWorldSettings *WorldSettings;                                                        // Offset: 0x440 | Size: 0x8
    char pad_0x448[0x8];                                                                         // Offset: 0x448 | Size: 0x8
    struct TArray<struct UAssetUserData *> AssetUserData;                                        // Offset: 0x450 | Size: 0x10
    char pad_0x460[0x10];                                                                        // Offset: 0x460 | Size: 0x10
    struct TArray<struct FReplicatedStaticActorDestructionInfo> DestroyedReplicatedStaticActors; // Offset: 0x470 | Size: 0x10
};

struct UWorld : UObject {
    // Fields
    char pad_0x28[0x8];                                                                        // Offset: 0x28 | Size: 0x8
    struct ULevel *PersistentLevel;                                                            // Offset: 0x30 | Size: 0x8
    struct UNetDriver *NetDriver;                                                              // Offset: 0x38 | Size: 0x8
    struct ULineBatchComponent *LineBatcher;                                                   // Offset: 0x40 | Size: 0x8
    struct ULineBatchComponent *PersistentLineBatcher;                                         // Offset: 0x48 | Size: 0x8
    struct ULineBatchComponent *ForegroundLineBatcher;                                         // Offset: 0x50 | Size: 0x8
    struct AGameNetworkManager *NetworkManager;                                                // Offset: 0x58 | Size: 0x8
    struct UPhysicsCollisionHandler *PhysicsCollisionHandler;                                  // Offset: 0x60 | Size: 0x8
    struct TArray<struct UObject *> ExtraReferencedObjects;                                    // Offset: 0x68 | Size: 0x10
    struct TArray<struct UObject *> PerModuleDataObjects;                                      // Offset: 0x78 | Size: 0x10
    struct TArray<struct ULevelStreaming *> StreamingLevels;                                   // Offset: 0x88 | Size: 0x10
    char StreamingLevelsToConsider[0x28];                                                      // Offset: 0x98 | Size: 0x28
    char StreamingLevelsPrefix[0x10];                                                          // Offset: 0xc0 | Size: 0x10
    struct ULevel *CurrentLevelPendingVisibility;                                              // Offset: 0xd0 | Size: 0x8
    struct ULevel *CurrentLevelPendingInvisibility;                                            // Offset: 0xd8 | Size: 0x8
    struct UDemoNetDriver *DemoNetDriver;                                                      // Offset: 0xe0 | Size: 0x8
    struct AParticleEventManager *MyParticleEventManager;                                      // Offset: 0xe8 | Size: 0x8
    struct APhysicsVolume *DefaultPhysicsVolume;                                               // Offset: 0xf0 | Size: 0x8
    char pad_0xF8[0x16];                                                                       // Offset: 0xf8 | Size: 0x16
    char pad_0x10E_0 : 2;                                                                      // Offset: 0x10e | Size: 0x1
    char bAreConstraintsDirty : 1;                                                             // Offset: 0x10e | Size: 0x1
    char pad_0x10E_3 : 5;                                                                      // Offset: 0x10e | Size: 0x1
    char pad_0x10F[0x1];                                                                       // Offset: 0x10f | Size: 0x1
    struct UNavigationSystemBase *NavigationSystem;                                            // Offset: 0x110 | Size: 0x8
    struct AGameModeBase *AuthorityGameMode;                                                   // Offset: 0x118 | Size: 0x8
    struct AGameStateBase *GameState;                                                          // Offset: 0x120 | Size: 0x8
    struct UAISystemBase *AISystem;                                                            // Offset: 0x128 | Size: 0x8
    struct UAvoidanceManager *AvoidanceManager;                                                // Offset: 0x130 | Size: 0x8
    struct TArray<struct ULevel *> Levels;                                                     // Offset: 0x138 | Size: 0x10
    struct TArray<struct FLevelCollection> LevelCollections;                                   // Offset: 0x148 | Size: 0x10
    char pad_0x158[0x28];                                                                      // Offset: 0x158 | Size: 0x28
    struct UGameInstance *OwningGameInstance;                                                  // Offset: 0x180 | Size: 0x8
    struct TArray<struct UMaterialParameterCollectionInstance *> ParameterCollectionInstances; // Offset: 0x188 | Size: 0x10
    struct UCanvas *CanvasForRenderingToTarget;                                                // Offset: 0x198 | Size: 0x8
    struct UCanvas *CanvasForDrawMaterialToRenderTarget;                                       // Offset: 0x1a0 | Size: 0x8
    char pad_0x1A8[0x50];                                                                      // Offset: 0x1a8 | Size: 0x50
    struct TArray<struct UPrimitiveComponent *> VolumeCacheComponents;                         // Offset: 0x1f8 | Size: 0x10
    char ComponentsThatNeedPreEndOfFrameSync[0x50];                                            // Offset: 0x208 | Size: 0x50
    struct TArray<struct UActorComponent *> ComponentsThatNeedEndOfFrameUpdate;                // Offset: 0x258 | Size: 0x10
    struct TArray<struct UActorComponent *> ComponentsThatNeedEndOfFrameUpdate_OnGameThread;   // Offset: 0x268 | Size: 0x10
    char pad_0x278[0x3a0];                                                                     // Offset: 0x278 | Size: 0x3a0
    struct UWorldComposition *WorldComposition;                                                // Offset: 0x618 | Size: 0x8
    char pad_0x620[0x90];                                                                      // Offset: 0x620 | Size: 0x90
    char PSCPool[0x58];                                                                        // Offset: 0x6b0 | Size: 0x58
    char pad_0x708[0x168];                                                                     // Offset: 0x708 | Size: 0x168
};

struct FVector {
    // Fields
    float X; // Offset: 0x0 | Size: 0x4
    float Y; // Offset: 0x4 | Size: 0x4
    float Z; // Offset: 0x8 | Size: 0x4

    FVector &operator+=(const FVector &v);
    FVector &operator+=(float fl);
    FVector &operator-=(const FVector &v);
    FVector &operator-=(float fl);
    FVector operator/(const FVector &v) const;
    FVector operator/(float mod) const;
    FVector operator-(const FVector &v) const;
    [[nodiscard]] float Distance(const FVector &to) const;
    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] float Dot(FVector const &other) const;
};

struct FRotator {
    // Fields
    float Pitch; // Offset: 0x0 | Size: 0x4
    float Yaw;   // Offset: 0x4 | Size: 0x4
    float Roll;  // Offset: 0x8 | Size: 0x4
};

struct AActor : UObject {
    // Fields
    char PrimaryActorTick[0x40];                                        // Offset: 0x28 | Size: 0x40
    char bNetTemporary : 1;                                             // Offset: 0x68 | Size: 0x1
    char bNetStartup : 1;                                               // Offset: 0x68 | Size: 0x1
    char bOnlyRelevantToOwner : 1;                                      // Offset: 0x68 | Size: 0x1
    char bAlwaysRelevant : 1;                                           // Offset: 0x68 | Size: 0x1
    char bReplicateMovement : 1;                                        // Offset: 0x68 | Size: 0x1
    char bHidden : 1;                                                   // Offset: 0x68 | Size: 0x1
    char bTearOff : 1;                                                  // Offset: 0x68 | Size: 0x1
    char bExchangedRoles : 1;                                           // Offset: 0x68 | Size: 0x1
    char bNetLoadOnClient : 1;                                          // Offset: 0x69 | Size: 0x1
    char bNetUseOwnerRelevancy : 1;                                     // Offset: 0x69 | Size: 0x1
    char bRelevantForNetworkReplays : 1;                                // Offset: 0x69 | Size: 0x1
    char bRelevantForLevelBounds : 1;                                   // Offset: 0x69 | Size: 0x1
    char bReplayRewindable : 1;                                         // Offset: 0x69 | Size: 0x1
    char bAllowTickBeforeBeginPlay : 1;                                 // Offset: 0x69 | Size: 0x1
    char bAutoDestroyWhenFinished : 1;                                  // Offset: 0x69 | Size: 0x1
    char bCanBeDamaged : 1;                                             // Offset: 0x69 | Size: 0x1
    char bBlockInput : 1;                                               // Offset: 0x6a | Size: 0x1
    char bCollideWhenPlacing : 1;                                       // Offset: 0x6a | Size: 0x1
    char bFindCameraComponentWhenViewTarget : 1;                        // Offset: 0x6a | Size: 0x1
    char bGenerateOverlapEventsDuringLevelStreaming : 1;                // Offset: 0x6a | Size: 0x1
    char bIgnoresOriginShifting : 1;                                    // Offset: 0x6a | Size: 0x1
    char bEnableAutoLODGeneration : 1;                                  // Offset: 0x6a | Size: 0x1
    char bIsEditorOnlyActor : 1;                                        // Offset: 0x6a | Size: 0x1
    char bActorSeamlessTraveled : 1;                                    // Offset: 0x6a | Size: 0x1
    char bReplicates : 1;                                               // Offset: 0x6b | Size: 0x1
    char bCanBeInCluster : 1;                                           // Offset: 0x6b | Size: 0x1
    char bAllowReceiveTickEventOnDedicatedServer : 1;                   // Offset: 0x6b | Size: 0x1
    char pad_0x6B_3 : 5;                                                // Offset: 0x6b | Size: 0x1
    char pad_0x6C_0 : 2;                                                // Offset: 0x6c | Size: 0x1
    char bActorEnableCollision : 1;                                     // Offset: 0x6c | Size: 0x1
    char bActorIsBeingDestroyed : 1;                                    // Offset: 0x6c | Size: 0x1
    char pad_0x6C_4 : 4;                                                // Offset: 0x6c | Size: 0x1
    char UpdateOverlapsMethodDuringLevelStreaming;                      // Offset: 0x6d | Size: 0x1
    char DefaultUpdateOverlapsMethodDuringLevelStreaming;               // Offset: 0x6e | Size: 0x1
    char RemoteRole;                                                    // Offset: 0x6f | Size: 0x1
    char pad_0x70[0x10];                                                // Offset: 0x70 | Size: 0x10
    char bManualReplicates : 1;                                         // Offset: 0x80 | Size: 0x1
    char bSubobjectsManualReplicates : 1;                               // Offset: 0x80 | Size: 0x1
    char bIgnoreAttachmentTranform : 1;                                 // Offset: 0x80 | Size: 0x1
    char pad_0x80_3 : 5;                                                // Offset: 0x80 | Size: 0x1
    char pad_0x81[0x3];                                                 // Offset: 0x81 | Size: 0x3
    char ReplicatedMovement[0x34];                                      // Offset: 0x84 | Size: 0x34
    float InitialLifeSpan;                                              // Offset: 0xb8 | Size: 0x4
    float CustomTimeDilation;                                           // Offset: 0xbc | Size: 0x4
    char pad_0xC0[0x8];                                                 // Offset: 0xc0 | Size: 0x8
    char AttachmentReplication[0x40];                                   // Offset: 0xc8 | Size: 0x40
    struct AActor *Owner;                                               // Offset: 0x108 | Size: 0x8
    struct FName NetDriverName;                                         // Offset: 0x110 | Size: 0x8
    char Role;                                                          // Offset: 0x118 | Size: 0x1
    char NetDormancy;                                                   // Offset: 0x119 | Size: 0x1
    char SpawnCollisionHandlingMethod;                                  // Offset: 0x11a | Size: 0x1
    char AutoReceiveInput;                                              // Offset: 0x11b | Size: 0x1
    int32_t InputPriority;                                              // Offset: 0x11c | Size: 0x4
    struct UInputComponent *InputComponent;                             // Offset: 0x120 | Size: 0x8
    float NetCullDistanceSquared;                                       // Offset: 0x128 | Size: 0x4
    int32_t NetTag;                                                     // Offset: 0x12c | Size: 0x4
    float NetUpdateFrequency;                                           // Offset: 0x130 | Size: 0x4
    float MinNetUpdateFrequency;                                        // Offset: 0x134 | Size: 0x4
    float NetPriority;                                                  // Offset: 0x138 | Size: 0x4
    char pad_0x13C[0x4];                                                // Offset: 0x13c | Size: 0x4
    struct APawn *Instigator;                                           // Offset: 0x140 | Size: 0x8
    struct TArray<struct AActor *> Children;                            // Offset: 0x148 | Size: 0x10
    struct USceneComponent *RootComponent;                              // Offset: 0x158 | Size: 0x8
    struct TArray<struct AMatineeActor *> ControllingMatineeActors;     // Offset: 0x160 | Size: 0x10
    char pad_0x170[0x8];                                                // Offset: 0x170 | Size: 0x8
    struct TArray<struct FName> Layers;                                 // Offset: 0x178 | Size: 0x10
    char ParentComponent[0x8];                                          // Offset: 0x188 | Size: 0x8
    char pad_0x190[0x8];                                                // Offset: 0x190 | Size: 0x8
    struct TArray<struct FName> Tags;                                   // Offset: 0x198 | Size: 0x10
    char OnTakeAnyDamage;                                               // Offset: 0x1a8 | Size: 0x1
    char OnTakePointDamage;                                             // Offset: 0x1a9 | Size: 0x1
    char OnTakeRadialDamage;                                            // Offset: 0x1aa | Size: 0x1
    char OnActorBeginOverlap;                                           // Offset: 0x1ab | Size: 0x1
    char OnActorEndOverlap;                                             // Offset: 0x1ac | Size: 0x1
    char OnBeginCursorOver;                                             // Offset: 0x1ad | Size: 0x1
    char OnEndCursorOver;                                               // Offset: 0x1ae | Size: 0x1
    char OnClicked;                                                     // Offset: 0x1af | Size: 0x1
    char OnReleased;                                                    // Offset: 0x1b0 | Size: 0x1
    char OnInputTouchBegin;                                             // Offset: 0x1b1 | Size: 0x1
    char OnInputTouchEnd;                                               // Offset: 0x1b2 | Size: 0x1
    char OnInputTouchEnter;                                             // Offset: 0x1b3 | Size: 0x1
    char OnInputTouchLeave;                                             // Offset: 0x1b4 | Size: 0x1
    char OnActorHit;                                                    // Offset: 0x1b5 | Size: 0x1
    char OnDestroyed;                                                   // Offset: 0x1b6 | Size: 0x1
    char OnEndPlay;                                                     // Offset: 0x1b7 | Size: 0x1
    char pad_0x1B8[0x68];                                               // Offset: 0x1b8 | Size: 0x68
    char OwnedComponentMap[0x50];                                       // Offset: 0x220 | Size: 0x50
    char pad_0x270[0x60];                                               // Offset: 0x270 | Size: 0x60
    struct TArray<struct UActorComponent *> InstanceComponents;         // Offset: 0x2d0 | Size: 0x10
    struct TArray<struct UActorComponent *> BlueprintCreatedComponents; // Offset: 0x2e0 | Size: 0x10
    char pad_0x2F0[0x10];                                               // Offset: 0x2f0 | Size: 0x10
};

struct APawn : AActor {
    // Fields
    char pad_0x300[0x8];                     // Offset: 0x300 | Size: 0x8
    char bUseControllerRotationPitch : 1;    // Offset: 0x308 | Size: 0x1
    char bUseControllerRotationYaw : 1;      // Offset: 0x308 | Size: 0x1
    char bUseControllerRotationRoll : 1;     // Offset: 0x308 | Size: 0x1
    char bCanAffectNavigationGeneration : 1; // Offset: 0x308 | Size: 0x1
    char pad_0x308_4 : 4;                    // Offset: 0x308 | Size: 0x1
    char pad_0x309[0x3];                     // Offset: 0x309 | Size: 0x3
    float BaseEyeHeight;                     // Offset: 0x30c | Size: 0x4
    char AutoPossessPlayer;                  // Offset: 0x310 | Size: 0x1
    char AutoPossessAI;                      // Offset: 0x311 | Size: 0x1
    char RemoteViewPitch;                    // Offset: 0x312 | Size: 0x1
    char pad_0x313[0x5];                     // Offset: 0x313 | Size: 0x5
    struct AController *AIControllerClass;   // Offset: 0x318 | Size: 0x8
    char OnReceivePhysicsEvent[0x10];        // Offset: 0x320 | Size: 0x10
    struct APlayerState *PlayerState;        // Offset: 0x330 | Size: 0x8
    char pad_0x338[0x8];                     // Offset: 0x338 | Size: 0x8
    struct AController *LastHitBy;           // Offset: 0x340 | Size: 0x8
    struct AController *Controller;          // Offset: 0x348 | Size: 0x8
    char pad_0x350[0x4];                     // Offset: 0x350 | Size: 0x4
    struct FVector ControlInputVector;       // Offset: 0x354 | Size: 0xc
    struct FVector LastControlInputVector;   // Offset: 0x360 | Size: 0xc
    char pad_0x36C[0x4];                     // Offset: 0x36c | Size: 0x4
};

struct AController : AActor {
    // Fields
    char pad_0x300[0x8];                        // Offset: 0x300 | Size: 0x8
    struct APlayerState *PlayerState;           // Offset: 0x308 | Size: 0x8
    char pad_0x310[0x8];                        // Offset: 0x310 | Size: 0x8
    char OnInstigatedAnyDamage[0x10];           // Offset: 0x318 | Size: 0x10
    struct FName StateName;                     // Offset: 0x328 | Size: 0x8
    struct APawn *Pawn;                         // Offset: 0x330 | Size: 0x8
    char pad_0x338[0x8];                        // Offset: 0x338 | Size: 0x8
    struct ACharacter *Character;               // Offset: 0x340 | Size: 0x8
    struct USceneComponent *TransformComponent; // Offset: 0x348 | Size: 0x8
    char pad_0x350[0x18];                       // Offset: 0x350 | Size: 0x18
    struct FRotator ControlRotation;            // Offset: 0x368 | Size: 0xc
    char bAttachToPawn : 1;                     // Offset: 0x374 | Size: 0x1
    char pad_0x374_1 : 7;                       // Offset: 0x374 | Size: 0x1
    char pad_0x375[0x3];                        // Offset: 0x375 | Size: 0x3
};

struct APlayerController : AController {
    // Fields
    struct UPlayer *Player;                                                      // Offset: 0x378 | Size: 0x8
    struct APawn *AcknowledgedPawn;                                              // Offset: 0x380 | Size: 0x8
    struct UInterpTrackInstDirector *ControllingDirTrackInst;                    // Offset: 0x388 | Size: 0x8
    struct AHUD *MyHUD;                                                          // Offset: 0x390 | Size: 0x8
    struct APlayerCameraManager *PlayerCameraManager;                            // Offset: 0x398 | Size: 0x8
    struct APlayerCameraManager *PlayerCameraManagerClass;                       // Offset: 0x3a0 | Size: 0x8
    bool bAutoManageActiveCameraTarget;                                          // Offset: 0x3a8 | Size: 0x1
    char pad_0x3A9[0x3];                                                         // Offset: 0x3a9 | Size: 0x3
    struct FRotator TargetViewRotation;                                          // Offset: 0x3ac | Size: 0xc
    char pad_0x3B8[0xc];                                                         // Offset: 0x3b8 | Size: 0xc
    float SmoothTargetViewRotationSpeed;                                         // Offset: 0x3c4 | Size: 0x4
    char pad_0x3C8[0x8];                                                         // Offset: 0x3c8 | Size: 0x8
    struct TArray<struct AActor *> HiddenActors;                                 // Offset: 0x3d0 | Size: 0x10
    struct TArray<struct AActor *> PotentialAntiPeekActors;                      // Offset: 0x3e0 | Size: 0x10
    char HiddenPrimitiveComponents[0x10];                                        // Offset: 0x3f0 | Size: 0x10
    char pad_0x400[0x4];                                                         // Offset: 0x400 | Size: 0x4
    float LastSpectatorStateSynchTime;                                           // Offset: 0x404 | Size: 0x4
    struct FVector LastSpectatorSyncLocation;                                    // Offset: 0x408 | Size: 0xc
    struct FRotator LastSpectatorSyncRotation;                                   // Offset: 0x414 | Size: 0xc
    int32_t ClientCap;                                                           // Offset: 0x420 | Size: 0x4
    char pad_0x424[0x4];                                                         // Offset: 0x424 | Size: 0x4
    struct UCheatManager *CheatManager;                                          // Offset: 0x428 | Size: 0x8
    struct UCheatManager *CheatClass;                                            // Offset: 0x430 | Size: 0x8
    struct UPlayerInput *PlayerInput;                                            // Offset: 0x438 | Size: 0x8
    struct TArray<struct FActiveForceFeedbackEffect> ActiveForceFeedbackEffects; // Offset: 0x440 | Size: 0x10
    bool bSlateForceFeedbackEnable;                                              // Offset: 0x450 | Size: 0x1
    bool bProcessDynamicFeedbackEnable;                                          // Offset: 0x451 | Size: 0x1
    char pad_0x452[0x76];                                                        // Offset: 0x452 | Size: 0x76
    char pad_0x4C8_0 : 4;                                                        // Offset: 0x4c8 | Size: 0x1
    char bPlayerIsWaiting : 1;                                                   // Offset: 0x4c8 | Size: 0x1
    char pad_0x4C8_5 : 3;                                                        // Offset: 0x4c8 | Size: 0x1
    char NetPlayerIndex;                                                         // Offset: 0x4c9 | Size: 0x1
    char pad_0x4CA[0x3e];                                                        // Offset: 0x4ca | Size: 0x3e
    struct UNetConnection *PendingSwapConnection;                                // Offset: 0x508 | Size: 0x8
    struct UNetConnection *NetConnection;                                        // Offset: 0x510 | Size: 0x8
    char pad_0x518[0xc];                                                         // Offset: 0x518 | Size: 0xc
    float InputYawScale;                                                         // Offset: 0x524 | Size: 0x4
    float InputPitchScale;                                                       // Offset: 0x528 | Size: 0x4
    float InputRollScale;                                                        // Offset: 0x52c | Size: 0x4
    char bShowMouseCursor : 1;                                                   // Offset: 0x530 | Size: 0x1
    char bEnableClickEvents : 1;                                                 // Offset: 0x530 | Size: 0x1
    char bEnableTouchEvents : 1;                                                 // Offset: 0x530 | Size: 0x1
    char bEnableMouseOverEvents : 1;                                             // Offset: 0x530 | Size: 0x1
    char bEnableTouchOverEvents : 1;                                             // Offset: 0x530 | Size: 0x1
    char bForceFeedbackEnabled : 1;                                              // Offset: 0x530 | Size: 0x1
    char pad_0x530_6 : 2;                                                        // Offset: 0x530 | Size: 0x1
    char pad_0x531[0x3];                                                         // Offset: 0x531 | Size: 0x3
    float ForceFeedbackScale;                                                    // Offset: 0x534 | Size: 0x4
    struct TArray<struct FKey> ClickEventKeys;                                   // Offset: 0x538 | Size: 0x10
    char DefaultMouseCursor;                                                     // Offset: 0x548 | Size: 0x1
    char CurrentMouseCursor;                                                     // Offset: 0x549 | Size: 0x1
    char DefaultClickTraceChannel;                                               // Offset: 0x54a | Size: 0x1
    char CurrentClickTraceChannel;                                               // Offset: 0x54b | Size: 0x1
    float HitResultTraceDistance;                                                // Offset: 0x54c | Size: 0x4
    uint16_t SeamlessTravelCount;                                                // Offset: 0x550 | Size: 0x2
    uint16_t LastCompletedSeamlessTravelCount;                                   // Offset: 0x552 | Size: 0x2
    char pad_0x554[0x74];                                                        // Offset: 0x554 | Size: 0x74
    struct UInputComponent *InactiveStateInputComponent;                         // Offset: 0x5c8 | Size: 0x8
    char pad_0x5D0_0 : 2;                                                        // Offset: 0x5d0 | Size: 0x1
    char bShouldPerformFullTickWhenPaused : 1;                                   // Offset: 0x5d0 | Size: 0x1
    char pad_0x5D0_3 : 5;                                                        // Offset: 0x5d0 | Size: 0x1
    char pad_0x5D1[0x17];                                                        // Offset: 0x5d1 | Size: 0x17
    struct UTouchInterface *CurrentTouchInterface;                               // Offset: 0x5e8 | Size: 0x8
    char pad_0x5F0[0x50];                                                        // Offset: 0x5f0 | Size: 0x50
    struct ASpectatorPawn *SpectatorPawn;                                        // Offset: 0x640 | Size: 0x8
    char pad_0x648[0x4];                                                         // Offset: 0x648 | Size: 0x4
    bool bIsLocalPlayerController;                                               // Offset: 0x64c | Size: 0x1
    char pad_0x64D[0x3];                                                         // Offset: 0x64d | Size: 0x3
    struct FVector SpawnLocation;                                                // Offset: 0x650 | Size: 0xc
    char pad_0x65C[0x14];                                                        // Offset: 0x65c | Size: 0x14
};

struct AInfo : AActor {
};

struct APlayerState : AInfo {
    // Fields
    float Score;                              // Offset: 0x300 | Size: 0x4
    int32_t PlayerId;                         // Offset: 0x304 | Size: 0x4
    char Ping;                                // Offset: 0x308 | Size: 0x1
    char pad_0x309[0x1];                      // Offset: 0x309 | Size: 0x1
    char bShouldUpdateReplicatedPing : 1;     // Offset: 0x30a | Size: 0x1
    char bIsSpectator : 1;                    // Offset: 0x30a | Size: 0x1
    char bOnlySpectator : 1;                  // Offset: 0x30a | Size: 0x1
    char bIsABot : 1;                         // Offset: 0x30a | Size: 0x1
    char pad_0x30A_4 : 1;                     // Offset: 0x30a | Size: 0x1
    char bIsInactive : 1;                     // Offset: 0x30a | Size: 0x1
    char bFromPreviousLevel : 1;              // Offset: 0x30a | Size: 0x1
    char pad_0x30A_7 : 1;                     // Offset: 0x30a | Size: 0x1
    char pad_0x30B[0x1];                      // Offset: 0x30b | Size: 0x1
    int32_t StartTime;                        // Offset: 0x30c | Size: 0x4
    struct ULocalMessage *EngineMessageClass; // Offset: 0x310 | Size: 0x8
    float ExactPing;                          // Offset: 0x318 | Size: 0x4
    float ExactPingV2;                        // Offset: 0x31c | Size: 0x4
    struct FString SavedNetworkAddress;       // Offset: 0x320 | Size: 0x10
    char UniqueID[0x28];                      // Offset: 0x330 | Size: 0x28
    char pad_0x358[0x8];                      // Offset: 0x358 | Size: 0x8
    struct APawn *PawnPrivate;                // Offset: 0x360 | Size: 0x8
    char pad_0x368[0xd8];                     // Offset: 0x368 | Size: 0xd8
    struct FString PlayerNamePrivate;         // Offset: 0x440 | Size: 0x10
    char pad_0x450[0x10];                     // Offset: 0x450 | Size: 0x10
};

struct AGameStateBase : AInfo {
    // Fields
    struct AGameModeBase *GameModeClass;              // Offset: 0x300 | Size: 0x8
    struct AGameModeBase *AuthorityGameMode;          // Offset: 0x308 | Size: 0x8
    struct ASpectatorPawn *SpectatorClass;            // Offset: 0x310 | Size: 0x8
    struct TArray<struct APlayerState *> PlayerArray; // Offset: 0x318 | Size: 0x10
    bool bReplicatedHasBegunPlay;                     // Offset: 0x328 | Size: 0x1
    char pad_0x329[0x3];                              // Offset: 0x329 | Size: 0x3
    float ReplicatedWorldTimeSeconds;                 // Offset: 0x32c | Size: 0x4
    float ServerWorldTimeSecondsDelta;                // Offset: 0x330 | Size: 0x4
    float ServerWorldTimeSecondsUpdateFrequency;      // Offset: 0x334 | Size: 0x4
    char pad_0x338[0x18];                             // Offset: 0x338 | Size: 0x18
};

struct UActorComponent : UObject {
    // Fields
    char pad_0x28[0x8];                                                 // Offset: 0x28 | Size: 0x8
    char PrimaryComponentTick[0x40];                                    // Offset: 0x30 | Size: 0x40
    struct TArray<struct FName> ComponentTags;                          // Offset: 0x70 | Size: 0x10
    struct TArray<struct UAssetUserData *> AssetUserData;               // Offset: 0x80 | Size: 0x10
    char pad_0x90[0x4];                                                 // Offset: 0x90 | Size: 0x4
    int32_t UCSSerializationIndex;                                      // Offset: 0x94 | Size: 0x4
    char pad_0x98_0 : 3;                                                // Offset: 0x98 | Size: 0x1
    char bNetAddressable : 1;                                           // Offset: 0x98 | Size: 0x1
    char bReplicates : 1;                                               // Offset: 0x98 | Size: 0x1
    char pad_0x98_5 : 3;                                                // Offset: 0x98 | Size: 0x1
    char pad_0x99_0 : 7;                                                // Offset: 0x99 | Size: 0x1
    char bAutoActivate : 1;                                             // Offset: 0x99 | Size: 0x1
    char bIsActive : 1;                                                 // Offset: 0x9a | Size: 0x1
    char bEditableWhenInherited : 1;                                    // Offset: 0x9a | Size: 0x1
    char pad_0x9A_2 : 1;                                                // Offset: 0x9a | Size: 0x1
    char bCanEverAffectNavigation : 1;                                  // Offset: 0x9a | Size: 0x1
    char pad_0x9A_4 : 1;                                                // Offset: 0x9a | Size: 0x1
    char bIsEditorOnly : 1;                                             // Offset: 0x9a | Size: 0x1
    char pad_0x9A_6 : 2;                                                // Offset: 0x9a | Size: 0x1
    char pad_0x9B[0x1];                                                 // Offset: 0x9b | Size: 0x1
    char CreationMethod;                                                // Offset: 0x9c | Size: 0x1
    char OnComponentActivated;                                          // Offset: 0x9d | Size: 0x1
    char OnComponentDeactivated;                                        // Offset: 0x9e | Size: 0x1
    char pad_0x9F[0x1];                                                 // Offset: 0x9f | Size: 0x1
    struct TArray<struct FSimpleMemberReference> UCSModifiedProperties; // Offset: 0xa0 | Size: 0x10
    char pad_0xB0[0x10];                                                // Offset: 0xb0 | Size: 0x10
    char bManualReplicates : 1;                                         // Offset: 0xc0 | Size: 0x1
    char pad_0xC0_1 : 7;                                                // Offset: 0xc0 | Size: 0x1
    char pad_0xC1[0x1b];                                                // Offset: 0xc1 | Size: 0x1b
    float MinTickInterval;                                              // Offset: 0xdc | Size: 0x4
};

struct USceneComponent : UActorComponent {
    // Fields
    char pad_0xE0[0x8];                                             // Offset: 0xe0 | Size: 0x8
    char PhysicsVolume[0x8];                                        // Offset: 0xe8 | Size: 0x8
    struct USceneComponent *AttachParent;                           // Offset: 0xf0 | Size: 0x8
    struct FName AttachSocketName;                                  // Offset: 0xf8 | Size: 0x8
    struct TArray<struct USceneComponent *> AttachChildren;         // Offset: 0x100 | Size: 0x10
    struct TArray<struct USceneComponent *> ClientAttachedChildren; // Offset: 0x110 | Size: 0x10
    char pad_0x120[0x34];                                           // Offset: 0x120 | Size: 0x34
    struct FVector RelativeLocation;                                // Offset: 0x154 | Size: 0xc
    struct FRotator RelativeRotation;                               // Offset: 0x160 | Size: 0xc
    struct FVector RelativeScale3D;                                 // Offset: 0x16c | Size: 0xc
    struct FVector ComponentVelocity;                               // Offset: 0x178 | Size: 0xc
    char bEnableMoveOverlapOpt : 1;                                 // Offset: 0x184 | Size: 0x1
    char bComponentToWorldUpdated : 1;                              // Offset: 0x184 | Size: 0x1
    char pad_0x184_2 : 1;                                           // Offset: 0x184 | Size: 0x1
    char bAbsoluteLocation : 1;                                     // Offset: 0x184 | Size: 0x1
    char bAbsoluteRotation : 1;                                     // Offset: 0x184 | Size: 0x1
    char bAbsoluteScale : 1;                                        // Offset: 0x184 | Size: 0x1
    char bVisible : 1;                                              // Offset: 0x184 | Size: 0x1
    char bShouldBeAttached : 1;                                     // Offset: 0x184 | Size: 0x1
    char bShouldSnapLocationWhenAttached : 1;                       // Offset: 0x185 | Size: 0x1
    char bShouldSnapRotationWhenAttached : 1;                       // Offset: 0x185 | Size: 0x1
    char bShouldUpdatePhysicsVolume : 1;                            // Offset: 0x185 | Size: 0x1
    char bHiddenInGame : 1;                                         // Offset: 0x185 | Size: 0x1
    char bBoundsChangeTriggersStreamingDataRebuild : 1;             // Offset: 0x185 | Size: 0x1
    char bUseAttachParentBound : 1;                                 // Offset: 0x185 | Size: 0x1
    char pad_0x185_6 : 2;                                           // Offset: 0x185 | Size: 0x1
    char pad_0x186[0x1];                                            // Offset: 0x186 | Size: 0x1
    char Mobility;                                                  // Offset: 0x187 | Size: 0x1
    char DetailMode;                                                // Offset: 0x188 | Size: 0x1
    char PhysicsVolumeChangedDelegate;                              // Offset: 0x189 | Size: 0x1
    char pad_0x18A[0x86];                                           // Offset: 0x18a | Size: 0x86
};

struct FMinimalViewInfo {
    // Fields
    struct FVector Location;                    // Offset: 0x0 | Size: 0xc
    struct FRotator Rotation;                   // Offset: 0xc | Size: 0xc
    float FOV;                                  // Offset: 0x18 | Size: 0x4
    float DesiredFOV;                           // Offset: 0x1c | Size: 0x4
    float OrthoWidth;                           // Offset: 0x20 | Size: 0x4
    float OrthoNearClipPlane;                   // Offset: 0x24 | Size: 0x4
    float OrthoFarClipPlane;                    // Offset: 0x28 | Size: 0x4
    float AspectRatio;                          // Offset: 0x2c | Size: 0x4
    char bConstrainAspectRatio : 1;             // Offset: 0x30 | Size: 0x1
    char bUseFieldOfViewForLOD : 1;             // Offset: 0x30 | Size: 0x1
    char pad_0x30_2 : 6;                        // Offset: 0x30 | Size: 0x1
    char ProjectionMode;                        // Offset: 0x31 | Size: 0x1
    char pad_0x32[0x2];                         // Offset: 0x32 | Size: 0x2
    float PostProcessBlendWeight;               // Offset: 0x34 | Size: 0x4
    char pad_0x38[0x8];                         // Offset: 0x38 | Size: 0x8
    char PostProcessSettings[0x610];            // Offset: 0x40 | Size: 0x610
    struct FVector2D OffCenterProjectionOffset; // Offset: 0x650 | Size: 0x8
    char FOVMode;                               // Offset: 0x658 | Size: 0x1
    char pad_0x659[0x47];                       // Offset: 0x659 | Size: 0x47
};

struct FCameraCacheEntry {
    // Fields
    float Timestamp;             // Offset: 0x0 | Size: 0x4
    char pad_0x4[0xc];           // Offset: 0x4 | Size: 0xc
    struct FMinimalViewInfo POV; // Offset: 0x10 | Size: 0x6a0
};

struct APlayerCameraManager : AActor {
    // Fields
    struct APlayerController *PCOwner;                                      // Offset: 0x300 | Size: 0x8
    struct USceneComponent *TransformComponent;                             // Offset: 0x308 | Size: 0x8
    char pad_0x310[0x8];                                                    // Offset: 0x310 | Size: 0x8
    float DefaultFOV;                                                       // Offset: 0x318 | Size: 0x4
    char pad_0x31C[0x4];                                                    // Offset: 0x31c | Size: 0x4
    float DefaultOrthoWidth;                                                // Offset: 0x320 | Size: 0x4
    char pad_0x324[0x4];                                                    // Offset: 0x324 | Size: 0x4
    float DefaultAspectRatio;                                               // Offset: 0x328 | Size: 0x4
    char pad_0x32C[0x44];                                                   // Offset: 0x32c | Size: 0x44
    struct FCameraCacheEntry CameraCache;                                   // Offset: 0x370 | Size: 0x6b0
    struct FCameraCacheEntry LastFrameCameraCache;                          // Offset: 0xa20 | Size: 0x6b0
    char ViewTarget[0x6c0];                                                 // Offset: 0x10d0 | Size: 0x6c0
    char PendingViewTarget[0x6c0];                                          // Offset: 0x1790 | Size: 0x6c0
    char pad_0x1E50[0x30];                                                  // Offset: 0x1e50 | Size: 0x30
    struct FCameraCacheEntry CameraCachePrivate;                            // Offset: 0x1e80 | Size: 0x6b0
    struct FCameraCacheEntry LastFrameCameraCachePrivate;                   // Offset: 0x2530 | Size: 0x6b0
    struct TArray<struct UCameraModifier *> ModifierList;                   // Offset: 0x2be0 | Size: 0x10
    struct TArray<struct UCameraModifier *> DefaultModifiers;               // Offset: 0x2bf0 | Size: 0x10
    float FreeCamDistance;                                                  // Offset: 0x2c00 | Size: 0x4
    struct FVector FreeCamOffset;                                           // Offset: 0x2c04 | Size: 0xc
    struct FVector ViewTargetOffset;                                        // Offset: 0x2c10 | Size: 0xc
    char pad_0x2C1C[0x4];                                                   // Offset: 0x2c1c | Size: 0x4
    char OnAudioFadeChangeEvent[0x10];                                      // Offset: 0x2c20 | Size: 0x10
    char pad_0x2C30[0x10];                                                  // Offset: 0x2c30 | Size: 0x10
    struct TArray<struct AEmitterCameraLensEffectBase *> CameraLensEffects; // Offset: 0x2c40 | Size: 0x10
    struct UCameraModifier_CameraShake *CachedCameraShakeMod;               // Offset: 0x2c50 | Size: 0x8
    struct UCameraAnimInst *AnimInstPool[0x8];                              // Offset: 0x2c58 | Size: 0x40
    struct TArray<struct FPostProcessSettings> PostProcessBlendCache;       // Offset: 0x2c98 | Size: 0x10
    char pad_0x2CA8[0x10];                                                  // Offset: 0x2ca8 | Size: 0x10
    struct TArray<struct UCameraAnimInst *> ActiveAnims;                    // Offset: 0x2cb8 | Size: 0x10
    struct TArray<struct UCameraAnimInst *> FreeAnims;                      // Offset: 0x2cc8 | Size: 0x10
    struct ACameraActor *AnimCameraActor;                                   // Offset: 0x2cd8 | Size: 0x8
    char bIsOrthographic : 1;                                               // Offset: 0x2ce0 | Size: 0x1
    char bDefaultConstrainAspectRatio : 1;                                  // Offset: 0x2ce0 | Size: 0x1
    char pad_0x2CE0_2 : 4;                                                  // Offset: 0x2ce0 | Size: 0x1
    char bClientSimulatingViewTarget : 1;                                   // Offset: 0x2ce0 | Size: 0x1
    char bUseClientSideCameraUpdates : 1;                                   // Offset: 0x2ce0 | Size: 0x1
    char pad_0x2CE1_0 : 2;                                                  // Offset: 0x2ce1 | Size: 0x1
    char bGameCameraCutThisFrame : 1;                                       // Offset: 0x2ce1 | Size: 0x1
    char pad_0x2CE1_3 : 5;                                                  // Offset: 0x2ce1 | Size: 0x1
    char pad_0x2CE2[0x2];                                                   // Offset: 0x2ce2 | Size: 0x2
    float ViewPitchMin;                                                     // Offset: 0x2ce4 | Size: 0x4
    float ViewPitchMax;                                                     // Offset: 0x2ce8 | Size: 0x4
    float ViewYawMin;                                                       // Offset: 0x2cec | Size: 0x4
    float ViewYawMax;                                                       // Offset: 0x2cf0 | Size: 0x4
    float ViewRollMin;                                                      // Offset: 0x2cf4 | Size: 0x4
    float ViewRollMax;                                                      // Offset: 0x2cf8 | Size: 0x4
    char pad_0x2CFC[0x4];                                                   // Offset: 0x2cfc | Size: 0x4
    float ServerUpdateCameraTimeout;                                        // Offset: 0x2d00 | Size: 0x4
    char pad_0x2D04[0xc];                                                   // Offset: 0x2d04 | Size: 0xc
};

struct ASGPlayerState : APlayerState {
    // Fields
    char pad_0x460[0x18];                                               // Offset: 0x460 | Size: 0x18
    struct TArray<struct ASGCharacter *> HitTeammates;                  // Offset: 0x478 | Size: 0x10
    char HitTeammatesController[0x50];                                  // Offset: 0x488 | Size: 0x50
    char pad_0x4D8[0x10];                                               // Offset: 0x4d8 | Size: 0x10
    struct USGPlayerStateTeamComponent *SGPlayerStateTeamComponent;     // Offset: 0x4e8 | Size: 0x8
    struct USGPlayerStateStatisComponent *SGPlayerStateStatisComponent; // Offset: 0x4f0 | Size: 0x8
    int32_t PlayerPlatformId;                                           // Offset: 0x4f8 | Size: 0x4
    bool bReceivePlatformId;                                            // Offset: 0x4fc | Size: 0x1
    bool bApplyZoneSvrInfo;                                             // Offset: 0x4fd | Size: 0x1
    bool bHasInitNewPlayer;                                             // Offset: 0x4fe | Size: 0x1
    bool bIsTired;                                                      // Offset: 0x4ff | Size: 0x1
    char CacheLootLimitUserTagTableRow[0x40];                           // Offset: 0x500 | Size: 0x40
    int32_t DelayRoughSearchTime;                                       // Offset: 0x540 | Size: 0x4
    char pad_0x544[0x4];                                                // Offset: 0x544 | Size: 0x4
    uint64_t GID;                                                       // Offset: 0x548 | Size: 0x8
    uint32_t GOpenID;                                                   // Offset: 0x550 | Size: 0x4
    char pad_0x554[0x4];                                                // Offset: 0x554 | Size: 0x4
    struct ASGCharacter *SGCharacter;                                   // Offset: 0x558 | Size: 0x8
    bool OnReconnect;                                                   // Offset: 0x560 | Size: 0x1
    char pad_0x561[0x7];                                                // Offset: 0x561 | Size: 0x7
    uint64_t RoomID;                                                    // Offset: 0x568 | Size: 0x8
    int32_t TeamIndex;                                                  // Offset: 0x570 | Size: 0x4
    char pad_0x574[0x4];                                                // Offset: 0x574 | Size: 0x4
    struct FString TeamName;                                            // Offset: 0x578 | Size: 0x10
    int32_t SquadIndex;                                                 // Offset: 0x588 | Size: 0x4
    int32_t GetOutOfStuckChance;                                        // Offset: 0x58c | Size: 0x4
    int32_t GetVehicleOutOfStuckChance;                                 // Offset: 0x590 | Size: 0x4
    int32_t SeasonID;                                                   // Offset: 0x594 | Size: 0x4
    bool bInitFromZone;                                                 // Offset: 0x598 | Size: 0x1
    char pad_0x599[0x3];                                                // Offset: 0x599 | Size: 0x3
    int32_t TeamType;                                                   // Offset: 0x59c | Size: 0x4
    int32_t game_mode;                                                  // Offset: 0x5a0 | Size: 0x4
    int32_t channelType;                                                // Offset: 0x5a4 | Size: 0x4
    uint32_t ranked_Level;                                              // Offset: 0x5a8 | Size: 0x4
    uint32_t ranked_Score;                                              // Offset: 0x5ac | Size: 0x4
    uint32_t ranking;                                                   // Offset: 0x5b0 | Size: 0x4
    uint32_t pranking;                                                  // Offset: 0x5b4 | Size: 0x4
    uint32_t ranked_rd;                                                 // Offset: 0x5b8 | Size: 0x4
    uint32_t Rank;                                                      // Offset: 0x5bc | Size: 0x4
    uint32_t CurSeason_ID;                                              // Offset: 0x5c0 | Size: 0x4
    uint32_t Server_Season_ID;                                          // Offset: 0x5c4 | Size: 0x4
    uint32_t PlatPrivilegeType;                                         // Offset: 0x5c8 | Size: 0x4
    uint32_t ranked_status;                                             // Offset: 0x5cc | Size: 0x4
    uint32_t Area_ID;                                                   // Offset: 0x5d0 | Size: 0x4
    uint32_t PMC_KillCountByPMC;                                        // Offset: 0x5d4 | Size: 0x4
    uint32_t SCAV_KillCountByPMC;                                       // Offset: 0x5d8 | Size: 0x4
    uint32_t DeathCountByPMC;                                           // Offset: 0x5dc | Size: 0x4
    int32_t PlatId;                                                     // Offset: 0x5e0 | Size: 0x4
    int32_t GameModeId;                                                 // Offset: 0x5e4 | Size: 0x4
    uint32_t bIsSingle;                                                 // Offset: 0x5e8 | Size: 0x4
    uint32_t AutoMatch;                                                 // Offset: 0x5ec | Size: 0x4
    uint32_t PlayerLevel;                                               // Offset: 0x5f0 | Size: 0x4
    char pad_0x5F4[0x4];                                                // Offset: 0x5f4 | Size: 0x4
    struct FString ZonePlayerName;                                      // Offset: 0x5f8 | Size: 0x10
    char CharacterSex;                                                  // Offset: 0x608 | Size: 0x1
    char pad_0x609[0x7];                                                // Offset: 0x609 | Size: 0x7
    char PlayerIconInfo[0x50];                                          // Offset: 0x610 | Size: 0x50
    uint32_t RewardBoxSkinId;                                           // Offset: 0x660 | Size: 0x4
    uint32_t Mentor_Identity;                                           // Offset: 0x664 | Size: 0x4
    uint32_t Mentor_Gid;                                                // Offset: 0x668 | Size: 0x4
    char pad_0x66C[0x4];                                                // Offset: 0x66c | Size: 0x4
    struct TArray<int32_t> Student_gid_List;                            // Offset: 0x670 | Size: 0x10
    struct FString Mentor_Name;                                         // Offset: 0x680 | Size: 0x10
    struct TArray<struct FString> Student_Name_List;                    // Offset: 0x690 | Size: 0x10
    int32_t LootLimitPoolID;                                            // Offset: 0x6a0 | Size: 0x4
    float EnterGameLeftTime;                                            // Offset: 0x6a4 | Size: 0x4
    float EnterGameTime;                                                // Offset: 0x6a8 | Size: 0x4
    int32_t NationalFlagIso;                                            // Offset: 0x6ac | Size: 0x4
    int32_t EnterDsZone;                                                // Offset: 0x6b0 | Size: 0x4
    int32_t IdealDsZone;                                                // Offset: 0x6b4 | Size: 0x4
    uint32_t mapunlockId;                                               // Offset: 0x6b8 | Size: 0x4
    char pad_0x6BC[0x4];                                                // Offset: 0x6bc | Size: 0x4
    struct TArray<int32_t> TakeOutInspectionLootPointIDArray;           // Offset: 0x6c0 | Size: 0x10
    struct TArray<int32_t> TakeInInspectionLootPointIDArray;            // Offset: 0x6d0 | Size: 0x10
    char GuranteedPool[0x50];                                           // Offset: 0x6e0 | Size: 0x50
    struct TArray<struct FString> ClientXIDArray;                       // Offset: 0x730 | Size: 0x10
    char CharacterLootExtDataMap[0x50];                                 // Offset: 0x740 | Size: 0x50
    struct TArray<int32_t> PlayerTagIds;                                // Offset: 0x790 | Size: 0x10
    uint64_t BanGrenadesTimestamp;                                      // Offset: 0x7a0 | Size: 0x8
    uint64_t BanGrenadesTimestampIDIP;                                  // Offset: 0x7a8 | Size: 0x8
    bool bShouldBanGrennade;                                            // Offset: 0x7b0 | Size: 0x1
    bool TempBanGrenade;                                                // Offset: 0x7b1 | Size: 0x1
    bool bCanBeSpectated;                                               // Offset: 0x7b2 | Size: 0x1
    char pad_0x7B3[0x1];                                                // Offset: 0x7b3 | Size: 0x1
    uint32_t BanTimeUseVehicle;                                         // Offset: 0x7b4 | Size: 0x4
    bool bNeedToRecordShootInputFlow;                                   // Offset: 0x7b8 | Size: 0x1
    char pad_0x7B9[0x7];                                                // Offset: 0x7b9 | Size: 0x7
    char IdentityType;                                                  // Offset: 0x7c0 | Size: 0x1
    char pad_0x7C1[0x7];                                                // Offset: 0x7c1 | Size: 0x7
    struct TArray<struct UActorComponent *> ComponentClasses;           // Offset: 0x7c8 | Size: 0x10
    struct TArray<struct UActorComponent *> ComponentClassesAdditional; // Offset: 0x7d8 | Size: 0x10
    struct ASGPlayerState *TemplateClass;                               // Offset: 0x7e8 | Size: 0x8
    bool bIsReconnectSpawn;                                             // Offset: 0x7f0 | Size: 0x1
    bool bCancelReconnection;                                           // Offset: 0x7f1 | Size: 0x1
    char pad_0x7F2[0xa];                                                // Offset: 0x7f2 | Size: 0xa
    int32_t PlayerTakeIn_BuyMoney;                                      // Offset: 0x7fc | Size: 0x4
    int32_t AccumulativeSpendMoney;                                     // Offset: 0x800 | Size: 0x4
    char pad_0x804[0xc];                                                // Offset: 0x804 | Size: 0xc
    struct FString KillByEnemy;                                         // Offset: 0x810 | Size: 0x10
    int32_t TotalDamageToEnemy;                                         // Offset: 0x820 | Size: 0x4
    int32_t DamageToEnemyHead;                                          // Offset: 0x824 | Size: 0x4
    int32_t DamageToEnemyChest;                                         // Offset: 0x828 | Size: 0x4
    int32_t DamageToEnemyStomach;                                       // Offset: 0x82c | Size: 0x4
    int32_t DamageToEnemyLeftArm;                                       // Offset: 0x830 | Size: 0x4
    int32_t DamageToEnemyRightArm;                                      // Offset: 0x834 | Size: 0x4
    int32_t DamageToEnemyLeg;                                           // Offset: 0x838 | Size: 0x4
    int32_t TotalHitTimes;                                              // Offset: 0x83c | Size: 0x4
    int32_t HitTimesToEnemyHead;                                        // Offset: 0x840 | Size: 0x4
    int32_t HitTimesToEnemyChest;                                       // Offset: 0x844 | Size: 0x4
    int32_t HitTimesToEnemyStomach;                                     // Offset: 0x848 | Size: 0x4
    int32_t HitTimesToEnemyLeftArm;                                     // Offset: 0x84c | Size: 0x4
    int32_t HitTimesToEnemyRightArm;                                    // Offset: 0x850 | Size: 0x4
    int32_t HitTimesToEnemyLeg;                                         // Offset: 0x854 | Size: 0x4
    int32_t TotalAmmoCost;                                              // Offset: 0x858 | Size: 0x4
    int32_t EnemyArmorDamageReduceAmount;                               // Offset: 0x85c | Size: 0x4
    char DeathType;                                                     // Offset: 0x860 | Size: 0x1
    char pad_0x861[0x3];                                                // Offset: 0x861 | Size: 0x3
    int32_t KillEnemyCount;                                             // Offset: 0x864 | Size: 0x4
    int32_t TotalGetDamage;                                             // Offset: 0x868 | Size: 0x4
    int32_t TotalGetDamageTimes;                                        // Offset: 0x86c | Size: 0x4
    int32_t HeadGetDamage;                                              // Offset: 0x870 | Size: 0x4
    int32_t ChestGetDamage;                                             // Offset: 0x874 | Size: 0x4
    int32_t StomachGetDamage;                                           // Offset: 0x878 | Size: 0x4
    int32_t LeftArmGetDamage;                                           // Offset: 0x87c | Size: 0x4
    int32_t RightArmGetDamage;                                          // Offset: 0x880 | Size: 0x4
    int32_t LegGetDamage;                                               // Offset: 0x884 | Size: 0x4
    int32_t MyselfArmorDamageReduceAmount;                              // Offset: 0x888 | Size: 0x4
    char pad_0x88C[0x4];                                                // Offset: 0x88c | Size: 0x4
    struct ASGCharacter *LastAimEnemy;                                  // Offset: 0x890 | Size: 0x8
    float LastAimEnemyTime;                                             // Offset: 0x898 | Size: 0x4
    char pad_0x89C[0x4];                                                // Offset: 0x89c | Size: 0x4
    struct TArray<int32_t> ChatVoiceIDs;                                // Offset: 0x8a0 | Size: 0x10
    struct TArray<int32_t> GestureIDs;                                  // Offset: 0x8b0 | Size: 0x10
    struct TArray<int32_t> PlayerVoiceIDs;                              // Offset: 0x8c0 | Size: 0x10
    struct TArray<int32_t> PlayerSprayIDs;                              // Offset: 0x8d0 | Size: 0x10
    struct FString EscapePointName;                                     // Offset: 0x8e0 | Size: 0x10
    int32_t KillPlayerPMC;                                              // Offset: 0x8f0 | Size: 0x4
    int32_t KillPlayerScav;                                             // Offset: 0x8f4 | Size: 0x4
    int32_t KillTeammate;                                               // Offset: 0x8f8 | Size: 0x4
    int32_t KillAIScav;                                                 // Offset: 0x8fc | Size: 0x4
    int32_t KillAIScavBoss;                                             // Offset: 0x900 | Size: 0x4
    int32_t KillAIPMC;                                                  // Offset: 0x904 | Size: 0x4
    int32_t KillTotalCount;                                             // Offset: 0x908 | Size: 0x4
    int32_t KillAIThemeBOSS;                                            // Offset: 0x90c | Size: 0x4
    bool IsOpenMic;                                                     // Offset: 0x910 | Size: 0x1
    char pad_0x911[0x3];                                                // Offset: 0x911 | Size: 0x3
    int32_t SignType;                                                   // Offset: 0x914 | Size: 0x4
    uint32_t NetOutLoss;                                                // Offset: 0x918 | Size: 0x4
    uint32_t NetInLoss;                                                 // Offset: 0x91c | Size: 0x4
    uint32_t NetInRate;                                                 // Offset: 0x920 | Size: 0x4
    uint32_t NetOutRate;                                                // Offset: 0x924 | Size: 0x4
    uint32_t NetSaturated;                                              // Offset: 0x928 | Size: 0x4
    uint32_t NetOutTotalPackets;                                        // Offset: 0x92c | Size: 0x4
    uint32_t NetTotalOutLoss;                                           // Offset: 0x930 | Size: 0x4
    uint32_t NetTotalInLoss;                                            // Offset: 0x934 | Size: 0x4
    int32_t TakeInValue;                                                // Offset: 0x938 | Size: 0x4
    int32_t TakeOutTotalValue;                                          // Offset: 0x93c | Size: 0x4
    int32_t TakeOutValue_Self;                                          // Offset: 0x940 | Size: 0x4
    int32_t TakeOutValue_Loot;                                          // Offset: 0x944 | Size: 0x4
    int32_t TakeOutValue_PMC;                                           // Offset: 0x948 | Size: 0x4
    int32_t TakeOutValue_SCAV;                                          // Offset: 0x94c | Size: 0x4
    int32_t TakeOutValue_AISCAV;                                        // Offset: 0x950 | Size: 0x4
    int32_t TakeOutValue_AIBOSS;                                        // Offset: 0x954 | Size: 0x4
    int32_t TakeOutValue_AIPMC;                                         // Offset: 0x958 | Size: 0x4
    int32_t TakeOutValue_Other;                                         // Offset: 0x95c | Size: 0x4
    int32_t TakeOutValue_AIFollower;                                    // Offset: 0x960 | Size: 0x4
    int32_t TakeOutValue_AIElit;                                        // Offset: 0x964 | Size: 0x4
    int32_t TakeOutValue_BOSS;                                          // Offset: 0x968 | Size: 0x4
    int32_t TakeOutValue_QuestEffectGive;                               // Offset: 0x96c | Size: 0x4
    int32_t TakeOutValue_GMCheat;                                       // Offset: 0x970 | Size: 0x4
    char pad_0x974[0x4];                                                // Offset: 0x974 | Size: 0x4
    struct TArray<uint64_t> LuckyStarInvitedGIDs;                       // Offset: 0x978 | Size: 0x10
};

struct ACharacter : APawn {
    // Fields
    struct USkeletalMeshComponent *Mesh;                                         // Offset: 0x370 | Size: 0x8
    struct UCharacterMovementComponent *CharacterMovement;                       // Offset: 0x378 | Size: 0x8
    struct UCapsuleComponent *CapsuleComponent;                                  // Offset: 0x380 | Size: 0x8
    char BasedMovement[0x38];                                                    // Offset: 0x388 | Size: 0x38
    char ReplicatedBasedMovement[0x38];                                          // Offset: 0x3c0 | Size: 0x38
    float AnimRootMotionTranslationScale;                                        // Offset: 0x3f8 | Size: 0x4
    struct FVector BaseTranslationOffset;                                        // Offset: 0x3fc | Size: 0xc
    char pad_0x408[0x8];                                                         // Offset: 0x408 | Size: 0x8
    char BaseRotationOffset[0x10];                                               // Offset: 0x410 | Size: 0x10
    float ReplicatedServerLastTransformUpdateTimeStamp;                          // Offset: 0x420 | Size: 0x4
    float ReplayLastTransformUpdateTimeStamp;                                    // Offset: 0x424 | Size: 0x4
    char ReplicatedMovementMode;                                                 // Offset: 0x428 | Size: 0x1
    bool bInBaseReplication;                                                     // Offset: 0x429 | Size: 0x1
    char pad_0x42A[0x2];                                                         // Offset: 0x42a | Size: 0x2
    float CrouchedEyeHeight;                                                     // Offset: 0x42c | Size: 0x4
    char bIsCrouched : 1;                                                        // Offset: 0x430 | Size: 0x1
    char bProxyIsJumpForceApplied : 1;                                           // Offset: 0x430 | Size: 0x1
    char bPressedJump : 1;                                                       // Offset: 0x430 | Size: 0x1
    char bClientUpdating : 1;                                                    // Offset: 0x430 | Size: 0x1
    char bClientWasFalling : 1;                                                  // Offset: 0x430 | Size: 0x1
    char bClientResimulateRootMotion : 1;                                        // Offset: 0x430 | Size: 0x1
    char bClientResimulateRootMotionSources : 1;                                 // Offset: 0x430 | Size: 0x1
    char bSimGravityDisabled : 1;                                                // Offset: 0x430 | Size: 0x1
    char bClientCheckEncroachmentOnNetUpdate : 1;                                // Offset: 0x431 | Size: 0x1
    char bServerMoveIgnoreRootMotion : 1;                                        // Offset: 0x431 | Size: 0x1
    char bWasJumping : 1;                                                        // Offset: 0x431 | Size: 0x1
    char pad_0x431_3 : 5;                                                        // Offset: 0x431 | Size: 0x1
    char pad_0x432[0x2];                                                         // Offset: 0x432 | Size: 0x2
    float JumpKeyHoldTime;                                                       // Offset: 0x434 | Size: 0x4
    float JumpForceTimeRemaining;                                                // Offset: 0x438 | Size: 0x4
    float ProxyJumpForceStartedTime;                                             // Offset: 0x43c | Size: 0x4
    float JumpMaxHoldTime;                                                       // Offset: 0x440 | Size: 0x4
    int32_t JumpMaxCount;                                                        // Offset: 0x444 | Size: 0x4
    int32_t JumpCurrentCount;                                                    // Offset: 0x448 | Size: 0x4
    int32_t JumpCurrentCountPreJump;                                             // Offset: 0x44c | Size: 0x4
    char pad_0x450[0x8];                                                         // Offset: 0x450 | Size: 0x8
    char OnReachedJumpApex[0x10];                                                // Offset: 0x458 | Size: 0x10
    char pad_0x468[0x10];                                                        // Offset: 0x468 | Size: 0x10
    char MovementModeChangedDelegate[0x10];                                      // Offset: 0x478 | Size: 0x10
    char OnCharacterMovementUpdated[0x10];                                       // Offset: 0x488 | Size: 0x10
    char SavedRootMotion[0x38];                                                  // Offset: 0x498 | Size: 0x38
    char ClientRootMotionParams[0x40];                                           // Offset: 0x4d0 | Size: 0x40
    struct TArray<struct FSimulatedRootMotionReplicatedMove> RootMotionRepMoves; // Offset: 0x510 | Size: 0x10
    char RepRootMotion[0x98];                                                    // Offset: 0x520 | Size: 0x98
    char pad_0x5B8[0x8];                                                         // Offset: 0x5b8 | Size: 0x8
};

enum class ECharacterType : uint8_t {
    ECharacterType_None = 0,
    ECharacterType_PMC = 1,
    ECharacterType_SCAV = 2,
    ECharacterType_AI_SCAV = 3,
    ECharacterType_AI_SCAV_BOSS = 4,
    ECharacterType_AI_PMC = 5,
    ECharacterType_AI_ELIT = 6,
    ECharacterType_BOSS = 7,
    ECharacterType_AI_SCAV_Follower = 8,
    ECharacterType_AI_Animal = 9,
    ECharacterType_MAX = 10
};

struct ASGCharacter : ACharacter {
    // Fields
    char pad_0x5C0[0x28];                                                             // Offset: 0x5c0 | Size: 0x28
    char JumpEvent[0x10];                                                             // Offset: 0x5e8 | Size: 0x10
    char LandedEvent[0x10];                                                           // Offset: 0x5f8 | Size: 0x10
    char MoveBlockedByEvent[0x10];                                                    // Offset: 0x608 | Size: 0x10
    char ContinuallyMoveBlockedByEvent[0x10];                                         // Offset: 0x618 | Size: 0x10
    char DeafEvent[0x10];                                                             // Offset: 0x628 | Size: 0x10
    char LackInMoistureEvent[0x10];                                                   // Offset: 0x638 | Size: 0x10
    char LackInFoodEvent[0x10];                                                       // Offset: 0x648 | Size: 0x10
    char EnduranceChangedEvent[0x10];                                                 // Offset: 0x658 | Size: 0x10
    char HealthConditionChangedEvent[0x10];                                           // Offset: 0x668 | Size: 0x10
    char HealthChangedEvent[0x10];                                                    // Offset: 0x678 | Size: 0x10
    char EnergyChangeEvent[0x10];                                                     // Offset: 0x688 | Size: 0x10
    char EnergyRecoverScaleEvent[0x10];                                               // Offset: 0x698 | Size: 0x10
    char EnergyLowExhaustedChangedEvent[0x10];                                        // Offset: 0x6a8 | Size: 0x10
    char MoistureChangeEvent[0x10];                                                   // Offset: 0x6b8 | Size: 0x10
    char FoodChangeEvent[0x10];                                                       // Offset: 0x6c8 | Size: 0x10
    char ReloadEvent[0x10];                                                           // Offset: 0x6d8 | Size: 0x10
    char OutOfEnduranceEvent[0x10];                                                   // Offset: 0x6e8 | Size: 0x10
    char OnCharacterUseVehicle[0x10];                                                 // Offset: 0x6f8 | Size: 0x10
    char OnCharacterLeaveVehicle[0x10];                                               // Offset: 0x708 | Size: 0x10
    char OnCharacterVehicleSwitchSeat[0x10];                                          // Offset: 0x718 | Size: 0x10
    char DamageSpreadingRoundFinishEvent[0x10];                                       // Offset: 0x728 | Size: 0x10
    char RecoverFromEnduranceEvent[0x10];                                             // Offset: 0x738 | Size: 0x10
    char PreTakeDamageEvent[0x10];                                                    // Offset: 0x748 | Size: 0x10
    char TakeDamageEvent[0x10];                                                       // Offset: 0x758 | Size: 0x10
    char PostTakeDamageEvent[0x10];                                                   // Offset: 0x768 | Size: 0x10
    char TakeRealDamageEvent[0x10];                                                   // Offset: 0x778 | Size: 0x10
    char TakeDamageByDebuffEvent[0x10];                                               // Offset: 0x788 | Size: 0x10
    char FFPTakeHitEvent[0x10];                                                       // Offset: 0x798 | Size: 0x10
    char F3PTakeHitEvent[0x10];                                                       // Offset: 0x7a8 | Size: 0x10
    char CauseDamageEvent[0x10];                                                      // Offset: 0x7b8 | Size: 0x10
    char CauseRealDamageEvent[0x10];                                                  // Offset: 0x7c8 | Size: 0x10
    char ArmorTakeDamageEvent[0x10];                                                  // Offset: 0x7d8 | Size: 0x10
    char MovableArmorTakeDamageEvent[0x10];                                           // Offset: 0x7e8 | Size: 0x10
    char CauseDebuffEvent[0x10];                                                      // Offset: 0x7f8 | Size: 0x10
    char CauseArmorDurabilityReduce[0x10];                                            // Offset: 0x808 | Size: 0x10
    char PreDiedEvent[0x10];                                                          // Offset: 0x818 | Size: 0x10
    char PostDiedEvent[0x10];                                                         // Offset: 0x828 | Size: 0x10
    char OnDeathAnimationEndedEvent[0x10];                                            // Offset: 0x838 | Size: 0x10
    char KillEvent[0x10];                                                             // Offset: 0x848 | Size: 0x10
    char AssistKillEvent[0x10];                                                       // Offset: 0x858 | Size: 0x10
    char BeKilledEvent[0x10];                                                         // Offset: 0x868 | Size: 0x10
    char BeKilledEquipmentEvent[0x10];                                                // Offset: 0x878 | Size: 0x10
    char PoseChangedEvent[0x10];                                                      // Offset: 0x888 | Size: 0x10
    char LeanTypeChangedEvent[0x10];                                                  // Offset: 0x898 | Size: 0x10
    char LeanRatioChangedEvent[0x10];                                                 // Offset: 0x8a8 | Size: 0x10
    char OnWeaponFireEvent[0x10];                                                     // Offset: 0x8b8 | Size: 0x10
    char OnWeaponHitEvent[0x10];                                                      // Offset: 0x8c8 | Size: 0x10
    char OnCharacterHoldingWeaponExtraMeshLoadCompleted[0x10];                        // Offset: 0x8d8 | Size: 0x10
    char DamageBlackborad[0x88];                                                      // Offset: 0x8e8 | Size: 0x88
    char TornOffEvent[0x10];                                                          // Offset: 0x970 | Size: 0x10
    char OnCharacterEnterTearGas[0x10];                                               // Offset: 0x980 | Size: 0x10
    char StartInteractBoxInventoryEvent[0x10];                                        // Offset: 0x990 | Size: 0x10
    char FinishGameEvent[0x10];                                                       // Offset: 0x9a0 | Size: 0x10
    char RotaterEvent[0x10];                                                          // Offset: 0x9b0 | Size: 0x10
    char OpenContextMenuEvent[0x10];                                                  // Offset: 0x9c0 | Size: 0x10
    char OpenContextMenuDTEvent[0x10];                                                // Offset: 0x9d0 | Size: 0x10
    char BagOpenedStateChangeEvent[0x10];                                             // Offset: 0x9e0 | Size: 0x10
    char BoxInventoryOpenedEvent[0x10];                                               // Offset: 0x9f0 | Size: 0x10
    char DoorEvent[0x10];                                                             // Offset: 0xa00 | Size: 0x10
    char LootContainerEvent[0x10];                                                    // Offset: 0xa10 | Size: 0x10
    char LootItemEvent[0x10];                                                         // Offset: 0xa20 | Size: 0x10
    char WalkEvent[0x10];                                                             // Offset: 0xa30 | Size: 0x10
    char LocationReport[0x10];                                                        // Offset: 0xa40 | Size: 0x10
    char StuckLocationReport[0x10];                                                   // Offset: 0xa50 | Size: 0x10
    struct FVector LastLocation;                                                      // Offset: 0xa60 | Size: 0xc
    char pad_0xA6C[0x3c];                                                             // Offset: 0xa6c | Size: 0x3c
    char StartUsingRecoveryItemEvent[0x10];                                           // Offset: 0xaa8 | Size: 0x10
    char RecoveryItemActivatedEvent[0x10];                                            // Offset: 0xab8 | Size: 0x10
    char ReceiveRecoveryEffectEvent[0x10];                                            // Offset: 0xac8 | Size: 0x10
    char OnCharacterInvEquipPositionChangedEvent[0x10];                               // Offset: 0xad8 | Size: 0x10
    char SwitchWeaponEvent[0x10];                                                     // Offset: 0xae8 | Size: 0x10
    char SwitchWeaponCompletedEvent[0x10];                                            // Offset: 0xaf8 | Size: 0x10
    char UseInventoryFlashEvent[0x10];                                                // Offset: 0xb08 | Size: 0x10
    char InventoryFlashDebuffEvent[0x10];                                             // Offset: 0xb18 | Size: 0x10
    char OnSetCurrentWeaponEvent[0x10];                                               // Offset: 0xb28 | Size: 0x10
    char OnSetCurrentWeaponAfterMontageEvent[0x10];                                   // Offset: 0xb38 | Size: 0x10
    char OnUpdateWeaponAnimationSetsEvent[0x10];                                      // Offset: 0xb48 | Size: 0x10
    char UnProne[0x10];                                                               // Offset: 0xb58 | Size: 0x10
    char ProneToCrouch[0x10];                                                         // Offset: 0xb68 | Size: 0x10
    char CrouchToProne[0x10];                                                         // Offset: 0xb78 | Size: 0x10
    char StandToProne[0x10];                                                          // Offset: 0xb88 | Size: 0x10
    char EnterDBNOStatusEvent[0x10];                                                  // Offset: 0xb98 | Size: 0x10
    char ExitDBNOStatusEvent[0x10];                                                   // Offset: 0xba8 | Size: 0x10
    char AbortRescueTeammateEvent[0x10];                                              // Offset: 0xbb8 | Size: 0x10
    char OnClickAtk[0x10];                                                            // Offset: 0xbc8 | Size: 0x10
    char OnClickStopAtk[0x10];                                                        // Offset: 0xbd8 | Size: 0x10
    char PreAddInventoryEvent[0x10];                                                  // Offset: 0xbe8 | Size: 0x10
    char OnPeriodicGameplayEffectExecuteEvent[0x10];                                  // Offset: 0xbf8 | Size: 0x10
    char OnGameplayEffectAppliedEvent[0x10];                                          // Offset: 0xc08 | Size: 0x10
    char OnAnyGameplayEffectRemovedEvent[0x10];                                       // Offset: 0xc18 | Size: 0x10
    char OnGameplayDebuffStatusChangedEvent[0x10];                                    // Offset: 0xc28 | Size: 0x10
    char OnStartSearchingContainerEvent[0x10];                                        // Offset: 0xc38 | Size: 0x10
    char OnContainerRoughSearchEndEvent[0x10];                                        // Offset: 0xc48 | Size: 0x10
    char OnContainerSearchEnd[0x10];                                                  // Offset: 0xc58 | Size: 0x10
    char OnRecvMsg[0x10];                                                             // Offset: 0xc68 | Size: 0x10
    char OnAvatarAddedEvent[0x10];                                                    // Offset: 0xc78 | Size: 0x10
    char OnNativeAvatarAddedEvent[0x10];                                              // Offset: 0xc88 | Size: 0x10
    char OnAvatarRemovedEvent[0x10];                                                  // Offset: 0xc98 | Size: 0x10
    char OnNativeAvatarRemovedEvent[0x10];                                            // Offset: 0xca8 | Size: 0x10
    char OnBadgeUpdateEvent[0x10];                                                    // Offset: 0xcb8 | Size: 0x10
    char OnEnterCheckVolumeEvent[0x10];                                               // Offset: 0xcc8 | Size: 0x10
    char OnBecomeViewTarget[0x10];                                                    // Offset: 0xcd8 | Size: 0x10
    char OnPossessedByController[0x10];                                               // Offset: 0xce8 | Size: 0x10
    char OnUnPossessedByController[0x10];                                             // Offset: 0xcf8 | Size: 0x10
    char OnStartSpectatedByController[0x10];                                          // Offset: 0xd08 | Size: 0x10
    char OnStopSpectatedByController[0x10];                                           // Offset: 0xd18 | Size: 0x10
    char OnUpdateOnBackWeaponVisibility[0x10];                                        // Offset: 0xd28 | Size: 0x10
    char OnGetViewedByController[0x10];                                               // Offset: 0xd38 | Size: 0x10
    char OnLockInventoriesInContainer[0x10];                                          // Offset: 0xd48 | Size: 0x10
    char OnUsingInventoryChanged[0x10];                                               // Offset: 0xd58 | Size: 0x10
    char EnterDSSendWeaponAttri[0x10];                                                // Offset: 0xd68 | Size: 0x10
    char OnKillQuestProgressAdd[0x10];                                                // Offset: 0xd78 | Size: 0x10
    char OnCollectQuestProgressChanged[0x10];                                         // Offset: 0xd88 | Size: 0x10
    char OnKillMissionProgressAdd[0x10];                                              // Offset: 0xd98 | Size: 0x10
    char OnCollectMissionProgressChanged[0x10];                                       // Offset: 0xda8 | Size: 0x10
    char ClientLocationCorrectionEvent[0x10];                                         // Offset: 0xdb8 | Size: 0x10
    char FrameRateJitterEvent[0x10];                                                  // Offset: 0xdc8 | Size: 0x10
    char ThermalViewHackEvent[0x10];                                                  // Offset: 0xdd8 | Size: 0x10
    char CharacterPreAddInventoryEvent[0x10];                                         // Offset: 0xde8 | Size: 0x10
    char CharacterAddInventoryEvent[0x10];                                            // Offset: 0xdf8 | Size: 0x10
    char CharacterRemoveInventoryEvent[0x10];                                         // Offset: 0xe08 | Size: 0x10
    char CharacterInventoryTotalCountChangeEvent[0x10];                               // Offset: 0xe18 | Size: 0x10
    char ActivityInventoryAddedEvent[0x10];                                           // Offset: 0xe28 | Size: 0x10
    char ActivityInventoryRemovedEvent[0x10];                                         // Offset: 0xe38 | Size: 0x10
    char CharacterRemoveWeaponEvent[0x10];                                            // Offset: 0xe48 | Size: 0x10
    char CharacterInventoryMovedEvent[0x10];                                          // Offset: 0xe58 | Size: 0x10
    char Client_InventoryGridInfoChangedEvent[0x10];                                  // Offset: 0xe68 | Size: 0x10
    char OnCharacterSexChanged[0x10];                                                 // Offset: 0xe78 | Size: 0x10
    char OnCharacterAvatarDataChanged[0x10];                                          // Offset: 0xe88 | Size: 0x10
    char OnCharacterRagdollPoseFinished[0x10];                                        // Offset: 0xe98 | Size: 0x10
    char OnCharacterMeshChanged[0x10];                                                // Offset: 0xea8 | Size: 0x10
    char OnCharacterAvatarMeshLoaded[0x10];                                           // Offset: 0xeb8 | Size: 0x10
    char OnCharacterAvatarMeshCheck[0x10];                                            // Offset: 0xec8 | Size: 0x10
    char OnCharacterAllAvatarMeshLoaded[0x10];                                        // Offset: 0xed8 | Size: 0x10
    char OnPrimaryWeaponAllMeshLoaded[0x10];                                          // Offset: 0xee8 | Size: 0x10
    char OnCharacterAvatarListChanged[0x10];                                          // Offset: 0xef8 | Size: 0x10
    char OnShowMergedAvatar[0x10];                                                    // Offset: 0xf08 | Size: 0x10
    char OnSetNewOwner[0x10];                                                         // Offset: 0xf18 | Size: 0x10
    char OnOwnerOrRoleChanged[0x10];                                                  // Offset: 0xf28 | Size: 0x10
    char OnRepControllerEvent[0x10];                                                  // Offset: 0xf38 | Size: 0x10
    char OnSetNewPlayerState[0x10];                                                   // Offset: 0xf48 | Size: 0x10
    char OnChangeCustomFOV[0x10];                                                     // Offset: 0xf58 | Size: 0x10
    char OnEnableCustomFOV[0x10];                                                     // Offset: 0xf68 | Size: 0x10
    char OnSprintRequest[0x10];                                                       // Offset: 0xf78 | Size: 0x10
    char OnSprintingChanged[0x10];                                                    // Offset: 0xf88 | Size: 0x10
    char OnCharacterZoomChanged[0x10];                                                // Offset: 0xf98 | Size: 0x10
    char OnAbilityActivated[0x10];                                                    // Offset: 0xfa8 | Size: 0x10
    char DBNOEndReport[0x10];                                                         // Offset: 0xfb8 | Size: 0x10
    char InventoryGiveComplete[0x10];                                                 // Offset: 0xfc8 | Size: 0x10
    char InventoryLoadComplete[0x10];                                                 // Offset: 0xfd8 | Size: 0x10
    char OnInventorySkeletalMeshLoadComplete[0x10];                                   // Offset: 0xfe8 | Size: 0x10
    char OnCharacterSkeletalMeshLoadComplete[0x10];                                   // Offset: 0xff8 | Size: 0x10
    char OnInventoryStaticMeshLoadComplete[0x10];                                     // Offset: 0x1008 | Size: 0x10
    char OnCharacterStaticMeshLoadComplete[0x10];                                     // Offset: 0x1018 | Size: 0x10
    char OnBeforeCharacterUpdateAvatarLayers[0x10];                                   // Offset: 0x1028 | Size: 0x10
    char OnAfterCharacterUpdateAvatarLayers[0x10];                                    // Offset: 0x1038 | Size: 0x10
    char OnEnableHighPowerScope[0x10];                                                // Offset: 0x1048 | Size: 0x10
    char OnCharacterBecomeCorpse[0x10];                                               // Offset: 0x1058 | Size: 0x10
    char OnCharacterLoadProtectStateChanged[0x10];                                    // Offset: 0x1068 | Size: 0x10
    char OnAutoAimAntiHackStatisReceived[0x10];                                       // Offset: 0x1078 | Size: 0x10
    char OnCarriedFaceShieldActivated[0x10];                                          // Offset: 0x1088 | Size: 0x10
    char OnCharacterCastShadow[0x10];                                                 // Offset: 0x1098 | Size: 0x10
    char OnCharacterDisplayPolicyChanged[0x10];                                       // Offset: 0x10a8 | Size: 0x10
    char OnWeaponHarmVerifyFail[0x10];                                                // Offset: 0x10b8 | Size: 0x10
    char OnPickUpInventory[0x10];                                                     // Offset: 0x10c8 | Size: 0x10
    char OnActivateInteract[0x10];                                                    // Offset: 0x10d8 | Size: 0x10
    char OnSoundLevelInfluenceFactorChanged[0x10];                                    // Offset: 0x10e8 | Size: 0x10
    char OnSoundMaxDistanceInfluenceFactorChanged[0x10];                              // Offset: 0x10f8 | Size: 0x10
    char TinnitusDelegate[0x10];                                                      // Offset: 0x1108 | Size: 0x10
    char FullBodyGestureTimeHandle[0x8];                                              // Offset: 0x1118 | Size: 0x8
    char FullBodyGestureDelegate[0x10];                                               // Offset: 0x1120 | Size: 0x10
    char OnDestroyActor[0x10];                                                        // Offset: 0x1130 | Size: 0x10
    char OnCharacterPreDestroyed[0x10];                                               // Offset: 0x1140 | Size: 0x10
    char OnCharacterDestroyed[0x10];                                                  // Offset: 0x1150 | Size: 0x10
    char OnFSMEscapeStateChange[0x10];                                                // Offset: 0x1160 | Size: 0x10
    char OnWeaponListChange[0x10];                                                    // Offset: 0x1170 | Size: 0x10
    char OnCharacterResetToLastSavedState[0x10];                                      // Offset: 0x1180 | Size: 0x10
    char OnActiveLongDistanceTacticalInv[0x10];                                       // Offset: 0x1190 | Size: 0x10
    char OnEquipSkeletalMeshLoadCompleted[0x10];                                      // Offset: 0x11a0 | Size: 0x10
    char OnEquipStaticMeshLoadCompleted[0x10];                                        // Offset: 0x11b0 | Size: 0x10
    char OnPreUpdateAvatars[0x10];                                                    // Offset: 0x11c0 | Size: 0x10
    char OnUpdateAvatarsAddInventory[0x10];                                           // Offset: 0x11d0 | Size: 0x10
    char OnUpdateAvatarsAfterAddInventory[0x10];                                      // Offset: 0x11e0 | Size: 0x10
    char OnEquipMeshDecideLoadSkeletalMeshInLobby[0x10];                              // Offset: 0x11f0 | Size: 0x10
    char OnInvMeshDecideLoadSkeletalMeshInLobby[0x10];                                // Offset: 0x1200 | Size: 0x10
    char OnInvAnimInstanceLoad[0x10];                                                 // Offset: 0x1210 | Size: 0x10
    char OnUpdateAvatarsFinish[0x10];                                                 // Offset: 0x1220 | Size: 0x10
    char OnGrenadeKillMulti[0x10];                                                    // Offset: 0x1230 | Size: 0x10
    char OnHarmTeammate[0x10];                                                        // Offset: 0x1240 | Size: 0x10
    char pad_0x1250[0x18];                                                            // Offset: 0x1250 | Size: 0x18
    char ASCRepPlayMontage[0x10];                                                     // Offset: 0x1268 | Size: 0x10
    char OnCharacterUseActorSuccess[0x10];                                            // Offset: 0x1278 | Size: 0x10
    char SuccessfullyUsedRecoveryInventoryDurabilityConsumed[0x10];                   // Offset: 0x1288 | Size: 0x10
    char OnUpdateAvatarsAddConfData[0x10];                                            // Offset: 0x1298 | Size: 0x10
    char OnUpdateAvatarsAfterAddConfData[0x10];                                       // Offset: 0x12a8 | Size: 0x10
    char OnCharacterCostBullet[0x10];                                                 // Offset: 0x12b8 | Size: 0x10
    char OnCollectCharacterInputAfterCertainCases[0x10];                              // Offset: 0x12c8 | Size: 0x10
    char OnCollectCharacterInputAfterGrenadeHit[0x10];                                // Offset: 0x12d8 | Size: 0x10
    char OnCollectCharacterInputAfterGreandeSideEffect[0x10];                         // Offset: 0x12e8 | Size: 0x10
    char OnCharacterCalcAcceleration[0x10];                                           // Offset: 0x12f8 | Size: 0x10
    char PostCharacterCalcVelocity[0x10];                                             // Offset: 0x1308 | Size: 0x10
    char OnControllerPreUpdateRotation[0x10];                                         // Offset: 0x1318 | Size: 0x10
    char OnGetBaseCameraWorldLocation[0x10];                                          // Offset: 0x1328 | Size: 0x10
    char OnRepWeaponAnimInfo[0x10];                                                   // Offset: 0x1338 | Size: 0x10
    char OnCharacterBeginTurnInPlace[0x10];                                           // Offset: 0x1348 | Size: 0x10
    char pad_0x1358[0x8];                                                             // Offset: 0x1358 | Size: 0x8
    struct USGCharacterSignificanceManager *SignificanceMgr;                          // Offset: 0x1360 | Size: 0x8
    char pad_0x1368[0x50];                                                            // Offset: 0x1368 | Size: 0x50
    struct USGCharacterTestDamageComponent *TestDamageComponent;                      // Offset: 0x13b8 | Size: 0x8
    bool bShouldDeferConstructionAIC;                                                 // Offset: 0x13c0 | Size: 0x1
    char pad_0x13C1[0x7];                                                             // Offset: 0x13c1 | Size: 0x7
    struct USGCharacterLocalDataComponent *LocalDataComponent;                        // Offset: 0x13c8 | Size: 0x8
    struct UCameraComponent *CharacterCameraComponent;                                // Offset: 0x13d0 | Size: 0x8
    struct UCameraComponent *FPPCameraComponent;                                      // Offset: 0x13d8 | Size: 0x8
    char pad_0x13E0[0x18];                                                            // Offset: 0x13e0 | Size: 0x18
    float SmoothTargetViewRotationSpeed;                                              // Offset: 0x13f8 | Size: 0x4
    char pad_0x13FC[0x4];                                                             // Offset: 0x13fc | Size: 0x4
    struct ASGPlayerController *CurrentViewerPC;                                      // Offset: 0x1400 | Size: 0x8
    bool bIsTurning;                                                                  // Offset: 0x1408 | Size: 0x1
    bool bEnableBeAutoAimed;                                                          // Offset: 0x1409 | Size: 0x1
    char pad_0x140A[0x16];                                                            // Offset: 0x140a | Size: 0x16
    bool bEnableViewSignificance;                                                     // Offset: 0x1420 | Size: 0x1
    char DefaultCharacterSex;                                                         // Offset: 0x1421 | Size: 0x1
    char pad_0x1422[0x2];                                                             // Offset: 0x1422 | Size: 0x2
    uint32_t PlayerViewInfo;                                                          // Offset: 0x1424 | Size: 0x4
    float PlayerViewPitch;                                                            // Offset: 0x1428 | Size: 0x4
    float PlayerViewYaw;                                                              // Offset: 0x142c | Size: 0x4
    char PlayerTurnInfo;                                                              // Offset: 0x1430 | Size: 0x1
    ECharacterType CachedCharacterType;                                               // Offset: 0x1431 | Size: 0x1
    char pad_0x1432[0x6];                                                             // Offset: 0x1432 | Size: 0x6
    uint64_t GID;                                                                     // Offset: 0x1438 | Size: 0x8
    char FactionType;                                                                 // Offset: 0x1440 | Size: 0x1
    char CharacterSex;                                                                // Offset: 0x1441 | Size: 0x1
    char pad_0x1442[0x6];                                                             // Offset: 0x1442 | Size: 0x6
    struct AController *LastPossessedController;                                      // Offset: 0x1448 | Size: 0x8
    bool bMeshTickThisFrame;                                                          // Offset: 0x1450 | Size: 0x1
    char pad_0x1451[0x17];                                                            // Offset: 0x1451 | Size: 0x17
    bool bEnterSmoke;                                                                 // Offset: 0x1468 | Size: 0x1
    char pad_0x1469[0x23];                                                            // Offset: 0x1469 | Size: 0x23
    int32_t UROLoadBalanceBudget;                                                     // Offset: 0x148c | Size: 0x4
    struct USGCharacterMovementComponent *SGCharacterMovement;                        // Offset: 0x1490 | Size: 0x8
    char pad_0x1498[0x28];                                                            // Offset: 0x1498 | Size: 0x28
    int64_t ProxyCharacterCounter;                                                    // Offset: 0x14c0 | Size: 0x8
    char pad_0x14C8[0x10];                                                            // Offset: 0x14c8 | Size: 0x10
    char OnReadyToPlay[0x10];                                                         // Offset: 0x14d8 | Size: 0x10
    struct UAbilitySystemComponent *AbilitySystemComponent;                           // Offset: 0x14e8 | Size: 0x8
    struct USGContextMenu *Menu;                                                      // Offset: 0x14f0 | Size: 0x8
    char LocationReportHandle[0x8];                                                   // Offset: 0x14f8 | Size: 0x8
    struct FVector CharacterMeshLocationOffset;                                       // Offset: 0x1500 | Size: 0xc
    bool bReadyToPlay;                                                                // Offset: 0x150c | Size: 0x1
    char pad_0x150D[0x3];                                                             // Offset: 0x150d | Size: 0x3
    char WaitingForInitComponents[0x50];                                              // Offset: 0x1510 | Size: 0x50
    char ActiveLongDistanceTacticalInvs[0x50];                                        // Offset: 0x1560 | Size: 0x50
    bool bTickingOnDeath;                                                             // Offset: 0x15b0 | Size: 0x1
    char pad_0x15B1[0x7];                                                             // Offset: 0x15b1 | Size: 0x7
    struct AActor *LastHitByDamageCauser;                                             // Offset: 0x15b8 | Size: 0x8
    char LastHitByOtherInfo[0x30];                                                    // Offset: 0x15c0 | Size: 0x30
    char pad_0x15F0[0x4];                                                             // Offset: 0x15f0 | Size: 0x4
    char Acceleration[0xc];                                                           // Offset: 0x15f4 | Size: 0xc
    char PredictStopLocation[0xc];                                                    // Offset: 0x1600 | Size: 0xc
    char pad_0x160C[0x8];                                                             // Offset: 0x160c | Size: 0x8
    bool bIsReplayViewTarget;                                                         // Offset: 0x1614 | Size: 0x1
    bool bSimulateNavWalkSnapFloor;                                                   // Offset: 0x1615 | Size: 0x1
    char LaserTraceCollisionChannel;                                                  // Offset: 0x1616 | Size: 0x1
    char pad_0x1617[0x1];                                                             // Offset: 0x1617 | Size: 0x1
    float CharacterSpawnTime;                                                         // Offset: 0x1618 | Size: 0x4
    bool bRestarting;                                                                 // Offset: 0x161c | Size: 0x1
    char pad_0x161D[0xb];                                                             // Offset: 0x161d | Size: 0xb
    struct USGCharacterWeaponManagerComponent *WeaponManagerComponent;                // Offset: 0x1628 | Size: 0x8
    struct USGCharacterDeathComponent *DeathComponent;                                // Offset: 0x1630 | Size: 0x8
    struct USGCharacterMovementAbilityComponent *MovementAbilityComponent;            // Offset: 0x1638 | Size: 0x8
    struct USGCharacterBaseTurnComponent *TurnComponent;                              // Offset: 0x1640 | Size: 0x8
    struct USGCharacterInputProcessComponent *InputProcessComponent;                  // Offset: 0x1648 | Size: 0x8
    struct USGCharacterMovementProneComponent *MovementProneComponent;                // Offset: 0x1650 | Size: 0x8
    struct USGCharacterMovementDBNOComponent *MovementDBNOComponent;                  // Offset: 0x1658 | Size: 0x8
    struct USGCharacterMovementVaultComponent *MovementVaultComponent;                // Offset: 0x1660 | Size: 0x8
    struct USGCharacterDBNOComponent *DBNOComponent;                                  // Offset: 0x1668 | Size: 0x8
    struct USGCharacterFallComponent *CharacterFallComponent;                         // Offset: 0x1670 | Size: 0x8
    struct USGCharacterLeanWallComponent *LeanComponent;                              // Offset: 0x1678 | Size: 0x8
    struct USGCharacterRagdollNewComponent *RagdollComponent;                         // Offset: 0x1680 | Size: 0x8
    struct USGCharacterAIComponent *AIComponent;                                      // Offset: 0x1688 | Size: 0x8
    struct USGCharacterAIPoseComponent *AIPoseComponent;                              // Offset: 0x1690 | Size: 0x8
    struct USGCharacterWeaponSwayComponent *WeaponSwayComponent;                      // Offset: 0x1698 | Size: 0x8
    struct USGUAMCharacterPoseCacheComponent *PoseCacheComponent;                     // Offset: 0x16a0 | Size: 0x8
    struct UMFClimateMovableRainWetnessComponent *WetnessComponent;                   // Offset: 0x16a8 | Size: 0x8
    struct USGCharacterEnergyComponent *EnergyComponent;                              // Offset: 0x16b0 | Size: 0x8
    struct USGCharacterSprintComponent *SprintComponent;                              // Offset: 0x16b8 | Size: 0x8
    struct USGActorUseComponent *UseComponent;                                        // Offset: 0x16c0 | Size: 0x8
    struct USGCharacterAnimationComponent *AnimationComponent;                        // Offset: 0x16c8 | Size: 0x8
    struct USGCharacterPreviewComponent *PreviewComponent;                            // Offset: 0x16d0 | Size: 0x8
    struct USGCharacterWeaponTraceComponent *WeaponTraceComponent;                    // Offset: 0x16d8 | Size: 0x8
    struct USGCharacterCurveBreathComponent *CurveBreathComponent;                    // Offset: 0x16e0 | Size: 0x8
    struct USGCharacterTakeHitEffectComponent *TakeHitEffectComponent;                // Offset: 0x16e8 | Size: 0x8
    struct USGCharacterTakeCoverComponent *TakeCoverComponent;                        // Offset: 0x16f0 | Size: 0x8
    struct USGCharacterMovementLadderClimbComponent *MovementLadderClimbComponent;    // Offset: 0x16f8 | Size: 0x8
    struct USGCharacterInteractionComponent *InteractionComponent;                    // Offset: 0x1700 | Size: 0x8
    struct USGCharacterTeamRescueComponent *TeamRescueComponent;                      // Offset: 0x1708 | Size: 0x8
    struct USGCharacterIKComponent *IKComponent;                                      // Offset: 0x1710 | Size: 0x8
    struct USGCharacterStunGrenadeEffectComponent *StunGrenadeEffectComponent;        // Offset: 0x1718 | Size: 0x8
    struct USGCharacterWeightComponent *WeightComponent;                              // Offset: 0x1720 | Size: 0x8
    struct USGCharacterSenseAbilityComponent *SenseAbilityComponent;                  // Offset: 0x1728 | Size: 0x8
    struct USGCharacterCastShadowComponent *CastShadowComp;                           // Offset: 0x1730 | Size: 0x8
    struct USGCharacterAvatarComponent *AvatarComp;                                   // Offset: 0x1738 | Size: 0x8
    struct USGCharacterAvatarManagerComponent *AvatarManagerComp;                     // Offset: 0x1740 | Size: 0x8
    struct USGCharacterMeshComponent *CharacterMeshComp;                              // Offset: 0x1748 | Size: 0x8
    struct USGCharacterAvatarMergeComponent *AvatarMergeComp;                         // Offset: 0x1750 | Size: 0x8
    struct USGCharacterBoneBreakComponent *CharacterBoneBreakComponent;               // Offset: 0x1758 | Size: 0x8
    struct USGCharacterFootEffectComponent *FootEffectComponent;                      // Offset: 0x1760 | Size: 0x8
    struct USGCharacterFragGrenadeEffectComponent *FragEffectComponent;               // Offset: 0x1768 | Size: 0x8
    struct USGCharacterFlashGrenadeEffectComponent *FlashEffectComponent;             // Offset: 0x1770 | Size: 0x8
    struct USGCharacterDebugMovementComponent *CharacterDebugMovementComponent;       // Offset: 0x1778 | Size: 0x8
    struct USGCharacterWeaponManagerComponent *CharacterWeaponManagerComponent;       // Offset: 0x1780 | Size: 0x8
    struct USGCharacterLoadProtectComponent *CharacterLoadProtectComponent;           // Offset: 0x1788 | Size: 0x8
    struct USGCharacterInventoryManagerComponent *CharacterInventoryManagerComponent; // Offset: 0x1790 | Size: 0x8
    struct USGMotionWarpingComponent *MotionWarpingComponent;                         // Offset: 0x1798 | Size: 0x8
    struct USGCharacterInventoryComponent *CharacterInventoryComponent;               // Offset: 0x17a0 | Size: 0x8
    struct USGCharacterMoveBlockedAvoidanceComponent *MoveBlockedAvoidanceComponent;  // Offset: 0x17a8 | Size: 0x8
    struct USGCharacterSoundComponent *CharacterSoundComponent;                       // Offset: 0x17b0 | Size: 0x8
    struct USGCharacterArmorManagerComponent *CharacterArmorManagerComponent;         // Offset: 0x17b8 | Size: 0x8
    struct USGCharacterWeaponAttachComponent *CharacterWeaponAttachComponent;         // Offset: 0x17c0 | Size: 0x8
    struct USGCharacterInventoryGiveComponent *CharacterInventoryGiveComponent;       // Offset: 0x17c8 | Size: 0x8
    struct USGCharacterDeliverCargoComponent *CharacterDeliverCargoComponent;         // Offset: 0x17d0 | Size: 0x8
    struct USGCharacterTacticalPistolComponent *CharacterTacticalPistolComponent;     // Offset: 0x17d8 | Size: 0x8
    struct USGCharacterVehicleComponent *VehicleComponent;                            // Offset: 0x17e0 | Size: 0x8
    struct USGCharacterLagCompensationComponent *LagCompensationComp;                 // Offset: 0x17e8 | Size: 0x8
    struct USGCharacterEnduranceComponent *CharacterEnduranceComponent;               // Offset: 0x17f0 | Size: 0x8
    struct USGCharacterSwimComponent *CharacterSwimComponent;                         // Offset: 0x17f8 | Size: 0x8
    struct USGCharacterEmplaceGunComponent *CharacterEmplaceGunComponent;             // Offset: 0x1800 | Size: 0x8
    struct USGCharacterHealthComponent *CharacterHealthComponent;                     // Offset: 0x1808 | Size: 0x8
    struct USGCharacterCustomCameraComponent *CharacterCustomCameraComponent;         // Offset: 0x1810 | Size: 0x8
    char pad_0x1818[0x74];                                                            // Offset: 0x1818 | Size: 0x74
    float PlayerViewInterpSpeedForReplay;                                             // Offset: 0x188c | Size: 0x4
    char pad_0x1890[0x4];                                                             // Offset: 0x1890 | Size: 0x4
    struct FRotator VehicleCameraRelativeRotation;                                    // Offset: 0x1894 | Size: 0xc
    char pad_0x18A0[0x1c];                                                            // Offset: 0x18a0 | Size: 0x1c
    struct FVector ViewPointOffset;                                                   // Offset: 0x18bc | Size: 0xc
    float DesiredFOV;                                                                 // Offset: 0x18c8 | Size: 0x4
    char pad_0x18CC[0x1c];                                                            // Offset: 0x18cc | Size: 0x1c
    struct ASGCharacterProxy *CharacterProxy;                                         // Offset: 0x18e8 | Size: 0x8
};

struct ASGAICharacter : ASGCharacter {
    // Fields
    char pad_0x18F0[0x10];                                     // Offset: 0x18f0 | Size: 0x10
    char RankData[0xc];                                        // Offset: 0x1900 | Size: 0xc
    float PatrolRadius;                                        // Offset: 0x190c | Size: 0x4
    float CautionRadius;                                       // Offset: 0x1910 | Size: 0x4
    float CrossFireRadius;                                     // Offset: 0x1914 | Size: 0x4
    int32_t Kill_PMCAI_Count;                                  // Offset: 0x1918 | Size: 0x4
    int32_t Kill_ScavAI_Count;                                 // Offset: 0x191c | Size: 0x4
    int32_t Kill_PMCPlayer_Count;                              // Offset: 0x1920 | Size: 0x4
    int32_t Kill_ScavPlayer_Count;                             // Offset: 0x1924 | Size: 0x4
    int32_t Loot_Count;                                        // Offset: 0x1928 | Size: 0x4
    struct FName SpawnedGroupID;                               // Offset: 0x192c | Size: 0x8
    int32_t EquipmentPoolID;                                   // Offset: 0x1934 | Size: 0x4
    int64_t AIID;                                              // Offset: 0x1938 | Size: 0x8
    int64_t AIUID;                                             // Offset: 0x1940 | Size: 0x8
    int64_t PathGroupID;                                       // Offset: 0x1948 | Size: 0x8
    int32_t AILevel;                                           // Offset: 0x1950 | Size: 0x4
    char pad_0x1954[0x4];                                      // Offset: 0x1954 | Size: 0x4
    struct TArray<int32_t> BattleVolumesIn;                    // Offset: 0x1958 | Size: 0x10
    float GrenadeThrowAngle;                                   // Offset: 0x1968 | Size: 0x4
    float GrenadeInitSpeed;                                    // Offset: 0x196c | Size: 0x4
    float GrenadeFlyingTime;                                   // Offset: 0x1970 | Size: 0x4
    float GrenadeHoldedTime;                                   // Offset: 0x1974 | Size: 0x4
    struct ANPCAINavModifierVolume *GrenadeNavModifier;        // Offset: 0x1978 | Size: 0x8
    float LastBattleEnterTime;                                 // Offset: 0x1980 | Size: 0x4
    char pad_0x1984[0x4];                                      // Offset: 0x1984 | Size: 0x4
    char OnAICharacterReady[0x10];                             // Offset: 0x1988 | Size: 0x10
    char OnAICharacterGoalEnemyDied[0x10];                     // Offset: 0x1998 | Size: 0x10
    char OnPathSegmentBecomeDanger[0x10];                      // Offset: 0x19a8 | Size: 0x10
    char OnAIAimingStartOneRound[0x10];                        // Offset: 0x19b8 | Size: 0x10
    char OnAIFireStartOneRound[0x10];                          // Offset: 0x19c8 | Size: 0x10
    char OnAIFireStopOneRound[0x10];                           // Offset: 0x19d8 | Size: 0x10
    char OnAIExitRLModeEvt[0x10];                              // Offset: 0x19e8 | Size: 0x10
    struct UDataTable *SoundData;                              // Offset: 0x19f8 | Size: 0x8
    bool bActive;                                              // Offset: 0x1a00 | Size: 0x1
    bool bInDSLODControlled;                                   // Offset: 0x1a01 | Size: 0x1
    bool bShouldPauseTick;                                     // Offset: 0x1a02 | Size: 0x1
    bool IsSafeKeeper;                                         // Offset: 0x1a03 | Size: 0x1
    char pad_0x1A04[0x8];                                      // Offset: 0x1a04 | Size: 0x8
    float DeathNetCullDistanceSquared;                         // Offset: 0x1a0c | Size: 0x4
    char EQSDataInstances[0x50];                               // Offset: 0x1a10 | Size: 0x50
    bool bAIReady;                                             // Offset: 0x1a60 | Size: 0x1
    bool bCanStartAI;                                          // Offset: 0x1a61 | Size: 0x1
    char pad_0x1A62[0x6];                                      // Offset: 0x1a62 | Size: 0x6
    struct FString AIName;                                     // Offset: 0x1a68 | Size: 0x10
    struct FString DirectorName;                               // Offset: 0x1a78 | Size: 0x10
    struct FString AIAvatarUrl;                                // Offset: 0x1a88 | Size: 0x10
    char pad_0x1A98[0x4];                                      // Offset: 0x1a98 | Size: 0x4
    int32_t Handle;                                            // Offset: 0x1a9c | Size: 0x4
    char MySpawnRecord[0x88];                                  // Offset: 0x1aa0 | Size: 0x88
    struct USGCharacterAIStaticsComponent *AIStaticsComponent; // Offset: 0x1b28 | Size: 0x8
    char TempCloseTickComps[0x50];                             // Offset: 0x1b30 | Size: 0x50
};

enum class EAIMainType : uint8_t {
    EAIMainType_Scav = 0,
    EAIMainType_Boss = 1,
    EAIMainType_Follower = 2,
    EAIMainType_PMCAI = 3,
    EAIMainType_TeachingAI = 4,
    EAIMainType_TestAI = 5,
    EAIMainType_Elit = 6,
    EAIMainType_Elit_PlayerScavAI = 7,
    EAIMainType_MAX = 8
};

enum class EFactionType : uint8_t {
    None = 0,
    NormalPMC = 1,
    NormalScav = 2,
    PlayerScav = 3,
    RebelFaction = 4,
    LakeFaction = 5,
    GangsterFaction = 6,
    KurtTeam = 7,
    NavyFaction = 8,
    Blackgold = 9,
    Gnesk = 10,
    MadDog = 11,
    EFactionType_MAX = 12
};

struct USGCharacterAIStaticsComponent : UActorComponent {
    // Fields
    char pad_0xE0[0x10];                                                     // Offset: 0xe0 | Size: 0x10
    EAIMainType AIMainType;                                                  // Offset: 0xf0 | Size: 0x1
    EFactionType AIFactionType;                                              // Offset: 0xf1 | Size: 0x1
    char pad_0xF2[0x2];                                                      // Offset: 0xf2 | Size: 0x2
    float WeaponShootingMovingScale;                                         // Offset: 0xf4 | Size: 0x4
    float DynamicMovingSpeedScale;                                           // Offset: 0xf8 | Size: 0x4
    float BulletArmorPenetrationNotHurt;                                     // Offset: 0xfc | Size: 0x4
    float ChangeFactionTime;                                                 // Offset: 0x100 | Size: 0x4
    char pad_0x104[0x4];                                                     // Offset: 0x104 | Size: 0x4
    struct UObject *AmmoClassCache;                                          // Offset: 0x108 | Size: 0x8
    struct UNPCAIPropertyComponent_AIType *AITypeProps;                      // Offset: 0x110 | Size: 0x8
    struct UNPCAIMultiTargetSelectorComponent *MultiTargetSelectorComponent; // Offset: 0x118 | Size: 0x8
    struct USGCharacterSignificanceManager *SignificanceMgr;                 // Offset: 0x120 | Size: 0x8
    char pad_0x128[0x10];                                                    // Offset: 0x128 | Size: 0x10
};

struct USGActorDeathComponent : UActorComponent {
    // Fields
    struct AController *DeferEventInstigator;          // Offset: 0xe0 | Size: 0x8
    struct AActor *DeferEffectCauser;                  // Offset: 0xe8 | Size: 0x8
    struct UDamageEventObject *DeferDamageEventObject; // Offset: 0xf0 | Size: 0x8
    bool DeferbForceDied;                              // Offset: 0xf8 | Size: 0x1
    char pad_0xF9[0x7];                                // Offset: 0xf9 | Size: 0x7
};

struct FCharacterDeathInfo {
    // Fields
    bool bIsDead;                  // Offset: 0x0 | Size: 0x1
    bool bForceDied;               // Offset: 0x1 | Size: 0x1
    char pad_0x2[0x2];             // Offset: 0x2 | Size: 0x2
    float Timestamp;               // Offset: 0x4 | Size: 0x4
    int32_t DeathCount;            // Offset: 0x8 | Size: 0x4
    struct FVector DeathLocation;  // Offset: 0xc | Size: 0xc
    struct FRotator DeathRotation; // Offset: 0x18 | Size: 0xc
    char pad_0x24[0x4];            // Offset: 0x24 | Size: 0x4
    struct AActor *DamageCauser;   // Offset: 0x28 | Size: 0x8
    char DamageCauserType;         // Offset: 0x30 | Size: 0x1
    char DeathBodyPart;            // Offset: 0x31 | Size: 0x1
    char pad_0x32[0x2];            // Offset: 0x32 | Size: 0x2
    struct FVector Momentum;       // Offset: 0x34 | Size: 0xc
    bool bPenetrateArmor;          // Offset: 0x40 | Size: 0x1
    char DeathAnimationState;      // Offset: 0x41 | Size: 0x1
    char HitGroup;                 // Offset: 0x42 | Size: 0x1
    char DamageTypeEnum;           // Offset: 0x43 | Size: 0x1
    char pad_0x44[0x4];            // Offset: 0x44 | Size: 0x4
};

struct USGCharacterDeathComponent : USGActorDeathComponent {
    // Fields
    char pad_0x100[0x18];                             // Offset: 0x100 | Size: 0x18
    struct ACharacter *CachedKillerCharacter;         // Offset: 0x118 | Size: 0x8
    bool bGMCanDie;                                   // Offset: 0x120 | Size: 0x1
    char pad_0x121[0x3];                              // Offset: 0x121 | Size: 0x3
    struct FVector LastSavedLocation;                 // Offset: 0x124 | Size: 0xc
    struct FVector AdjustBoxExtend;                   // Offset: 0x130 | Size: 0xc
    float ReturnToLobbyTime;                          // Offset: 0x13c | Size: 0x4
    bool bKeepCorpse;                                 // Offset: 0x140 | Size: 0x1
    char pad_0x141[0x3];                              // Offset: 0x141 | Size: 0x3
    float DeathCountInterval;                         // Offset: 0x144 | Size: 0x4
    char DogTagClassMap[0x50];                        // Offset: 0x148 | Size: 0x50
    int32_t DogTagType;                               // Offset: 0x198 | Size: 0x4
    bool bDogTagEnable;                               // Offset: 0x19c | Size: 0x1
    char pad_0x19D[0x3];                              // Offset: 0x19d | Size: 0x3
    float CorpseDestroyTime;                          // Offset: 0x1a0 | Size: 0x4
    float DelaySpawnDeathBoxTime;                     // Offset: 0x1a4 | Size: 0x4
    struct AActor *DeathBoxClass;                     // Offset: 0x1a8 | Size: 0x8
    char OverrideDeathBoxClass[0x50];                 // Offset: 0x1b0 | Size: 0x50
    float IgnoreDeathEffectHiddenTime;                // Offset: 0x200 | Size: 0x4
    char pad_0x204[0x4];                              // Offset: 0x204 | Size: 0x4
    struct UParticleSystem *DeathEffect;              // Offset: 0x208 | Size: 0x8
    float DistanceNoDeathEffectIfNonRendered;         // Offset: 0x210 | Size: 0x4
    float FPAnimationPlayRate;                        // Offset: 0x214 | Size: 0x4
    float TPAnimationPlayRate;                        // Offset: 0x218 | Size: 0x4
    float DeathAnimationBlendIn;                      // Offset: 0x21c | Size: 0x4
    float AssistKillStatisTime;                       // Offset: 0x220 | Size: 0x4
    char pad_0x224[0x4];                              // Offset: 0x224 | Size: 0x4
    struct FCharacterDeathInfo DeathInfo;             // Offset: 0x228 | Size: 0x48
    struct FCharacterDeathInfo OldDeathInfo;          // Offset: 0x270 | Size: 0x48
    struct ASGInventory *CachedDeathBox;              // Offset: 0x2b8 | Size: 0x8
    int32_t CachedDeadNationalFlagIso;                // Offset: 0x2c0 | Size: 0x4
    char pad_0x2C4[0x4];                              // Offset: 0x2c4 | Size: 0x4
    char DeathCountTimerHandle[0x8];                  // Offset: 0x2c8 | Size: 0x8
    char DelayPlayDeathEffectTimerHandle[0x8];        // Offset: 0x2d0 | Size: 0x8
    char DisableCompReplicationHandle[0x8];           // Offset: 0x2d8 | Size: 0x8
    struct AController *CachedVictimPlayerController; // Offset: 0x2e0 | Size: 0x8
    struct UAnimMontage *CachedDeathAnimation;        // Offset: 0x2e8 | Size: 0x8
    bool bShouldSpawnDogtag;                          // Offset: 0x2f0 | Size: 0x1
    char pad_0x2F1[0x3f];                             // Offset: 0x2f1 | Size: 0x3f
    float MeshInterpDuration;                         // Offset: 0x330 | Size: 0x4
    char pad_0x334[0x4];                              // Offset: 0x334 | Size: 0x4
    char AgainstorData[0x180];                        // Offset: 0x338 | Size: 0x180
    char DBNOKillerData[0x180];                       // Offset: 0x4b8 | Size: 0x180
    char FinalKilledData[0x60];                       // Offset: 0x638 | Size: 0x60
    char DBNOKilledData[0x60];                        // Offset: 0x698 | Size: 0x60
    char SuicideWithoutCauserDeathTypes[0x10];        // Offset: 0x6f8 | Size: 0x10
    float ResetReplay1PSeconds;                       // Offset: 0x708 | Size: 0x4
    char pad_0x70C[0xd0];                             // Offset: 0x70c | Size: 0xd0
    char CachedPlayerStateBeforeDied[0x8];            // Offset: 0x7dc | Size: 0x8
    char pad_0x7E4[0x4];                              // Offset: 0x7e4 | Size: 0x4
    char CachedDeadCharacterInfo[0x48];               // Offset: 0x7e8 | Size: 0x48
    char pad_0x830[0x10];                             // Offset: 0x830 | Size: 0x10
    struct ASGInventory *DogTagInv;                   // Offset: 0x840 | Size: 0x8
    char pad_0x848[0x18];                             // Offset: 0x848 | Size: 0x18
    uint32_t DeathBoxSkinId;                          // Offset: 0x860 | Size: 0x4
    bool bAllowUseKillerDeathBoxSkin;                 // Offset: 0x864 | Size: 0x1
    char pad_0x865[0x3];                              // Offset: 0x865 | Size: 0x3
    uint32_t DefaultDeathBoxSkinId;                   // Offset: 0x868 | Size: 0x4
    char pad_0x86C[0x4];                              // Offset: 0x86c | Size: 0x4
};