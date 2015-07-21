// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
// Please see LICENSE.md in repository root for license information
// https://github.com/AtomicGameEngine/AtomicGameEngine

#include <Atomic/Core/StringUtils.h>
#include <Atomic/Engine/Engine.h>
#include <Atomic/IO/FileSystem.h>
#include <Atomic/Input/Input.h>
#include <Atomic/Resource/ResourceCache.h>
#include <Atomic/Graphics/Graphics.h>
#include <Atomic/Atomic3D/AnimatedModel.h>

#include <Atomic/UI/UI.h>

#include <AtomicJS/Javascript/Javascript.h>

#include <ToolCore/ToolSystem.h>
#include <ToolCore/ToolEnvironment.h>
#include <ToolCore/License/LicenseEvents.h>
#include <ToolCore/License/LicenseSystem.h>

#include "AEEditorApp.h"
#include "AEPreferences.h"

// Move me
#include <Atomic/Environment/Environment.h>

using namespace ToolCore;

namespace ToolCore
{
    extern void jsapi_init_toolcore(JSVM* vm);
}

namespace AtomicEditor
{

extern void jsapi_init_editor(JSVM* vm);

AEEditorApp::AEEditorApp(Context* context) :
    Application(context)
{

}

void AEEditorApp::Start()
{

    // Do not create bone structure by default when in the editor
    // this can be toggled temporarily, for example to setup an animation preview
    AnimatedModel::SetBoneCreationEnabled(false);

    context_->SetEditorContent(true);

    Input* input = GetSubsystem<Input>();
    input->SetMouseVisible(true);

    // move UI initialization to JS
    UI* ui = GetSubsystem<UI>();
    ui->Initialize("AtomicEditor/resources/language/lng_en.tb.txt");
    ui->LoadSkin("AtomicEditor/resources/default_skin/skin.tb.txt", "AtomicEditor/editor/skin/skin.tb.txt");
    ui->AddFont("AtomicEditor/resources/vera.ttf", "Vera");
    ui->AddFont("AtomicEditor/resources/MesloLGS-Regular.ttf", "Monaco");
    ui->SetDefaultFont("Vera", 12);

    Javascript* javascript = new Javascript(context_);
    context_->RegisterSubsystem(javascript);

    SubscribeToEvent(E_JSERROR, HANDLER(AEEditorApp, HandleJSError));
    SubscribeToEvent(E_EXITREQUESTED, HANDLER(AEEditorApp, HandleExitRequested));

    // Instantiate and register the Javascript subsystem
    vm_ = javascript->InstantiateVM("MainVM");
    vm_->InitJSContext();
    vm_->SetModuleSearchPaths("AtomicEditor");

    jsapi_init_toolcore(vm_);
    jsapi_init_editor(vm_);

    SharedPtr<File> file (GetSubsystem<ResourceCache>()->GetFile("AtomicEditor/main.js"));

    if (file.Null())
    {
        ErrorExit("Unable to load AtomicEditor/main.js");
        return;
    }

    if (!vm_->ExecuteFile(file))
    {
        ErrorExit("Error executing AtomicEditor/main.js");
        return;
    }

    GetSubsystem<LicenseSystem>()->Initialize();

}

void AEEditorApp::Setup()
{
    RegisterEnvironmentLibrary(context_);

    context_->RegisterSubsystem(new AEPreferences(context_));

    FileSystem* filesystem = GetSubsystem<FileSystem>();
    ToolEnvironment* env = new ToolEnvironment(context_);
    context_->RegisterSubsystem(env);

    ToolSystem* system = new ToolSystem(context_);
    context_->RegisterSubsystem(system);

#ifdef ATOMIC_DEV_BUILD

    if (!env->InitFromJSON())
    {
        ErrorExit(ToString("Unable to initialize tool environment from %s", env->GetDevConfigFilename().CString()));
        return;
    }

#endif

    // env->Dump();

    engineParameters_["WindowTitle"] = "AtomicEditor";
    engineParameters_["WindowResizable"] = true;
    engineParameters_["FullScreen"] = false;
    engineParameters_["LogName"] = filesystem->GetAppPreferencesDir("AtomicEditor", "Logs") + "AtomicEditor.log";
    engineParameters_["LogLevel"] = LOG_DEBUG;

#ifdef ATOMIC_PLATFORM_OSX
    engineParameters_["WindowIcon"] = "Images/AtomicLogo32.png";
#endif

#ifdef ATOMIC_DEV_BUILD
    engineParameters_["ResourcePrefixPath"] = "";
    String ScriptPath = env->GetRootSourceDir() + "Script";
    String resourcePaths = env->GetCoreDataDir() + ";" +  env->GetEditorDataDir() + ";" + ScriptPath;
    engineParameters_["ResourcePaths"] = resourcePaths;
#else

    #error ATOMIC_DEV_BUILD not defined

#endif // ATOMIC_DEV_BUILD


}

void AEEditorApp::Stop()
{
    vm_ = 0;
    context_->RemoveSubsystem<Javascript>();
    // make sure JSVM is really down and no outstanding refs
    // as if not, will hold on engine subsystems, which is bad
    assert(!JSVM::GetJSVM(0));
}

void AEEditorApp::HandleExitRequested(StringHash eventType, VariantMap& eventData)
{
}

void AEEditorApp::HandleJSError(StringHash eventType, VariantMap& eventData)
{
    using namespace JSError;
    //String errName = eventData[P_ERRORNAME].GetString();
    String errMessage = eventData[P_ERRORMESSAGE].GetString();
    String errFilename = eventData[P_ERRORFILENAME].GetString();
    //String errStack = eventData[P_ERRORSTACK].GetString();
    int errLineNumber = eventData[P_ERRORLINENUMBER].GetInt();

    String errorString = ToString("%s - %s - Line: %i",
                                  errFilename.CString(), errMessage.CString(), errLineNumber);

    ErrorExit(errorString);

}




}
