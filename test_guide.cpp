#include "guide.hpp"
#include <cassert>
#include <iostream>
#include <limits>
int main() {
    using namespace hopout;
    step s; s.stand={0,0,0};s.look={0,0,1};s.delay=0;
    route r{"Box \"practice\"",{s,s}};
    std::stringstream data;write_route(data,r);route loaded;
    assert(read_route(data,loaded));assert(loaded.name==r.name);assert(loaded.steps.size()==2);
    route annotated=r;annotated.category=1;annotated.full_distance=3;annotated.hidden_distance=18;
    annotated.steps[0].info="Stand on the box\nAim at the \"corner\" \\ edge";
    annotated.steps[0].wall_text=true;annotated.steps[0].active_only=true;
    annotated.steps[0].info_position={12,34,56};
    annotated.stand_style={{1,.2f,.3f},2,24};annotated.look_style={{.7f,.1f,1},1,16};annotated.show_labels=false;
    std::stringstream notes;write_route(notes,annotated);route restored;
    assert(read_route(notes,restored));assert(restored.category==1&&restored.full_distance==3&&restored.hidden_distance==18);
    assert(restored.steps[0].info==annotated.steps[0].info&&restored.steps[0].wall_text&&restored.steps[0].active_only);
    assert(restored.steps[0].info_position.z==56);
    assert(restored.stand_style.colour[0]==1&&restored.stand_style.colour[1]==.2f&&restored.stand_style.shape==2&&restored.stand_style.size==24);
    assert(restored.look_style.colour[0]==.7f&&restored.look_style.shape==1&&restored.look_style.size==16&&!restored.show_labels);
    for(int invalid=0;invalid<4;++invalid){
        auto broken=annotated;
        if(invalid==0)broken.stand_style.colour[0]=1.1f;
        if(invalid==1)broken.look_style.shape=3;
        if(invalid==2)broken.stand_style.size=0;
        if(invalid==3)broken.look_style.colour[2]=std::numeric_limits<float>::quiet_NaN();
        std::stringstream bad;write_route(bad,broken);assert(!read_route(bad,restored));
    }
    std::istringstream v2("HOPOUT 2 \"Previous route\" 1 5 40\nstep 0 0 0 0 0 1 .65 .06 1 \"Jump\" 0 0 0 0 0 \"Tip\"\n");
    assert(read_route(v2,restored));assert(restored.category==1&&restored.show_labels&&restored.stand_style.size==10&&restored.steps[0].info=="Tip");
    std::istringstream legacy("HOPOUT 1 \"Old route\"\nstep 0 0 0 0 0 1 .65 .06 1 \"Jump\"\n");
    assert(read_route(legacy,restored));assert(restored.category==0&&restored.steps[0].info.empty());
    assert(proximity_alpha(0,5,40)==1&&proximity_alpha(5,5,40)==1&&proximity_alpha(40,5,40)==0&&proximity_alpha(100,5,40)==0);
    assert(std::abs(proximity_alpha(22.5f,5,40)-.5f)<.0001f);
    assert(proximity_alpha(10,40,5)==0&&proximity_alpha(std::numeric_limits<float>::quiet_NaN(),5,40)==0);
    assert(proximity_alpha(10,5,40)>proximity_alpha(20,5,40));
    for(const auto *bad:{"HOPOUT 2 \"bad\" 2 5 40", "HOPOUT 2 \"bad\" 1 40 5", "HOPOUT 2 \"bad\" 1 5 5"}) {
        std::istringstream in(bad);assert(!read_route(in,restored));
    }
    auto invalid_note=annotated;invalid_note.steps[0].info=std::string(601,'x');
    std::stringstream long_note;write_route(long_note,invalid_note);assert(!read_route(long_note,restored));
    for(const auto *bad:{"HOPOUT 2 \"bad\"", "HOPOUT 1 \"empty\"", "HOPOUT 1 \"bad\" step 1 2", "HOPOUT 1 \"bad\" junk"}) {
        std::istringstream in(bad);assert(!read_route(in,loaded));assert(loaded.steps.size()==2);
    }
    s.radius=-1;assert(!valid(s));s.radius=.65f;
    s.look.x=std::numeric_limits<float>::infinity();assert(!valid(s));s.look={0,0,1};
    route too_many{"Too many",std::vector<step>(65,s)};
    std::stringstream excess;write_route(excess,too_many);assert(!read_route(excess,loaded));
    // A simple perspective matrix: clip.w = z, near=.1, far=100.
    float m[16]={1,0,0,0, 0,1,0,0, 0,0,1.001001f,1, 0,0,-.1001001f,0};
    float inv[16],x,y,w;assert(inverse(m,inv));
    assert(project({0,0,1},m,x,y,w)&&std::abs(x)<.0001f&&std::abs(y)<.0001f);
    assert(!project({0,0,-1},m,x,y,w));
    assert(project({1,0,1},m,x,y,w)&&x>.99f);
    vec3 target;assert(aim_point(inv,4,target));assert(std::abs(target.z-4.f)<.001f);
    float reversed[16]={1,0,0,0, 0,1,0,0, 0,0,-.001001f,1, 0,0,.1001001f,0};
    assert(inverse(reversed,inv));assert(aim_point(inv,4,target));assert(std::abs(target.z-4.f)<.001f);
    // Translate the camera to (10,20,30), still looking along +Z.
    float translated[16]={1,0,0,0, 0,1,0,0, 0,0,1.001001f,1, -10,-20,-30.13013f,-30};
    assert(inverse(translated,inv));assert(aim_point(inv,4,target));
    assert(std::abs(target.x-10)<.001f&&std::abs(target.y-20)<.001f&&std::abs(target.z-34)<.001f);
    float singular[16]={};assert(!inverse(singular,inv));
    coach c;
    c.update(r,true,{10,0,0},m,.1f,true);assert(c.state==phase::stand&&c.index==0);
    auto off=r;off.steps[0].look={1,0,1};c.update(off,true,{0,0,0},m,.1f,true);assert(c.state==phase::look);
    c.update(r,true,{0,0,0},m,.1f,true);assert(c.state==phase::countdown);
    c.update(r,false,{0,0,0},m,.1f,true);assert(c.held==0&&c.index==0);
    for(int i=0;i<6;++i)c.update(r,true,{0,0,0},m,.1f,false);
    assert(c.state==phase::action);
    c.update(r,true,{10,0,0},m,.1f,false);assert(c.state==phase::action);
    c.update(r,true,{10,0,0},m,.1f,true);assert(c.index==1&&c.state==phase::stand);
    for(int i=0;i<6;++i)c.update(r,true,{0,0,0},m,.1f,false);
    c.update(r,true,{10,0,0},m,.1f,true);assert(c.state==phase::complete&&c.index==2);
    c.reset();c.update(r,true,{0,0,0},m,100,false);assert(c.state==phase::countdown); // No frame-stall skip.
    c.update(r,true,{2,0,0},m,.1f,false);assert(c.held==0); // Leaving marker cancels countdown.
    std::cout << "PASS: v1/v2/v3 routes, marker styles/validation, categories, multiline wall info, fade bounds, malformed data, projection/ray math, alignment, telemetry loss, manual completion, frame-stall guard\n";
}
