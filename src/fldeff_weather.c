#include "global.h"
#include "fldeff.h"
#include "field_move.h"
#include "field_weather.h"   //HYDRA SetWeather
#include "party_menu.h"      //HYDRA gPostMenuFieldCallback, FieldCallback_PrepareFadeInFromMenu
#include "overworld.h"       //HYDRA gFieldCallback2
#include "constants/weather.h"

//HYDRA ---- Weather field moves (Rain Dance / Sunny Day / Sandstorm / Hail) ----------
// These behave like an HM field move: they can be assigned to the quick hotbar (gated on a party mon
// knowing the move + owning the matching TM -- see hydra_hotbar.c) and used from the party menu by any
// mon that knows the move. Using one sets the OVERWORLD weather, which then carries into battle via the
// existing overworld->battle weather path.
//
// Both entry points are handled: the party-menu path fades back in (gFieldCallback2) then runs the
// post-menu callback; the hotbar's in-place path calls gPostMenuFieldCallback directly. Either way the
// weather is applied by FieldCallback_SetWeather.

static u8 sPendingFieldWeather; //HYDRA overworld WEATHER_* to apply when the post-menu callback runs

static void FieldCallback_SetWeather(void)
{
    SetWeather(sPendingFieldWeather);
}

static bool32 SetUpWeatherFieldMove(u8 weather)
{
    sPendingFieldWeather = weather;
    gFieldCallback2 = FieldCallback_PrepareFadeInFromMenu;
    gPostMenuFieldCallback = FieldCallback_SetWeather;
    return TRUE;
}

bool32 SetUpFieldMove_RainDance(void) { return SetUpWeatherFieldMove(WEATHER_RAIN); }
bool32 SetUpFieldMove_SunnyDay(void)  { return SetUpWeatherFieldMove(WEATHER_DROUGHT); }
bool32 SetUpFieldMove_Sandstorm(void) { return SetUpWeatherFieldMove(WEATHER_SANDSTORM); }
bool32 SetUpFieldMove_Hail(void)      { return SetUpWeatherFieldMove(WEATHER_HAIL); }
