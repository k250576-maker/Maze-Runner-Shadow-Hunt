#include "raylib.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int key=KEY_NULL,repeated=KEY_NULL;bool pressed=false,down=false;Vector2 cursor{};
bool Key(int code){return code==key;}
bool Repeat(int code){return code==repeated;}
bool MousePress(int code){return code==MOUSE_BUTTON_LEFT&&pressed;}
bool MouseDown(int code){return code==MOUSE_BUTTON_LEFT&&down;}
Vector2 Cursor(){return cursor;}
Vector2 Delta(){return pressed?Vector2{1,0}:Vector2{};}
#define IsKeyPressed Key
#define IsKeyDown Key
#define IsKeyPressedRepeat Repeat
#define IsMouseButtonPressed MousePress
#define IsMouseButtonDown MouseDown
#define GetMousePosition Cursor
#define GetMouseDelta Delta
#include "../include/audio_settings_ui.h"
#undef IsKeyPressed
#undef IsKeyDown
#undef IsKeyPressedRepeat
#undef IsMouseButtonPressed
#undef IsMouseButtonDown
#undef GetMousePosition
#undef GetMouseDelta
void Mouse(float fraction,int row)
{
    auto t=AudioSettings::Track(row);
    cursor=GetWorldToScreen2D({t.x+t.width*fraction,t.y+t.height/2},AudioSettings::Layout());
}
int main()
{
    InitWindow(720,608,"Audio settings input checks");SetExitKey(KEY_NULL);
    AudioSettings::State state;AudioPreferences::Settings values;
    const auto tick=[&](){UI::BeginFrame();return AudioSettings::Input(state,values);};
    Mouse(0,0);pressed=down=true;assert(tick().changed&&values.master==0);
    pressed=false;Mouse(1,0);assert(tick().changed&&values.master==1);
    Mouse(-0.2f,0);tick();assert(values.master==0);
    down=false;assert(tick().save&&state.dragging==-1);
    key=KEY_RIGHT;assert(tick().changed&&std::fabs(values.master-0.02f)<0.0001f);
    key=KEY_NULL;repeated=KEY_RIGHT;tick();assert(std::fabs(values.master-0.04f)<0.0001f);repeated=KEY_NULL;
    key=KEY_DOWN;tick();assert(state.selected==1);key=KEY_LEFT;tick();assert(std::fabs(values.music-0.23f)<0.0001f);
    key=KEY_NULL;state.selected=3;key=KEY_ENTER;assert(tick().changed&&values.muteMusic&&!values.muteSfx);
    key=KEY_NULL;tick();assert(values.muteMusic);
    state.selected=4;key=KEY_ENTER;tick();assert(values.muteSfx&&values.muteMusic);key=KEY_NULL;tick();
    const auto back=AudioSettings::Back();cursor=GetWorldToScreen2D({back.x+back.width/2,back.y+back.height/2},AudioSettings::Layout());
    pressed=down=true;assert(tick().closed);
    Mouse(0.9f,0);const float original=values.master;tick();assert(values.master==original&&state.dragging==-1);
    pressed=down=false;tick();
    SetWindowSize(1100,760);BeginDrawing();ClearBackground(BLACK);EndDrawing();
    Mouse(0.75f,2);pressed=down=true;tick();assert(std::fabs(values.sfx-0.75f)<0.0001f);
    pressed=down=false;tick();key=KEY_ESCAPE;assert(tick().closed);
    CloseWindow();std::puts("Settings input: dragging, bounds, keyboard/repeat, independent toggles, back, click-through suppression and resized layout passed.");
}
