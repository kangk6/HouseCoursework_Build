#pragma once
#include "CoreMinimal.h"

// Stable IDs keep saved UI selections and the original living/master tests valid.
namespace CourseworkRooms
{
struct FRoom { const TCHAR* Name; const TCHAR* Floor; FVector Position; FVector Focus; };
inline const FRoom Rooms[]={
    {TEXT("客厅"),TEXT("一楼"),{-2100,9870,151.5},{-2335,9925,160}},
    {TEXT("主卧"),TEXT("二楼"),{-2280,9470,413},{-2340,9293,380}},
    {TEXT("厨房"),TEXT("一楼"),{-1990,9450,150},{-1668,9400,220}},
    {TEXT("儿童房"),TEXT("二楼"),{-1792.57,9116.673,413.394},{-1804,9323,375}},
    {TEXT("书房"),TEXT("二楼"),{-1892,10300,413},{-1760,10644,430}},
    {TEXT("卫生间"),TEXT("一楼"),{-1770,10228,150},{-1638,10212,220}},
    {TEXT("卫生间"),TEXT("二楼"),{-2395,9620,413},{-2413,9693,400}},
    {TEXT("餐厅"),TEXT("一楼"),{-2200,10400,150},{-2423,10594,231}},
    {TEXT("走廊楼梯"),TEXT("一、二楼"),{-2114,9980,413},{-2003,9857,495}}
};
inline constexpr int32 Count=UE_ARRAY_COUNT(Rooms);
inline const TCHAR* Ids[]={TEXT("living"),TEXT("master"),TEXT("kitchen"),TEXT("child"),TEXT("study"),TEXT("bath_ground"),TEXT("bath_upper"),TEXT("dining"),TEXT("hall_stairs")};
static_assert(UE_ARRAY_COUNT(Ids)==Count,"Each lighting room needs a stable save identifier");
inline constexpr int32 DisplayOrder[]={0,2,7,5,1,3,4,6,8};
inline int32 DisplayIndex(int32 Room){for(int32 I=0;I<Count;++I)if(DisplayOrder[I]==Room)return I;return 0;}
inline int32 Clamp(int32 Room){return FMath::Clamp(Room,0,Count-1);}
inline const FRoom& Get(int32 Room){return Rooms[Clamp(Room)];}
inline constexpr float RowY(int32 Room){return 196.f+68.f*Room;}
}
