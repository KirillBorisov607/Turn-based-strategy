"""The region map, redrawn: medallions, trails, the squad's token travelling, the squad panel, node descriptions.
The new widget code is in map_widget_block.cpp next to this script."""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
blk = open(os.path.join(HERE, 'map_widget_block.cpp'), encoding='utf-8').read()
# The shade over the island and the board are painted under the trails.
blk = blk.replace('''		// The island behind, darker at the edges.
		UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Shade->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.04f, 0.45f));
		Place(Shade, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f));
''', '''		// The shade over the island and the board are painted under the trails (NativePaint).
		BoardBrush = KeepersStyle::Panel(FLinearColor(0.03f, 0.04f, 0.05f, 0.42f), 22.f, FLinearColor(1.f, 0.78f, 0.3f, 0.35f), 2.f);
''')
blk = blk.replace('''		// The board.
		UBorder* Board = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Board->SetBrush(KeepersStyle::Panel(FLinearColor(0.03f, 0.04f, 0.05f, 0.42f), 22.f, FLinearColor(1.f, 0.78f, 0.3f, 0.35f), 2.f));
		Place(Board, FAnchors(KeepersMap::BoardMin.X, KeepersMap::BoardMin.Y, KeepersMap::BoardMax.X, KeepersMap::BoardMax.Y), FMargin(0.f));
		BoardCanvas''', '''		BoardCanvas''')
blk = blk.replace('''	const int32 TrailLayer = LayerId + 3;
	if (Campaign)
	{''', '''	const int32 TrailLayer = LayerId + 3;
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(), FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None, FLinearColor(0.01f, 0.02f, 0.04f, 0.45f));
	const FVector2D BoardTopLeft = KeepersMap::BoardMin * Size;
	FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry((KeepersMap::BoardMax - KeepersMap::BoardMin) * Size, FSlateLayoutTransform(BoardTopLeft)),
		&BoardBrush, ESlateDrawEffect::None, FLinearColor::White);
	if (Campaign)
	{''')
assert 'BoardBrush = KeepersStyle' in blk and 'MakeBox' in blk and 'UBorder* Board =' not in blk

os.chdir(r'C:/ProjectsUE/Turn-based-strategy/The_battle_code/Plugins/KeepersGame/Source/KeepersGame')


def load(p):
    s = open(p, 'rb').read().decode('utf-8-sig')
    return s.replace('\r\n', '\n'), '\r\n' in s


def save(p, s, crlf):
    open(p, 'w', encoding='utf-8', newline='\r\n' if crlf else '\n').write(s)


p = 'Private/KeepersCampaignUI.cpp'
s, crlf = load(p)
sep = '// ---------------------------------------------------------------------------------------------------------------------\n\n'
start = s.rindex(sep, 0, s.index('FVector2D UKeepersCampaignMapWidget::NodeAnchor(int32 NodeIndex) const'))
end = s.index('void UKeepersCampaignMapWidget::HandleMenuClicked()')
s = s[:start] + blk + '\n' + s[end:]
a = '''void UKeepersCampaignMapWidget::OnNodePicked(int32 NodeIndex)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (!Campaign || !Campaign->GetNodes().IsValidIndex(NodeIndex))
	{
		return;
	}'''
b = '''void UKeepersCampaignMapWidget::OnNodePicked(int32 NodeIndex)
{
	const UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (!Campaign || !Campaign->GetAvailableNodes().Contains(NodeIndex) || Travelling != INDEX_NONE)
	{
		return;
	}
	// The squad walks there first.
	Travelling = NodeIndex;
	TravelTime = 0.f;
	TravelFrom = TokenAnchor;
	TokenAnchor = NodeAnchor(NodeIndex);
	ShowTip(NodeIndex);
	UKeepersJuiceSubsystem::Sound(this, TEXT("Leaves"), 0.6f);
}

void UKeepersCampaignMapWidget::Arrive(int32 NodeIndex)
{
	UKeepersCampaignSubsystem* Campaign = UKeepersCampaignSubsystem::Get(this);
	if (!Campaign || !Campaign->GetNodes().IsValidIndex(NodeIndex))
	{
		return;
	}'''
assert s.count(a) == 1
s = s.replace(a, b)
for inc in ['#include "Components/WrapBox.h"\n', '#include "Components/Overlay.h"\n', '#include "Components/OverlaySlot.h"\n', '#include "Styling/CoreStyle.h"\n']:
    if inc not in s:
        s = s.replace('#include "Components/VerticalBox.h"\n', '#include "Components/VerticalBox.h"\n' + inc, 1)
save(p, s, crlf)

p = 'Public/KeepersCampaignUI.h'
s, crlf = load(p)
i = s.index('class KEEPERSGAME_API UKeepersCampaignMapWidget : public UUserWidget')
j = s.index('};', i) + 2
s = s[:i] + open(os.path.join(HERE, 'map_widget_header.h'), encoding='utf-8').read().strip() + s[j:]
save(p, s, crlf)
print('ok')
