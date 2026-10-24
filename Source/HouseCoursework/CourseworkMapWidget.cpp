#include "CourseworkMapWidget.h"
#include "CourseworkHUD.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Input/Reply.h"
#include "MapWalls.inl"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Engine/World.h"
#include "Misc/Paths.h"

namespace
{
struct FMapMarker { FVector2D World; int32 Floor; int32 Destination; const TCHAR* Label; };
const FMapMarker Markers[] = {
    { {-2205,8000},0,0,TEXT("外观") }, { {-1990,9800},0,1,TEXT("客厅") },
    { {-1990,9450},0,2,TEXT("厨房") }, { {-2190,10070},1,3,TEXT("二楼走廊") },
    { {-2205,8890},0,4,TEXT("入口") }, { {-2190,10350},0,-1,TEXT("楼梯") },
    { {-2190,10350},1,-1,TEXT("楼梯") }
};
// Room labels are located using the beds and toilets in the saved main map.
const FMapMarker MapRoomMarkers[] = {
    { {-1765,10230},0,-1,TEXT("卫生间") },
    { {-2320,9320},1,-1,TEXT("卧室") },
    { {-1800,9320},1,-1,TEXT("儿童房") },
    { {-2350,9690},1,-1,TEXT("卫生间") }
};

class SCourseworkMap : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCourseworkMap) {} SLATE_END_ARGS()
    void Construct(const FArguments&, ACourseworkHUD* HUD, bool Large)
    {
        Owner = HUD; bLarge = Large;
        RegisterActiveTimer(0.1f,FWidgetActiveTimerDelegate::CreateLambda([this](double,float) { Invalidate(EInvalidateWidgetReason::Paint); return EActiveTimerReturnType::Continue; }));
    }
    virtual FVector2D ComputeDesiredSize(float) const override { return bLarge ? FVector2D(840,360) : FVector2D(200,132); }
    FVector Player() const
    {
        if (Owner.IsValid()) if (auto* PC=Owner->GetOwningPlayerController()) if (auto* P=PC->GetPawn()) return P->GetActorLocation();
        return FVector(-2205,8890,160);
    }
    FVector2D Project(const FVector2D& P, const FVector2D& Size) const
    {
        const FVector Position=Player();
        const bool Energy=bLarge&&Owner.IsValid()&&Owner->IsEnergyMapLayer();
        const FVector2D Center=bLarge ? (Energy?FVector2D(-2050,9880):FVector2D(-2050,9440)) : FVector2D(Position.X,Position.Y);
        const float Scale=bLarge ? (Energy?FMath::Min((Size.X-140)/2200.f,(Size.Y-90)/1050.f):FMath::Min((Size.X-48)/3300.f,(Size.Y-68)/1200.f)) : 0.15f;
        return FVector2D(Size.X/2, bLarge?(Size.Y+8)/2:Size.Y*.56f)+FVector2D(P.Y-Center.Y,Center.X-P.X)*Scale;
    }
    int32 DisplayFloor() const { return Owner.IsValid() ? (bLarge?Owner->GetMapFloor():Owner->GetPlayerFloor()) : 0; }
    FVector2D LabelOffset(int32 I) const
    {
        if(I==3) return FVector2D(-80,-26);
        if(I==5 || I==6) return FVector2D(8,14);
        return FVector2D(8,-8);
    }
    int32 HitMarker(FVector2D Mouse, FVector2D Size) const
    {
        if (!bLarge || (Owner.IsValid() && (Owner->IsSafetyDrillRunning()||Owner->IsEnergyMapLayer()))) return -1;
        for (int32 I=0;I<UE_ARRAY_COUNT(Markers);++I)
            if (Markers[I].Floor==DisplayFloor() && Markers[I].Destination>=0)
            {
                const FVector2D P=Project(Markers[I].World,Size);
                const FVector2D Label=P+LabelOffset(I);
                if(FVector2D::Distance(Mouse,P)<20 || (Mouse.X>=Label.X-4 && Mouse.X<=Label.X+FCString::Strlen(Markers[I].Label)*14 && Mouse.Y>=Label.Y-4 && Mouse.Y<=Label.Y+20)) return I;
            }
        return -1;
    }
    virtual FReply OnMouseMove(const FGeometry& G,const FPointerEvent& E) override
    {
        Hover=HitMarker(G.AbsoluteToLocal(E.GetScreenSpacePosition()),G.GetLocalSize());
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Unhandled();
    }
    virtual void OnMouseLeave(const FPointerEvent& E) override { Hover=-1; SLeafWidget::OnMouseLeave(E); }
    virtual FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E) override
    {
        if (E.GetEffectingButton()!=EKeys::LeftMouseButton || !Owner.IsValid()) return FReply::Unhandled();
        if (!bLarge) { Owner->ToggleMap(); return FReply::Handled(); }
        const FVector2D P=G.AbsoluteToLocal(E.GetScreenSpacePosition()),Size=G.GetLocalSize();
        if(P.X>=Size.X-164&&P.X<=Size.X-12&&P.Y>=8&&P.Y<=39){Owner->ToggleEnergyMapLayer();return FReply::Handled();}
        const int32 Hit=HitMarker(P,Size);
        if (Hit>=0) { Owner->GoToLocation(Markers[Hit].Destination); return FReply::Handled(); }
        return FReply::Handled();
    }
    bool ExerciseDestination(int32 Destination)
    {
        if(!bLarge||!Owner.IsValid()||Owner->GetPanel()!=5)return false;
        const FGeometry G=GetCachedGeometry();if(G.GetLocalSize().X<200)return false;
        for(const auto& Marker:Markers)if(Marker.Destination==Destination&&Marker.Floor==DisplayFloor())
        {
            const FVector2D P=G.LocalToAbsolute(Project(Marker.World,G.GetLocalSize()));
            const TSet<FKey> Keys={EKeys::LeftMouseButton};const FPointerEvent E(0,P,P,Keys,EKeys::LeftMouseButton,0,FModifierKeysState());
            return OnMouseButtonDown(G,E).IsEventHandled();
        }
        return false;
    }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        const FVector2D Size=G.GetLocalSize();
        const FSlateBrush* Brush=FCoreStyle::Get().GetBrush("WhiteBrush");
        auto Box=[&](FVector2D P,FVector2D S,FLinearColor C,int32 L) { FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(S,FSlateLayoutTransform(P)),Brush,ESlateDrawEffect::None,C); };
        auto Text=[&](FVector2D P,const FString& T,int32 Font,FLinearColor C,int32 L)
        {
            FSlateDrawElement::MakeText(Out,L,G.ToPaintGeometry(Size,FSlateLayoutTransform(P+FVector2D(0,1))),T,FCoreStyle::GetDefaultFontStyle("Regular",Font),ESlateDrawEffect::None,FLinearColor(0,0,0,.65f));
            FSlateDrawElement::MakeText(Out,L+1,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),T,FCoreStyle::GetDefaultFontStyle("Regular",Font),ESlateDrawEffect::None,C);
        };
        auto Line=[&](const TArray<FVector2D>& Points,FLinearColor C,float Width,int32 L) { FSlateDrawElement::MakeLines(Out,L,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,C,true,Width); };
        auto Polygon=[&](const TArray<FVector2D>& Points,FLinearColor C,int32 L)
        {
            TArray<FSlateVertex> Vertices; TArray<SlateIndex> Indices;
            for(const auto& P:Points) Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(P),FVector2f(.5f,.5f),C.ToFColor(true)));
            for(int32 I=1;I+1<Points.Num();++I) { Indices.Add(0); Indices.Add(I); Indices.Add(I+1); }
            FSlateDrawElement::MakeCustomVerts(Out,L,FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Brush),Vertices,Indices,nullptr,0,0);
        };
        const FLinearColor Paper(.88f,.86f,.8f,1), MapMuted(.43f,.43f,.40f,1), Accent(.59f,.57f,.46f,1);
        Box({2,3},Size,FLinearColor(0,0,0,.22f),Layer);
        Box({0,0},Size,FLinearColor(.009f,.010f,.010f,bLarge?.98f:.72f),Layer+1);
        Out.PushClip(FSlateClippingZone(G.ToPaintGeometry(Size-FVector2D(4,4),FSlateLayoutTransform(FVector2D(2,2)))));
        const int32 Floor=DisplayFloor();
        auto Room=[&](FVector2D Min,FVector2D Max,FLinearColor Color)
        {
            const FVector2D A=Project({Max.X,Min.Y},Size),B=Project({Min.X,Max.Y},Size);
            Box(A,B-A,Color,Layer+2);
        };
        Room({-2470,9030},{-1630,10730},FLinearColor(.22f,.22f,.205f,.95f));
        if(Floor==0) Room({-1885,10020},{-1630,10490},FLinearColor(.28f,.28f,.26f,1));
        else
        {
            Room({-2470,9030},{-2185,9550},FLinearColor(.28f,.28f,.26f,1));
            Room({-1903,9030},{-1630,9370},FLinearColor(.27f,.27f,.25f,1));
            Room({-2470,9550},{-2185,9849},FLinearColor(.16f,.16f,.15f,1));
        }
        Room({-2270,10160},{-2100,10460},FLinearColor(.12f,.12f,.11f,1));
        for (const auto& Segment : MapWalls)
            if (int32(Segment[0])==Floor) Line({Project({Segment[1],Segment[2]},Size),Project({Segment[3],Segment[4]},Size)},FLinearColor(.032f,.033f,.031f,1),bLarge?3.f:2.2f,Layer+3);
        for(int32 Y=10170;Y<=10450;Y+=70) Line({Project({-2250,float(Y)},Size),Project({-2130,float(Y)},Size)},FLinearColor(.25f,.25f,.23f,1),1,Layer+3);
        if(bLarge&&!Owner->IsEnergyMapLayer())
        {
            for(const auto& RoomLabel:MapRoomMarkers) if(RoomLabel.Floor==Floor)
                Text(Project(RoomLabel.World,Size)-FVector2D(FCString::Strlen(RoomLabel.Label)*6,7),RoomLabel.Label,12,Paper,Layer+4);
            for(int32 I=0;I<UE_ARRAY_COUNT(Markers);++I)
            {
                const auto& Marker=Markers[I]; if(Marker.Floor!=Floor) continue;
                const FVector2D P=Project(Marker.World,Size);
                const FLinearColor Color=Marker.Destination<0?MapMuted:(Hover==I?FLinearColor::White:Paper);
                Box(P-FVector2D(2,2),{4,4},Color,Layer+4);
                const FVector2D Label=P+LabelOffset(I);
                Box(Label-FVector2D(3,1),{float(FCString::Strlen(Marker.Label)*14+6),20},FLinearColor(.012f,.013f,.013f,.8f),Layer+4);
                Text(Label,Marker.Label,13,Color,Layer+5);
            }
        }
        if(bLarge&&Owner.IsValid()&&Owner->IsEnergyMapLayer())
        {
            const FLinearColor Flow(.86f,.69f,.40f),Idle(.26f,.28f,.27f);
            auto EnergyText=[&](FVector2D P,const FString& T,FLinearColor C){
                const FSlateFontInfo Font(FPaths::ProjectContentDir()/TEXT("Slate/Fonts/LXGWWenKai-Light.ttf"),16);
                FSlateDrawElement::MakeText(Out,Layer+10,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),T,Font,ESlateDrawEffect::None,C);
            };
            const FVector2D Supply=Project({-2540,8890},Size),Hub=Project({-2040,8990},Size);
            const bool Powered=Owner->GetTotalWatts()>.001f;const FLinearColor Main=Powered?Flow:Idle;
            Line({Supply,Hub},Main,2,Layer+6);Box(Supply-FVector2D(4),{8,8},Main,Layer+7);
            if(Powered){const FVector2D D=Hub-Supply,Dir=D.GetSafeNormal(),Side(-Dir.Y,Dir.X);const float Phase=FMath::Fmod(Owner->GetWorld()->GetTimeSeconds()*25.f,32.f);for(float T=Phase;T<D.Size();T+=32){const FVector2D P=Supply+Dir*T;Line({P-Dir*3-Side*2,P,P-Dir*3+Side*2},Main,1.2f,Layer+7);}}
            EnergyText(Supply+FVector2D(-5,-20),TEXT("住宅电源"),Paper);
            EnergyText(Hub+FVector2D(5,-20),TEXT("照明配电"),Paper);
            Box(Hub-FVector2D(3),{6,6},Main,Layer+7);
            for(int32 I=0;I<CourseworkRooms::Count;++I)
            {
                const bool Upper=I==1||I==3||I==4||I==6||I==8;if(int32(Upper)!=Floor)continue;
                const auto& R=CourseworkRooms::Get(I);const float Power=Owner->GetRoomWatts(I);const bool Active=Power>.001f;
                const FVector2D End=Project({R.Position.X,R.Position.Y},Size),Joint=Project({-2040,R.Position.Y},Size);
                const TArray<FVector2D> Path={Hub,Joint,End};const FLinearColor C=Active?Flow:Idle;
                Line(Path,FLinearColor(0,0,0,.85f),4,Layer+5);Line(Path,C,Active?1.5f:1.f,Layer+6);
                if(Active)for(int32 J=1;J<Path.Num();++J)
                {
                    const FVector2D D=Path[J]-Path[J-1];const float Length=D.Size();if(Length<3)continue;
                    const FVector2D Dir=D/Length,Side(-Dir.Y,Dir.X);
                    const float Phase=FMath::Fmod(Owner->GetWorld()->GetTimeSeconds()*(18+32*Power/Owner->GetRoomRatedWatts(I)),32.f);
                    for(float T=Phase;T<Length;T+=32.f){const FVector2D P=Path[J-1]+Dir*T;Line({P-Dir*3-Side*2,P,P-Dir*3+Side*2},C,1.2f,Layer+7);}
                }
                Box(End-FVector2D(3),{6,6},C,Layer+7);
                const FString Label=FString::Printf(TEXT("%s %.1f W"),R.Name,Power);
                const FVector2D LabelAt=End+FVector2D(7,Upper?-24:8);
                Box(LabelAt-FVector2D(2),{float(Label.Len()*15+8),23},FLinearColor(.01f,.015f,.018f,.97f),Layer+9);
                EnergyText(LabelAt,Label,Active?Paper:Idle);
            }
            EnergyText({14,Size.Y-32},TEXT("照明用电估算"),Paper);
        }
        if(!Owner.IsValid() || Owner->GetPlayerFloor()==Floor)
        {
            const FVector P=Player(); const FVector2D Center=Project({P.X,P.Y},Size);
            float Yaw=0; if(Owner.IsValid()) if(auto* PC=Owner->GetOwningPlayerController()) Yaw=PC->GetControlRotation().Yaw;
            const float R=FMath::DegreesToRadians(Yaw);
            const FVector2D Forward(FMath::Sin(R),-FMath::Cos(R)),Right(FMath::Cos(R),FMath::Sin(R));
            Polygon({Center,Center+Forward*38-Right*20,Center+Forward*38+Right*20},FLinearColor(.8f,.8f,.75f,.09f),Layer+6);
            Polygon({Center+Forward*11,Center-Forward*6-Right*6,Center-Forward*2,Center-Forward*6+Right*6},FLinearColor::Black,Layer+7);
            Polygon({Center+Forward*9,Center-Forward*5-Right*4.8f,Center-Forward*1,Center-Forward*5+Right*4.8f},Paper,Layer+8);
        }
        if(Owner.IsValid() && Owner->IsSafetyActive())
        {
            const FLinearColor Escape(.41f,.85f,.68f),Alarm(.98f,.35f,.19f);
            // Fixed, schematic corridors from the collision-tested walking route.
            // These lines are not runtime pathfinding and do not open doors.
            auto Route=[&](const TArray<FVector2D>& World)
            {
                TArray<FVector2D> Points;for(const auto& P:World)Points.Add(Project(P,Size));
                Line(Points,FLinearColor(0,0,0,.85f),bLarge?6.f:4.f,Layer+6);
                Line(Points,Escape,bLarge?2.5f:1.6f,Layer+7);
                for(int32 I=1;I<Points.Num();++I)
                {
                    const FVector2D D=Points[I]-Points[I-1];if(D.Size()<16)continue;
                    const FVector2D Forward=D.GetSafeNormal(),Side(-Forward.Y,Forward.X),P=(Points[I]+Points[I-1])*.5f;
                    Line({P-Forward*4-Side*3,P,P-Forward*4+Side*3},Escape,1.5f,Layer+7);
                }
            };
            if(Floor==0)
            {
                const FVector Goal=Owner->GetAssemblyPoint();
                Route({{-2190,10520},{-1990,10520},{-1990,9170},{-2205,9170},{-2205,8700},{Goal.X,Goal.Y}});
                Route({{-1990,9450},{-1990,9170}});
                Route({{-2100,9870},{-1990,9800}});
                const FVector2D Hazard=Project(FVector2D(Owner->GetAlarmLocation().X,Owner->GetAlarmLocation().Y),Size);
                const float Pulse=6.f+2.f*FMath::Sin(float(Owner->GetWorld()->GetTimeSeconds())*4.f);
                TArray<FVector2D> Circle;for(int32 I=0;I<=24;++I){const float A=I*2*PI/24;Circle.Add(Hazard+FVector2D(FMath::Cos(A),FMath::Sin(A))*Pulse);}
                Line(Circle,Alarm,2.f,Layer+8);Box(Hazard-FVector2D(2,2),{4,4},Alarm,Layer+8);
                const FVector2D Exit=Project({Goal.X,Goal.Y},Size);
                Box(Exit-FVector2D(4,4),{8,8},Escape,Layer+8);
                if(bLarge){Text(Hazard+FVector2D(10,-22),TEXT("厨房报警"),12,Alarm,Layer+8);Text(Exit+FVector2D(-40,18),TEXT("室外集合点"),12,Escape,Layer+8);}
            }
            else
            {
                Route({{-2280,9470},{-2114,9470},{-2114,9760},{-2114,10070},{-2190,10070},{-2190,10450}});
                Route({{-1780,9116},{-2010,9116},{-2010,9470},{-2114,9470}});
                Route({{-1892,10300},{-1892,10070},{-2114,10070}});
                if(bLarge)Text(Project({-2190,10450},Size)+FVector2D(-100,28),TEXT("下楼 → 一楼出口"),12,Escape,Layer+8);
            }
            const FVector Person=Owner->GetResidentPosition();
            if((Person.Z>350?1:0)==Floor)
            {
                const FVector2D P=Project({Person.X,Person.Y},Size);const FLinearColor C=Owner->HasResidentGathered()?Escape:Paper;
                Polygon({P+FVector2D(0,-5),P+FVector2D(5,0),P+FVector2D(0,5),P+FVector2D(-5,0)},C,Layer+8);
                if(bLarge)Text(P+FVector2D(9,-18),Owner->HasResidentGathered()?TEXT("住户已集合"):TEXT("住户"),12,C,Layer+8);
            }
        }
        Out.PopClip();
        Line({{0,0},{Size.X,0},{Size.X,Size.Y},{0,Size.Y},{0,0}},FLinearColor(.5f,.5f,.46f,.22f),1,Layer+9);
        if(bLarge)
        {
            Text({14,12},Floor==0?TEXT("一楼 / 1F"):TEXT("二楼 / 2F"),14,Paper,Layer+10);
            Box({Size.X-164,8},{152,31},FLinearColor(.05f,.065f,.07f,.95f),Layer+10);
            Text({Size.X-155,14},Owner->IsSafetyDrillRunning()?TEXT("疏散优先"):Owner->IsEnergyMapLayer()?TEXT("G  返回导航图"):TEXT("G  查看用电流向"),11,Paper,Layer+11);

        }
        else
        {
            Text({8,Size.Y-18},Floor==0?TEXT("1F"):TEXT("2F"),10,Paper,Layer+10);
            Text({Size.X-17,6},TEXT("N"),9,Paper,Layer+10);
        }
        return Layer+13;
    }
private:
    TWeakObjectPtr<ACourseworkHUD> Owner;
    bool bLarge=false;
    int32 Hover=-1;
};
}

void UCourseworkMapWidget::Configure(ACourseworkHUD* HUD,bool bExpanded) { OwnerHUD=HUD; bLarge=bExpanded; }
TSharedRef<SWidget> UCourseworkMapWidget::RebuildWidget()
{
    MapSlate=SNew(SCourseworkMap,OwnerHUD.Get(),bLarge);
    return MapSlate.ToSharedRef();
}
void UCourseworkMapWidget::ReleaseSlateResources(bool Children)
{
    Super::ReleaseSlateResources(Children); MapSlate.Reset();
}


bool UCourseworkMapWidget::ExerciseLayerButton()
{
    if(!MapSlate||!bLarge)return false;
    const FGeometry G=MapSlate->GetCachedGeometry();if(G.GetLocalSize().X<200)return false;
    const FVector2D P=G.LocalToAbsolute(FVector2D(G.GetLocalSize().X-80,22));
    const TSet<FKey> Keys={EKeys::LeftMouseButton};const FPointerEvent E(0,P,P,Keys,EKeys::LeftMouseButton,0,FModifierKeysState());
    return MapSlate->OnMouseButtonDown(G,E).IsEventHandled();
}

bool UCourseworkMapWidget::ExerciseDestination(int32 Destination)
{
    return MapSlate&&StaticCastSharedPtr<SCourseworkMap>(MapSlate)->ExerciseDestination(Destination);
}
