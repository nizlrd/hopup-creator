// Exercise the actual editor/save/reload workflow against isolated local files.
// No ReShade callbacks or game-memory readers are invoked by this test.
#include "hopout_addon.cpp"
#include <cassert>
#include <iostream>
// A minimal host table exercises info rendering without loading ReShade/FiveM.
ImGuiIO::ImGuiIO() : DisplaySize(1920,1080) {}
void test_info_rendering() {
    static ImGuiIO io;static int windows=0,texts=0,styles=0,colours=0,wraps=0;
    imgui_function_table table={};
    table.GetIO=[]()->ImGuiIO&{return io;};
    table.CalcTextSize=[](const char*,const char*,bool,float)->ImVec2{assert(false&&"Unsafe aggregate-return ABI must never be called");return {};};
    table.SetNextWindowPos=[](const ImVec2&,ImGuiCond,const ImVec2&){};
    table.SetNextWindowSize=[](const ImVec2&,ImGuiCond){};
    table.PushStyleVar=[](ImGuiStyleVar,float){++styles;};
    table.PushStyleVar2=[](ImGuiStyleVar,const ImVec2&){++styles;};
    table.PopStyleVar=[](int count){styles-=count;};
    table.PushStyleColor2=[](ImGuiCol,const ImVec4&){++colours;};
    table.PopStyleColor=[](int count){colours-=count;};
    table.Begin=[](const char*,bool*,ImGuiWindowFlags){++windows;return true;};
    table.End=[](){--windows;};
    table.PushTextWrapPos=[](float){++wraps;};table.PopTextWrapPos=[](){--wraps;};
    table.TextUnformatted=[](const char* text,const char*){assert(std::string(text)=="Wall info\n100% ready ## literal");++texts;};
    imgui_function_table_instance()=&table;
    snapshot={};snapshot.matrix[0]=snapshot.matrix[5]=snapshot.matrix[10]=snapshot.matrix[15]=1;
    hopout::step note;note.info="Wall info\n100% ready ## literal";note.wall_text=true;
    info_box(note,.5f);assert(texts==1&&windows==0&&styles==0&&colours==0&&wraps==0);
    info_box(note,0);assert(texts==1);
    note.info_position.x=10;info_box(note,1);assert(texts==1);
    imgui_function_table_instance()=nullptr;
    std::cout<<"PASS: info text rendering avoids unsafe CalcTextSize and balances UI state\n";
}
int main() {
    // All blocked lifecycle states must clear stale coordinates and close the
    // lowest-level reader, even if a previous frame was active.
    int local_value=42,copy=0;
    assert(!telemetry::read(reinterpret_cast<uintptr_t>(&local_value),copy)&&copy==0);
    for(int state=0;state<4;++state){
        enabled=state!=0;visible=state!=1;
        telemetry::set_access(true);supported=live=true;snapshot.player={1,2,3};coach.held=1;
        assert(!prepare_telemetry(state!=2,state!=3));
        assert(!telemetry::access_allowed&&!supported&&!live&&snapshot.player.x==0&&coach.held==0);
        assert(!telemetry::read(reinterpret_cast<uintptr_t>(&local_value),copy)&&copy==0);
        assert(!telemetry::initialize());
    }
    enabled=false;visible=true;
    std::cout<<"PASS: disabled, hidden, unfocused and idle states block game reads and clear stale telemetry\n";
    test_info_rendering();
    route_directory=fs::temp_directory_path()/(L"hopout-save-test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    assert(fs::create_directory(route_directory));
    new_route(1);
    assert(draft.category==1&&editing&&std::string(route_name)=="Hopup 1");
    draft.stand_style={{1,0,0},2,22};draft.look_style={{1,1,0},1,18};draft.show_labels=false;
    pending.stand={1,2,3};pending.look={4,5,6};have_stand=have_look=true;
    save(); // No explicit Add step required.
    assert(routes.size()==1&&routes[selected].name=="Hopup 1"&&!editing&&!has_pending());
    new_route(1);
    assert(std::string(route_name)=="Hopup 2");
    pending.stand={10,20,30};pending.look={40,50,60};have_stand=have_look=true;
    save();
    assert(routes.size()==2&&routes[selected].name=="Hopup 2");
    reload();
    assert(routes.size()==2&&routes[0].steps[0].stand.x==1&&routes[1].steps[0].stand.x==10);
    assert(routes[0].stand_style.shape==2&&routes[0].stand_style.colour[0]==1&&routes[0].stand_style.size==22);
    assert(routes[0].look_style.colour[1]==1&&routes[0].look_style.size==18&&!routes[0].show_labels);
    new_route(1);std::snprintf(route_name,sizeof(route_name),"Hopup 1");
    pending.stand={100,200,300};pending.look={400,500,600};have_stand=have_look=true;
    save();assert(routes.size()==2&&has_pending()); // No accidental replacement.
    std::snprintf(route_name,sizeof(route_name),"Hopup 3");have_look=false;
    save();assert(routes.size()==2&&have_stand&&editing); // Partial steps never save.
    new_route(1);assert(have_stand&&std::string(route_name)=="Hopup 3"); // Preserve draft.
    have_look=true;save();assert(routes.size()==3&&routes[selected].name=="Hopup 3");
    const auto original=routes[0].source_path;
    edit_route(0);assert(editing&&have_stand&&have_look);
    std::snprintf(route_name,sizeof(route_name),"Renamed box");
    std::snprintf(stand_label,sizeof(stand_label),"Stand on this corner");
    std::snprintf(look_label,sizeof(look_label),"Aim at the sign 100%%");
    std::snprintf(info_text,sizeof(info_text),"Stand on the corner.\nAim at the sign, then mantle.");
    pending.stand.x=7;draft.look_style.size=30;
    save();reload();assert(routes.size()==3);
    auto saved=std::find_if(routes.begin(),routes.end(),[&](const auto &r){return r.source_path==original;});
    assert(saved!=routes.end()&&saved->name=="Renamed box"&&saved->steps[0].stand.x==7);
    assert(saved->stand_label=="Stand on this corner"&&saved->look_label=="Aim at the sign 100%"&&saved->look_style.size==30);
    assert(saved->steps[0].info=="Stand on the corner.\nAim at the sign, then mantle.");
    for(auto &r:routes){r.shown=false;assert(store_route(r,r.source_path));}
    reload();assert(!selected_hopups());
    routes[0].shown=routes[1].shown=true;
    assert(store_route(routes[0],routes[0].source_path)&&store_route(routes[1],routes[1].source_path));
    reload();assert(selected_hopups()&&routes[0].shown&&routes[1].shown&&!routes[2].shown);
    auto invalid=routes[0];invalid.stand_label=std::string(121,'x');
    assert(!store_route(invalid,invalid.source_path));reload();assert(routes[0].stand_label.size()<=120);
    std::cout<<"PASS: in-place rename, marker text/position/style edits, multiple selections persist, invalid replacement preserves original\n";
    for(const auto &file:fs::directory_iterator(route_directory))assert(fs::remove(file.path()));
    assert(fs::remove(route_directory));
    std::cout<<"PASS: multiple hopups, automatic pending-step save, reload persistence, unique names, incomplete-step rejection, draft preservation\n";
}
