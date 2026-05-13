#pragma once

bool shouldRenderPeriodicFrame(unsigned long nowMs, unsigned long lastUpdateMs, unsigned long intervalMs, bool forceRedraw);
int choosePortraitTextSplit(const char* text, int maxCols);
