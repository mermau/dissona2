#!/bin/bash
# Installa DISSONA 2 (VST3, AU, Standalone). Richiede la password di amministratore.
cd "$(dirname "$0")"
set -e
echo "Installazione di DISSONA 2…"
sudo mkdir -p /Library/Audio/Plug-Ins/VST3 /Library/Audio/Plug-Ins/Components
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/DISSONA 2.vst3" "/Library/Audio/Plug-Ins/Components/DISSONA 2.component" "/Applications/DISSONA 2.app"
sudo cp -R "DISSONA 2.vst3" /Library/Audio/Plug-Ins/VST3/
sudo cp -R "DISSONA 2.component" /Library/Audio/Plug-Ins/Components/
sudo cp -R "DISSONA 2.app" /Applications/
sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/DISSONA 2.vst3" "/Library/Audio/Plug-Ins/Components/DISSONA 2.component" "/Applications/DISSONA 2.app" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Fatto. Riapri la DAW per vedere DISSONA 2 tra gli strumenti (Collateral Effects)."
