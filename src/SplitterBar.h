#pragma once

namespace SplitterBar {

constexpr int kWidth = 5;
constexpr int kMinTreeWidth = 150;
constexpr int kMinPanelWidth = 200;

int ClampSplitX(int splitX, int clientWidth);
bool HitTest(int splitX, int x, int y, int clientHeight, int topPadding);

}  // namespace SplitterBar
