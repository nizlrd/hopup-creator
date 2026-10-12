#pragma once
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace hopout {
struct vec3 { float x = 0, y = 0, z = 0; };
inline bool valid(vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
        std::abs(v.x) < 20000 && std::abs(v.y) < 20000 && std::abs(v.z) < 10000;
}
inline float distance(vec3 a, vec3 b) {
    return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
}
struct step {
    vec3 stand, look;
    float radius = .65f, aim = .06f, delay = 1;
    std::string action = "Mantle now using your usual game control. Press F8 after landing.";
    std::string info;
    vec3 info_position;
    bool wall_text=false, active_only=false;
};
struct marker_style {
    float colour[3]={1,1,1};
    int shape=0; // circle, diamond, square
    float size=10;
};
inline bool valid(const marker_style &style) {
    for(float value:style.colour)if(!std::isfinite(value)||value<0||value>1)return false;
    return style.shape>=0&&style.shape<=2&&std::isfinite(style.size)&&style.size>=6&&style.size<=36;
}
struct route {
    std::string name; std::vector<step> steps;
    int category=0; // 0: Hopouts, 1: Hopups
    float full_distance=5, hidden_distance=40;
    marker_style stand_style={{74/255.f,236/255.f,157/255.f},0,10};
    marker_style look_style={{88/255.f,188/255.f,1},1,10};
    bool show_labels=true;
    std::string stand_label="STAND HERE", look_label="LOOK HERE";
    bool shown=true;
    std::wstring source_path; // Runtime identity, never taken from route contents.
};
inline bool valid_fade(float full, float hidden) {
    return std::isfinite(full)&&std::isfinite(hidden)&&full>=0&&hidden>full&&hidden<=500;
}
inline float proximity_alpha(float distance, float full, float hidden) {
    if(!std::isfinite(distance)||!valid_fade(full,hidden))return 0;
    const float t=std::clamp((hidden-distance)/(hidden-full),0.f,1.f);
    return t*t*(3-2*t);
}
inline bool valid(const step &s) {
    return valid(s.stand) && valid(s.look) && distance(s.stand,s.look) > .1f &&
        std::isfinite(s.radius) && s.radius >= .1f && s.radius <= 5 &&
        std::isfinite(s.aim) && s.aim >= .01f && s.aim <= .3f &&
        std::isfinite(s.delay) && s.delay >= 0 && s.delay <= 10 &&
        !s.action.empty() && s.action.size() <= 240 && s.action.find_first_of("\r\n") == std::string::npos &&
        s.info.size()<=600 && valid(s.info_position);
}
inline bool read_route(std::istream &in, route &out) {
    route candidate; std::string magic; int version = 0;
    if (!(in >> magic >> version >> std::quoted(candidate.name)) || magic != "HOPOUT" || (version < 1 || version > 4) ||
        candidate.name.empty() || candidate.name.size() > 80 || candidate.name.find_first_of("\r\n") != std::string::npos) return false;
    if(version>=2 && (!(in>>candidate.category>>candidate.full_distance>>candidate.hidden_distance)||
        candidate.category<0||candidate.category>1||!valid_fade(candidate.full_distance,candidate.hidden_distance)))return false;
    if(version>=3) {
        auto style=[&](marker_style &s){return bool(in>>s.colour[0]>>s.colour[1]>>s.colour[2]>>s.shape>>s.size)&&valid(s);};
        int labels=0;
        if(!style(candidate.stand_style)||!style(candidate.look_style)||!(in>>labels)||labels<0||labels>1)return false;
        candidate.show_labels=labels!=0;
    }
    if(version>=4) {
        int shown=0;
        if(!(in>>std::quoted(candidate.stand_label)>>std::quoted(candidate.look_label)>>shown)||shown<0||shown>1)return false;
        for(const auto &label:{candidate.stand_label,candidate.look_label})
            if(label.size()>120||label.find_first_of("\r\n")!=std::string::npos)return false;
        candidate.shown=shown!=0;
    }
    while (in >> magic) {
        step s;
        if (magic != "step" || candidate.steps.size() >= 64 ||
            !(in >> s.stand.x >> s.stand.y >> s.stand.z >> s.look.x >> s.look.y >> s.look.z >>
                s.radius >> s.aim >> s.delay >> std::quoted(s.action)) || !valid(s)) return false;
        if(version>=2) {
            int wall=0,active=0;
            if(!(in>>wall>>active>>s.info_position.x>>s.info_position.y>>s.info_position.z>>std::quoted(s.info))||
                wall<0||wall>1||active<0||active>1||!valid(s))return false;
            s.wall_text=wall!=0;s.active_only=active!=0;
        }
        candidate.steps.push_back(s);
    }
    if (!in.eof() || candidate.steps.empty()) return false;
    out = std::move(candidate); return true;
}
inline void write_route(std::ostream &out, const route &r) {
    out << std::setprecision(9) << "HOPOUT 4 " << std::quoted(r.name) << ' ' << r.category << ' '
        << r.full_distance << ' ' << r.hidden_distance;
    for(const auto &s:{r.stand_style,r.look_style})out<<' '<<s.colour[0]<<' '<<s.colour[1]<<' '<<s.colour[2]<<' '<<s.shape<<' '<<s.size;
    out<<' '<<r.show_labels<<' '<<std::quoted(r.stand_label)<<' '<<std::quoted(r.look_label)<<' '<<r.shown<<'\n';
    for (const auto &s : r.steps)
        out << "step " << s.stand.x << ' ' << s.stand.y << ' ' << s.stand.z << ' '
            << s.look.x << ' ' << s.look.y << ' ' << s.look.z << ' ' << s.radius << ' ' << s.aim << ' ' << s.delay << ' '
            << std::quoted(s.action) << ' ' << s.wall_text << ' ' << s.active_only << ' '
            << s.info_position.x << ' ' << s.info_position.y << ' ' << s.info_position.z << ' ' << std::quoted(s.info) << '\n';
}
// Matrices use the same row-vector convention as the existing FiveM minimap.
inline bool project(vec3 p, const float *m, float &x, float &y, float &w) {
    x=p.x*m[0]+p.y*m[4]+p.z*m[8]+m[12];
    y=p.x*m[1]+p.y*m[5]+p.z*m[9]+m[13];
    w=p.x*m[3]+p.y*m[7]+p.z*m[11]+m[15];
    if (!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(w)||w<=.01f) return false;
    x/=w; y/=w; return std::isfinite(x)&&std::isfinite(y);
}
inline bool inverse(const float *m, float *result) {
    double a[4][8] = {};
    for(int r=0;r<4;++r) for(int c=0;c<4;++c) {
        if(!std::isfinite(m[r*4+c])) return false;
        a[r][c]=m[r*4+c]; a[r][c+4]=r==c?1:0;
    }
    for(int c=0;c<4;++c) {
        int pivot=c; for(int r=c+1;r<4;++r) if(std::abs(a[r][c])>std::abs(a[pivot][c]))pivot=r;
        if(std::abs(a[pivot][c])<1e-10)return false;
        for(int j=0;j<8;++j)std::swap(a[c][j],a[pivot][j]);
        const double d=a[c][c]; for(int j=0;j<8;++j)a[c][j]/=d;
        for(int r=0;r<4;++r) if(r!=c) {const double f=a[r][c];for(int j=0;j<8;++j)a[r][j]-=f*a[c][j];}
    }
    for(int r=0;r<4;++r)for(int c=0;c<4;++c) {result[r*4+c]=static_cast<float>(a[r][c+4]);if(!std::isfinite(result[r*4+c]))return false;}
    return true;
}
// Recover the perspective camera origin from clip (0,0,1,0), then a finite
// point on the center ray. Works for normal and reversed-Z depth conventions.
inline bool aim_point(const float *inv, float metres, vec3 &out) {
    const float origin_w=inv[11], ray_w=.5f*inv[11]+inv[15];
    if(!std::isfinite(origin_w)||std::abs(origin_w)<1e-8f||
       !std::isfinite(ray_w)||std::abs(ray_w)<1e-8f||!std::isfinite(metres)||metres<=0)return false;
    const vec3 origin={inv[8]/origin_w,inv[9]/origin_w,inv[10]/origin_w};
    const vec3 ray={(.5f*inv[8]+inv[12])/ray_w,(.5f*inv[9]+inv[13])/ray_w,(.5f*inv[10]+inv[14])/ray_w};
    const float length=distance(origin,ray);
    if(!std::isfinite(length)||length<1e-6f)return false;
    out={origin.x+(ray.x-origin.x)*metres/length,
         origin.y+(ray.y-origin.y)*metres/length,
         origin.z+(ray.z-origin.z)*metres/length};
    return valid(out);
}
enum class phase { stand, look, countdown, action, complete };
struct coach {
    size_t index=0; phase state=phase::stand; float held=0;
    void reset() {index=0;state=phase::stand;held=0;}
    void update(const route &r, bool live, vec3 player, const float *matrix, float dt, bool confirm) {
        if(index>=r.steps.size()) {state=phase::complete;return;}
        if(!live) {held=0;state=phase::stand;return;}
        // Once the cue is issued, movement is expected. Completion is manual.
        if(state==phase::action) {if(confirm){++index;held=0;state=index==r.steps.size()?phase::complete:phase::stand;}return;}
        const auto &s=r.steps[index]; float x=0,y=0,w=0;
        if(distance(player,s.stand)>s.radius){state=phase::stand;held=0;return;}
        if(!project(s.look,matrix,x,y,w)||x*x+y*y>s.aim*s.aim){state=phase::look;held=0;return;}
        held+=std::clamp(dt,0.f,.1f);
        state=held>=s.delay+.5f?phase::action:phase::countdown;
    }
};
}
