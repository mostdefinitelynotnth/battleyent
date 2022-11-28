#include <Windows.h>
#include <iostream>

struct MonoDomain;
struct MonoThread;
struct MonoImage;
struct MonoClass;
struct MonoMethod;

MonoDomain* mono_get_root_domain_prot();
static decltype(&mono_get_root_domain_prot) mono_get_root_domain = nullptr;

MonoThread* mono_thread_attach_prot(MonoDomain* domain);
static decltype(&mono_thread_attach_prot) mono_thread_attach = nullptr;

MonoImage* mono_image_loaded_prot(const char* name);
static decltype(&mono_image_loaded_prot) mono_image_loaded = nullptr;

MonoClass* mono_class_from_name_prot(MonoImage* image, const char* name_space, const char* name);
static decltype(&mono_class_from_name_prot) mono_class_from_name = nullptr;

MonoMethod* mono_class_get_methods_prot(MonoClass* klass, void** iter);
static decltype(&mono_class_get_methods_prot) mono_class_get_methods = nullptr;

char* mono_method_get_name_prot(MonoMethod* method);
static decltype(&mono_method_get_name_prot) mono_method_get_name = nullptr;

void* mono_compile_method_prot(MonoMethod* method);
static decltype(&mono_compile_method_prot) mono_compile_method = nullptr;

MonoMethod* find_be_init(MonoClass* main_application)
{
    void* iter = nullptr;
    MonoMethod* method;
    while (method = mono_class_get_methods(main_application, &iter))
    {
        auto name = mono_method_get_name(method);
        if ((unsigned(name[0]) & 0xFF) == 0xEE && (unsigned(name[1]) & 0xFF) == 0x80 && (unsigned(name[2]) & 0xFF) == 0x81) // UTF-8 for \uE001
            return method;
    }

    return nullptr;
}

MonoMethod* find_show_error_screen(MonoClass* preloader_ui)
{
    void* iter = nullptr;
    MonoMethod* method;
    while (method = mono_class_get_methods(preloader_ui, &iter))
    {
        auto name = mono_method_get_name(method);

        if (strcmp(name, "ShowErrorScreen") == 0)
            return method;
    }

    return nullptr;
}

void patch_method(MonoMethod* method)
{
    auto method_compiled = mono_compile_method(method);
    if (method_compiled == nullptr)
    {
        printf("- can't compile method!\n");
        return;
    }
    printf("- method compiled, ptr: 0x%p\n", method_compiled);

    *(unsigned char*)(method_compiled) = 0xC3; // ret
    printf("- method patched\n");
}

void start()
{
    AllocConsole();
    freopen("CONOUT$", "w", stdout);

    HMODULE mono = nullptr;
    while (mono == nullptr)
    {
        mono = GetModuleHandleA("mono-2.0-bdwgc.dll");
        Sleep(100);
    }
    printf("- mono-2.0-bdwgc.dll found\n");

    // mono functions
    mono_get_root_domain = decltype(&mono_get_root_domain_prot)(GetProcAddress(mono, "mono_get_root_domain"));
    mono_thread_attach = decltype(&mono_thread_attach_prot)(GetProcAddress(mono, "mono_thread_attach"));
    mono_image_loaded = decltype(&mono_image_loaded_prot)(GetProcAddress(mono, "mono_image_loaded"));
    mono_class_from_name = decltype(&mono_class_from_name_prot)(GetProcAddress(mono, "mono_class_from_name"));
    mono_class_get_methods = decltype(&mono_class_get_methods_prot)(GetProcAddress(mono, "mono_class_get_methods"));
    mono_method_get_name = decltype(&mono_method_get_name_prot)(GetProcAddress(mono, "mono_method_get_name"));
    mono_compile_method = decltype(&mono_compile_method_prot)(GetProcAddress(mono, "mono_compile_method"));

    // find image
    MonoImage* image = nullptr;
    while (image == nullptr)
    {
        image = mono_image_loaded("Assembly-CSharp");
        Sleep(500);
    }
    printf("- Assembly-CSharp found, MonoImage: 0x%p\n", image);

    auto domain = mono_get_root_domain();
    printf("- gotten root domain, MonoDomain: %p\n", domain);
    auto thread = mono_thread_attach(domain);
    printf("- attached to thread, MonoThread: %p\n", thread);

    printf("\n- patching battleye init method\n");

    auto main_application = mono_class_from_name(image, "EFT", "TarkovApplication");
    if (main_application == nullptr)
    {
        printf("- can't find EFT.MainApplcation class!\n");
        return;
    }
    printf("- EFT.MainApplication found, MonoClass: 0x%p\n", main_application);

    auto battleye_init_method = find_be_init(main_application);
    if (battleye_init_method == nullptr)
    {
        printf("- can't find battleye initialization method!\n");
        return;
    }
    printf("- battleye initialization method found, MonoMethod: 0x%p\n", battleye_init_method);

    patch_method(battleye_init_method);

    printf("\n- patching error screen method\n");

    auto preloader_ui = mono_class_from_name(image, "EFT.UI", "PreloaderUI");
    if (preloader_ui == nullptr)
    {
        printf("- can't find EFT.UI.PreloaderUI class!\n");
        return;
    }
    printf("- EFT.UI.PreloaderUI found, MonoClass: 0x%p\n", preloader_ui);

    auto show_error_screen = find_show_error_screen(preloader_ui);
    if (show_error_screen == nullptr)
    {
        printf("- can't find ShowErrorScreen!\n");
        return;
    }
    printf("- ShowErrorScreen found, MonoMethod: 0x%p\n", show_error_screen);

    patch_method(show_error_screen);

    printf("\n- all done!\n");
}

BOOL APIENTRY DllMain( HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(nullptr, 0, LPTHREAD_START_ROUTINE(start), nullptr, 0, nullptr);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

