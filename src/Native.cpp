#include <xorstr.hpp>

#define LOG_TAG xorstr_("mono")

#include <And64InlineHook.hpp>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <Native.hpp>
#include <android/log_macros.h>
#include <android/native_window_jni.h>
#include <android/surface_texture_jni.h>
#include <android_native_app_glue.h>
#include <dlfcn.h>
#include <imgui_impl_android.h>
#include <imgui_impl_opengl3.h>
#include <math.h>

uint64_t Base = 0;
UWorld **GWorldPtr = nullptr;
FNamePool *NamePool = nullptr;
FUObjectArray *GUObjectArray = nullptr;
struct android_app *g_App = nullptr;

std::string CharacterType(ECharacterType id) {
    if (id == ECharacterType::ECharacterType_None)
        return "[None]";
    if (id == ECharacterType::ECharacterType_PMC)
        return "[PMC]";
    if (id == ECharacterType::ECharacterType_SCAV)
        return "[SCAV]";
    if (id == ECharacterType::ECharacterType_AI_SCAV)
        return "[AI_SCAV]";
    if (id == ECharacterType::ECharacterType_AI_SCAV_BOSS)
        return "[AI_SCAV_BOSS]";
    if (id == ECharacterType::ECharacterType_AI_PMC)
        return "[AI_PMC]";
    if (id == ECharacterType::ECharacterType_AI_ELIT)
        return "[AI_ELIT]";
    if (id == ECharacterType::ECharacterType_BOSS)
        return "[BOSS]";
    if (id == ECharacterType::ECharacterType_AI_SCAV_Follower)
        return "[AI_SCAV_Follower]";
    if (id == ECharacterType::ECharacterType_AI_Animal)
        return "[AI_Animal]";
    if (id == ECharacterType::ECharacterType_MAX)
        return "[MAX]";
    return "[None]";
}

std::string AIMainType(EAIMainType id) {
    if (id == EAIMainType::EAIMainType_Scav)
        return "[SCAV]";
    if (id == EAIMainType::EAIMainType_Boss)
        return "[BOSS]";
    if (id == EAIMainType::EAIMainType_Follower)
        return "[Follower]";
    if (id == EAIMainType::EAIMainType_PMCAI)
        return "[PMCAI]";
    if (id == EAIMainType::EAIMainType_TeachingAI)
        return "[TeachingAI]";
    if (id == EAIMainType::EAIMainType_TestAI)
        return "[TestAI]";
    if (id == EAIMainType::EAIMainType_Elit)
        return "[Elit]";
    if (id == EAIMainType::EAIMainType_Elit_PlayerScavAI)
        return "[Elit_PlayerScavAI]";
    if (id == EAIMainType::EAIMainType_MAX)
        return "[MAX]";
    return "[None]";
}

std::string FactionType(EFactionType id) {
    if (id == EFactionType::None)
        return "[None]";
    if (id == EFactionType::NormalPMC)
        return "[NormalPMC]";
    if (id == EFactionType::NormalScav)
        return "[NormalScav]";
    if (id == EFactionType::PlayerScav)
        return "[PlayerScav]";
    if (id == EFactionType::RebelFaction)
        return "[RebelFaction]";
    if (id == EFactionType::LakeFaction)
        return "[LakeFaction]";
    if (id == EFactionType::GangsterFaction)
        return "[GangsterFaction]";
    if (id == EFactionType::KurtTeam)
        return "[KurtTeam]";
    if (id == EFactionType::NavyFaction)
        return "[NavyFaction]";
    if (id == EFactionType::Blackgold)
        return "[Blackgold]";
    if (id == EFactionType::Gnesk)
        return "[Gnesk]";
    if (id == EFactionType::MadDog)
        return "[MadDog]";
    if (id == EFactionType::EFactionType_MAX)
        return "[MAX]";
    return "[None]";
}

EGLint glWidth = 0;
EGLint glHeight = 0;

bool g_Initialized = false;
void Init() {
    if (!Base)
        return;

    GWorldPtr = (UWorld **)(Base + Core::Offset::GWorld);
    NamePool = (FNamePool *)(Base + Core::Offset::GNames);
    GUObjectArray = (FUObjectArray *)(Base + Core::Offset::GObjects);
    g_App = *(struct android_app **)(Base + Core::Offset::GNativeAndroidApp);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplAndroid_Init(g_App->window);
    ImGui_ImplOpenGL3_Init(xorstr_("#version 300 es"));

    g_Initialized = true;
}

typedef struct _D3DMATRIX {
    union {
        struct {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };
} D3DMATRIX;

D3DMATRIX Matrix(FRotator rot, FVector origin = FVector(0, 0, 0)) {
    float radPitch = (rot.Pitch * float(M_PI) / 180.f);
    float radYaw = (rot.Yaw * float(M_PI) / 180.f);
    float radRoll = (rot.Roll * float(M_PI) / 180.f);

    float SP = sinf(radPitch);
    float CP = cosf(radPitch);
    float SY = sinf(radYaw);
    float CY = cosf(radYaw);
    float SR = sinf(radRoll);
    float CR = cosf(radRoll);

    D3DMATRIX matrix;
    matrix.m[0][0] = CP * CY;
    matrix.m[0][1] = CP * SY;
    matrix.m[0][2] = SP;
    matrix.m[0][3] = 0.f;
    matrix.m[1][0] = SR * SP * CY - CR * SY;
    matrix.m[1][1] = SR * SP * SY + CR * CY;
    matrix.m[1][2] = -SR * CP;
    matrix.m[1][3] = 0.f;
    matrix.m[2][0] = -(CR * SP * CY + SR * SY);
    matrix.m[2][1] = CY * SR - CR * SP * SY;
    matrix.m[2][2] = CR * CP;
    matrix.m[2][3] = 0.f;
    matrix.m[3][0] = origin.X;
    matrix.m[3][1] = origin.Y;
    matrix.m[3][2] = origin.Z;
    matrix.m[3][3] = 1.f;

    return matrix;
}

FCameraCacheEntry CameraCache;
FVector2D WorldToScreen(const FVector &WorldLocation) {
    FVector2D Screenlocation = FVector2D(0, 0);
    FRotator Rotation = CameraCache.POV.Rotation;
    D3DMATRIX tempMatrix = Matrix(Rotation);
    FVector vAxisX, vAxisY, vAxisZ;

    vAxisX = FVector(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
    vAxisY = FVector(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
    vAxisZ = FVector(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

    FVector vDelta = WorldLocation - CameraCache.POV.Location;
    FVector vTransformed = FVector(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

    if (vTransformed.Z < 1.f)
        vTransformed.Z = 1.f;

    float FovAngle = CameraCache.POV.FOV;

    float ScreenCenterX = glWidth / 2.0f;
    float ScreenCenterY = glHeight / 2.0f;

    Screenlocation.X = ScreenCenterX + vTransformed.X * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.Z;
    Screenlocation.Y = ScreenCenterY - vTransformed.Y * (ScreenCenterX / tanf(FovAngle * (float)M_PI / 360.f)) / vTransformed.Z;

    if (Screenlocation.X >= 0 && Screenlocation.X <= glWidth && Screenlocation.Y >= 0 && Screenlocation.Y <= glHeight) {
        return Screenlocation;
    } else {
        return FVector2D(0, 0);
    }
}

void ShitStuff() {
    ImGuiIO &io = ImGui::GetIO();

    UWorld *GWorld = *GWorldPtr;
    if (!GWorld)
        return;

    auto GameInstance = GWorld->OwningGameInstance;
    if (!GameInstance)
        return;

    auto LocalPlayer = GameInstance->LocalPlayers.Data[0];
    if (!LocalPlayer)
        return;

    auto PlayerController = LocalPlayer->PlayerController;
    if (!PlayerController)
        return;

    auto CameraController = PlayerController->PlayerCameraManager;
    if (!CameraController)
        return;

    CameraCache = CameraController->CameraCachePrivate;

    auto LocalPawn = PlayerController->AcknowledgedPawn;
    if (!LocalPawn)
        return;

    auto LocalRoot = LocalPawn->RootComponent;
    if (!LocalRoot)
        return;

    for (int i = 0; i < GWorld->Levels.Count; i++) {
        auto Level = GWorld->Levels.Data[i];
        if (!Level)
            continue;

        for (int j = 0; j < Level->Actors.Count; j++) {
            auto Actor = Level->Actors.Data[j];
            if (!Actor || LocalPawn == Actor)
                continue;

            auto objName = Actor->GetName();
            if (objName.empty())
                continue;

            auto ActorRoot = Actor->RootComponent;
            if (!ActorRoot)
                continue;

            auto ActorLocation = ActorRoot->RelativeLocation;
            if (!ActorLocation.IsValid())
                continue;

            int MeToActor = LocalRoot->RelativeLocation.Distance(ActorLocation) / 100;
            if (MeToActor >= 100)
                continue;

            if (!objName.find("BP_UamAICharacter_C")) {
                auto AI = (ASGAICharacter *)Actor;
                if (!AI->DeathComponent || AI->DeathComponent->DeathInfo.bIsDead)
                    continue;

                auto ScreenPos = WorldToScreen(ActorLocation);
                if (ScreenPos.X == 0 && ScreenPos.Y == 0)
                    continue;

                std::string txtMainType = "[None]";
                std::string txtDistance = "[" + std::to_string(MeToActor) + "m]";
                if (AI->AIStaticsComponent)
                    txtMainType = AIMainType(AI->AIStaticsComponent->AIMainType);

                ImGui::GetBackgroundDrawList()->AddLine({io.DisplaySize.x / 2, 0}, {ScreenPos.X, ScreenPos.Y}, ImColor{Colors::GREEN});
                ImGui::GetBackgroundDrawList()->AddText({ScreenPos.X, ScreenPos.Y + 16}, ImColor{Colors::GREEN}, txtMainType.c_str());
                ImGui::GetBackgroundDrawList()->AddText({ScreenPos.X, ScreenPos.Y + 32}, ImColor{Colors::GREEN}, txtDistance.c_str());
            }

            if (!objName.find("BP_UamCharacter_C")) {
                auto Human = (ASGCharacter *)Actor;
                if (!Human->DeathComponent || Human->DeathComponent->DeathInfo.bIsDead)
                    continue;

                auto ScreenPos = WorldToScreen(ActorLocation);
                if (ScreenPos.X == 0 && ScreenPos.Y == 0)
                    continue;

                std::string txtHumanName = "[None]";
                std::string txtDistance = "[" + std::to_string(MeToActor) + "m]";
                if (Human->PlayerState)
                    txtHumanName = "[" + Human->PlayerState->PlayerNamePrivate.ToString() + "]";

                ImGui::GetBackgroundDrawList()->AddLine({io.DisplaySize.x / 2, 0}, {ScreenPos.X, ScreenPos.Y}, ImColor{Colors::GREEN});
                ImGui::GetBackgroundDrawList()->AddText({ScreenPos.X, ScreenPos.Y + 16}, ImColor{Colors::GREEN}, txtHumanName.c_str());
                ImGui::GetBackgroundDrawList()->AddText({ScreenPos.X, ScreenPos.Y + 32}, ImColor{Colors::GREEN}, txtDistance.c_str());
            }
        }
    }
}

void MainLoopStep() {
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();

    ImGuiIO &io = ImGui::GetIO();
    io.DisplayFramebufferScale = {glWidth / io.DisplaySize.x, glHeight / io.DisplaySize.y};
    ImGui::NewFrame();

    // Do some shits here.
    ShitStuff();

    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

double lastLoop = 0;
EGLBoolean (*o_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface) = nullptr;
EGLBoolean h_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

    if (!g_Initialized)
        Init();
    else if (Core::GetTickCount() > lastLoop + 33.333333333) {
        MainLoopStep();
        lastLoop = Core::GetTickCount();
    }

    return o_eglSwapBuffers(dpy, surface);
}

bool wrapped = false;
bool foundBase = false;
void *(*o_dlsym)(void *handle, const char *symbol) = nullptr;
void *h_dlsym(void *handle, const char *symbol) {
    if (!foundBase && strcmp(symbol, xorstr_("ANativeActivity_onCreate")) == 0) {
        void *onCreate = o_dlsym(handle, xorstr_("ANativeActivity_onCreate"));
        Base = (uint64_t)onCreate - Core::Offset::ANativeActivity_onCreate;
        foundBase = true;
        return onCreate;
    }

    if (!wrapped && strcmp(symbol, xorstr_("glGetStringi")) == 0) {
        void *eglSwapBuffers = o_dlsym(handle, xorstr_("eglSwapBuffers"));
        A64HookFunction(eglSwapBuffers, (void *)&h_eglSwapBuffers, (void **)&o_eglSwapBuffers);
        wrapped = true;
    }

    return o_dlsym(handle, symbol);
}

__attribute__((constructor)) void OnLoad() {
    A64HookFunction((void *)&dlsym, (void *)&h_dlsym, (void **)&o_dlsym);
}

std::string FNameEntry::ToString() {
    if (Header.bIsWide) {
        return {};
    }
    return {AnsiName, Header.Len};
}

FNameEntry *FName::GetDisplayNameEntry() const {
    return &NamePool->Resolve(ComparisonIndex);
}

std::string FName::GetName() {
    auto entry = GetDisplayNameEntry();
    auto name = entry->ToString();
    if (Number > 0) {
        name += '_' + std::to_string(Number);
    }
    auto pos = name.rfind('/');
    if (pos != std::string::npos) {
        name = name.substr(pos + 1);
    }
    return name;
}

std::string UObject::GetName() {
    return NamePrivate.GetName();
}

std::string UObject::GetFullName() {
    std::string name;
    for (auto outer = OuterPrivate; outer; outer = outer->OuterPrivate) {
        name = outer->GetName() + "." + name;
    }
    name = ClassPrivate->GetName() + " " + name + GetName();
    return name;
}

FVector FVector::operator/(const FVector &v) const {
    FVector result(this->X / v.X, this->Y / v.Y, this->Z / v.Z);
    return result;
}

FVector FVector::operator/(float mod) const {
    FVector result(this->X / mod, this->Y / mod, this->Z / mod);
    return result;
}

FVector &FVector::operator+=(const FVector &v) {
    this->X += v.X;
    this->Y += v.Y;
    this->Z += v.Z;
    return *this;
}

FVector &FVector::operator+=(float fl) {
    this->X += fl;
    this->Y += fl;
    this->Z += fl;
    return *this;
}

FVector &FVector::operator-=(const FVector &v) {
    this->X -= v.X;
    this->Y -= v.Y;
    this->Z -= v.Z;
    return *this;
}

FVector &FVector::operator-=(float fl) {
    this->X -= fl;
    this->Y -= fl;
    this->Z -= fl;
    return *this;
}

float FVector::Distance(const FVector &to) const {
    return sqrtf(powf(to.X - X, 2) + powf(to.Y - Y, 2) + powf(to.Z - Z, 2));
}

bool FVector::IsValid() const {
    return this->X != 0 && this->Y != 0;
}

FVector FVector::operator-(const FVector &v) const {
    FVector result(this->X - v.X, this->Y - v.Y, this->Z - v.Z);
    return result;
}

float FVector::Dot(FVector const &other) const {
    return this->X * other.X + this->Y * other.Y + this->Z * other.Z;
}
