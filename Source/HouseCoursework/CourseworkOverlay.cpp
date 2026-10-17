#include "CourseworkOverlay.h"
#include "CourseworkHUD.h"
#include "CourseworkMapWidget.h"
#include "CourseworkControlWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"

namespace
{
    UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 Size = 18)
    {
        auto* Label = Tree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(Text));
        Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.88f, 0.86f, 0.80f)));
        Label->SetShadowOffset(FVector2D(0, 0));
        Label->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.75f));
        Label->SetVisibility(ESlateVisibility::HitTestInvisible);
        return Label;
    }
    void Place(UCanvasPanel* Root, UWidget* Widget, FVector2D Anchor, FVector2D Alignment, FVector2D Position)
    {
        auto* Slot = Root->AddChildToCanvas(Widget);
        Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
        Slot->SetAlignment(Alignment);
        Slot->SetPosition(Position);
        Slot->SetAutoSize(true);
    }
}

void UCourseworkOverlay::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this);
    auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    WidgetTree->RootWidget = Root;
    MiniMap = WidgetTree->ConstructWidget<UCourseworkMapWidget>();
    MiniMap->Configure(OwnerHUD,false);
    Place(Root, MiniMap, FVector2D(0,1), FVector2D(0,1), FVector2D(36,-36));
    InteractionCard = WidgetTree->ConstructWidget<UBorder>();
    InteractionCard->SetBrushColor(FLinearColor::Transparent);
    InteractionCard->SetPadding(FMargin(0));
    Place(Root,InteractionCard,FVector2D(1,1),FVector2D(1,1),FVector2D(-36,-36));
    auto* Description=WidgetTree->ConstructWidget<UVerticalBox>(); InteractionCard->AddChild(Description);
    InteractionName=MakeText(WidgetTree,TEXT(""),14);
    InteractionName->SetJustification(ETextJustify::Right);
    InteractionName->SetColorAndOpacity(FSlateColor(FLinearColor(.58f,.57f,.53f,1)));
    auto* NameSlot=Description->AddChildToVerticalBox(InteractionName); NameSlot->SetHorizontalAlignment(HAlign_Right); NameSlot->SetPadding(FMargin(0,0,0,5));
    auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();
    Description->AddChildToVerticalBox(Row)->SetHorizontalAlignment(HAlign_Right);
    Interaction=MakeText(WidgetTree,TEXT(""),19); Interaction->SetJustification(ETextJustify::Right);
    auto* ActionSlot=Row->AddChildToHorizontalBox(Interaction); ActionSlot->SetVerticalAlignment(VAlign_Center); ActionSlot->SetPadding(FMargin(0,0,10,0));
    auto* KeySize=WidgetTree->ConstructWidget<USizeBox>(); KeySize->SetWidthOverride(24); KeySize->SetHeightOverride(24);
    auto* Key=WidgetTree->ConstructWidget<UBorder>();
    InteractionKey=Key;
    FSlateBrush KeyBrush; KeyBrush.DrawAs=ESlateBrushDrawType::RoundedBox; KeyBrush.TintColor=FSlateColor(FLinearColor(0,0,0,.20f));
    KeyBrush.OutlineSettings=FSlateBrushOutlineSettings(2.f,FSlateColor(FLinearColor(.85f,.84f,.78f,.9f)),1.f);
    Key->SetBrush(KeyBrush); Key->SetPadding(FMargin(0)); Key->SetHorizontalAlignment(HAlign_Center); Key->SetVerticalAlignment(VAlign_Center);
    Key->AddChild(MakeText(WidgetTree,TEXT("E"),14)); KeySize->AddChild(Key);
    Row->AddChildToHorizontalBox(KeySize)->SetVerticalAlignment(VAlign_Center);
    InteractionDimWidth=WidgetTree->ConstructWidget<USizeBox>();InteractionDimWidth->SetWidthOverride(190);InteractionDimWidth->SetHeightOverride(3);
    InteractionDimBar=WidgetTree->ConstructWidget<UProgressBar>();
    FSlateBrush Track;Track.TintColor=FSlateColor(FLinearColor(.9f,.88f,.8f,.18f));
    FSlateBrush Fill;Fill.TintColor=FSlateColor(FLinearColor::White);
    FProgressBarStyle BarStyle;BarStyle.SetBackgroundImage(Track);BarStyle.SetFillImage(Fill);
    InteractionDimBar->SetWidgetStyle(BarStyle);InteractionDimBar->SetFillColorAndOpacity(FLinearColor(.86f,.69f,.40f));
    InteractionDimBar->SetBarFillStyle(EProgressBarFillStyle::Scale);InteractionDimWidth->AddChild(InteractionDimBar);
    auto* DimSlot=Description->AddChildToVerticalBox(InteractionDimWidth);DimSlot->SetHorizontalAlignment(HAlign_Right);DimSlot->SetPadding(FMargin(0,10,0,0));
    InteractionDimWidth->SetVisibility(ESlateVisibility::Collapsed);
    InteractionCard->SetVisibility(ESlateVisibility::Collapsed);
    InteractionCard->SetRenderOpacity(0);
    Toast = MakeText(WidgetTree, TEXT(""), 12);
    Toast->SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.69f, 0.63f)));
    Place(Root, Toast, FVector2D(0.5, 1), FVector2D(0.5, 1), FVector2D(0, -40));
    SafetyBanner=WidgetTree->ConstructWidget<UBorder>();
    SafetyBanner->SetBrushColor(FLinearColor(.032f,.015f,.012f,.94f));
    SafetyBanner->SetPadding(FMargin(20,10));
    SafetyBanner->SetVisibility(ESlateVisibility::Collapsed);
    SafetyText=MakeText(WidgetTree,TEXT(""),17);
    SafetyText->SetFont(FSlateFontInfo(FPaths::ProjectContentDir()/TEXT("Slate/Fonts/LXGWWenKai-Light.ttf"),17));
    SafetyText->SetColorAndOpacity(FSlateColor(FLinearColor(.95f,.67f,.45f)));
    SafetyBanner->AddChild(SafetyText);
    Place(Root,SafetyBanner,FVector2D(.5f,0),FVector2D(.5f,0),FVector2D(0,24));

    auto* Width = WidgetTree->ConstructWidget<USizeBox>();
    PanelWidth = Width;
    Width->SetWidthOverride(960);
    PanelScrim=WidgetTree->ConstructWidget<UBorder>();PanelScrim->SetBrushColor(FLinearColor(0,0,0,.52f));
    auto* ScrimSlot=Root->AddChildToCanvas(PanelScrim);ScrimSlot->SetAnchors(FAnchors(0,0,1,1));ScrimSlot->SetOffsets(FMargin(0));ScrimSlot->SetZOrder(-1);
    Place(Root, Width, FVector2D(0.5, 0.5), FVector2D(0.5, 0.5), FVector2D(0, 0));
    Panel = WidgetTree->ConstructWidget<UBorder>();
    FSlateBrush PanelBrush;PanelBrush.DrawAs=ESlateBrushDrawType::RoundedBox;PanelBrush.TintColor=FSlateColor(FLinearColor(.022f,.026f,.028f,.98f));
    PanelBrush.OutlineSettings=FSlateBrushOutlineSettings(16.f,FSlateColor(FLinearColor(.14f,.16f,.17f)),1.f);Panel->SetBrush(PanelBrush);
    Panel->SetPadding(FMargin(40,32));
    Width->AddChild(Panel);
    auto* Content = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->AddChild(Content);
    auto* Eyebrow=MakeText(WidgetTree,TEXT("HOUSE  /  住宅交互演示"),12);Eyebrow->SetColorAndOpacity(FSlateColor(FLinearColor(.58f,.46f,.28f)));
    Content->AddChildToVerticalBox(Eyebrow)->SetPadding(FMargin(0,0,0,12));
    PanelTitle = MakeText(WidgetTree, TEXT(""), 30);
    Content->AddChildToVerticalBox(PanelTitle)->SetPadding(FMargin(0, 0, 0, 12));
    PanelTabs=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChildToVerticalBox(PanelTabs)->SetPadding(FMargin(0,6,0,22));
    PanelBody = MakeText(WidgetTree, TEXT(""), 15);
    PanelBody->SetColorAndOpacity(FSlateColor(FLinearColor(.50f,.54f,.55f)));
    PanelBody->SetAutoWrapText(true);
    Content->AddChildToVerticalBox(PanelBody)->SetPadding(FMargin(0, 0, 0, 14));
    MapArea = WidgetTree->ConstructWidget<USizeBox>();
    MapArea->SetHeightOverride(360);
    ExpandedMap=WidgetTree->ConstructWidget<UCourseworkMapWidget>(); ExpandedMap->Configure(OwnerHUD,true);
    MapArea->AddChild(ExpandedMap);
    Content->AddChildToVerticalBox(MapArea)->SetPadding(FMargin(0,0,0,10));
    Buttons = WidgetTree->ConstructWidget<UVerticalBox>();
    Content->AddChildToVerticalBox(Buttons);
    ControlMenu=WidgetTree->ConstructWidget<UCourseworkControlWidget>();
    ControlMenu->Configure(OwnerHUD);
    auto* ControlSlot=Root->AddChildToCanvas(ControlMenu);
    ControlSlot->SetAnchors(FAnchors(0,0,1,1));ControlSlot->SetOffsets(FMargin(0));ControlSlot->SetZOrder(3);
    RefreshPanel();
}

void UCourseworkOverlay::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshPanel();
}

void UCourseworkOverlay::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    if(SafetyBanner && OwnerHUD)
    {
        SafetyBanner->SetVisibility(OwnerHUD->GetSafetyState()!=0 && OwnerHUD->GetPanel()==0?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
        SafetyText->SetText(FText::FromString(OwnerHUD->GetSafetyStatus()+TEXT("   ·   M 查看路线   /   Tab 安全")));
    }
    const bool Show=bInteractionWanted && OwnerHUD && OwnerHUD->GetPanel()==0;
    InteractionOpacity=FMath::FInterpConstantTo(InteractionOpacity,Show?1.f:0.f,DeltaTime,Show?9.f:7.f);
    if(InteractionCard)
    {
        InteractionCard->SetRenderOpacity(InteractionOpacity);
        InteractionCard->SetVisibility(InteractionOpacity>.01f?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    }
}

void UCourseworkOverlay::SetOwnerHUD(ACourseworkHUD* Value)
{
    OwnerHUD=Value;
    if(MiniMap) MiniMap->Configure(Value,false);
    if(ExpandedMap) ExpandedMap->Configure(Value,true);
    if(ControlMenu) ControlMenu->Configure(Value);
}

UButton* UCourseworkOverlay::MakePanelButton(const FString& Label,FName Handler,bool Accent)
{
    auto* Button=WidgetTree->ConstructWidget<UButton>();
    auto Brush=[](FLinearColor Color){FSlateBrush B;B.DrawAs=ESlateBrushDrawType::RoundedBox;B.TintColor=FSlateColor(Color);B.OutlineSettings=FSlateBrushOutlineSettings(7.f,FSlateColor(FLinearColor(.12f,.14f,.15f)),1.f);return B;};
    FButtonStyle Style;Style.SetNormal(Brush(Accent?FLinearColor(.17f,.13f,.075f):FLinearColor(.045f,.052f,.056f)));
    Style.SetHovered(Brush(FLinearColor(.13f,.15f,.16f)));Style.SetPressed(Brush(FLinearColor(.20f,.17f,.12f)));
    Style.SetNormalPadding(FMargin(18,12));Style.SetPressedPadding(FMargin(18,12));Button->SetStyle(Style);
    auto* Text=MakeText(WidgetTree,Label,17);Text->SetJustification(ETextJustify::Center);Button->AddChild(Text);
    FScriptDelegate Click;Click.BindUFunction(this,Handler);Button->OnClicked.Add(Click);return Button;
}
void UCourseworkOverlay::AddButton(const FString& Label,FName Handler)
{
    auto* Height=WidgetTree->ConstructWidget<USizeBox>();Height->SetHeightOverride(48);
    Height->AddChild(MakePanelButton(Label,Handler));Buttons->AddChildToVerticalBox(Height)->SetPadding(FMargin(0,4));
}

void UCourseworkOverlay::RefreshPanel()
{
    if (!Panel || !OwnerHUD) return;
    const int32 Mode = OwnerHUD->GetPanel();
    Panel->SetVisibility(Mode == 0 ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    if(Mode!=0) { InteractionOpacity=0; InteractionCard->SetRenderOpacity(0); InteractionCard->SetVisibility(ESlateVisibility::Collapsed); }
    MiniMap->SetVisibility(Mode==0?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    Toast->SetVisibility(Mode==0?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    MapArea->SetVisibility(Mode==5?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    PanelWidth->SetWidthOverride(Mode==5?(OwnerHUD->IsEnergyMapLayer()?1200:1000):960);
    MapArea->SetHeightOverride(Mode==5&&OwnerHUD->IsEnergyMapLayer()?460:360);
    PanelScrim->SetVisibility(Mode==0?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
    const bool EdgeMenu=Mode==1||Mode==4||Mode==6||Mode==7||Mode==8||Mode==9;
    PanelWidth->SetVisibility(EdgeMenu||Mode==0?ESlateVisibility::Collapsed:ESlateVisibility::SelfHitTestInvisible);
    ControlMenu->SetVisibility(EdgeMenu?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    if(EdgeMenu)
    {
        PanelScrim->SetVisibility(ESlateVisibility::Collapsed);
        LightSlider=nullptr;LightValue=nullptr;
        return;
    }
    PanelTabs->ClearChildren();PanelTabs->SetVisibility((Mode==1||Mode==4||Mode==6)?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
    LightSlider=nullptr;LightValue=nullptr;
    if(Mode==1||Mode==4||Mode==6)
    {
        const FString Names[]={TEXT("参观导航"),TEXT("室内灯光"),TEXT("操作设置")};
        const FName Handlers[]={GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,Back),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,Lights),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,Settings)};
        for(int32 I=0;I<3;++I){auto* S=PanelTabs->AddChildToHorizontalBox(MakePanelButton(Names[I],Handlers[I],Mode==(I==0?1:I==1?6:4)));S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));S->SetPadding(FMargin(I==0?0:6,0,I==2?0:6,0));}
    }
    Buttons->ClearChildren();
    if (Mode == 0) return;
    if (Mode == 1)
    {
        PanelTitle->SetText(FText::FromString(TEXT("参观控制面板")));
        PanelBody->SetText(FText::FromString(TEXT("选择一个地点开始观察。关闭面板后恢复移动和 E 交互。")));
        const FString Names[]={TEXT("01  房屋外观"),TEXT("02  客厅"),TEXT("03  厨房"),TEXT("04  二楼"),TEXT("05  回到入口"),TEXT("06  操作帮助")};
        const FName Handlers[]={GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,Outside),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,Living),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,Kitchen),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,Upstairs),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,Entrance),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,Help)};
        for(int32 RowIndex=0;RowIndex<3;++RowIndex)
        {
            auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();Buttons->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,5));
            for(int32 Col=0;Col<2;++Col){auto* Height=WidgetTree->ConstructWidget<USizeBox>();Height->SetHeightOverride(64);Height->AddChild(MakePanelButton(Names[RowIndex*2+Col],Handlers[RowIndex*2+Col]));auto* S=Row->AddChildToHorizontalBox(Height);S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));S->SetPadding(FMargin(Col==0?0:6,0,Col==0?6:0,0));}
        }
        AddButton(TEXT("返回参观     Tab / Esc"),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,ClosePanel));
    }
    else if (Mode == 2)
    {
        PanelTitle->SetText(FText::FromString(TEXT("已暂停")));
        PanelBody->SetText(FText::FromString(TEXT("参观已暂停。选择继续或返回入口。")));
        AddButton(TEXT("继续参观  ·  Esc"), GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay, ClosePanel));
        AddButton(TEXT("回到入口"), GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay, Entrance));
        AddButton(TEXT("操作帮助"), GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay, Help));
        AddButton(TEXT("退出程序"), GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay, Quit));
    }
    else if (Mode == 3)
    {
        PanelTitle->SetText(FText::FromString(TEXT("开始参观")));
        PanelBody->SetText(FText::FromString(TEXT("W / A / S / D：移动    鼠标：观察\nE：操作准星指向的门、冰箱、抽屉或灯具\n灯具：短按 E 开关；按住 E 滚轮调光，Shift 细调\nM：楼层地图    Tab：控制面板    Esc：暂停 / 返回\n\n左下地图显示所在楼层、位置与朝向。\n右下出现对象提示后，按 E 操作。\n进入推拉门时，从打开的门洞通过。\n上下楼从台阶中间行走，避免贴着扶手。")));
        AddButton(TEXT("开始 / 继续参观"), GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay, ClosePanel));
        AddButton(TEXT("地点导航"), GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay, Back));
    }
    else if (Mode == 4)
    {
        PanelTitle->SetText(FText::FromString(TEXT("移动与鼠标设置")));
        PanelBody->SetText(FText::FromString(TEXT("调整只作用于本次运行。重新启动后恢复默认值。")));
        Buttons->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("鼠标灵敏度  ·  0.25 ～ 2 倍"), 17))->SetPadding(FMargin(0, 8));
        auto* SensitivitySlider = WidgetTree->ConstructWidget<USlider>();
        SensitivitySlider->SetSliderBarColor(FLinearColor(.20f,.23f,.24f));SensitivitySlider->SetSliderHandleColor(FLinearColor(.75f,.59f,.35f));
        SensitivitySlider->SetMinValue(0.25f); SensitivitySlider->SetMaxValue(2.f);
        SensitivitySlider->SetValue(OwnerHUD->GetSensitivity());
        SensitivitySlider->OnValueChanged.AddDynamic(this, &UCourseworkOverlay::SensitivityChanged);
        Buttons->AddChildToVerticalBox(SensitivitySlider)->SetPadding(FMargin(0, 8, 0, 16));
        Buttons->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("移动速度  ·  0.5 ～ 2 倍"), 17))->SetPadding(FMargin(0, 8));
        auto* SpeedSlider = WidgetTree->ConstructWidget<USlider>();
        SpeedSlider->SetSliderBarColor(FLinearColor(.20f,.23f,.24f));SpeedSlider->SetSliderHandleColor(FLinearColor(.75f,.59f,.35f));
        SpeedSlider->SetMinValue(0.5f); SpeedSlider->SetMaxValue(2.f);
        SpeedSlider->SetValue(OwnerHUD->GetSpeedScale());
        SpeedSlider->OnValueChanged.AddDynamic(this, &UCourseworkOverlay::SpeedChanged);
        Buttons->AddChildToVerticalBox(SpeedSlider)->SetPadding(FMargin(0, 8, 0, 16));
        AddButton(TEXT("返回控制面板"), GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay, Back));
        AddButton(TEXT("返回参观"), GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay, ClosePanel));
    }
    else if(Mode==6)
    {
        PanelTitle->SetText(FText::FromString(TEXT("室内灯光")));
        PanelBody->SetText(FText::FromString(TEXT("选择房间，调整照明。关灯后保留上次亮度。")));
        auto* Rooms=WidgetTree->ConstructWidget<UHorizontalBox>();Buttons->AddChildToVerticalBox(Rooms)->SetPadding(FMargin(0,0,0,20));
        auto* A=Rooms->AddChildToHorizontalBox(MakePanelButton(TEXT("一楼 · 客厅"),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,LivingLights),LightingRoom==0));A->SetSize(FSlateChildSize(ESlateSizeRule::Fill));A->SetPadding(FMargin(0,0,6,0));
        auto* B=Rooms->AddChildToHorizontalBox(MakePanelButton(TEXT("二楼 · 主卧"),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,BedroomLights),LightingRoom==1));B->SetSize(FSlateChildSize(ESlateSizeRule::Fill));B->SetPadding(FMargin(6,0,0,0));
        auto* Card=WidgetTree->ConstructWidget<UBorder>();Card->SetBrushColor(FLinearColor(.035f,.041f,.044f));Card->SetPadding(FMargin(26));Buttons->AddChildToVerticalBox(Card);
        auto* Content=WidgetTree->ConstructWidget<UVerticalBox>();Card->AddChild(Content);
        Content->AddChildToVerticalBox(MakeText(WidgetTree,LightingRoom==0?TEXT("客厅照明"):TEXT("主卧照明"),23))->SetPadding(FMargin(0,0,0,18));
        auto* Switch=MakePanelButton(OwnerHUD->IsRoomLightOn(LightingRoom)?TEXT("灯光已开启     点击关闭"):TEXT("灯光已关闭     点击开启"),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,ToggleLights),OwnerHUD->IsRoomLightOn(LightingRoom));Content->AddChildToVerticalBox(Switch)->SetPadding(FMargin(0,0,0,22));
        LightValue=MakeText(WidgetTree,TEXT(""),17);Content->AddChildToVerticalBox(LightValue)->SetPadding(FMargin(0,0,0,14));
        LightSlider=WidgetTree->ConstructWidget<USlider>();FSliderStyle SliderStyle;
        FSlateBrush Rail;Rail.DrawAs=ESlateBrushDrawType::RoundedBox;Rail.TintColor=FSlateColor(FLinearColor(.20f,.23f,.24f));Rail.ImageSize=FVector2D(200,5);Rail.OutlineSettings=FSlateBrushOutlineSettings(3.f);SliderStyle.SetNormalBarImage(Rail);SliderStyle.SetHoveredBarImage(Rail);SliderStyle.SetDisabledBarImage(Rail);
        FSlateBrush Thumb;Thumb.DrawAs=ESlateBrushDrawType::RoundedBox;Thumb.TintColor=FSlateColor(FLinearColor(.75f,.59f,.35f));Thumb.ImageSize=FVector2D(18,18);Thumb.OutlineSettings=FSlateBrushOutlineSettings(9.f);SliderStyle.SetNormalThumbImage(Thumb);SliderStyle.SetHoveredThumbImage(Thumb);SliderStyle.SetDisabledThumbImage(Thumb);SliderStyle.SetBarThickness(5);LightSlider->SetWidgetStyle(SliderStyle);
        LightSlider->SetMinValue(0);LightSlider->SetMaxValue(1.f);LightSlider->SetValue(OwnerHUD->IsRoomLightOn(LightingRoom)?OwnerHUD->GetRoomBrightness(LightingRoom):0);LightSlider->SetSliderBarColor(FLinearColor(.20f,.23f,.24f));LightSlider->SetSliderHandleColor(FLinearColor(.75f,.59f,.35f));LightSlider->SetIsEnabled(true);LightSlider->OnValueChanged.AddDynamic(this,&UCourseworkOverlay::LightBrightnessChanged);Content->AddChildToVerticalBox(LightSlider)->SetPadding(FMargin(0,0,0,10));RefreshLightReadout();
        auto* Footer=WidgetTree->ConstructWidget<UHorizontalBox>();Buttons->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0,22,0,0));
        auto* Reset=Footer->AddChildToHorizontalBox(MakePanelButton(TEXT("恢复本房间默认"),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,ResetLights)));Reset->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Reset->SetPadding(FMargin(0,0,6,0));
        auto* Close=Footer->AddChildToHorizontalBox(MakePanelButton(TEXT("返回参观   Tab / Esc"),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,ClosePanel)));Close->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Close->SetPadding(FMargin(6,0,0,0));
    }
    else if (Mode == 5)
    {
        PanelTitle->SetText(FText::FromString(OwnerHUD->IsEnergyMapLayer()?TEXT("照明用电流向"):TEXT("楼层地图")));
        PanelBody->SetText(FText::FromString(OwnerHUD->IsSafetyDrillRunning()?TEXT("烟感与疏散演练 · 报警后显示厨房位置和集合路线。住户会自行撤离，请步行前往室外绿色集合点。"):(OwnerHUD->GetPlayerFloor()==0?TEXT("当前位于一楼  ·  选择楼层查看布局，点击地点名称前往。 "):TEXT("当前位于二楼  ·  选择楼层查看布局，点击地点名称前往。 "))));
        if(OwnerHUD->IsEnergyMapLayer())PanelBody->SetText(FText::FromString(TEXT("照明回路的用电流向示意 · 发光箭头随功率变化，关闭房间照明后停止。G 返回导航图。")));
        auto* Floors=WidgetTree->ConstructWidget<UHorizontalBox>();
        Buttons->AddChildToVerticalBox(Floors)->SetPadding(FMargin(0,3,0,8));
        for(int32 Floor=0;Floor<2;++Floor)
        {
            auto* Height=WidgetTree->ConstructWidget<USizeBox>(); Height->SetHeightOverride(38);
            auto* Button=WidgetTree->ConstructWidget<UButton>();
            Button->SetBackgroundColor(OwnerHUD->GetMapFloor()==Floor?FLinearColor(.16f,.17f,.15f):FLinearColor(.045f,.048f,.045f));
            const FString Label=(OwnerHUD->GetMapFloor()==Floor?TEXT("● "):TEXT("○ "))+FString(Floor==0?TEXT("一楼  ·  1"):TEXT("二楼  ·  2"));
            auto* Text=MakeText(WidgetTree,Label,17); Text->SetJustification(ETextJustify::Center); Button->AddChild(Text);
            FScriptDelegate Click; Click.BindUFunction(this,Floor==0?GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,FloorOne):GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,FloorTwo)); Button->OnClicked.Add(Click);
            Height->AddChild(Button);
            auto* FloorSlot=Floors->AddChildToHorizontalBox(Height); FloorSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); FloorSlot->SetPadding(FMargin(Floor==0?0:4,0,Floor==0?4:0,0));
        }
        AddButton(TEXT("返回参观  ·  M / Esc"),GET_FUNCTION_NAME_CHECKED(UCourseworkOverlay,ClosePanel));
    }
}

void UCourseworkOverlay::SetInteraction(const FString& Text)
{
    if(!Interaction || !InteractionCard) return;
    if(InteractionDimWidth)InteractionDimWidth->SetVisibility(ESlateVisibility::Collapsed);
    if(InteractionKey)InteractionKey->SetRenderOpacity(1);
    bInteractionWanted=!Text.IsEmpty();
    if(!bInteractionWanted) return;
    FString Name,Action;
    if(!Text.Split(TEXT("  ·  按 E "),&Name,&Action)) { Name=Text; Action=TEXT(""); }
    InteractionName->SetText(FText::FromString(Name));
    Interaction->SetText(FText::FromString(Action));
}
void UCourseworkOverlay::SetLightInteraction(const FString& Name,const FString& Status,float Brightness,bool Dimming)
{
    if(!Interaction||!InteractionCard)return;
    bInteractionWanted=true;InteractionName->SetText(FText::FromString(Name));Interaction->SetText(FText::FromString(Status));
    InteractionDimWidth->SetVisibility(Dimming?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    InteractionDimBar->SetPercent(Brightness);
    // One E badge only; no action legend or extra instructional text.
    InteractionKey->SetRenderOpacity(Dimming?.7f:1.f);
}
bool UCourseworkOverlay::CheckLightFeedback(int32 Room,bool Dimming)const
{
    if(!OwnerHUD||!Interaction||!InteractionDimBar||!InteractionDimWidth)return false;
    const bool On=OwnerHUD->IsRoomLightOn(Room);const float Actual=On?OwnerHUD->GetRoomBrightness(Room):0;
    const FString Expected=FString::Printf(TEXT("%s  ·  %d%%"),On?TEXT("开启"):TEXT("关闭"),FMath::RoundToInt(Actual*100));
    return bInteractionWanted&&Interaction->GetText().ToString()==Expected&&FMath::IsNearlyEqual(InteractionDimBar->GetPercent(),Actual,.0001f)
        &&(InteractionDimWidth->GetVisibility()!=ESlateVisibility::Collapsed)==Dimming;
}
FReply UCourseworkOverlay::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if(OwnerHUD&&OwnerHUD->GetPanel()==5&&Event.GetKey()==EKeys::G){OwnerHUD->ToggleEnergyMapLayer();return FReply::Handled();}
    if(OwnerHUD && Event.GetKey()==EKeys::M)
    {
        OwnerHUD->ToggleMap();
        return FReply::Handled();
    }
    if(OwnerHUD && OwnerHUD->GetPanel()==5 && (Event.GetKey()==EKeys::One || Event.GetKey()==EKeys::Two))
    {
        OwnerHUD->SelectMapFloor(Event.GetKey()==EKeys::One?0:1);
        return FReply::Handled();
    }
    if (OwnerHUD && OwnerHUD->GetPanel() != 0 && (Event.GetKey() == EKeys::Tab || Event.GetKey() == EKeys::Escape))
    {
        OwnerHUD->SetPanel(0);
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, Event);
}
void UCourseworkOverlay::SetToast(const FString& Text) { if (Toast) Toast->SetText(FText::FromString(Text)); }
void UCourseworkOverlay::ClosePanel() { if (OwnerHUD) OwnerHUD->SetPanel(0); }
void UCourseworkOverlay::Outside() { if (OwnerHUD) OwnerHUD->GoToLocation(0); }
void UCourseworkOverlay::Living() { if (OwnerHUD) OwnerHUD->GoToLocation(1); }
void UCourseworkOverlay::Kitchen() { if (OwnerHUD) OwnerHUD->GoToLocation(2); }
void UCourseworkOverlay::Upstairs() { if (OwnerHUD) OwnerHUD->GoToLocation(3); }
void UCourseworkOverlay::Entrance() { if (OwnerHUD) OwnerHUD->GoToLocation(4); }
void UCourseworkOverlay::Help() { if (OwnerHUD) OwnerHUD->SetPanel(3); }
void UCourseworkOverlay::Settings() { if (OwnerHUD) OwnerHUD->SetPanel(4); }
void UCourseworkOverlay::Back() { if (OwnerHUD) OwnerHUD->ToggleMap(); }
void UCourseworkOverlay::Quit() { if (OwnerHUD) OwnerHUD->QuitApplication(); }
void UCourseworkOverlay::SensitivityChanged(float Value) { if (OwnerHUD) OwnerHUD->ApplySensitivity(Value); }
void UCourseworkOverlay::SpeedChanged(float Value) { if (OwnerHUD) OwnerHUD->ApplySpeed(Value); }
void UCourseworkOverlay::FloorOne() { if(OwnerHUD) OwnerHUD->SelectMapFloor(0); }
void UCourseworkOverlay::FloorTwo() { if(OwnerHUD) OwnerHUD->SelectMapFloor(1); }


void UCourseworkOverlay::Lights(){if(OwnerHUD)OwnerHUD->SetPanel(6);}
void UCourseworkOverlay::ShowLightingRoom(int32 Room){LightingRoom=CourseworkRooms::Clamp(Room);if(ControlMenu)ControlMenu->SelectRoom(LightingRoom);RefreshPanel();}
bool UCourseworkOverlay::ExerciseControlPointer(FVector2D Position){return ControlMenu && ControlMenu->ExercisePointer(Position);}
void UCourseworkOverlay::LivingLights(){ShowLightingRoom(0);}
void UCourseworkOverlay::BedroomLights(){ShowLightingRoom(1);}
void UCourseworkOverlay::ToggleLights(){const bool On=OwnerHUD->IsRoomLightOn(LightingRoom);OwnerHUD->SetRoomLighting(LightingRoom,!On,On?OwnerHUD->GetRoomBrightness(LightingRoom):OwnerHUD->GetRoomRestoreBrightness(LightingRoom));RefreshPanel();}
void UCourseworkOverlay::ResetLights(){OwnerHUD->ResetRoomLighting(LightingRoom);RefreshPanel();}
void UCourseworkOverlay::RefreshLightReadout()
{
    if(!LightValue)return;
    const bool On=OwnerHUD->IsRoomLightOn(LightingRoom);const int32 Stored=FMath::RoundToInt(OwnerHUD->GetRoomBrightness(LightingRoom)*100);
    LightValue->SetText(FText::FromString(On?FString::Printf(TEXT("亮度    %d%%    实时调节"),Stored):FString::Printf(TEXT("亮度    0%%    再次开启恢复至 %d%%"),Stored)));
}
void UCourseworkOverlay::LightBrightnessChanged(float Value){OwnerHUD->SetRoomLighting(LightingRoom,Value>0,Value);RefreshLightReadout();}

bool UCourseworkOverlay::ExerciseMapDestination(int32 Destination){return ExpandedMap&&ExpandedMap->ExerciseDestination(Destination);}

bool UCourseworkOverlay::ExerciseEnergyMapButton(){return ExpandedMap&&ExpandedMap->ExerciseLayerButton();}

bool UCourseworkOverlay::ExerciseControlDrag(FVector2D Position,int32 Phase){return ControlMenu&&ControlMenu->ExerciseDrag(Position,Phase);}
