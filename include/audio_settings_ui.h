#ifndef SHADOW_HUNT_AUDIO_SETTINGS_UI_H
#define SHADOW_HUNT_AUDIO_SETTINGS_UI_H
#include "audio_preferences.h"
#include "ui_interaction.h"

namespace AudioSettings
{
constexpr float Width=470,Height=430;
struct State { int selected=0,dragging=-1;bool dirty=false,saveFailed=false; };
struct InputResult { bool changed=false,closed=false,save=false; };
inline Camera2D Layout(){return UI::Centered(GetScreenWidth(),GetScreenHeight(),Width,Height);}
inline Rectangle Track(int row){return {40,112.0f+row*76,390,16};}
inline Rectangle Mute(int row){return {40.0f+row*205,316,185,34};}
inline Rectangle Back(){return {155,368,160,36};}
inline InputResult Input(State& state,AudioPreferences::Settings& settings)
{
    InputResult result;
    const bool wasDragging=state.dragging>=0;
    const Camera2D layout=Layout();
    if(IsKeyPressed(KEY_ESCAPE)||UI::Click(Back(),layout)||(state.selected==5&&UI::ConfirmKey()))
    {
        state.dragging=-1;result.closed=true;result.save=true;UI::ScreenChanged();return result;
    }
    if(IsKeyPressed(KEY_UP)||IsKeyPressed(KEY_W))state.selected=(state.selected+5)%6;
    if(IsKeyPressed(KEY_DOWN)||IsKeyPressed(KEY_S)||IsKeyPressed(KEY_TAB))state.selected=(state.selected+1)%6;
    float* values[]={&settings.master,&settings.music,&settings.sfx};
    for(int i=0;i<3;++i)
    {
        auto hit=Track(i);hit.y-=12;hit.height+=24;
        if(UI::Click(hit,layout)){state.selected=i;state.dragging=i;}
    }
    if(state.dragging>=0)
    {
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            const auto track=Track(state.dragging);const auto cursor=GetScreenToWorld2D(GetMousePosition(),layout);
            const float value=std::clamp((cursor.x-track.x)/track.width,0.0f,1.0f);
            if(value!=*values[state.dragging]){*values[state.dragging]=value;result.changed=true;}
            UI::consumed=true;
        }
        else state.dragging=-1;
    }
    if(state.selected<3)
    {
        const float step=(IsKeyPressed(KEY_RIGHT)||IsKeyPressedRepeat(KEY_RIGHT)?0.02f:0)
                         -(IsKeyPressed(KEY_LEFT)||IsKeyPressedRepeat(KEY_LEFT)?0.02f:0);
        if(step!=0){const float old=*values[state.selected];*values[state.selected]=std::clamp(old+step,0.0f,1.0f);result.changed|=old!=*values[state.selected];}
    }
    for(int i=0;i<2;++i)
        if(UI::Click(Mute(i),layout)||(state.selected==3+i&&UI::ConfirmKey()))
        {
            bool& muted=i==0?settings.muteMusic:settings.muteSfx;muted=!muted;
            state.selected=3+i;result.changed=true;UI::consumed=true;
        }
    settings.Validate();state.dirty|=result.changed;
    // Save after a change/release, not every frame after a failed write.
    result.save=state.dirty&&state.dragging<0&&(result.changed||wasDragging);
    return result;
}
inline void Draw(const State& state,const AudioPreferences::Settings& settings,int width,int height)
{
    DrawRectangle(0,0,width,height,{7,6,11,215});
    const Camera2D layout=UI::Centered(width,height,Width,Height);BeginMode2D(layout);
    Hud::DrawStonePanel({0,0,Width,Height});
    Hud::DrawCenteredText("Audio Settings",235,25,28,Hud::Gold);Hud::DrawDivider(36,70,398);
    const char* labels[]={"Master Volume","Music Volume","Sound Effects Volume"};
    const float values[]={settings.master,settings.music,settings.sfx};
    for(int i=0;i<3;++i)
    {
        const Rectangle track=Track(i);Rectangle hit=track;hit.y-=12;hit.height+=24;
        const bool highlight=state.selected==i||UI::Hover(hit,layout);
        DrawText(labels[i],40,static_cast<int>(track.y)-26,18,highlight?Hud::Gold:Hud::Text);
        DrawText(TextFormat("%d%%",static_cast<int>(std::lround(values[i]*100))),377,static_cast<int>(track.y)-26,18,Hud::Gold);
        DrawRectangleRounded(track,0.5f,6,{32,27,27,255});
        DrawRectangleRounded({track.x,track.y,track.width*values[i],track.height},0.5f,6,{147,107,53,255});
        DrawRectangleRoundedLinesEx(track,0.5f,6,highlight?2:1,highlight?Hud::Gold:Hud::Border);
        DrawCircleV({track.x+track.width*values[i],track.y+track.height/2},highlight?10:8,highlight?Hud::Gold:Hud::Brass);
    }
    UI::Button(Mute(0),settings.muteMusic?"Music: MUTED":"Mute Music",state.selected==3,layout,16);
    UI::Button(Mute(1),settings.muteSfx?"SFX: MUTED":"Mute SFX",state.selected==4,layout,16);
    UI::Button(Back(),"Back",state.selected==5,layout);
    Hud::DrawCenteredText(state.saveFailed?"Could not save settings.cfg":"Up/Down: select   Left/Right: adjust",235,412,12,state.saveFailed?RED:Hud::DimText);
    EndMode2D();
}
}
#endif
