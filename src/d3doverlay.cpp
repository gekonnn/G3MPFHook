#include "pch.h"
#include "resource.h"

#include <cmath>

#define message_x 16
#define message_y 16
#define message_padding 8
#define message_font "Consolas"
#define message_height 20
#define message_bg_alpha 150

#define alt_message_x 6
#define alt_message_y 6
#define alt_message_font "Arial"
#define alt_message_height 14

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define enable_demo 0

D3DOverlay::ScreenMessage D3DOverlay::message;
D3DOverlay::ScreenMessage D3DOverlay::dbg_msg;

namespace
{
    bool im_overlay_enabled = false;

    bool im_initialized = false;
    ImFont* im_font_body = NULL;

    CreateDeviceEx_t oCreateDeviceEx = NULL;
    Present_t oPresent = NULL;
    WNDPROC oWndProc = NULL;

    enum class StartupLogoPhase
    {
        Waiting,
        Visible,
        FadePending,
        Fading,
        Complete
    };

    struct StartupLogo
    {
        HMODULE module = NULL;
        LPDIRECT3DDEVICE9 device = NULL;
        LPDIRECT3DTEXTURE9 texture = NULL;
        LPD3DXSPRITE sprite = NULL;
        LPD3DXFONT font = NULL;
        StartupLogoPhase phase = StartupLogoPhase::Waiting;
        float pulsePhase = -1.5707963f;
        std::chrono::steady_clock::time_point fadeStart;
        std::string status;
    } startup_logo;

    bool game_input_blocked = false;
    bool cursor_confined = false;

    const gCQuest_PS* selected_quest;

    void ReleaseCursorConfinement()
    {
        if (!cursor_confined)
            return;

        ClipCursor(nullptr);
        cursor_confined = false;
    }

    void UpdateCursorConfinement()
    {
        eCApplication& application = eCApplication::GetInstance();
        HWND window = application.GetHandle();
        gCSession& session = gCSession::GetInstance();
        gCGUIManager* guiManager = session.GetGUIManager();

        const bool shouldConfine =
            window != nullptr &&
            GetForegroundWindow() == window &&
            !IsIconic(window) &&
            session.IsGameRunning() &&
            !session.IsPaused() &&
            guiManager != nullptr &&
            !guiManager->IsOpen() &&
            !im_overlay_enabled;

        if (!shouldConfine)
        {
            ReleaseCursorConfinement();
            return;
        }

        RECT clientRect;
        if (!GetClientRect(window, &clientRect))
        {
            ReleaseCursorConfinement();
            return;
        }

        SetLastError(ERROR_SUCCESS);
        const int mappedPixels = MapWindowPoints(
            window, nullptr, reinterpret_cast<POINT*>(&clientRect), 2);
        if (mappedPixels == 0 && GetLastError() != ERROR_SUCCESS)
        {
            ReleaseCursorConfinement();
            return;
        }

        if (ClipCursor(&clientRect))
        {
            cursor_confined = true;
            SetCursor(nullptr);
        }
    }

    void SetGameInputBlocked(bool blocked)
    {
        if (game_input_blocked == blocked)
            return;

        eCActionMapper::GetInstance().ClearOccuredEvents();
        game_input_blocked = blocked;
    }

    void UpdateGameInputCapture()
    {
        if (!im_initialized || !im_overlay_enabled)
        {
            SetGameInputBlocked(false);
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        SetGameInputBlocked(io.WantCaptureMouse || io.WantCaptureKeyboard);
    }

    struct ComInit
    {
        ComInit() { HRESULT _hr = CoInitialize(nullptr); }
        ~ComInit() { CoUninitialize(); }
    };

    struct GfxContextAdminAccess : eCGfxContextAdmin
    {
        static eCAPIDevice* GetAPIDevice() { return sGetAPIDevice(); }
    };

    bool CaptureEngineDevice()
    {
        if (startup_logo.device)
            return true;

        eCAPIDevice* apiDevice = GfxContextAdminAccess::GetAPIDevice();
        if (!apiDevice)
            return false;

        eCDX9Device* dx9Device = static_cast<eCDX9Device*>(apiDevice);
        if (!dx9Device->m_pDevice)
            return false;

        startup_logo.device = dx9Device->m_pDevice;
        startup_logo.device->AddRef();
        return true;
    }

    bool im_quest_filter(const char* filter, const gCQuest_PS* quest)
    {
        std::string slFltr = Utils::to_lower(filter);
        std::string slQuestName = Utils::to_lower(quest->GetName().GetText());
        return slQuestName.find(slFltr) != std::string::npos;
    }

    bool im_template_filter(const char* filter, const eCTemplateEntity* templ)
    {
        std::string slFltr = Utils::to_lower(filter);
        std::string slQuestName = Utils::to_lower(templ->GetName().GetText());
        return slQuestName.find(slFltr) != std::string::npos;
    }

    void im_quest_treenode(const gCQuest_PS* quest, const char* filter = "")
    {
        if (!im_quest_filter(filter, quest)) return;

        const char* status_str = "unknown";

        gEQuestStatus status = quest->GetStatus();
        switch (status)
        {
        case gEQuestStatus_Open: status_str = "Open"; break;
        case gEQuestStatus_Running: status_str = "Running"; break;
        case gEQuestStatus_Success: status_str = "Success"; break;
        case gEQuestStatus_Failed: status_str = "Failed"; break;
        case gEQuestStatus_Obsolete: status_str = "Obsolete"; break;
        case gEQuestStatus_Cancelled: status_str = "Cancelled"; break;
        case gEQuestStatus_Lost: status_str = "Lost"; break;
        case gEQuestStatus_Won: status_str = "Won"; break;
        }

        ImGui::TreeNodeEx(quest, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "[%s] %s", status_str, quest->GetName().GetText());
        if (ImGui::IsItemClicked())
            selected_quest = quest;
    }

    void im_vfs_recursive_file_listing(const char* path)
    {
        eCVirtualFileSystem& vfs = eCVirtualFileSystem::GetInstance();

        bTObjArray<bCString> files;
        bTObjArray<bCString> directories;

        vfs.FindFiles(path, files);
        vfs.FindDirectories(path, directories);

        GE_ARRAY_FOR_EACH(filename, files)
        {
            ImGui::TreeNodeEx(filename.GetText(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
        }

        GE_ARRAY_FOR_EACH(filename, directories)
        {
            if (ImGui::TreeNode(filename.GetText()))
            {
                std::string sub_path = std::string(path) + "/" + filename.GetText();
                im_vfs_recursive_file_listing(sub_path.c_str());
                ImGui::TreePop();
            }
        }
    }

    void im_vfs_mpf_btn_tooltip()
    {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("WARNING:\nmounting any pack files now might be ineffective for specific mountpoints");
    }

    const char* im_bool_to_str(bool v)
    {
        return v ? "true" : "false";
    }

    void InitImGuiOverlay(LPDIRECT3DDEVICE9 pDevice, HWND hwnd)
    {
        if (im_initialized) return;

        HWND window = eCApplication::GetInstance().GetHandle();

        //D3DOverlay::DrawMessage(5, "Press \"F1\" to G3MPFHook access menu");
        //msg_starttime += std::chrono::seconds(1); // delay it a bit

        D3DOverlay::ShowMessage(D3DOverlay::dbg_msg, 10, "G3MPFHook | press F1 for menu");
        D3DOverlay::dbg_msg.start += std::chrono::seconds(1);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        io.MouseDrawCursor = false;
        io.BackendFlags &= ~ImGuiBackendFlags_HasMouseCursors;

        im_font_body = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f, NULL, io.Fonts->GetGlyphRangesDefault());
        IM_ASSERT(im_font_body != NULL);

        ImGui_ImplWin32_Init(window);
        ImGui_ImplDX9_Init(pDevice);

        oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)D3DOverlay::hkWndProc);

        im_initialized = true;
    }

    void DrawImGuiOverlay()
    {
        if (!im_initialized) return;

        ImGuiIO& io = ImGui::GetIO();

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

#if enable_demo
        static bool demo_p_open = true;
        ImGui::ShowDemoWindow(&demo_p_open);
#endif

        ImGui::PushFont(im_font_body);

        ImGui::Begin("G3MPFHook", &im_overlay_enabled);

        io.MouseDrawCursor = (io.WantCaptureMouse || io.WantCaptureKeyboard);

        ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
        if (ImGui::BeginTabBar("MainTabBar", tab_bar_flags))
        {
            if (ImGui::BeginTabItem("Engine"))
            {
                ImGui::Text("FPS: %.0f", eCApplication::GetInstance().GetFPS());

                if (ImGui::CollapsingHeader("gCScriptAdmin"))
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                    ImGui::BeginChild("modules", ImVec2(0, 100), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY, ImGuiWindowFlags_MenuBar);

                    if (ImGui::BeginMenuBar())
                    {
                        ImGui::Text("Loaded modules");
                    }
                    ImGui::EndMenuBar();

                    gCScriptAdminExt& scriptAdmin = GetScriptAdminExt();
                    GE_ARRAY_FOR_EACH(pDll, scriptAdmin.GetLoadedDLLs())
                    {
                        ImGui::Text(pDll->m_strFileName.GetText());
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                }

                if (ImGui::CollapsingHeader("eCVirtualFileSystem"))
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                    ImGui::BeginChild("filesystem", ImVec2(0, 100), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY, ImGuiWindowFlags_MenuBar);

                    if (ImGui::Button("Mount pack file..."))
                    {
                        CHAR _filename[MAX_PATH];

                        OPENFILENAMEA ofn = { 0 };
                        ZeroMemory(&_filename, sizeof(_filename));
                        ZeroMemory(&ofn, sizeof(ofn));
                        ofn.lStructSize = sizeof(ofn);
                        ofn.hwndOwner = NULL;
                        ofn.lpstrFilter = "All Files\0*.*\0";
                        ofn.lpstrFile = _filename;
                        ofn.nMaxFile = MAX_PATH;
                        ofn.lpstrTitle = "Select a Gothic 3 archive file";
                        ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST;

                        if (GetOpenFileNameA(&ofn))
                        {
                            std::string path(_filename);
                            std::string filename = path.substr(path.find_last_of("\\/") + 1);

                            bool success = G3MPFHook::GetInstance()->MountPackFile(path);
                            D3DOverlay::ShowMessage(D3DOverlay::message, 5, success ? "Mounted %s successfully" : "Failed to mount %s", filename.c_str());
                        }
                    }
                    im_vfs_mpf_btn_tooltip();

                    ImGui::SameLine();

                    if (ImGui::Button("Mount directory..."))
                    {
                        ComInit com;

                        CComPtr<IFileOpenDialog> pFolderDlg;
                        HRESULT _hr = pFolderDlg.CoCreateInstance(CLSID_FileOpenDialog);

                        FILEOPENDIALOGOPTIONS opt{};
                        pFolderDlg->GetOptions(&opt);
                        pFolderDlg->SetOptions(opt | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);

                        if (SUCCEEDED(pFolderDlg->Show(nullptr)))
                        {
                            CComPtr<IShellItem> pSelectedItem;
                            pFolderDlg->GetResult(&pSelectedItem);

                            CComHeapPtr<wchar_t> pPath;
                            pSelectedItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath);

                            std::wstring ws(pPath.m_pData);

                            int size_needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, NULL, 0, NULL, NULL);
                            std::string str(size_needed, 0);
                            WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &str[0], size_needed, NULL, NULL);

                            if (!str.empty() && str.back() == '\0')
                                str.pop_back();

                            std::string folderName = str.substr(str.find_last_of("\\/") + 1);

                            bool success = G3MPFHook::GetInstance()->MountAllFilesInDirectory(str);
                            D3DOverlay::ShowMessage(D3DOverlay::message, 5, success ? "Mounted %s successfully" : "Failed to mount %s", folderName.c_str());
                        }
                    }
                    im_vfs_mpf_btn_tooltip();

                    if (ImGui::BeginMenuBar())
                    {
                        ImGui::Text("Data");
                    }
                    ImGui::EndMenuBar();

                    if (ImGui::TreeNode("Data"))
                    {
                        im_vfs_recursive_file_listing("Data");
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Scripts"))
                    {
                        im_vfs_recursive_file_listing("Scripts");
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Backup"))
                    {
                        im_vfs_recursive_file_listing("Backup");
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("SaveGame"))
                    {
                        im_vfs_recursive_file_listing("SaveGame");
                        ImGui::TreePop();
                    }
                    if (ImGui::TreeNode("Ini"))
                    {
                        im_vfs_recursive_file_listing("Ini");
                        ImGui::TreePop();
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Game"))
            {
                if (ImGui::CollapsingHeader("PSPlayerMemory"))
                {
                    eCDynamicEntity* eCDEPlayer = gCSession::GetInstance().GetPlayer();
                    ImGui::Text(":)");
                    //Entity* ePlayer = reinterpret_cast<Entity*>(eCDEPlayer);
                }
                if (ImGui::CollapsingHeader("gCSession"))
                {
                    gCSessionInfo sInfo = gCSession::GetInstance().GetSessionInfo();

                    ImGui::Text("Session name: %s", sInfo.GetName());
                    ImGui::Text("Hours played: %.1f", sInfo.GetNumHoursPlayed());
                    ImGui::Text("Has player cheated?: %s", im_bool_to_str(sInfo.GetPlayerHasCheated()));

                    const char* state_str = "unknown";

                    gESession_State status = gCSession::GetInstance().GetState();
                    switch (status)
                    {
                    case gESession_State_None: state_str = "None"; break;
                    case gESession_State_Movement: state_str = "Movement"; break;
                    case gESession_State_Fight: state_str = "Fight"; break;
                    case gESession_State_Ride_Movement: state_str = "Ride_Movement"; break;
                    case gESession_State_Ride_Fight: state_str = "Ride_Fight"; break;
                    case gESession_State_ItemUse: state_str = "ItemUse"; break;
                    case gESession_State_Inventory: state_str = "Inventory"; break;
                    case gESession_State_Dialog: state_str = "Dialog"; break;
                    case gESession_State_Trade: state_str = "Trade"; break;
                    case gESession_State_InteractObj: state_str = "InteractObj"; break;
                    case gESession_State_Journal: state_str = "Journal"; break;
                    case gESession_State_Editor: state_str = "Editor"; break;
                    }

                    ImGui::Text("State: %s", state_str);

                    if (ImGui::Button("Pause"))
                        gCSession::GetInstance().Pause();
                    ImGui::SameLine();
                    if (ImGui::Button("Resume"))
                        gCSession::GetInstance().Resume();

                    bool testmode = gCSession::GetInstance().IsInTestMode();
                    if (ImGui::Checkbox("Testmode", &testmode))
                    {
                        gCSession::GetInstance().SetTestMode(testmode);
                    }
                }

                if (ImGui::CollapsingHeader("gCQuestManager_PS"))
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                    ImGui::BeginChild("quests", ImVec2(0, 100), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY, ImGuiWindowFlags_MenuBar);

                    if (ImGui::BeginMenuBar())
                    {
                        ImGui::Text("Quests");
                    }
                    ImGui::EndMenuBar();

                    static char quest_filter_buf[256];
                    quest_filter_buf[IM_ARRAYSIZE(quest_filter_buf) - 1] = '\0';
                    ImGui::InputText("Filter", quest_filter_buf, IM_ARRAYSIZE(quest_filter_buf));

                    if (ImGui::BeginTabBar("QuestsTabBar", tab_bar_flags))
                    {
                        if (ImGui::BeginTabItem("Active quests"))
                        {
                            // trying to call gCQuestManager_PS::GetInstance() while
                            // in the main menu crashes the game
                            if (gCSession::GetInstance().IsGameRunning())
                            {
                                gCQuestManager_PS* questManager = gCQuestManager_PS::GetInstance();
                                auto& quests = questManager->GetQuests();

                                GE_ARRAY_FOR_EACH(quest, quests)
                                {
                                    const gCQuest_PS* pQuest = static_cast<const gCQuest_PS*>(quest);

                                    gEQuestStatus status = quest->GetStatus();
                                    if (status == gEQuestStatus::gEQuestStatus_Running)
                                        im_quest_treenode(pQuest, quest_filter_buf);
                                }
                            }
                            ImGui::EndTabItem();
                        }
                        if (ImGui::BeginTabItem("All quests"))
                        {
                            if (gCSession::GetInstance().IsGameRunning())
                            {
                                gCQuestManager_PS* questManager = gCQuestManager_PS::GetInstance();
                                auto& quests = questManager->GetQuests();

                                GE_ARRAY_FOR_EACH(quest, quests)
                                {
                                    const gCQuest_PS* pQuest = static_cast<const gCQuest_PS*>(quest);

                                    if (!im_quest_filter(quest_filter_buf, pQuest)) continue;

                                    im_quest_treenode(pQuest, quest_filter_buf);
                                }
                            }
                            ImGui::EndTabItem();
                        }

                        ImGui::EndTabBar();
                    }

                    ImGui::EndChild();

                    ImGui::BeginChild("selected_quest_props", ImVec2(0, 100), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY, ImGuiWindowFlags_MenuBar);

                    if (ImGui::BeginMenuBar())
                    {
                        ImGui::Text("Properties");
                    }
                    ImGui::EndMenuBar();

                    if (gCSession::GetInstance().IsGameRunning() && selected_quest)
                    {
                        gCQuestManager_PS* questManager = gCQuestManager_PS::GetInstance();

                        ImGui::Columns(2, "selected_quest_cols");
                        ImGui::Text("Name: %s", selected_quest->GetName().GetText());

                        const char* status_str = "unknown";

                        gEQuestStatus status = selected_quest->GetStatus();
                        switch (status)
                        {
                        case gEQuestStatus_Open: status_str = "Open"; break;
                        case gEQuestStatus_Running: status_str = "Running"; break;
                        case gEQuestStatus_Success: status_str = "Success"; break;
                        case gEQuestStatus_Failed: status_str = "Failed"; break;
                        case gEQuestStatus_Obsolete: status_str = "Obsolete"; break;
                        case gEQuestStatus_Cancelled: status_str = "Cancelled"; break;
                        case gEQuestStatus_Lost: status_str = "Lost"; break;
                        case gEQuestStatus_Won: status_str = "Won"; break;
                        }

                        ImGui::Text("Status: %s", status_str);

                        if (status == gEQuestStatus_Running)
                        {
                            if (ImGui::Button("Succeed quest"))
                                questManager->SucceedQuest(selected_quest->GetName());
                            if (ImGui::Button("Fail quest"))
                                questManager->FailQuest(selected_quest->GetName());
                        }
                        else if (status == gEQuestStatus_Open)
                        {
                            if (ImGui::Button("Start"))
                                questManager->RunQuest(selected_quest->GetName());
                        }

                        if (ImGui::Button("Remove quest"))
                            questManager->RemoveQuest(selected_quest->GetName());

                        ImGui::NextColumn();
                        ImGui::Text("Destination entity: %s", selected_quest->GetDestinationEntity());

                        ImGui::BeginChild("quest_delivery_ents", ImVec2(0, 32), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY, ImGuiWindowFlags_MenuBar);

                        if (ImGui::BeginMenuBar())
                        {
                            ImGui::Text("Delivery entities");
                        }
                        ImGui::EndMenuBar();

                        auto& deliveryEntities = selected_quest->GetDeliveryEntities();
                        if (deliveryEntities.GetCount() > 0)
                        {
                            for (int i = 0; i < deliveryEntities.GetCount(); i++)
                            {
                                bCString ent = deliveryEntities[i];
                                GEU32 amount = selected_quest->GetDeliveryAmounts()[i];
                                GEI32 counter = selected_quest->GetDeliveryCounter()[i];
                                ImGui::Text("%s: %d / %d", ent.GetText(), counter, amount);
                            }
                        }

                        ImGui::Columns();
                        ImGui::EndChild();
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Console"))
            {
                static std::vector<bCString> console_log = {};

                static bool AutoScroll = true;
                static bool ScrollToBottom = false;

                static char InputBuf[256];

                const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
                if (ImGui::BeginChild("console_region", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
                {
                    if (ImGui::BeginPopupContextWindow())
                    {
                        if (ImGui::Selectable("Clear")) console_log.clear();
                        ImGui::EndPopup();
                    }

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

                    for (const bCString& item : console_log)
                    {
                        ImGui::TextUnformatted(item.GetText());
                    }

                    if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                        ImGui::SetScrollHereY(1.0f);
                    ScrollToBottom = false;

                    ImGui::PopStyleVar();
                }

                ImGui::EndChild();

                ImGui::Separator();

                bool reclaim_focus = false;
                ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll;
                if (ImGui::InputText("Input", InputBuf, IM_ARRAYSIZE(InputBuf), input_text_flags))
                {
                    char* s = InputBuf;
                    bCString bCSCmd(s);
                    bCSCmd.Trim();
                    if (!bCSCmd.IsEmpty())
                    {
                        console_log.push_back("> " + bCSCmd);

                        bCString result = eCConsole::AccessConsole().Execute(bCSCmd);

                        if (!result.IsEmpty())
                            console_log.push_back(result);
                    }
                        
                    strcpy_s(InputBuf, sizeof(InputBuf), "");

                    reclaim_focus = true;
                }

                ImGui::SetItemDefaultFocus();
                if (reclaim_focus)
                    ImGui::SetKeyboardFocusHere(-1);

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Debug"))
            {
                Entity ePlayer = Entity::ms_Player;

                if (ImGui::CollapsingHeader("Player inventory"))
                {
                    gCEntity* gCEPlayer = ePlayer.GetGameEntity();
                    gCNPC_PS* playerNpcPS = nullptr;
                    gCInventory_PS* playerInventory = nullptr;

                    if (gCEPlayer)
                    {
                        playerNpcPS = reinterpret_cast<gCNPC_PS*>(gCEPlayer->GetPropertySet(eEPropertySetType_NPC));
                        playerInventory = reinterpret_cast<gCInventory_PS*>(gCEPlayer->GetPropertySet(eEPropertySetType_Inventory));
                    }

                    static eCTemplateEntity* selected_template = nullptr;

                    //if (playerNpcPS) {
                    //    static int player_lvl = playerNpcPS->GetLevel();
                    //    if (ImGui::InputInt("player lvl", &player_lvl, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    //        playerNpcPS->SetLevel(player_lvl);
                    //    }
                    //}

                    static char templ_filter_buf[256];
                    templ_filter_buf[IM_ARRAYSIZE(templ_filter_buf) - 1] = '\0';
                    ImGui::InputText("Filter", templ_filter_buf, IM_ARRAYSIZE(templ_filter_buf));

                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                    ImGui::BeginChild("itemslist", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY, ImGuiWindowFlags_MenuBar);

                    if (ImGui::BeginMenuBar())
                    {
                        ImGui::Text("Items");
                    }
                    ImGui::EndMenuBar();

                    eCSceneAdmin* sceneAdmin = reinterpret_cast<eCSceneAdmin*>(eCModuleAdmin::GetInstance().FindModule("eCSceneAdmin"));
                    for (auto it = sceneAdmin->TemplateEntitiesBegin(); it != sceneAdmin->TemplateEntitiesEnd(); ++it)
                    {
                        const bCPropertyID& key = it.GetKey();
                        eCTemplateEntity* value = *it;

                        if (gCItem_PS* ent_item = reinterpret_cast<gCItem_PS*>(value->GetPropertySet(eEPropertySetType_Item)))
                        {
                            if (templ_filter_buf[0] != '\0' && !im_template_filter(templ_filter_buf, value)) continue;

                            ImGui::TreeNodeEx(value, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%s", value->GetName().GetText());

                            if (ImGui::IsItemClicked())
                                selected_template = selected_template != value ? value : nullptr;
                        }
                    }

                    ImGui::EndChild();

                    ImGui::BeginChild("itemprops", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY, ImGuiWindowFlags_MenuBar);

                    if (ImGui::BeginMenuBar())
                    {
                        ImGui::Text("Properties");
                    }
                    ImGui::EndMenuBar();

                    if (selected_template)
                    {
                        gCItem_PS* item = reinterpret_cast<gCItem_PS*>(selected_template->GetPropertySet(eEPropertySetType_Item));

                        ImGui::Text("%s", selected_template->GetName().GetText());

                        static bool quality_s_Diseased = false;
                        static bool quality_s_Poisoned = false;
                        static bool quality_s_Burning = false;
                        static bool quality_s_Frozen = false;
                        static bool quality_s_Sharp = false;
                        static bool quality_s_Blessed = false;
                        static bool quality_s_Forged = false;
                        static bool quality_s_Worn = false;

                        ImGui::Checkbox("Diseased", &quality_s_Diseased);
                        ImGui::Checkbox("Poisoned", &quality_s_Poisoned);
                        ImGui::Checkbox("Burning", &quality_s_Burning);
                        ImGui::Checkbox("Frozen", &quality_s_Frozen);
                        ImGui::Checkbox("Sharp", &quality_s_Sharp);
                        ImGui::Checkbox("Blessed", &quality_s_Blessed);
                        ImGui::Checkbox("Forged", &quality_s_Forged);
                        ImGui::Checkbox("Worn", &quality_s_Worn);

                        gEItemQuality final_quality = (gEItemQuality)(
                            (quality_s_Diseased ? gEItemQuality_Diseased : 0) |
                            (quality_s_Poisoned ? gEItemQuality_Poisoned : 0) |
                            (quality_s_Burning ? gEItemQuality_Burning : 0) |
                            (quality_s_Frozen ? gEItemQuality_Frozen : 0) |
                            (quality_s_Sharp ? gEItemQuality_Sharp : 0) |
                            (quality_s_Blessed ? gEItemQuality_Blessed : 0) |
                            (quality_s_Forged ? gEItemQuality_Forged : 0) |
                            (quality_s_Worn ? gEItemQuality_Worn : 0));

                        static GEU32 amount = 1;

                        ImGui::InputScalar("Amount", ImGuiDataType_U32, &amount);
                        ImGui::SameLine();
                        if (ImGui::Button("Give"))
                        {


                            printf("%d\n", (GEInt)final_quality);

                            if (playerInventory)
                            {
                                // stack idx                                   template          gEItemQuality        amount
                                GEInt stack_idx = playerInventory->CreateItems(selected_template, (GEInt)final_quality, amount);
                                if (gCInventoryStack* stack = playerInventory->GetStack(stack_idx))
                                {
                                    // access to gCInventorySlot & gCInventoryStack! :)
                                    printf("%d\n", stack->GetAmount());
                                }
                            }
                        }
                    }

                    ImGui::EndChild();

                    ImGui::PopStyleVar();
                }


                if (ImGui::Button("Crash!"))
                {
                    bCErrorAdmin::GetInstance().CallFatalError("G3MPFHOOK", "A crash has been invoked.", "d3doverlay.cpp", 586);
                }                

                if (ImGui::BeginTabBar("debug_tabs"))
                {
                    if (ImGui::BeginTabItem("G3MMPINF"))
                    {
                        g3mmpinf pinf = G3MPFHook::GetInstance()->get_pinf();

                        ImGui::SeparatorText("G3MMPINF");
                        ImGui::Text("VERSION: %d", pinf.VERSION);
                        ImGui::Text("INFO.GAME_PATH: %s", pinf.INFO.GAME_PATH.DATA);
                        ImGui::Text("INFO.LAUNCHER_PATH: %s", pinf.INFO.LAUNCHER_PATH.DATA);
                        ImGui::Text("INFO.SHOW_CONSOLE: %d", pinf.INFO.SHOW_CONSOLE);
                        ImGui::Text("MOD_COUNT: %d", pinf.MOD_COUNT);

                        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                        ImGui::BeginChild("mods", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY, ImGuiWindowFlags_MenuBar);

                        if (ImGui::BeginMenuBar())
                        {
                            ImGui::Text("Mods");
                        }
                        ImGui::EndMenuBar();

                        for (int i = 0; i < pinf.MOD_COUNT; ++i) {
                            g3mmmod& mod = pinf.MOD_ARRAY[i];
                            ImGui::Text("%s", mod.NAME.DATA);
                        }

                        ImGui::EndChild();
                        ImGui::PopStyleVar();
                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("DetourManager"))
                    {
                        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
                        ImGui::BeginChild("regdetours", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY, ImGuiWindowFlags_MenuBar);

                        if (ImGui::BeginMenuBar())
                        {
                            ImGui::Text("Registered detours");
                        }
                        ImGui::EndMenuBar();

                        ImGuiTableFlags table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
                        if (ImGui::BeginTable("regdetours_table", 4, table_flags))
                        {
                            ImGui::TableSetupColumn("Module");
                            ImGui::TableSetupColumn("Function");
                            ImGui::TableSetupColumn("RVA");
                            ImGui::TableSetupColumn("orig_func", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableHeadersRow();

                            for (auto& detour : DetourManager::GetInstance().GetRegisteredDetours())
                            {
                                //ImGui::Text("%s::%s@0x%06x [0x%08x]", detour.module_name.c_str(), detour.func_name.c_str(), detour.rva, detour.orig_func);

                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text(detour.module_name.c_str());
                                ImGui::TableNextColumn();
                                ImGui::Text(detour.func_name.c_str());
                                ImGui::TableNextColumn();
                                ImGui::Text("0x%06x", detour.rva);
                                ImGui::TableNextColumn();
                                ImGui::Text("0x%08x", *detour.orig_func);
                            }
                            ImGui::EndTable();
                        }

                        ImGui::EndChild();
                        ImGui::PopStyleVar();
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::PopFont();

        ImGui::End();
        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

        UpdateGameInputCapture();
    }
}

namespace D3DOverlay
{
    bool IsCapturingGameInput()
    {
        return game_input_blocked;
    }

    void ReleaseStartupLogoResources()
    {
        if (startup_logo.font)
        {
            startup_logo.font->Release();
            startup_logo.font = NULL;
        }
        if (startup_logo.sprite)
        {
            startup_logo.sprite->Release();
            startup_logo.sprite = NULL;
        }
        if (startup_logo.texture)
        {
            startup_logo.texture->Release();
            startup_logo.texture = NULL;
        }
    }

    void CompleteStartupLogo()
    {
        ReleaseStartupLogoResources();
        if (startup_logo.device)
        {
            startup_logo.device->Release();
            startup_logo.device = NULL;
        }
        startup_logo.status.clear();
        startup_logo.phase = StartupLogoPhase::Complete;
    }

    bool EnsureStartupLogoResources()
    {
        if (!startup_logo.texture)
        {
            HRSRC resource = FindResourceW(startup_logo.module, MAKEINTRESOURCEW(IDR_G3MM_LOGO), RT_RCDATA);
            if (!resource)
                return false;

            HGLOBAL loadedResource = LoadResource(startup_logo.module, resource);
            const void* resourceData = loadedResource ? LockResource(loadedResource) : nullptr;
            DWORD resourceSize = SizeofResource(startup_logo.module, resource);
            if (!resourceData || !resourceSize || FAILED(D3DXCreateTextureFromFileInMemory(startup_logo.device, resourceData, resourceSize, &startup_logo.texture)))
                return false;
        }

        if (!startup_logo.sprite && FAILED(D3DXCreateSprite(startup_logo.device, &startup_logo.sprite)))
            return false;

        if (!startup_logo.font && FAILED(D3DXCreateFontA(startup_logo.device, 15, 0, FW_SEMIBOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI Semibold", &startup_logo.font)))
            return false;

        return true;
    }

    bool DrawStartupLogo(bool clearBackground = true, float alpha = 1.0f)
    {
        if (!EnsureStartupLogoResources())
            return false;

        D3DSURFACE_DESC backBufferDesc = {};
        LPDIRECT3DSURFACE9 backBuffer = nullptr;
        if (FAILED(startup_logo.device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
            return false;
        backBuffer->GetDesc(&backBufferDesc);
        backBuffer->Release();

        D3DSURFACE_DESC textureDesc = {};
        startup_logo.texture->GetLevelDesc(0, &textureDesc);

        LPDIRECT3DSTATEBLOCK9 state = nullptr;
        if (SUCCEEDED(startup_logo.device->CreateStateBlock(D3DSBT_ALL, &state)))
            state->Capture();

        HRESULT drawResult = startup_logo.device->BeginScene();
        if (SUCCEEDED(drawResult))
        {
            if (clearBackground)
                startup_logo.device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

            const float logoSize = 32.0f;
            const float padding = 20.0f;
            const float scale = std::min(logoSize / textureDesc.Width, logoSize / textureDesc.Height);
            D3DXVECTOR2 scaling(scale, scale);
            D3DXVECTOR2 translation(padding, backBufferDesc.Height - textureDesc.Height * scale - padding);
            D3DXMATRIX transform;
            D3DXMatrixTransformation2D(&transform, nullptr, 0.0f, &scaling, nullptr, 0.0f, &translation);
            startup_logo.sprite->SetTransform(&transform);
            drawResult = startup_logo.sprite->Begin(D3DXSPRITE_ALPHABLEND);
            if (SUCCEEDED(drawResult))
            {
                const bool fading = startup_logo.phase == StartupLogoPhase::Fading;
                const float brightness = fading ? 1.0f : 0.75f + 0.25f * std::sin(startup_logo.pulsePhase);
                const int color = static_cast<int>(255.0f * brightness);
                if (!fading)
                {
                    static auto lastPulseTime = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    const float dt = std::chrono::duration<float>(now - lastPulseTime).count();
                    lastPulseTime = now;

                    const float pulseSpeedRadPerSec = 2.5f;
                    startup_logo.pulsePhase += pulseSpeedRadPerSec * dt;
                    if (startup_logo.pulsePhase > 4.7123890f)
                        startup_logo.pulsePhase -= 6.2831855f;
                }
                drawResult = startup_logo.sprite->Draw(startup_logo.texture, nullptr, nullptr, nullptr, D3DCOLOR_ARGB(static_cast<int>(255.0f * alpha), color, color, color));
                startup_logo.sprite->End();

                if (!startup_logo.status.empty())
                {
                    const LONG textLeft = static_cast<LONG>(padding + textureDesc.Width * scale + 12.0f);
                    const LONG textCenter = static_cast<LONG>(translation.y + textureDesc.Height * scale * 0.5f);
                    RECT textRect{ textLeft, textCenter - 16, static_cast<LONG>(backBufferDesc.Width - padding), textCenter + 16 };
                    RECT shadowRect = textRect;
                    OffsetRect(&shadowRect, 1, 1);
                    //startup_logo.font->DrawTextA(nullptr, startup_logo.status.c_str(), -1, &shadowRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, D3DCOLOR_ARGB(static_cast<int>(180.0f * alpha), 0, 0, 0));
                    //startup_logo.font->DrawTextA(nullptr, startup_logo.status.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE, D3DCOLOR_ARGB(static_cast<int>(245.0f * alpha), 224, 220, 214));
                }
            }
            startup_logo.device->EndScene();
        }

        if (state)
        {
            state->Apply();
            state->Release();
        }
        if (FAILED(drawResult))
            return false;

        return true;
    }

    void SetStartupLogoStatus(const char* status)
    {
        startup_logo.status = status ? status : "";
        if (startup_logo.phase == StartupLogoPhase::Complete || startup_logo.phase == StartupLogoPhase::FadePending || startup_logo.phase == StartupLogoPhase::Fading || !CaptureEngineDevice() || !startup_logo.module)
            return;

        if (DrawStartupLogo())
        {
            HRESULT result = oPresent ? oPresent(startup_logo.device, nullptr, nullptr, nullptr, nullptr) : startup_logo.device->Present(nullptr, nullptr, nullptr, nullptr);
            if (SUCCEEDED(result))
                startup_logo.phase = StartupLogoPhase::Visible;
        }
    }

    void FadeOutStartupLogo()
    {
        if (startup_logo.phase == StartupLogoPhase::Visible)
            startup_logo.phase = StartupLogoPhase::FadePending;
        else if (startup_logo.phase == StartupLogoPhase::Waiting)
            CompleteStartupLogo();
    }

    DWORD WINAPI ThreadInit(PVOID pModule)
    {
        startup_logo.module = static_cast<HMODULE>(pModule);
        LPDIRECT3D9EX d3dEx = nullptr;
        if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &d3dEx)) || !d3dEx)
            return FALSE;

        void** d3dExVtable = *reinterpret_cast<void***>(d3dEx);
        oCreateDeviceEx = (CreateDeviceEx_t)d3dExVtable[20];

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)oCreateDeviceEx, hkCreateDeviceEx);
        DetourTransactionCommit();
        d3dEx->Release();

        dbg_msg.x = alt_message_x;
        dbg_msg.y = alt_message_y;
        dbg_msg.height = alt_message_height;
        dbg_msg.font = alt_message_font;
        dbg_msg.background = false;

        message.x = message_x;
        message.y = message_y;
        message.height = message_height;
        message.font = message_font;
        message.padding = message_padding;
        message.background = true;

        return TRUE;
    }

    void ShowMessage(ScreenMessage& m, float duration, const char* fmt, ...)
    {
        char buf[1024];

        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        m.text = buf;
        m.duration = duration;
        m.start = std::chrono::steady_clock::now();
        m.enabled = true;
    }

    void DrawScreenMessage(LPDIRECT3DDEVICE9 dev, ScreenMessage& m)
    {
        if (!m.enabled)
            return;

        if (!m.dxFont)
            D3DXCreateFontA(dev,m.height, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, m.font, &m.dxFont);

        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - m.start).count();

        if (elapsed >= m.duration)
        {
            m.enabled = false;

            if (m.dxFont)
            {
                m.dxFont->Release();
                m.dxFont = nullptr;
            }
            return;
        }

        const float fadeTime = 1.0f;

        float alpha = 1.0f;

        if (elapsed < fadeTime)
            alpha = elapsed / fadeTime;
        else
            alpha = (m.duration - elapsed) / fadeTime;

        alpha = std::clamp(alpha, 0.0f, 1.0f);

        int txAlpha = (int)(alpha * 255);

        D3DCOLOR textColor = D3DCOLOR_ARGB(txAlpha, 255, 255, 255);
        D3DCOLOR bgColor = D3DCOLOR_ARGB((int)(alpha * 180), 0, 0, 0);

        RECT textRect{ 0,0,0,0 };
        m.dxFont->DrawTextA(NULL, m.text.c_str(), -1, &textRect, DT_CALCRECT, 0);

        int w = textRect.right - textRect.left;
        int h = textRect.bottom - textRect.top;

        RECT drawRect{m.x, m.y, m.x + w + m.padding * 2, m.y + h + m.padding * 2 };

        LPDIRECT3DSTATEBLOCK9 state = nullptr;
        dev->CreateStateBlock(D3DSBT_ALL, &state);
        state->Capture();

        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

        if (m.background)
        {
            struct vert { float x, y, z, rhw; DWORD c; };

            vert quad[4] = {
                {(float)drawRect.left, (float)drawRect.top, 0, 1, bgColor},
                {(float)drawRect.right, (float)drawRect.top, 0, 1, bgColor},
                {(float)drawRect.left, (float)drawRect.bottom, 0, 1, bgColor},
                {(float)drawRect.right, (float)drawRect.bottom, 0, 1, bgColor}
            };

            dev->SetTexture(0, NULL);
            dev->SetPixelShader(NULL);
            dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

            dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(vert));
        }

        RECT textPos{
            m.x + m.padding,
            m.y + m.padding,
            m.x + m.padding + w,
            m.y + m.padding + h
        };

        m.dxFont->DrawTextA(NULL, m.text.c_str(), -1, &textPos, DT_NOCLIP, textColor);

        state->Apply();
        state->Release();
    }

    LRESULT __stdcall hkWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        ImGuiIO& io = ImGui::GetIO();

        if (msg == WM_KILLFOCUS || (msg == WM_ACTIVATEAPP && wParam == FALSE))
            ReleaseCursorConfinement();

        if (msg == WM_SETCURSOR && cursor_confined)
        {
            SetCursor(nullptr);
            return TRUE;
        }

        if (msg == WM_KEYDOWN && wParam == IMGUI_OVERLAY_TOGGLE_KEY && !(lParam & (1 << 30)))
        {
            im_overlay_enabled = !im_overlay_enabled;
            if (im_overlay_enabled)
                ReleaseCursorConfinement();
            UpdateGameInputCapture();
            return true;
        }

        if (msg == WM_KEYUP && wParam == IMGUI_OVERLAY_TOGGLE_KEY)
            return true;

        if (im_overlay_enabled)
        {
            LPARAM adjustedLParam = lParam;

            if (msg == WM_MOUSEMOVE)
            {
                if (eCApplication::GetInstance().GetWindowMode() == eCWindow::eEWindowMode_Fullscreen &&
                    eCApplication::GetInstance().GetEngineSetup().CaptureCursor == GETrue)
                {
                    HWND window = eCApplication::GetInstance().GetHandle();
                    RECT windowRect;
                    GetWindowRect(window, &windowRect);

                    bCPoint virtualRes = eCGUIModule::GetInstance().GetVirtualResolution();

                    float scaleX = (windowRect.right - windowRect.left) / static_cast<float>(virtualRes.GetX());
                    float scaleY = (windowRect.bottom - windowRect.top) / static_cast<float>(virtualRes.GetY());

                    ImVec2 scaledMousePos;
                    scaledMousePos.x = GET_X_LPARAM(lParam) * scaleX;
                    scaledMousePos.y = GET_Y_LPARAM(lParam) * scaleY;

                    adjustedLParam = MAKELPARAM((int)scaledMousePos.x, (int)scaledMousePos.y);
                }

                ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, adjustedLParam);

                if (io.WantCaptureMouse)
                    return true;
            }
            else
            {
                ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

                switch (msg)
                {
                case WM_LBUTTONDOWN:
                case WM_LBUTTONUP:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                case WM_XBUTTONDOWN:
                case WM_XBUTTONUP:
                case WM_MOUSEWHEEL:
                case WM_MOUSEHWHEEL:
                    if (io.WantCaptureMouse)
                        return true;
                    break;

                case WM_KEYDOWN:
                case WM_KEYUP:
                case WM_CHAR:
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                    if (io.WantCaptureKeyboard)
                        return true;
                    break;
                }
            }
        }

        return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
    }

    HRESULT __stdcall hkCreateDeviceEx(LPDIRECT3D9EX pD3D, UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* params, D3DDISPLAYMODEEX* fullscreenMode, LPDIRECT3DDEVICE9EX* device)
    {
        HRESULT result = oCreateDeviceEx(pD3D, adapter, deviceType, focusWindow, behaviorFlags, params, fullscreenMode, device);
        if (FAILED(result) || !device || !*device)
            return result;

        if (!oPresent)
        {
            void** deviceVtable = *reinterpret_cast<void***>(*device);
            oPresent = (Present_t)deviceVtable[17];
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&)oPresent, hkPresent);
            DetourTransactionCommit();
        }

        if (!startup_logo.device)
        {
            startup_logo.device = *device;
            startup_logo.device->AddRef();
            SetStartupLogoStatus("Initializing");
        }

        return result;
    }

    // called by eCDX9Device::SwapScreen at rva 0x9d2ab
    HRESULT __stdcall hkPresent(LPDIRECT3DDEVICE9 pDevice, const RECT* src, const RECT* dest, HWND hwnd, const RGNDATA* dirty)
    {
        if (startup_logo.phase == StartupLogoPhase::Waiting && !startup_logo.device)
        {
            startup_logo.device = pDevice;
            startup_logo.device->AddRef();
            if (!startup_logo.status.empty())
                startup_logo.phase = StartupLogoPhase::Visible;
        }

        if (startup_logo.phase == StartupLogoPhase::Visible)
        {
            DrawStartupLogo();
            return oPresent(pDevice, src, dest, hwnd, dirty);
        }

        if (startup_logo.phase == StartupLogoPhase::FadePending)
        {
            startup_logo.phase = StartupLogoPhase::Fading;
            startup_logo.fadeStart = std::chrono::steady_clock::now();
        }

        if (!im_initialized)
            InitImGuiOverlay(pDevice, hwnd);

        if (im_overlay_enabled)
            DrawImGuiOverlay();
        else
            UpdateGameInputCapture();

        UpdateCursorConfinement();

        DrawScreenMessage(pDevice, dbg_msg);
        DrawScreenMessage(pDevice, message);

        if (startup_logo.phase == StartupLogoPhase::Fading)
        {
            const float fadeDuration = 0.8f;
            const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startup_logo.fadeStart).count();
            const float alpha = std::clamp(1.0f - elapsed / fadeDuration, 0.0f, 1.0f);
            if (alpha > 0.0f)
                DrawStartupLogo(false, alpha);
            else
                CompleteStartupLogo();
        }

        return oPresent(pDevice, src, dest, hwnd, dirty);
    }
}
