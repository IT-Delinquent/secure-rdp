#include "SplitterBar.h"

#include <algorithm>

namespace SplitterBar {

int ClampSplitX(int splitX, int clientWidth) {
    const int maxSplit = clientWidth - kMinPanelWidth - kWidth;
    const int minSplit = kMinTreeWidth;
    if (maxSplit < minSplit) {
        return std::max(0, clientWidth / 2);
    }
    return std::clamp(splitX, minSplit, maxSplit);
}

bool HitTest(int splitX, int x, int y, int clientHeight, int topPadding) {
    if (y < topPadding || y >= clientHeight) {
        return false;
    }
    return x >= splitX && x < splitX + kWidth;
}

}  // namespace SplitterBar
