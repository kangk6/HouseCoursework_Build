#include "CourseworkControlWidget.h"
#include "CourseworkHUD.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"
#include "Input/Reply.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/FontCache.h"
#include "Misc/CommandLine.h"

namespace
{
const FLinearColor Ivory(.94f,.91f,.82f), Muted(.55f,.57f,.57f), Gold(.86f,.69f,.40f), Ink(.018f,.021f,.024f);
struct FControlHit { FSlateRect Rect; int32 Action; };
}

class SCourseworkControl : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCourseworkControl) {} SLATE_END_ARGS()
    void Construct(const FArguments&)
    {
        SetCanTick(false);
        FParse::Value(FCommandLine::Get(),TEXT("HouseTypography="),Typography);
        Typography=FMath::Clamp(Typography,0,4);
        bDetail=FParse::Param(FCommandLine::Get(),TEXT("HouseTypographyDetail"));
        bBodyDetail=FParse::Param(FCommandLine::Get(),TEXT("HouseBodyDetail"));
        FParse::Value(FCommandLine::Get(),TEXT("HouseBodyTypography="),BodyTypography);
        BodyTypography=FMath::Clamp(BodyTypography,0,3);
        RegisterActiveTimer(.1f,FWidgetActiveTimerDelegate::CreateLambda([this](double,float){if(Owner.IsValid()&&(Owner->GetPanel()==6||Owner->GetPanel()==7||Owner->GetPanel()==8||Owner->GetPanel()==9))Invalidate(EInvalidateWidgetReason::Paint);return EActiveTimerReturnType::Continue;}));
    }
    FSlateFontInfo FontFor(int32 Size,bool Headline=false,bool Number=false,int32 Variant=-1,int32 Body=-1) const
    {
        const int32 Style=Variant<0?Typography:Variant;
        if(Number)return FCoreStyle::GetDefaultFontStyle(Style==2?"Regular":"Light",Style>=3&&Size>=60?64:Size);
        FString File=Style==1?TEXT("LXGWNeoXiHei.ttf"):TEXT("SourceHanSansCN-Regular.otf");
        if(Style==2 && Headline && Size>=23)File=TEXT("SmileySans-Oblique.ttf");
        if(Style>=3)
        {
            const TCHAR* BodyFiles[]={TEXT("LXGWWenKai-Light.ttf"),TEXT("SourceHanSansCN-Regular.otf"),TEXT("LXGWNeoXiHei.ttf"),TEXT("SourceHanSerifCN-Light.otf")};
            File=BodyFiles[Body<0?BodyTypography:FMath::Clamp(Body,0,3)];
            if(Headline && Size>=23)
            {
                File=Style==4?TEXT("LongCang-Regular.ttf"):TEXT("LXGWWenKai-Light.ttf");
                Size=Size>=30?(Style==3?44:54):27;
            }
        }
        FSlateFontInfo Font(FPaths::ProjectContentDir()/TEXT("Slate/Fonts/")+File,Size);
        Font.LetterSpacing=Headline?(Style>=3?120:60):10;
        return Font;
    }
    FVector2D InkBounds(const FString& Text,const FSlateFontInfo& Font) const
    {
        // Align visible glyphs rather than the unequal ascender/descender boxes
        // of Chinese and Latin fonts. The short UI labels do not require ligatures.
        auto& Characters=FSlateApplication::Get().GetRenderer()->GetFontCache()->GetCharacterList(Font,1.f);
        float Top=MAX_flt,Bottom=-MAX_flt;
        for(TCHAR Character:Text)
        {
            const auto& Glyph=Characters.GetCharacter(Character,EFontFallback::FF_Max);
            if(!Glyph.Valid||Glyph.VSize==0)continue;
            const float Y=-Glyph.VerticalOffset+(Characters.GetMaxHeight()+Glyph.GlobalDescender)/Glyph.BitmapRenderScale;
            Top=FMath::Min(Top,Y);Bottom=FMath::Max(Bottom,Y+Glyph.VSize*Glyph.BitmapRenderScale);
        }
        return Top==MAX_flt?FVector2D(0,0):FVector2D(Top,Bottom);
    }
    void Configure(ACourseworkHUD* HUD) { Owner=HUD; }
    int32 Room=0;
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1600,900); }
    float Scale(const FGeometry& G) const { return FMath::Min(G.GetLocalSize().X/1600.f,G.GetLocalSize().Y/900.f); }
    FVector2D Origin(const FGeometry& G) const { return (G.GetLocalSize()-FVector2D(1600,900)*Scale(G))*.5f; }
    FVector2D DesignPoint(const FGeometry& G,FVector2D Absolute) const { return (G.AbsoluteToLocal(Absolute)-Origin(G))/Scale(G); }
    TArray<FControlHit> Targets() const
    {
        TArray<FControlHit> H;
        auto Add=[&](float X,float Y,float R,float B,int32 A){H.Add({FSlateRect(X,Y,R,B),A});};
        Add(286,16,376,85,6);Add(406,16,496,85,7);Add(526,16,616,85,8);Add(646,16,736,85,9);Add(1280,16,1400,78,101);Add(1480,16,1544,78,4);Add(46,838,178,898,100);Add(1420,838,1554,898,100);
        if(!Owner.IsValid()) return H;
        const int32 Mode=Owner->GetPanel();
        if(Mode==6){for(int32 I=0;I<CourseworkRooms::Count;++I)Add(52,CourseworkRooms::RowY(I),246,CourseworkRooms::RowY(I)+62,110+CourseworkRooms::DisplayOrder[I]);Add(296,455,690,535,200);Add(456,575,568,625,210);Add(568,575,682,625,211);Add(296,706,524,758,212);for(int32 I=0;I<3;++I)Add(1004+180*I,119,1176+180*I,169,213+I);}
        if(Mode==6){Add(1340,253,1516,305,700);Add(904,342,1198,420,701);Add(1214,342,1516,420,702);}
        if(Mode==8)
        {
            for(int32 I=0;I<CourseworkRooms::Count;++I)Add(52,CourseworkRooms::RowY(I),246,CourseworkRooms::RowY(I)+62,110+CourseworkRooms::DisplayOrder[I]);
            Add(300,650,472,708,500);Add(484,650,656,708,501);Add(668,650,840,708,502);Add(1050,650,1300,708,503);
            Add(300,570,600,612,506);
            for(int32 I=0;I<3;++I)Add(1130+92*I,195,1212+92*I,233,510+I);
        }
        if(Mode==9){Add(48,195,246,242,640);Add(48,247,246,294,641);}
        if(Mode==9&&Owner->GetComfortPage()==1){for(int32 I=0;I<CourseworkRooms::Count;++I)Add(52,320+49*I,246,367+49*I,110+CourseworkRooms::DisplayOrder[I]);Add(300,552,550,608,650);Add(566,552,816,608,651);Add(1050,686,1380,742,652);Add(566,686,816,742,653);Add(300,686,550,742,654);}
        if(Mode==9&&Owner->GetComfortPage()==0){Add(296,435,844,505,600);for(int32 I=0;I<4;++I)Add(300+136*I,552,428+136*I,608,610+I);Add(300,686,560,742,620);Add(1050,686,1380,742,621);}
        if(Mode==1) for(int32 I=0;I<6;++I) Add(300,288+I*64,682,347+I*64,300+I);
        if(Mode==4){Add(296,325,690,395,220);Add(296,496,690,566,221);}
        if(Mode==7){for(int32 I=0;I<3;++I)Add(300+180*I,585,470+180*I,641,400+I);Add(300,695,560,751,403);}
        return H;
    }
    int32 Hit(FVector2D P) const { for(const auto& H:Targets()) if(H.Rect.ContainsPoint(P))return H.Action;return INDEX_NONE; }
    void ValueAt(int32 Action,float X)
    {
        if(!Owner.IsValid())return;
        const float T=FMath::Clamp((X-300.f)/382.f,0.f,1.f);
        if(Action==600){const float DayT=FMath::Clamp((X-300.f)/540.f,0.f,1.f);Owner->SetDaylightHour(DayT<.00001f?6.f:DayT>.99999f?22.f:6+16*DayT);}
        if(Action==200) Owner->SetRoomLighting(Room,T>0,T);
        if(Action==220)Owner->ApplySensitivity(.25f+1.75f*T);
        if(Action==221)Owner->ApplySpeed(.5f+1.5f*T);
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    bool Activate(FVector2D P)
    {
        if(!Owner.IsValid())return false;
        const int32 A=Hit(P);if(A<0)return false;
        if(A==4||A==6||A==7||A==8||A==9)Owner->SetPanel(A);
        else if(A==101)Owner->ToggleMap();
        else if(A==100)Owner->SetPanel(0);
        else if(A>=110&&A<110+CourseworkRooms::Count)Room=A-110;
        else if(A==210||A==211)Owner->SetRoomLighting(Room,A==211,Owner->GetSmartLightingScene()?Owner->GetRoomRestoreBrightness(Room):(Owner->IsRoomLightOn(Room)?Owner->GetRoomBrightness(Room):Owner->GetRoomRestoreBrightness(Room)));
        else if(A==212)Owner->ResetRoomLighting(Room);
        else if(A==213||A==214)Owner->SetAllRoomLighting(A==213);
        else if(A==215)Owner->ResetAllRoomLighting();
        else if(A>=700&&A<=702){Owner->SetSmartLightingScene(A-700);if(A==702)Room=4;}
        else if(A>=610&&A<=613){const float Hours[]={8,12,17,21};Owner->SetDaylightHour(Hours[A-610]);}
        else if(A==640||A==641)Owner->SelectComfortPage(A-640);
        else if(A==650||A==651)Owner->SetRoomWindows(Room,A==650);
        else if(A==652)Owner->ToggleAirflowDisplay();
        else if(A==653)Owner->CloseAllWindows();
        else if(A==654&&Owner->GetRoomWindowCount(Room)>0)Owner->ObserveVentilation(Room);
        else if(A==620&&!Owner->IsSafetyDrillRunning())Owner->RestoreDaylight();
        else if(A==621)Owner->ObserveDaylight();
        else if(A==500||A==501)Owner->ApplyEnergyScenario(A==501);
        else if(A==502)Owner->RestoreEnergyComparison();
        else if(A==503)Owner->OpenEnergyMap();
        else if(A==506)Owner->SetPanel(6);
        else if(A>=510&&A<=512)Owner->SetEnergyHours(A==510?1.f:A==511?4.f:8.f);
        else if(A==400)Owner->TriggerKitchenAlarm();
        else if(A==401)Owner->HandleKitchenAlarm();
        else if(A==402)Owner->ResetKitchenAlarm();
        else if(A==403)Owner->ToggleMap();
        else if(A>=300&&A<=304)Owner->GoToLocation(A-300);
        else if(A==305)Owner->SetPanel(3);
        else if(A==200||A==220||A==221||A==600)ValueAt(A,P.X);
        Invalidate(EInvalidateWidgetReason::Paint);return true;
    }
    virtual FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E) override
    {
        if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
        const FVector2D P=DesignPoint(G,E.GetScreenSpacePosition());const int32 A=Hit(P);
        Activate(P);
        if((A==200 && Owner.IsValid())||A==220||A==221||A==600)
        { Drag=A;return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this)); }
        if(Owner.IsValid()&&Owner->GetPanel()==0)return FReply::Handled();
        return FReply::Handled().SetUserFocus(SharedThis(this));
    }
    virtual FReply OnMouseButtonUp(const FGeometry& G,const FPointerEvent& E) override
    {
        if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
        if(Drag>=0)ValueAt(Drag,DesignPoint(G,E.GetScreenSpacePosition()).X);
        Drag=INDEX_NONE;return FReply::Handled().ReleaseMouseCapture();
    }
    virtual FReply OnMouseMove(const FGeometry& G,const FPointerEvent& E) override
    {
        const FVector2D P=DesignPoint(G,E.GetScreenSpacePosition());Hover=Hit(P);
        if(Drag>=0)ValueAt(Drag,P.X);
        Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();
    }
    virtual void OnMouseLeave(const FPointerEvent& E) override { Hover=INDEX_NONE;Invalidate(EInvalidateWidgetReason::Paint);SLeafWidget::OnMouseLeave(E); }
    virtual void OnMouseCaptureLost(const FCaptureLostEvent& E) override { Drag=INDEX_NONE;SLeafWidget::OnMouseCaptureLost(E); }
    virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent& E) override
    {
        if(!Owner.IsValid())return FReply::Unhandled();
        if(E.GetKey()==EKeys::Tab||E.GetKey()==EKeys::Escape){Owner->SetPanel(0);return FReply::Handled();}
        if(Owner->GetPanel()==9&&Owner->GetComfortPage()==0&&(E.GetKey()==EKeys::Left||E.GetKey()==EKeys::Right))
        {Owner->SetDaylightHour(Owner->GetDaylightHour()+(E.GetKey()==EKeys::Right?.25f:-.25f));Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();}
        if(Owner->GetPanel()==6)
        {
            if(E.GetKey()==EKeys::Up||E.GetKey()==EKeys::Down){Room=CourseworkRooms::DisplayOrder[(CourseworkRooms::DisplayIndex(Room)+CourseworkRooms::Count+(E.GetKey()==EKeys::Down?1:-1))%CourseworkRooms::Count];Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();}
            if(E.GetKey()==EKeys::Left||E.GetKey()==EKeys::Right){if(Owner->IsRoomLightOn(Room))Owner->SetRoomLighting(Room,true,Owner->GetRoomBrightness(Room)+(E.GetKey()==EKeys::Right?.05f:-.05f));Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();}
            if(E.GetKey()==EKeys::Enter){Owner->SetRoomLighting(Room,!Owner->IsRoomLightOn(Room),Owner->GetSmartLightingScene()?Owner->GetRoomRestoreBrightness(Room):Owner->GetRoomBrightness(Room));Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();}
        }
        return FReply::Unhandled();
    }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        if(!Owner.IsValid())return Layer;
        const float S=Scale(G);const FVector2D O=Origin(G);const int32 Mode=Owner->GetPanel();
        const auto* White=FCoreStyle::Get().GetBrush("WhiteBrush");
        // Gradient and solid controls need distinct layers: Slate can batch boxes
        // separately from gradients on one layer regardless of submission order.
        auto Box=[&](float X,float Y,float W,float H,FLinearColor C){FSlateDrawElement::MakeBox(Out,Layer+1,G.ToPaintGeometry(FVector2D(W,H),FSlateLayoutTransform(S,O+FVector2D(X,Y)*S)),White,ESlateDrawEffect::None,C);};
        auto Line=[&](TArray<FVector2D> P,FLinearColor C,float Thickness=1.f){for(auto& V:P)V=O+V*S;FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,Thickness*S);};
        auto Text=[&](float X,float Y,const FString& T,int32 Size,FLinearColor C,bool Medium=false,bool Number=false,int32 Style=-1,int32 Body=-1){
            const FSlateFontInfo F=FontFor(Size,Medium,Number,Style,Body);
            FSlateDrawElement::MakeText(Out,Layer+3,G.ToPaintGeometry(FVector2D(1600,900),FSlateLayoutTransform(S,O+FVector2D(X,Y)*S)),T,F,ESlateDrawEffect::None,C);
        };
        auto Rect=[&](float X,float Y,float W,float H,FLinearColor C){Line({{X,Y},{X+W,Y},{X+W,Y+H},{X,Y+H},{X,Y}},C);};
        auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        auto CenterText=[&](FSlateRect R,const FString& T,int32 Size,FLinearColor C,int32 Style=-1,int32 Body=-1){
            const auto Font=FontFor(Size,false,false,Style,Body);const float W=Measure->Measure(T,Font).X;const FVector2D InkRange=InkBounds(T,Font);
            Text((R.Left+R.Right-W)*.5f,(R.Top+R.Bottom-InkRange.X-InkRange.Y)*.5f,T,Size,C,false,false,Style,Body);
        };
        auto KeyHint=[&](float X,const FString& Key,const FString& Action,float CenterY=862,int32 Style=-1,int32 Body=-1){
            const float KeyW=54,KeyH=36,Gap=16;
            Rect(X,CenterY-KeyH*.5f,KeyW,KeyH,Hover==100?Gold:Muted);
            CenterText(FSlateRect(X,CenterY-KeyH*.5f,X+KeyW,CenterY+KeyH*.5f),Key,15,Ivory,Style,Body);
            const FVector2D Bounds=InkBounds(Action,FontFor(17,false,false,Style,Body));
            Text(X+KeyW+Gap,CenterY-(Bounds.X+Bounds.Y)*.5f,Action,17,Ivory,false,false,Style,Body);
        };
        if(bBodyDetail && Mode==6)
        {
            Box(0,0,1600,900,FLinearColor(.012f,.017f,.022f));
            Text(64,28,TEXT("正文对照 · 标题统一使用龙藏体"),20,Muted,false,false,0);
            const TCHAR* Names[]={TEXT("A · 文楷细字重 / 手写感"),TEXT("B · 思源黑体 / 清爽"),TEXT("C · 霞鹜新晰黑 / 舒展"),TEXT("D · 思源宋体细字重 / 书卷感")};
            for(int32 Body=0;Body<4;++Body)
            {
                const float X=64+(Body%2)*800,Y=94+(Body/2)*398;
                Text(X,Y,Names[Body],17,Gold,false,false,0);
                Text(X,Y+32,TEXT("室内灯光"),34,Ivory,true,false,4,Body);
                Text(X,Y+135,TEXT("客厅  /  一楼"),24,Ivory,false,false,4,Body);
                Text(X,Y+188,TEXT("亮度    照明    恢复默认"),22,Muted,false,false,4,Body);
                Line({{X,Y+247},{X+590,Y+247}},FLinearColor(.3f,.3f,.3f,.3f));
                KeyHint(X,TEXT("Tab"),TEXT("关闭"),Y+289,4,Body);
                const float W=70+Measure->Measure(TEXT("返回"),FontFor(17,false,false,4,Body)).X;
                KeyHint(X+590-W,TEXT("Esc"),TEXT("返回"),Y+289,4,Body);
            }
            Line({{800,94},{800,841}},FLinearColor(.3f,.3f,.3f,.22f));
            return Layer+4;
        }
        if(bDetail && Mode==6)
        {
            // Actual Slate component specimens, enlarged for comparison. No raster editing.
            Box(0,0,1600,900,FLinearColor(.012f,.017f,.022f));
            Text(64,32,TEXT("标题 / 控制区 / 按键提示"),20,Muted,false,false,0);
            Line({{800,118},{800,836}},FLinearColor(.3f,.3f,.3f,.22f));
            for(int32 Style=3;Style<=4;++Style)
            {
                const float X=64+(Style-3)*800;
                Text(X,95,Style==3?TEXT("温润手写 · 霞鹜文楷"):TEXT("轻盈行笔 · 龙藏体"),18,Gold,false,false,0);
                Text(X,143,TEXT("室内灯光"),34,Ivory,true,false,Style);
                Text(X,240,TEXT("客厅  /  一楼"),18,Muted,false,false,Style);
                Text(X,313,TEXT("亮度"),18,Muted,false,false,Style);
                Text(X,350,TEXT("70"),76,Ivory,false,true,Style);
                const float NW=Measure->Measure(TEXT("70"),FontFor(76,false,true,Style)).X;
                const float PY=350+InkBounds(TEXT("70"),FontFor(76,false,true,Style)).Y-InkBounds(TEXT("%"),FontFor(29,false,true,Style)).Y;
                Text(X+NW+9,PY,TEXT("%"),29,Muted,false,true,Style);
                Box(X,451,590,4,FLinearColor(.2f,.22f,.23f));Box(X,451,393,4,Gold);Box(X+386,444,14,18,Ivory);
                Text(X,477,TEXT("10%"),14,Muted,false,true,Style);Text(X+545,477,TEXT("100%"),14,Muted,false,true,Style);
                Text(X,549,TEXT("照明"),21,Ivory,false,false,Style);
                Rect(X+350,534,226,50,Muted);Box(X+464,535,111,48,Ivory);
                CenterText(FSlateRect(X+350,534,X+463,584),TEXT("关"),18,Muted,Style);
                CenterText(FSlateRect(X+463,534,X+576,584),TEXT("开"),18,Ink,Style);
                Text(X,642,TEXT("恢复默认"),18,Muted,false,false,Style);
                Line({{X,720},{X+590,720}},FLinearColor(.3f,.3f,.3f,.25f));
                KeyHint(X,TEXT("Tab"),TEXT("关闭"),774,Style);
                const float W=70+Measure->Measure(TEXT("返回"),FontFor(17,false,false,Style)).X;
                KeyHint(X+590-W,TEXT("Esc"),TEXT("返回"),774,Style);
            }
            return Layer+4;
        }
        auto House=[&](float X,float Y,FLinearColor C){Line({{X,Y+12},{X+13,Y},{X+26,Y+12},{X+23,Y+12},{X+23,Y+28},{X+15,Y+28},{X+15,Y+18},{X+10,Y+18},{X+10,Y+28},{X+3,Y+28},{X+3,Y+12}},C,1.6f);};
        auto Slider=[&](float Y,float T,bool Enabled){
            const FLinearColor Active=Enabled?Gold:Muted;Box(300,Y,382,5,FLinearColor(.20f,.22f,.23f));Box(300,Y,382*T,5,Active);Box(292+382*T,Y-7,16,19,Enabled?Ivory:Muted);
            for(int32 I=0;I<5;++I)Line({{300+95.5f*I,Y+19},{300+95.5f*I,Y+25}},FLinearColor(.35f,.36f,.36f));
        };
        // A smooth gradient keeps the TV/wall pattern from competing with controls.
        TArray<FSlateGradientStop> Stops;
        for(int32 I=0;I<=16;++I){const float X=100.f*I;Stops.Emplace(FVector2f(X,0),FLinearColor(.006f,.008f,.012f,.97f*(1.f-FMath::SmoothStep(590.f,1000.f,X))));}
        FSlateDrawElement::MakeGradient(Out,Layer,G.ToPaintGeometry(FVector2D(1600,900),FSlateLayoutTransform(S,O)),Stops,Orient_Vertical);
        Box(0,0,1600,86,FLinearColor(0,0,0,.78f));Line({{0,86},{1600,86}},FLinearColor(.5f,.5f,.48f,.25f));
        const FVector2D HeaderInk=InkBounds(TEXT("住宅控制"),FontFor(23,true));
        Text(56,43-(HeaderInk.X+HeaderInk.Y)*.5f,TEXT("住宅控制"),23,Ivory,true);Line({{227,33},{227,61}},Muted);
        const int32 Modes[]={6,7,8,9};const TCHAR* Tabs[]={TEXT("智慧"),TEXT("安全"),TEXT("能源"),TEXT("舒适")};
        for(int32 I=0;I<4;++I){const float X=300+120.f*I;const bool Selected=Mode==Modes[I];if(Hover==Modes[I])Box(X-14,16,86,62,FLinearColor(1,1,1,.045f));const FVector2D TabInk=InkBounds(Tabs[I],FontFor(20));Text(X,43-(TabInk.X+TabInk.Y)*.5f,Tabs[I],20,Selected?Ivory:Muted);if(Selected)Box(X-10,82,66,3,Gold);}
        if(Hover==101)Box(1280,16,120,62,FLinearColor(1,1,1,.045f));
        CenterText(FSlateRect(1280,16,1400,78),TEXT("地图  M"),16,Hover==101?Ivory:Muted);
        // Draw the gear as vector lines: independent of font symbol coverage.
        if(Hover==4||Mode==4)Box(1480,16,64,62,FLinearColor(1,1,1,.045f));
        const FLinearColor GearColor=Mode==4?Gold:Hover==4?Ivory:Muted;
        TArray<FVector2D> Gear,Hub;
        for(int32 I=0;I<=32;++I){const float Angle=I*2*PI/32;const float Radius=(I%4==1||I%4==2)?12.f:9.f;Gear.Add(FVector2D(1512+FMath::Cos(Angle)*Radius,43+FMath::Sin(Angle)*Radius));}
        for(int32 I=0;I<=20;++I){const float Angle=I*2*PI/20;Hub.Add(FVector2D(1512+FMath::Cos(Angle)*4,43+FMath::Sin(Angle)*4));}
        Line(Gear,GearColor,1.4f);Line(Hub,GearColor,1.4f);
        CenterText(FSlateRect(1480,59,1544,77),TEXT("设置"),11,GearColor);
        if(Mode==6)
        {
            int32 EnabledRooms=0;for(int32 I=0;I<CourseworkRooms::Count;++I)if(Owner->IsRoomLightOn(I))++EnabledRooms;
            Box(884,108,660,98,FLinearColor(.012f,.016f,.020f,.88f));
            Text(904,125,TEXT("全屋"),17,Ivory);Text(904,164,FString::Printf(TEXT("%d / %d"),EnabledRooms,CourseworkRooms::Count),12,Gold,false,true);
            const TCHAR* WholeLabels[]={TEXT("全部开启"),TEXT("全部关闭"),TEXT("恢复默认")};
            for(int32 I=0;I<3;++I)
            {
                const float X=1004+180.f*I;const bool Active=(I==0&&EnabledRooms==CourseworkRooms::Count)||(I==1&&EnabledRooms==0);
                if(Active||Hover==213+I)Box(X,119,172,50,FLinearColor(Gold.R,Gold.G,Gold.B,.12f));
                Rect(X,119,172,50,Active||Hover==213+I?Gold:Muted);
                CenterText(FSlateRect(X,119,X+172,169),WholeLabels[I],16,Active?Gold:Ivory);
            }
            Text(1004,177,Owner->GetLightingSaveStatus(),13,Muted);
            Text(56,154,TEXT("房间"),17,Muted);
            for(int32 I=0;I<CourseworkRooms::Count;++I)
            {
                const int32 ID=CourseworkRooms::DisplayOrder[I];const float Y=CourseworkRooms::RowY(I);const bool Selected=Room==ID;
                const FLinearColor C=Selected?Ink:Ivory;const auto& Item=CourseworkRooms::Get(ID);
                if(Selected)Box(52,Y,194,62,Ivory);else if(Hover==110+ID)Box(52,Y,194,62,FLinearColor(1,1,1,.065f));
                House(68,Y+20,C);Text(115,Y+2,Item.Name,19,C);Text(115,Y+33,Item.Floor,12,Selected?Ink:Muted);
                if(Owner->IsRoomLightOn(ID))Box(232,Y+28,3,6,Selected?Ink:Gold);
            }
            const auto& SelectedRoom=CourseworkRooms::Get(Room);
            Text(300,160,FString::Printf(TEXT("%s  /  %s"),SelectedRoom.Name,SelectedRoom.Floor),14,Muted);
            Text(300,197,TEXT("室内灯光"),34,Ivory,true);
            Text(300,314,Owner->GetSmartLightingScene()?TEXT("区域等效亮度"):TEXT("亮度"),17,Muted);
            const bool On=Owner->IsRoomLightOn(Room);const float Brightness=On?Owner->GetRoomBrightness(Room):Owner->GetRoomRestoreBrightness(Room);
            const FString Number=FString::FromInt(FMath::RoundToInt((On?Brightness:0)*100));
            Text(298,344,Number,76,On?Ivory:Muted,false,true);
            const float NumberWidth=Measure->Measure(Number,FontFor(76,false,true)).X;
            const float PercentY=344+InkBounds(Number,FontFor(76,false,true)).Y-InkBounds(TEXT("%"),FontFor(29,false,true)).Y;
            Text(298+NumberWidth+7,PercentY,TEXT("%"),29,Muted,false,true);
            TArray<FVector2D> Circle;for(int32 I=0;I<=32;++I){const float A=I*2*PI/32;Circle.Add(FVector2D(648+15*FMath::Cos(A),393+15*FMath::Sin(A)));}Line(Circle,On?Gold:Muted,1.5f);
            for(int32 I=0;I<8;++I){const float A=I*PI/4;Line({{648+23*FMath::Cos(A),393+23*FMath::Sin(A)},{648+30*FMath::Cos(A),393+30*FMath::Sin(A)}},On?Gold:Muted,1.5f);}
            Slider(477,On?Brightness:0,true);Text(300,503,TEXT("0%"),13,Muted,false,true);Text(652,503,TEXT("100%"),13,Muted,false,true);
            Line({{303,606},{309,583},{323,583},{329,606},{303,606},{316,606},{316,620},{309,620},{323,620}},On?Gold:Muted,1.5f);Text(349,583,TEXT("照明"),19,Ivory);
            Rect(456,575,226,50,FLinearColor(.37f,.39f,.39f));Box(On?569:457,576,112,48,Ivory);
            if(Hover==210||Hover==211)Rect(Hover==210?456:569,575,113,50,Gold);
            CenterText(FSlateRect(456,575,569,625),TEXT("关"),18,On?Muted:Ink);CenterText(FSlateRect(569,575,682,625),TEXT("开"),18,On?Ink:Muted);
            if(Hover==212)Box(296,706,228,52,FLinearColor(1,1,1,.05f));
            TArray<FVector2D> Arc;for(int32 I=0;I<=20;++I){const float A=-PI*.9f+I*PI*1.75f/20;Arc.Add(FVector2D(313+11*FMath::Cos(A),731+11*FMath::Sin(A)));}Line(Arc,Muted,1.5f);Line({{300,718},{301,729},{311,727}},Muted,1.5f);Text(349,716,TEXT("房间复位"),17,Muted);
            if(!On)Text(300,651,FString::Printf(TEXT("再次开启恢复至 %d%%"),FMath::RoundToInt(Brightness*100)),13,Muted);
            // Lifestyle controls occupy the existing unused right column.
            // Keep the established typefaces, margins and understated palette.
            Box(884,238,660,558,FLinearColor(.012f,.016f,.020f,.90f));
            Text(906,260,TEXT("生活照明"),27,Ivory,true);
            const int32 Scene=Owner->GetSmartLightingScene();
            Rect(1340,253,176,52,Hover==700?Gold:Muted);
            CenterText(FSlateRect(1340,253,1516,305),Scene?TEXT("退出场景"):TEXT("日常手动"),16,Scene?Ivory:Muted);
            const TCHAR* SceneNames[]={TEXT("就寝起夜"),TEXT("阅读助手")};
            for(int32 I=0;I<2;++I)
            {
                const float X=I?1214:904;const float W=I?302:294;const bool Selected=Scene==I+1;
                if(Selected||Hover==701+I)Box(X,342,W,78,FLinearColor(Gold.R,Gold.G,Gold.B,.10f));
                Rect(X,342,W,78,Selected?Gold:Muted);
                CenterText(FSlateRect(X,342,X+W,420),SceneNames[I],22,Selected?Gold:Ivory);
            }
            Text(906,462,Owner->GetSmartLightingStatus(),20,Scene?Gold:Ivory);
            Text(906,508,Owner->GetSmartLightingDetail(),15,Muted);
            const bool Reading=Scene==2;
            const float Live=Scene?(Reading?Owner->GetSmartFixtureLevel(FName(TEXT("SM_TableLamp_A1_32"))):Owner->GetSmartRoomLevel(8)):0;
            Text(906,572,Reading?TEXT("书房台灯"):TEXT("走廊灯具等效亮度"),16,Ivory);
            Text(1360,572,FString::Printf(TEXT("%d %%"),FMath::RoundToInt(Live*100)),19,Scene?Gold:Muted,false,true);
            Box(906,612,608,2,FLinearColor(.22f,.24f,.25f));if(Live>0)Box(906,612,608*Live,2,Gold);
            Text(906,651,Scene?FString::Printf(TEXT("手动优先  ·  已接管 %d 项"),Owner->GetSmartManualCount()):TEXT("起夜按路引光  /  阅读只补亮桌面"),15,Muted);
            Text(906,703,TEXT("场景中按 E 可单独控制灯具"),14,Muted);
            Text(906,738,TEXT("自动变化不写入日常配置  ·  感知为场景模拟"),13,Muted);
        }
        else if(Mode==8)
        {
            Box(272,110,1270,686,FLinearColor(.010f,.014f,.019f,.92f));
            Text(56,154,TEXT("房间"),17,Muted);
            for(int32 I=0;I<CourseworkRooms::Count;++I)
            {
                const int32 ID=CourseworkRooms::DisplayOrder[I];const float Y=CourseworkRooms::RowY(I);const bool Selected=ID==Room;
                if(Selected)Box(52,Y,194,62,Ivory);else if(Hover==110+ID)Box(52,Y,194,62,FLinearColor(1,1,1,.045f));
                House(68,Y+17,Selected?Ink:Muted);Text(116,Y+5,CourseworkRooms::Get(ID).Name,19,Selected?Ink:Ivory);
                Text(116,Y+34,FString::Printf(TEXT("%s · %.1f W"),CourseworkRooms::Get(ID).Floor,Owner->GetRoomWatts(ID)),12,Selected?Ink:Muted);
                if(Owner->GetRoomWatts(ID)>0)Box(232,Y+28,3,6,Selected?Ink:Gold);
            }
            Text(300,125,TEXT("照明能耗"),34,Ivory,true);
            Text(300,227,Owner->GetEnergyScenarioName(),17,Gold);
            const FString Power=FString::Printf(TEXT("%.1f"),Owner->GetTotalWatts());
            Text(296,264,Power,76,Ivory,false,true);
            const float PW=Measure->Measure(Power,FontFor(76,false,true)).X;
            Text(305+PW,306,TEXT("W"),25,Muted,false,true);
            Text(300,376,TEXT("当前照明功率  /  九个照明回路"),14,Muted);
            Line({{300,412},{796,412}},FLinearColor(.3f,.32f,.33f,.6f));
            Text(300,437,FString::Printf(TEXT("%s · %s"),CourseworkRooms::Get(Room).Floor,CourseworkRooms::Get(Room).Name),19,Ivory);
            Text(300,479,FString::Printf(TEXT("%.1f W"),Owner->GetRoomWatts(Room)),35,Ivory,false,true);
            Text(300,535,FString::Printf(TEXT("回路额定 %.0f W   ·   当前亮度 %d%%"),Owner->GetRoomRatedWatts(Room),FMath::RoundToInt((Owner->IsRoomLightOn(Room)?Owner->GetRoomBrightness(Room):0)*100)),13,Muted);
            Rect(300,570,300,42,Hover==506?Gold:Muted);CenterText(FSlateRect(300,570,600,612),TEXT("调节该房间照明"),16,Ivory);
            Line({{842,142},{842,594}},FLinearColor(.3f,.32f,.33f,.45f));
            Text(884,144,TEXT("同一时长 · 电量对比"),22,Ivory);
            Text(884,201,TEXT("持续时长"),14,Muted);
            const float Hours[]={1,4,8};
            for(int32 I=0;I<3;++I){const float X=1130+92.f*I;const bool Selected=FMath::IsNearlyEqual(Owner->GetEnergyHours(),Hours[I]);Rect(X,195,82,38,Selected?Gold:Muted);if(Selected)Box(X+1,196,80,36,FLinearColor(.22f,.20f,.15f,.5f));CenterText(FSlateRect(X,195,X+82,233),FString::Printf(TEXT("%.0f h"),Hours[I]),14,Selected?Ivory:Muted);}
            const float Baseline=Owner->GetEstimatedKWh(true),Current=Owner->GetEstimatedKWh();
            Text(884,269,TEXT("全开基准"),16,Muted);Text(1230,261,FString::Printf(TEXT("%.3f kWh"),Baseline),25,Ivory,false,true);
            Box(884,315,530,7,FLinearColor(.29f,.31f,.32f));
            Text(884,367,Owner->GetEnergyScenarioName(),16,Muted);Text(1230,359,FString::Printf(TEXT("%.3f kWh"),Current),25,Gold,false,true);
            Box(884,413,530,7,FLinearColor(.12f,.15f,.16f));Box(884,413,530*Current/Baseline,7,Gold);
            Text(884,474,FString::Printf(TEXT("比全开少用 %.3f kWh"),Baseline-Current),20,Ivory);
            Text(884,520,FString::Printf(TEXT("减少 %.1f%%"),(1.f-Current/Baseline)*100),27,Gold,false,true);
            const TCHAR* Labels[]={TEXT("全开对比"),TEXT("节能对比"),TEXT("恢复原配置")};
            for(int32 I=0;I<3;++I){const float X=300+184.f*I;const bool Disabled=Owner->IsSafetyDrillRunning()||(I==2&&!Owner->IsEnergyComparisonActive());Rect(X,650,172,58,Disabled?FLinearColor(.23f,.25f,.26f):Hover==500+I?Gold:Muted);CenterText(FSlateRect(X,650,X+172,708),Labels[I],17,Disabled?Muted:Ivory);}
            Rect(1050,650,250,58,Owner->IsSafetyDrillRunning()?Muted:Hover==503?Gold:Muted);CenterText(FSlateRect(1050,650,1300,708),TEXT("查看用电流向"),17,Owner->IsSafetyDrillRunning()?Muted:Ivory);
            Text(300,730,Owner->IsSafetyDrillRunning()?TEXT("疏散演练中 · 能源方案切换已暂停"):Owner->IsEnergyComparisonActive()?TEXT("正在对比 · 调整暂不保存，可恢复进入前的灯光配置"):TEXT("当前配置 · 通过 E 或灯光页调整，数据随场景同步"),14,Gold);
            Text(300,768,TEXT("模拟估算 · 功率按亮度线性估算；电量按所选时长持续使用计算"),12,Muted);
        }
        else if(Mode==7)
        {
            const int32 State=Owner->GetSafetyState();const bool Running=Owner->IsSafetyDrillRunning();
            const FLinearColor Alert(.93f,.39f,.22f),Safe(.52f,.72f,.61f),StatusColor=State==3?Safe:State>=2?Alert:Gold;
            Text(56,154,TEXT("演练场景"),17,Muted);Box(52,203,194,76,Ivory);
            Text(76,207,TEXT("烟感与疏散"),20,Ink);Text(76,246,TEXT("厨房 / 室外集合点"),12,Ink);
            Text(300,160,TEXT("住宅安全  /  人员疏散"),14,Muted);Text(300,197,TEXT("平安有序"),34,Ivory,true);
            Text(300,302,TEXT("当前状态"),15,Muted);
            Box(300,352,4,42,StatusColor);Text(323,347,Owner->GetSafetyStatus(),22,StatusColor);
            const TCHAR* Steps[]={TEXT("01  烟感监测"),TEXT("02  报警"),TEXT("03  人员撤离"),TEXT("04  集合")};
            for(int32 I=0;I<4;++I)
            {
                const float X=300+136.f*I;const bool Reached=Running&&(I==0||(I<3&&State>=2)||(I==3&&State==3));
                Box(X,440,124,2,Reached?StatusColor:FLinearColor(.20f,.22f,.23f));
                Text(X,459,Steps[I],14,Reached?Ivory:Muted);
            }
            Box(300,519,530,4,FLinearColor(.20f,.22f,.23f));Box(300,519,530*Owner->GetSafetyProgress(),4,StatusColor);
            const FString Reading=FString::Printf(TEXT("模拟烟雾指数 %.0f / 100 · 报警阈值 30 · 已集合 %d / 2"),Owner->GetSmokeReading(),int32(Owner->HasPlayerGathered())+int32(Owner->HasResidentGathered()));
            Text(300,541,Reading,14,Muted);
            const TCHAR* Actions[]={TEXT("启动演练"),TEXT("结束演练"),TEXT("中止复位")};
            const bool Enabled[]={!Running&&Owner->IsSafetyReady(),State==3,Running};
            for(int32 I=0;I<3;++I)
            {
                const float X=300+180.f*I;const FLinearColor C=Enabled[I]?(I==0?Gold:Ivory):Muted;
                if(Enabled[I]&&Hover==400+I)Box(X,585,170,56,FLinearColor(1,1,1,.075f));
                Rect(X,585,170,56,Enabled[I]?C:FLinearColor(.22f,.24f,.24f));
                CenterText(FSlateRect(X,585,X+170,641),Actions[I],18,C);
            }
            if(Hover==403)Box(300,695,260,56,FLinearColor(1,1,1,.06f));
            Text(300,711,TEXT("查看集合点与路线  →"),18,Ivory);
            Text(300,773,Running?TEXT("演练期间请步行前往集合点，住户会自行撤离"):Owner->GetLastEvacuationResult(),13,Muted);
            Box(1000,260,508,460,FLinearColor(.012f,.016f,.020f,.94f));
            Text(1032,291,TEXT("人员清点"),25,Ivory,true);
            Text(1032,365,TEXT("你的状态"),14,Muted);
            Text(1032,398,Owner->HasPlayerGathered()?TEXT("已到室外集合点"):TEXT("尚未到达 · 请沿地图路线撤离"),17,Owner->HasPlayerGathered()?Safe:Ivory);
            Text(1032,449,TEXT("住户状态"),14,Muted);
            Text(1032,482,Owner->GetResidentEvacuationStatus(),17,Owner->HasResidentGathered()?Safe:Ivory);
            Text(1032,545,FString::Printf(TEXT("报警后用时    %.1f 秒"),Owner->GetAlarmElapsed()),17,Gold);
            Text(1032,593,TEXT("集合点：入户门外绿色圆环"),16,Muted);
            Text(1032,637,TEXT("人员集合不代表厨房火警解除"),15,Gold);
            Text(1032,680,TEXT("教学模拟 · 无真实传感器接入"),13,Muted);
            if(!Owner->IsSafetyReady())Text(300,773,TEXT("演练资源不完整，请查看运行日志"),14,Alert);
        }
        else if(Mode==9&&Owner->GetComfortPage()==1)
        {
            Text(56,154,TEXT("舒适"),17,Muted);
            Text(56,205,TEXT("日照采光"),21,Hover==640?Ivory:Muted);
            Text(56,257,TEXT("自然通风"),21,Ivory);Box(48,253,3,29,Gold);
            for(int32 I=0;I<CourseworkRooms::Count;++I){const int32 ID=CourseworkRooms::DisplayOrder[I];const float Y=320+49.f*I;const bool Selected=Room==ID;if(Selected)Box(52,Y,194,47,Ivory);else if(Hover==110+ID)Box(52,Y,194,47,FLinearColor(1,1,1,.045f));const auto& R=CourseworkRooms::Get(ID);Text(68,Y+3,R.Name,16,Selected?Ink:Muted);Text(68,Y+27,R.Floor,11,Selected?Ink:Muted);}
            const auto& R=CourseworkRooms::Get(Room);const int32 Count=Owner->GetRoomWindowCount(Room),Opened=Owner->GetRoomOpenWindowCount(Room);const bool Locked=Owner->IsSafetyDrillRunning(),Cross=Owner->HasRoomCrossFlow(Room);const float Strength=Owner->GetRoomVentilation(Room);
            Text(300,160,FString::Printf(TEXT("%s  /  %s"),R.Floor,R.Name),14,Muted);Text(300,197,TEXT("让空气流动"),34,Ivory,true);
            Text(300,296,TEXT("自然换气状态"),18,Muted);
            Text(300,338,Locked?TEXT("演练优先"):Cross?TEXT("对流通风"):Opened?TEXT("局部换气"):Strength>.03f?TEXT("间接换气"):TEXT("尚未开窗"),34,Ivory,true);
            Box(300,424,540,4,FLinearColor(.2f,.22f,.23f));Box(300,424,540*Strength,4,Gold);
            Text(300,446,TEXT("弱"),13,Muted);Text(550,446,TEXT("一般"),13,Muted);Text(812,446,TEXT("较好"),13,Muted);
            Text(300,498,Count?FString::Printf(TEXT("本房已开  %d / %d 扇窗"),Opened,Count):TEXT("本房没有直接外窗"),20,Count?Ivory:Muted);
            const FLinearColor ActionColor=Count&&!Locked?Ivory:Muted;
            Rect(300,552,250,56,Hover==650?Gold:Muted);CenterText(FSlateRect(300,552,550,608),TEXT("打开本房窗户"),18,ActionColor);
            Rect(566,552,250,56,Hover==651?Gold:Muted);CenterText(FSlateRect(566,552,816,608),TEXT("关闭本房窗户"),18,ActionColor);
            Rect(300,686,250,56,Hover==654?Gold:Muted);CenterText(FSlateRect(300,686,550,742),TEXT("前往窗边观察"),18,ActionColor);
            Rect(566,686,250,56,Hover==653?Gold:Muted);CenterText(FSlateRect(566,686,816,742),TEXT("关闭全部窗户"),18,Locked?Muted:Ivory);
            Box(1016,282,428,340,FLinearColor(.015f,.022f,.028f,.86f));
            Text(1050,315,TEXT("空气从哪里来"),22,Ivory);Text(1050,370,TEXT("室外风向：东向西"),17,Muted);Text(1050,412,TEXT("门窗共同决定进、出风口"),17,Muted);Text(1050,454,TEXT("气流沿连通空间由高压向低压"),17,Muted);
            Text(1050,536,Locked?TEXT("演练期间暂停展示"):Cross?TEXT("对向开口 · 通路已连通"):Opened?TEXT("局部换气 · 窗边为主"):Count?TEXT("打开窗户开始观察"):TEXT("通过相邻房间换气"),16,Gold);
            Rect(1050,686,330,56,Hover==652?Gold:Muted);CenterText(FSlateRect(1050,686,1380,742),Owner->IsAirflowDisplayed()?TEXT("隐藏气流"):TEXT("显示气流"),18,Locked?Muted:Ivory);
            Text(300,773,TEXT("自然通风示意 · 固定室外风向 · 非真实流体计算"),13,Muted);
        }
        else if(Mode==9)
        {
            const bool Active=Owner->IsDaylightActive(),Locked=Owner->IsSafetyDrillRunning();
            const FLinearColor ButtonColor=Locked?Muted:Ivory;
            const float Hour=Owner->GetDaylightHour();const int32 Minutes=FMath::RoundToInt(Hour*60);
            Text(56,154,TEXT("舒适"),17,Muted);Text(56,205,TEXT("日照采光"),21,Ivory);Box(48,201,3,29,Gold);Text(56,257,TEXT("自然通风"),21,Hover==641?Ivory:Muted);
            Text(300,160,TEXT("客厅  /  同一位置对比"),14,Muted);Text(300,197,TEXT("光影之间"),34,Ivory,true);
            Text(300,296,Active?TEXT("预览时刻"):TEXT("原场景 · 尚未开始预览"),18,Muted);
            Text(300,329,Active?FString::Printf(TEXT("%02d:%02d"),Minutes/60,Minutes%60):FString(TEXT("--:--")),68,Ivory,false,true);
            const float T=(Hour-6)/16.f;Box(300,468,540,4,FLinearColor(.2f,.22f,.23f));Box(300,468,540*T,4,Gold);Box(294+540*T,461,12,18,Ivory);
            Text(300,493,TEXT("06:00"),13,Muted,false,true);Text(797,493,TEXT("22:00"),13,Muted,false,true);
            const TCHAR* Labels[]={TEXT("上午"),TEXT("正午"),TEXT("傍晚"),TEXT("夜间")};const float Hours[]={8,12,17,21};
            for(int32 I=0;I<4;++I){const float X=300+136.f*I;const bool Selected=Active&&FMath::IsNearlyEqual(Hour,Hours[I],.01f);if(Selected||Hover==610+I)Box(X,552,128,56,FLinearColor(Gold.R,Gold.G,Gold.B,.12f));Rect(X,552,128,56,Selected?Gold:Muted);CenterText(FSlateRect(X,552,X+128,608),Labels[I],18,Selected?Gold:ButtonColor);}
            Rect(300,686,260,56,Hover==620&&!Locked?Gold:Muted);CenterText(FSlateRect(300,686,560,742),TEXT("恢复原夜景"),18,ButtonColor);
            Box(1016,282,428,340,FLinearColor(.015f,.022f,.028f,.86f));
            Text(1050,315,TEXT("观察什么"),22,Ivory);Text(1050,370,TEXT("窗边光斑的范围"),18,Muted);Text(1050,412,TEXT("地面与家具的投影"),18,Muted);Text(1050,454,TEXT("不同时段的室内明暗"),18,Muted);
            Text(1050,536,Active?FString::Printf(TEXT("太阳高度角  %.1f°"),Owner->GetDaylightElevation()):TEXT("选择时段开始对比"),17,Gold);
            Rect(1050,686,330,56,Hover==621?Gold:Muted);CenterText(FSlateRect(1050,686,1380,742),TEXT("前往客厅观察点"),18,ButtonColor);
            Text(300,773,Locked?TEXT("疏散演练中 · 采光切换暂不可用"):TEXT("教学日照示意 · 灯具配置保留 · 关闭面板后继续观察"),13,Locked?Gold:Muted);
        }
        else if(Mode==1)
        {
            Text(56,154,TEXT("参观"),17,Muted);Text(56,205,TEXT("地点导航"),21,Ivory);
            Text(300,160,TEXT("自由探索  /  住宅"),14,Muted);Text(300,197,TEXT("参观导航"),34,Ivory,true);
            const TCHAR* Destinations[]={TEXT("房屋外观"),TEXT("客厅"),TEXT("厨房"),TEXT("二楼走廊"),TEXT("回到入口"),TEXT("操作帮助")};
            for(int32 I=0;I<6;++I){const float Y=288+64.f*I;if(Hover==300+I)Box(296,Y,390,59,FLinearColor(1,1,1,.08f));Text(302,Y+15,FString::Printf(TEXT("%02d"),I+1),16,Gold,false,true);Text(348,Y+12,Destinations[I],21,Ivory);Text(658,Y+13,TEXT("›"),23,Muted,false,true);Line({{300,Y+60},{682,Y+60}},FLinearColor(.4f,.4f,.4f,.14f));}
            Text(300,713,TEXT("选择地点前往，或关闭面板继续参观"),13,Muted);
        }
        else if(Mode==4)
        {
            Text(56,154,TEXT("设置"),17,Muted);Text(56,205,TEXT("操作手感"),21,Ivory);
            Text(300,160,TEXT("鼠标  /  移动"),14,Muted);Text(300,197,TEXT("操作设置"),34,Ivory,true);
            Text(300,295,TEXT("鼠标灵敏度"),19,Ivory);Text(601,295,FString::Printf(TEXT("%.2f ×"),Owner->GetSensitivity()),18,Gold,false,true);Slider(353,(Owner->GetSensitivity()-.25f)/1.75f,true);Text(300,379,TEXT("0.25 ×"),13,Muted,false,true);Text(653,379,TEXT("2 ×"),13,Muted,false,true);
            Text(300,466,TEXT("移动速度"),19,Ivory);Text(601,466,FString::Printf(TEXT("%.2f ×"),Owner->GetSpeedScale()),18,Gold,false,true);Slider(524,(Owner->GetSpeedScale()-.5f)/1.5f,true);Text(300,550,TEXT("0.5 ×"),13,Muted,false,true);Text(653,550,TEXT("2 ×"),13,Muted,false,true);
            Text(300,713,TEXT("仅作用于本次运行，重新启动后恢复默认"),13,Muted);
        }
        Box(0,824,1600,76,FLinearColor(0,0,0,.26f));Line({{0,824},{1600,824}},FLinearColor(.5f,.5f,.48f,.25f));
        KeyHint(56,TEXT("Tab"),TEXT("关闭"));
        const float RightWidth=54+16+Measure->Measure(TEXT("返回"),FontFor(17)).X;
        KeyHint(1544-RightWidth,TEXT("Esc"),TEXT("返回"));
        return Layer+4;
    }
private:
    TWeakObjectPtr<ACourseworkHUD> Owner;
    int32 Hover=INDEX_NONE,Drag=INDEX_NONE;
    int32 Typography=4;
    bool bDetail=false;
    bool bBodyDetail=false;
    int32 BodyTypography=0;
};

TSharedRef<SWidget> UCourseworkControlWidget::RebuildWidget(){Menu=SNew(SCourseworkControl);Menu->Configure(Owner);return Menu.ToSharedRef();}
void UCourseworkControlWidget::Configure(ACourseworkHUD* HUD){Owner=HUD;if(Menu)Menu->Configure(HUD);}
void UCourseworkControlWidget::SelectRoom(int32 Room){if(Menu){Menu->Room=CourseworkRooms::Clamp(Room);Menu->Invalidate(EInvalidateWidgetReason::Paint);}}
int32 UCourseworkControlWidget::GetRoom()const{return Menu?Menu->Room:0;}
bool UCourseworkControlWidget::ExercisePointer(FVector2D P)
{
    if(!Menu)return false;
    const FGeometry G=Menu->GetCachedGeometry();if(G.GetLocalSize().X<1)return false;
    const FVector2D Absolute=G.LocalToAbsolute(Menu->Origin(G)+P*Menu->Scale(G));
    const TSet<FKey> Pressed={EKeys::LeftMouseButton},Released;
    const FPointerEvent Down(0,Absolute,Absolute,Pressed,EKeys::LeftMouseButton,0,FModifierKeysState());
    const FPointerEvent Up(0,Absolute,Absolute,Released,EKeys::LeftMouseButton,0,FModifierKeysState());
    const bool Hit=Menu->Hit(P)>=0;
    UE_LOG(LogTemp,Display,TEXT("CONTROL_POINTER mode=%d room=%d geometry=%s input=%s mapped=%s hit=%d before=%.4f"),Owner?Owner->GetPanel():-1,Menu->Room,*G.GetLocalSize().ToString(),*P.ToString(),*Menu->DesignPoint(G,Absolute).ToString(),Menu->Hit(P),Owner?Owner->GetRoomBrightness(Menu->Room):0);
    Menu->OnMouseButtonDown(G,Down);Menu->OnMouseButtonUp(G,Up);
    UE_LOG(LogTemp,Display,TEXT("CONTROL_POINTER after=%.4f"),Owner?Owner->GetRoomBrightness(Menu->Room):0);
    return Hit;
}
void UCourseworkControlWidget::ReleaseSlateResources(bool Children){Super::ReleaseSlateResources(Children);Menu.Reset();}

bool UCourseworkControlWidget::ExerciseDrag(FVector2D P,int32 Phase)
{
    if(!Menu)return false;const FGeometry G=Menu->GetCachedGeometry();if(G.GetLocalSize().X<1)return false;
    const FVector2D Absolute=G.LocalToAbsolute(Menu->Origin(G)+P*Menu->Scale(G));
    TSet<FKey> Keys;if(Phase!=2)Keys.Add(EKeys::LeftMouseButton);
    const FPointerEvent E(0,Absolute,Absolute,Keys,Phase==1?FKey():EKeys::LeftMouseButton,0,FModifierKeysState());
    if(Phase==0)return Menu->OnMouseButtonDown(G,E).IsEventHandled();
    if(Phase==1)return Menu->OnMouseMove(G,E).IsEventHandled();
    return Menu->OnMouseButtonUp(G,E).IsEventHandled();
}
