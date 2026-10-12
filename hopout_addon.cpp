#include <imgui.h>
#include <reshade.hpp>
#include "telemetry.hpp"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <windows.h>
#include <shellapi.h>

namespace {
namespace fs = std::filesystem;
fs::path route_directory;
std::vector<hopout::route> routes;
hopout::route draft;
hopout::step pending;
hopout::coach coach;
telemetry::snapshot snapshot;
bool enabled=false, supported=false, live=false, editing=false;
bool visible=true, stream_safe=false;
bool advanced_mode=false;
bool have_stand=false, have_look=false;
bool have_info_position=false;
int selected=0, category_filter=0, editing_step=-1;
float marker_distance=4;
char route_name[81]="My hopout", instruction[241]="Mantle now using your usual game control. Press F8 after landing.";
char info_text[601]={};
char stand_label[121]="STAND HERE", look_label[121]="LOOK HERE";
std::string status="Click Create a hopup to get started.";

void share_route(const hopout::route &r) {
    if(r.source_path.empty() || !fs::exists(r.source_path)) { status="Save this hopup before sharing it."; return; }
    std::wstring args=L"/select,\""+r.source_path+L"\"";
    ShellExecuteW(nullptr,L"open",L"explorer.exe",args.c_str(),nullptr,SW_SHOWNORMAL);
    status="Opened the route file in Explorer. Share the .hopout file with your learners.";
}

bool has_pending() {return have_stand||have_look||have_info_position||info_text[0]||editing_step>=0;}
void pause_telemetry() {
    telemetry::set_access(false);supported=false;live=false;snapshot={};coach.held=0;
}
bool prepare_telemetry(bool focused,bool needed) {
    if(!enabled||!visible||!focused||!needed){pause_telemetry();return false;}
    if(!telemetry::access_allowed){
        telemetry::set_access(true);supported=telemetry::initialize();
    }
    return supported;
}
bool add_pending_step() {
    if(!have_stand||!have_look){status="Save your standing point and look direction before saving the hopup.";return false;}
    if(info_text[0]&&pending.wall_text&&!have_info_position){status="Place your wall text anchor with F10 first.";return false;}
    pending.action=instruction;pending.info=info_text;
    if(!hopout::valid(pending)||draft.steps.size()>=64){status="Invalid step or route full. Check your markers and action text.";return false;}
    if(editing_step>=0)draft.steps.insert(draft.steps.begin()+editing_step,pending);else draft.steps.push_back(pending);
    editing_step=-1;pending={};info_text[0]=0;have_stand=have_look=have_info_position=false;
    return true;
}
bool add_another_step() {
    if(!have_stand||!have_look){status="Capture the current Stand Here and Look Here markers first.";return false;}
    if(!add_pending_step())return false;
    status="Stand Here marker added. Capture the next starting point, then its look target.";
    return true;
}
void new_route(int category) {
    if(!draft.steps.empty()||has_pending()){status="Save your current draft before creating another hopup.";editing=true;return;}
    draft={};draft.category=category;pending={};editing_step=-1;
    int number=1;
    for(;;++number) {
        std::snprintf(route_name,sizeof(route_name),"%s %d",category==1?"Hopup":"Hopout",number);
        if(std::none_of(routes.begin(),routes.end(),[&](const auto &r){return r.category==category&&r.name==route_name;}))break;
    }
    std::snprintf(stand_label,sizeof(stand_label),"%s",draft.stand_label.c_str());
    std::snprintf(look_label,sizeof(look_label),"%s",draft.look_label.c_str());
    editing=true;coach.reset();category_filter=category;
    status="New hopup: follow the two capture steps, then save. You can name it in the editor.";
}

void reload() {
    routes.clear(); selected=0;  coach.reset();
    std::error_code ec;
    if(!fs::exists(route_directory,ec)) {status="No saved routes yet. Create your first hopout below.";return;}
    fs::directory_iterator it(route_directory,ec), end;
    size_t rejected=0;
    for(;!ec && it!=end;it.increment(ec)) {
        if(it->path().extension()!=L".hopout")continue;
        const auto size=it->file_size(ec);
        if(ec)break;
        if(size>262144){++rejected;continue;}
        std::ifstream file(it->path()); hopout::route r;
        if(hopout::read_route(file,r)){r.source_path=it->path().wstring();routes.push_back(std::move(r));} else ++rejected;
        if(routes.size()>=256)break;
    }
    std::sort(routes.begin(),routes.end(),[](const auto &a,const auto &b){return a.name<b.name;});
    for(size_t i=0;i<routes.size();++i)if(routes[i].category==category_filter){selected=static_cast<int>(i);break;}
    status=ec?"Could not read route directory: "+ec.message():
        std::to_string(routes.size())+" routes loaded; "+std::to_string(rejected)+" invalid files skipped.";
}
void capture_stand(bool aimed) {
    if(!enabled||!live||!editing)return;
    hopout::vec3 point=snapshot.player;
    if(aimed) {
        if(!hopout::aim_point(snapshot.inverse,marker_distance,point)){status="Cannot place aim marker with this camera.";return;}
        // The saved stand coordinate is the ped origin; the ring is drawn at foot height.
        point.z+=1;
    }
    pending.stand=point; have_stand=true;
    status=aimed?"Stand marker placed at aim distance. Check its height before saving.":"Standing position captured.";
}
void capture_look() {
    if(!enabled||!live||!editing)return;
    if(!hopout::aim_point(snapshot.inverse,marker_distance,pending.look)){status="Cannot place aim marker with this camera.";return;}
    have_look=true;status="Look target captured.";
}
void capture_info() {
    if(!enabled||!live||!editing)return;
    if(!hopout::aim_point(snapshot.inverse,marker_distance,pending.info_position)){status="Cannot place text with this camera.";return;}
    pending.wall_text=true;have_info_position=true;status="Text anchor placed. Enter your message in the editor.";
}
// Publish a complete validated file atomically; a failed edit leaves the original intact.
bool store_route(const hopout::route &r,const fs::path &path) {
    std::ostringstream data;hopout::write_route(data,r);
    std::istringstream check(data.str());hopout::route verified;
    if(!hopout::read_route(check,verified)){status="Invalid profile. Check names, markers and fade distances.";return false;}
    const fs::path temp=path.wstring()+L".tmp-"+std::to_wstring(GetCurrentProcessId());
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){status="Cannot save profile: folder is not writable.";return false;}
    const auto text=data.str();DWORD written=0;
    const bool ok=WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&written,nullptr)&&written==text.size()&&FlushFileBuffers(file);
    CloseHandle(file);
    const bool moved=ok&&MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    if(!moved){std::error_code ec;fs::remove(temp,ec);status="Save failed. Original profile and draft kept.";}
    return moved;
}
void edit_route(size_t index) {
    if(editing||!draft.steps.empty()||has_pending()){status="Save or cancel the current edit first.";return;}
    draft=routes[index];selected=static_cast<int>(index);
    std::snprintf(route_name,sizeof(route_name),"%s",draft.name.c_str());
    std::snprintf(stand_label,sizeof(stand_label),"%s",draft.stand_label.c_str());
    std::snprintf(look_label,sizeof(look_label),"%s",draft.look_label.c_str());
    pending=draft.steps.front();draft.steps.erase(draft.steps.begin());editing_step=0;
    std::snprintf(info_text,sizeof(info_text),"%s",pending.info.c_str());
    std::snprintf(instruction,sizeof(instruction),"%s",pending.action.c_str());
    have_stand=have_look=true;have_info_position=pending.wall_text;editing=true;
    status="Editing saved hopup. Save updates this profile; Cancel keeps the original.";
}
bool selected_hopups() {
    return std::any_of(routes.begin(),routes.end(),[](const auto &r){return r.category==1&&r.shown;});
}
void save() {
    if(!hopout::valid(draft.stand_style)||!hopout::valid(draft.look_style)){
        status="Check your marker colours and sizes before saving.";return;
    }
    draft.name=route_name;draft.stand_label=stand_label;draft.look_label=look_label;
    if(draft.name.find_first_not_of(" \t")==std::string::npos){status="Give the route a name first.";return;}
    if(std::any_of(routes.begin(),routes.end(),[&](const auto &r){return r.category==draft.category&&r.name==draft.name&&(draft.source_path.empty()||r.source_path!=draft.source_path);})) {
        status="That name is already saved in this category. Choose a different name.";return;
    }
    if(routes.size()>=256&&draft.source_path.empty()){status="The route library is full (256). Back up and move unused route files before saving more.";return;}
    if(has_pending()&&!add_pending_step())return;
    if(draft.steps.empty()){status="Capture stand (F6) and look (F7) before saving.";return;}
    std::error_code ec; fs::create_directories(route_directory,ec);
    if(ec){status="Cannot create route directory: "+ec.message();return;}
    fs::path destination=draft.source_path;
    if(destination.empty()) {
        for(int i=0;;++i){destination=route_directory/(L"hopout-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(i)+L".hopout");if(!fs::exists(destination))break;}
    }
    if(!store_route(draft,destination))return;
    category_filter=draft.category;reload();
    for(size_t i=0;i<routes.size();++i)if(routes[i].category==draft.category&&routes[i].name==draft.name){selected=static_cast<int>(i);break;}
    status="Saved "+draft.name+". Checked hopups show automatically.";
    draft={}; pending={}; info_text[0]=0; have_stand=have_look=have_info_position=false; editing=false;
}
void advanced_settings(reshade::api::effect_runtime *) {
    ImGui::TextUnformatted("Hopup Creator | read-only FiveM tool");
    ImGui::TextWrapped("Places visual markers and instructions. You perform every movement yourself.");
    ImGui::TextUnformatted("Educational edition. Local position / camera reads are read-only.");
    ImGui::TextWrapped("%s",status.c_str());
    if(ImGui::Checkbox("Stream-safe mode: hide overlays (F11)",&stream_safe)){visible=!stream_safe;pause_telemetry();}
    ImGui::TextUnformatted("F11: hide / show for streaming    F12: save current route");
    ImGui::TextWrapped("Hiding pauses guidance and hides markers, text and the HUD. Close this menu to use hotkeys.");
    if(enabled) ImGui::TextUnformatted(live?"Local position and camera available.":"Waiting for supported local position and camera.");
    if(ImGui::Button("Reload saved routes"))reload();
    category_filter=1;
    const auto count=std::count_if(routes.begin(),routes.end(),[](const auto &r){return r.category==1;});
    ImGui::Text("Saved hopups: %d",static_cast<int>(count));
    if(ImGui::Button("+ New hopup"))new_route(1);
    if(editing) {
        ImGui::InputText("Route name",route_name,sizeof(route_name));
        ImGui::InputText("Instructions",info_text,sizeof(info_text));
        ImGui::Checkbox("Show a vehicle placement guide",&draft.vehicle_guide);
        if(draft.vehicle_guide) { ImGui::Combo("Vehicle guide",&draft.vehicle_shape,"Dot\0Simple vehicle outline\0"); ImGui::Text("Vehicle label: %s",draft.vehicle_label.c_str()); }
        draft.category=1;
        if(ImGui::Button("Save hopup (F12)"))save();
        ImGui::TextWrapped("Save includes your current complete stand/look markers. Each named hopup is stored separately and loads again next launch.");
        ImGui::TextWrapped("F6: save where you stand. F7: save look target. F9: place stand marker at your aim distance. Close the ReShade menu to aim.");
        ImGui::SliderFloat("Aim marker distance",&marker_distance,.5f,50.f,"%.1f m");
        ImGui::TextWrapped("Aimed placement uses distance, not surface collision. Adjust distance and XYZ to match the box or ledge. The yellow preview shows the aim point.");
        ImGui::BeginDisabled(!live);
        if(ImGui::Button("Capture standing position (F6)"))capture_stand(false);
        if(ImGui::Button("Capture look target (F7)"))capture_look();
        if(ImGui::Button("Place stand at aim distance (F9)"))capture_stand(true);
        ImGui::EndDisabled();
        if(have_stand) {float v[]={pending.stand.x,pending.stand.y,pending.stand.z};if(ImGui::DragFloat3("Stand XYZ (ped origin)",v,.05f))pending.stand={v[0],v[1],v[2]};}
        if(have_look) {float v[]={pending.look.x,pending.look.y,pending.look.z};if(ImGui::DragFloat3("Look XYZ",v,.05f))pending.look={v[0],v[1],v[2]};}
        ImGui::SliderFloat("Standing tolerance",&pending.radius,.1f,2.f,"%.2f m");
        ImGui::SliderFloat("Look tolerance",&pending.aim,.01f,.2f,"%.2f screen units");
        ImGui::SliderFloat("Delay after alignment",&pending.delay,0.f,5.f,"%.1f seconds");
        ImGui::InputText("Action instruction",instruction,sizeof(instruction));
        ImGui::Separator();
        ImGui::TextUnformatted("Text box / arrival info (optional)");
        ImGui::InputTextMultiline("Message",info_text,sizeof(info_text),ImVec2(-1,80));
        ImGui::Checkbox("Anchor text on a wall / aimed point",&pending.wall_text);
        ImGui::TextWrapped("Info appears near checked hopups, using the saved fade distances.");
        if(pending.wall_text) {
            ImGui::TextUnformatted("Aim and press F10 to place the text anchor.");
            ImGui::BeginDisabled(!live);
            if(ImGui::Button("Place text at aim (F10)"))capture_info();
            ImGui::EndDisabled();
            if(have_info_position){float v[]={pending.info_position.x,pending.info_position.y,pending.info_position.z};if(ImGui::DragFloat3("Text anchor XYZ",v,.05f))pending.info_position={v[0],v[1],v[2]};}
        }
        ImGui::Text("Stand: %s | Look: %s | Saved draft steps: %d",have_stand?"set":"missing",have_look?"set":"missing",static_cast<int>(draft.steps.size()));
        ImGui::BeginDisabled(!have_stand||!have_look||draft.steps.size()>=64||(info_text[0]&&pending.wall_text&&!have_info_position));
        if(ImGui::Button("Add step to draft")) {
            if(add_pending_step())status="Step added. Place the next step or press F12 to save.";
        }
        ImGui::EndDisabled();
        for(size_t i=0;i<draft.steps.size();++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::Text("%d. %s",static_cast<int>(i+1),draft.steps[i].action.c_str());
            ImGui::BeginDisabled(have_stand||have_look||info_text[0]);
            if(ImGui::Button("Edit step")) {
                pending=draft.steps[i];editing_step=static_cast<int>(i);draft.steps.erase(draft.steps.begin()+i);
                std::snprintf(instruction,sizeof(instruction),"%s",pending.action.c_str());
                std::snprintf(info_text,sizeof(info_text),"%s",pending.info.c_str());
                have_stand=have_look=true;have_info_position=pending.wall_text;
                status="Editing step. Add step to draft saves it in its original position.";
            }
            ImGui::EndDisabled();ImGui::PopID();
        }
        ImGui::BeginDisabled(editing_step>=0);
        if(!draft.steps.empty() && ImGui::Button("Remove last draft step"))draft.steps.pop_back();
        ImGui::EndDisabled();
        ImGui::BeginDisabled(draft.steps.empty()&&!has_pending());
        if(ImGui::Button("Save route to disk"))save();
        ImGui::EndDisabled();
        if(has_pending()){ImGui::TextUnformatted("Save includes a complete pending step. Incomplete markers must be finished or discarded.");if(ImGui::Button("Discard pending markers")){editing_step=-1;have_stand=have_look=have_info_position=false;pending={};info_text[0]=0;}}
    }
    ImGui::Separator();
    ImGui::TextWrapped("Game memory access is read-only. Only your route files are written. Read-only is not a guarantee of Cfx/server approval. This prototype does not detect mantle animations or collision surfaces.");
}

void appearance_editor() {
    if(!ImGui::CollapsingHeader("Marker appearance"))return;
    auto edit=[](const char *label,hopout::marker_style &style){
        ImGui::PushID(label);ImGui::TextUnformatted(label);
        ImGui::ColorEdit3("Colour",style.colour,ImGuiColorEditFlags_NoInputs);
        ImGui::Combo("Shape",&style.shape,"Circle\0Diamond\0Square\0");
        ImGui::SliderFloat("Size",&style.size,6,36,"%.0f px",ImGuiSliderFlags_AlwaysClamp);ImGui::PopID();
    };
    edit("Standing marker",draft.stand_style);edit("Look target",draft.look_style);
    ImGui::Checkbox("Show marker labels",&draft.show_labels);
    if(ImGui::Button("Reset marker appearance")){
        const hopout::route defaults;draft.stand_style=defaults.stand_style;draft.look_style=defaults.look_style;draft.show_labels=true;
    }
    ImGui::TextWrapped("Colours, shapes and sizes save with this hopup. Use your own crosshair to line up the look target.");
}
void settings(reshade::api::effect_runtime *runtime) {
    ImGui::TextUnformatted("Hopup Creator — Educational Edition");
    if(ImGui::Checkbox("Enable for this session",&enabled))pause_telemetry();
    ImGui::TextWrapped("Enable only where your server permits this add-on. Disabled by default each launch; Cfx approval is not guaranteed.");
    if(ImGui::Checkbox("Stream-safe mode: hide overlays (F11)",&stream_safe)){visible=!stream_safe;pause_telemetry();}
    if(!enabled)ImGui::TextUnformatted("Game position / camera reads are off.");
    else if(!live)ImGui::TextWrapped("Create or select a hopup to read your position. Requires FiveM GTA build 3258.");
    ImGui::TextWrapped("%s",status.c_str());
    if(ImGui::Button("+ New hopup"))new_route(1);
    ImGui::Separator();
    if(editing) {
        ImGui::InputText("Name your hopup",route_name,sizeof(route_name));
        ImGui::TextUnformatted(!have_stand?"1. Stand where this step starts.":!have_look?"2. Aim where this step should look.":"Step ready. Save it or add another Stand Here.");
        ImGui::BeginDisabled(!live);
        if(!have_stand) {
            if(ImGui::Button("Save where I am standing (F6)"))capture_stand(false);
        } else if(!have_look) {
            if(ImGui::Button("Save where I am looking (F6)"))capture_look();
        } else {
            if(ImGui::Button("Redo standing point")){capture_stand(false);have_look=false;}
            ImGui::SameLine();if(ImGui::Button("Redo look direction"))have_look=false;
            ImGui::SameLine();if(ImGui::Button("Place stand at aim distance")){capture_stand(true);have_look=false;}
        }
        ImGui::EndDisabled();
        if(have_stand&&have_look) {
            ImGui::Text("Steps in this hopup: %d",static_cast<int>(draft.steps.size()+1));
            if(ImGui::Button("+ Add another Stand Here"))add_another_step();
            ImGui::SameLine();ImGui::TextDisabled("Each step gets its own stand and look marker.");
        } else if(!draft.steps.empty()) {
            ImGui::Text("Saved steps waiting: %d",static_cast<int>(draft.steps.size()));
        }
        ImGui::TextWrapped("Close the ReShade menu to move or aim, then press F6 for the step shown above.");
        ImGui::TextUnformatted("Instructions box (optional)");
        ImGui::InputTextMultiline("##HopupInstructions",info_text,sizeof(info_text),ImVec2(-1,90));
        ImGui::TextWrapped("Write what to do here, such as: Stand on the corner, aim at the sign, then mantle. Leave blank for no box. Instructions save with this hopup and fade with its markers.");
        ImGui::InputText("Standing marker text",stand_label,sizeof(stand_label));
        ImGui::InputText("Look marker text",look_label,sizeof(look_label));
        ImGui::Checkbox("Show vehicle placement guide",&draft.vehicle_guide);
        if(draft.vehicle_guide) {
            ImGui::Combo("Vehicle guide style",&draft.vehicle_shape,"Dot\0Simple vehicle outline\0");
            char vehicle_label[121]; std::snprintf(vehicle_label,sizeof(vehicle_label),"%s",draft.vehicle_label.c_str());
            if(ImGui::InputText("Vehicle label",vehicle_label,sizeof(vehicle_label)))draft.vehicle_label=vehicle_label;
            ImGui::TextWrapped("This is a visual reference only. Place the vehicle yourself and follow the server rules.");
        }
        appearance_editor();
        ImGui::TextUnformatted("Show nearby only");
        ImGui::SliderFloat("Fully visible within",&draft.full_distance,0.f,100.f,"%.1f m",ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Hidden beyond",&draft.hidden_distance,1.f,500.f,"%.1f m",ImGuiSliderFlags_AlwaysClamp);
        if(draft.hidden_distance<=draft.full_distance)draft.hidden_distance=draft.full_distance+1;
        ImGui::TextWrapped("Saved separately for this hopup. Its markers, arrows and text fade out as you walk away and disappear beyond the outer distance.");
        ImGui::BeginDisabled((!have_stand||!have_look)&&draft.steps.empty());
        if(ImGui::Button("Save hopup (F12)"))save();
        ImGui::EndDisabled();
        if(ImGui::Button("Cancel edit")){draft={};pending={};editing=false;editing_step=-1;have_stand=have_look=have_info_position=false;info_text[0]=0;status="Edit cancelled. Saved profiles unchanged.";}
        ImGui::TextWrapped("Save updates the profile you are editing. Create a hopup makes a separate profile.");
    } else {
        ImGui::TextUnformatted("My saved hopups");
        bool any=false;
        for(size_t i=0;i<routes.size();++i)if(routes[i].category==1) {
            any=true;ImGui::PushID(static_cast<int>(i));
            bool show=routes[i].shown;
            if(ImGui::Checkbox("Toggle hop",&show)){
                auto updated=routes[i];updated.shown=show;
                if(store_route(updated,updated.source_path))routes[i].shown=show;
                pause_telemetry();
            }
            ImGui::SameLine();ImGui::TextUnformatted(routes[i].name.c_str());
            ImGui::SameLine();if(ImGui::Button("Edit hopup"))edit_route(i);
            ImGui::SameLine();if(ImGui::Button("Share"))share_route(routes[i]);
            ImGui::Text("Visibility: %.0f-%.0f m%s",routes[i].full_distance,routes[i].hidden_distance,routes[i].vehicle_guide?" | vehicle guide":"");
            ImGui::PopID();
        }
        if(!any)ImGui::TextWrapped("No hopups saved yet. Click Create a hopup to make your first.");
        ImGui::TextWrapped("Checked hopups show automatically nearby. F11 hides / shows all overlays.");
    }
    ImGui::Separator();
    advanced_mode=ImGui::CollapsingHeader("Advanced options (optional)");
    if(advanced_mode)advanced_settings(runtime);
}

ImVec2 screen(float x,float y,ImVec2 size){return {(x*.5f+.5f)*size.x,(.5f-y*.5f)*size.y};}
void info_box(const hopout::step &s,float alpha) {
    if(s.info.empty()||alpha<=.005f)return;
    const hopout::vec3 point=s.wall_text?s.info_position:hopout::vec3{s.stand.x,s.stand.y,s.stand.z+1};
    float x,y,w;if(!hopout::project(point,snapshot.matrix,x,y,w)||std::abs(x)>1||std::abs(y)>1)return;
    const auto size=ImGui::GetIO().DisplaySize;
    const auto p=screen(x,y,size);
    const float wrap=std::min(300.f,std::max(40.f,size.x-40));
    // Do not call CalcTextSize across the MSVC host / MinGW add-on boundary:
    // ImVec2 returns use a hidden result pointer on MSVC but RAX on MinGW.
    // Let an auto-sized native ImGui window measure and wrap its own text.
    char name[80];std::snprintf(name,sizeof(name),"Hopup info##%p",static_cast<const void *>(&s));
    ImGui::SetNextWindowPos(p,ImGuiCond_Always,ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize({wrap+24,0},ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(12,10));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,6.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(12/255.f,22/255.f,32/255.f,.92f));
    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(245/255.f,248/255.f,1,1));
    if(ImGui::Begin(name,nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|
        ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::PushTextWrapPos(0);
        ImGui::TextUnformatted(s.info.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
    ImGui::PopStyleColor(2);ImGui::PopStyleVar(3);
}
ImU32 marker_colour(const hopout::marker_style &style,float alpha=1) {
    return ImGui::ColorConvertFloat4ToU32({style.colour[0],style.colour[1],style.colour[2],alpha});
}
void marker(hopout::vec3 point, ImU32 color, const char *label, bool ring, float radius,
    const hopout::marker_style &style, bool labels);
void vehicle_marker(hopout::vec3 point, const hopout::route &r, float alpha) {
    if(!r.vehicle_guide || alpha<=.005f)return;
    auto draw=ImGui::GetBackgroundDrawList(); const auto size=ImGui::GetIO().DisplaySize;
    if(r.vehicle_shape==0) { marker(point,marker_colour(r.stand_style,alpha),r.vehicle_label.c_str(),false,0.65f,r.stand_style,r.show_labels); return; }
    const float length=2.2f,width=1.05f,z=point.z-.8f;
    const hopout::vec3 corners[]={{point.x-length*.5f,point.y-width*.5f,z},{point.x+length*.5f,point.y-width*.5f,z},{point.x+length*.5f,point.y+width*.5f,z},{point.x-length*.5f,point.y+width*.5f,z}};
    ImU32 color=marker_colour(r.stand_style,alpha);
    for(int i=0;i<4;++i){float ax,ay,aw,bx,by,bw;if(hopout::project(corners[i],snapshot.matrix,ax,ay,aw)&&hopout::project(corners[(i+1)%4],snapshot.matrix,bx,by,bw))draw->AddLine(screen(ax,ay,size),screen(bx,by,size),color,2);}
    float x,y,w;if(hopout::project({point.x,point.y,z+1},snapshot.matrix,x,y,w)&&std::abs(x)<1&&std::abs(y)<1)draw->AddText(screen(x,y,size),color,r.vehicle_label.c_str());
}
void nearest_guide_box(const hopout::route &r, const hopout::step &s, float metres) {
    if(s.info.empty()&&s.action.empty())return;
    ImGui::SetNextWindowPos({20,20},ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(.9f);
    if(ImGui::Begin("Hopup guide##top-left",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::Text("HOPUP: %s  |  %.1f m",r.name.c_str(),metres);
        ImGui::TextWrapped("%s",s.info.empty()?s.action.c_str():s.info.c_str());
        if(!s.info.empty()&&!s.action.empty())ImGui::TextWrapped("Next: %s",s.action.c_str());
        ImGui::Text("Visibility %.0f-%.0f m",r.full_distance,r.hidden_distance);
    }
    ImGui::End();
}
void marker(hopout::vec3 point, ImU32 color, const char *label, bool ring=false, float radius=.65f,
    const hopout::marker_style &style=hopout::marker_style{},bool labels=true) {
    if((color&IM_COL32_A_MASK)==0)return;
    auto draw=ImGui::GetBackgroundDrawList();const auto size=ImGui::GetIO().DisplaySize;
    float x=0,y=0,w=0;
    if(ring) {
        point.z-=1; // Visual foot offset only; arrival checks use captured ped origin.
        const int segments=style.shape==0?48:4;
        const float rotation=style.shape==2?.78539816f:0;
        for(int i=0;i<segments;++i) {
            const float a=i*6.2831853f/segments+rotation,b=(i+1)*6.2831853f/segments+rotation;
            float ax,ay,aw,bx,by,bw;
            if(hopout::project({point.x+std::cos(a)*radius,point.y+std::sin(a)*radius,point.z},snapshot.matrix,ax,ay,aw)&&
               hopout::project({point.x+std::cos(b)*radius,point.y+std::sin(b)*radius,point.z},snapshot.matrix,bx,by,bw)&&
               std::abs(ax)<3&&std::abs(ay)<3&&std::abs(bx)<3&&std::abs(by)<3)
                draw->AddLine(screen(ax,ay,size),screen(bx,by,size),color,3);
        }
    }
    const bool front=hopout::project(point,snapshot.matrix,x,y,w);
    if(front && std::abs(x)<.9f && std::abs(y)<.86f) {
        const auto p=screen(x,y,size);
        const float size=style.size;
        if(style.shape==0)draw->AddCircle(p,size,color,24,2);
        else if(style.shape==1)draw->AddQuad({p.x,p.y-size},{p.x+size,p.y},{p.x,p.y+size},{p.x-size,p.y},color,2);
        else draw->AddRect({p.x-size,p.y-size},{p.x+size,p.y+size},color,0,0,2);
        if(labels)draw->AddText({p.x+size+4,p.y-8},color,label);
    } else {
        // Use raw clip direction for targets behind the camera; keep the arrow on screen.
        if(!front){x=point.x*snapshot.matrix[0]+point.y*snapshot.matrix[4]+point.z*snapshot.matrix[8]+snapshot.matrix[12];y=point.x*snapshot.matrix[1]+point.y*snapshot.matrix[5]+point.z*snapshot.matrix[9]+snapshot.matrix[13];}
        if(!std::isfinite(x)||!std::isfinite(y))return;
        const float length=std::sqrt(x*x+y*y);if(length<.001f){x=1;y=0;}else{x/=length;y/=length;}
        const ImVec2 p(size.x*.5f+x*size.x*.38f,size.y*.5f-y*size.y*.35f);
        const ImVec2 direction(x,-y),side(y,x);
        draw->AddTriangleFilled({p.x+direction.x*14,p.y+direction.y*14},
            {p.x-direction.x*10+side.x*8,p.y-direction.y*10+side.y*8},
            {p.x-direction.x*10-side.x*8,p.y-direction.y*10-side.y*8},color);
        if(labels)draw->AddText({p.x-45,p.y+20},color,label);
    }
}
void draw(reshade::api::effect_runtime *runtime) {
    const bool keys=!ImGui::GetIO().WantTextInput && !ImGui::GetIO().WantCaptureKeyboard;
    DWORD foreground_process=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&foreground_process);
    const bool focused=foreground_process==GetCurrentProcessId();
    if(!focused){pause_telemetry();return;}
    if(keys&&runtime->is_key_pressed(VK_F11)){stream_safe=!stream_safe;visible=!stream_safe;pause_telemetry();}
    if(stream_safe||!visible){pause_telemetry();return;}
    if(keys&&editing&&runtime->is_key_pressed(VK_F12))save();
    if(!prepare_telemetry(focused,editing||selected_hopups()))return;
    live=telemetry::sample(snapshot);
    if(!live){snapshot={};coach.held=0;}
    if(live && editing && keys) {
        if(runtime->is_key_pressed(VK_F6)){
            if(advanced_mode||!have_stand)capture_stand(false);else if(!have_look)capture_look();
        }
        if(advanced_mode){
            if(runtime->is_key_pressed(VK_F7))capture_look();
            if(runtime->is_key_pressed(VK_F9))capture_stand(true);
            if(runtime->is_key_pressed(VK_F10))capture_info();
        }
    }
    if(live && editing) {
        const float alpha=have_stand?hopout::proximity_alpha(hopout::distance(snapshot.player,pending.stand),draft.full_distance,draft.hidden_distance):0;
        if(have_stand)marker(pending.stand,marker_colour(draft.stand_style,alpha),stand_label,true,pending.radius,draft.stand_style,draft.show_labels);
        if(have_stand)vehicle_marker(pending.stand,draft,alpha);
        if(have_look)marker(pending.look,marker_colour(draft.look_style,alpha),look_label,false,.65f,draft.look_style,draft.show_labels);
        for(const auto &s:draft.steps)info_box(s,hopout::proximity_alpha(hopout::distance(snapshot.player,s.stand),draft.full_distance,draft.hidden_distance));
        if(have_stand && (!pending.wall_text||have_info_position)) {
            auto preview_info=pending;preview_info.info=info_text;
            info_box(preview_info,alpha);
        }
        hopout::vec3 preview;
        if(advanced_mode&&hopout::aim_point(snapshot.inverse,marker_distance,preview))marker(preview,IM_COL32(255,218,96,255),"AIM PLACEMENT");
    }
    if(live&&!editing) {
        const hopout::route *nearest_route=nullptr; const hopout::step *nearest_step=nullptr; float nearest_distance=1e9f;
        for(const auto &r:routes)if(r.category==1&&r.shown) {
        for(const auto &step:r.steps) {
            const float alpha=hopout::proximity_alpha(hopout::distance(snapshot.player,step.stand),r.full_distance,r.hidden_distance);
            if(alpha<=.005f)continue;
            const float metres=hopout::distance(snapshot.player,step.stand);
            if(metres<nearest_distance){nearest_distance=metres;nearest_route=&r;nearest_step=&step;}
            marker(step.stand,marker_colour(r.stand_style,alpha),r.stand_label.c_str(),true,step.radius,r.stand_style,r.show_labels);
            marker(step.look,marker_colour(r.look_style,alpha),r.look_label.c_str(),false,.65f,r.look_style,r.show_labels);
            vehicle_marker(step.stand,r,alpha);
            info_box(step,alpha);
        }
        }
        if(nearest_route&&nearest_step)nearest_guide_box(*nearest_route,*nearest_step,nearest_distance);
    }
    // Keep capture help out of the editor and away from world-space instructions.
    if(!editing||!keys)return;
    ImGui::SetNextWindowPos({24,ImGui::GetIO().DisplaySize.y-24},ImGuiCond_Always,ImVec2(0,1));
    ImGui::SetNextWindowBgAlpha(.9f);
    ImGui::SetNextWindowSize({420,0});
    if(ImGui::Begin("Hopout guide HUD",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::TextUnformatted("HOPUP CREATOR | EDITING");
        if(!live)ImGui::TextWrapped("Position / camera unavailable. Guidance paused. Requires the supported FiveM GTA build 3258.");
        else if(editing) {
            if(advanced_mode){ImGui::TextUnformatted("F6 Stand | F7 Look | F9 Aim stand | F10 Text");}
            else ImGui::TextUnformatted(!have_stand?"Stand at the start, then press F6.":!have_look?"Aim where you should look, then press F6.":"Ready! Press F12 to save your hopup.");
            ImGui::TextUnformatted("F11  Show / hide     F12  Save route");
            ImGui::TextWrapped("%s",status.c_str());
            ImGui::Text("Aim distance: %.1f m | Draft: %d steps",marker_distance,static_cast<int>(draft.steps.size()));
            ImGui::TextWrapped("Open ReShade > Add-ons > Hopup Creator to adjust markers and save.");
        }
    }
    ImGui::End();
}
}
extern "C" __declspec(dllexport) const char *NAME="Hopup Creator (Educational)";
extern "C" __declspec(dllexport) const char *DESCRIPTION="Educational read-only local position/camera guide with user-placed stand and look markers. FiveM GTA build 3258.";
extern "C" __declspec(dllexport) bool AddonInit(HMODULE module,HMODULE reshade_module) {
    if(!reshade::register_addon(module,reshade_module))return false;
    wchar_t path[32768]={};
    const DWORD length=GetModuleFileNameW(module,path,32768);
    if(!length||length>=32768){reshade::unregister_addon(module);return false;}
    route_directory=fs::path(path).parent_path()/L"HopoutGuide-routes";
    reload();
    reshade::register_overlay(nullptr,settings);
    reshade::register_event<reshade::addon_event::reshade_overlay>(draw);
    return true;
}
extern "C" __declspec(dllexport) void AddonUninit(HMODULE module,HMODULE) {
    enabled=false;pause_telemetry();
    reshade::unregister_event<reshade::addon_event::reshade_overlay>(draw);
    reshade::unregister_overlay(nullptr,settings);
    reshade::unregister_addon(module);
}
BOOL WINAPI DllMain(HMODULE,DWORD,LPVOID){return TRUE;}
