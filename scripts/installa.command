#!/bin/bash
# Installa DS-78 (VST3, AU, Standalone). Richiede la password di amministratore.
cd "$(dirname "$0")"
set -e
echo "Installazione di DS-78…"
sudo mkdir -p /Library/Audio/Plug-Ins/VST3 /Library/Audio/Plug-Ins/Components
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/DS-78.vst3" "/Library/Audio/Plug-Ins/Components/DS-78.component" "/Applications/DS-78.app"
sudo cp -R "DS-78.vst3" /Library/Audio/Plug-Ins/VST3/
sudo cp -R "DS-78.component" /Library/Audio/Plug-Ins/Components/
sudo cp -R "DS-78.app" /Applications/
sudo xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/DS-78.vst3" "/Library/Audio/Plug-Ins/Components/DS-78.component" "/Applications/DS-78.app" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Fatto. Riapri la DAW per vedere DS-78 tra gli strumenti (Collateral Effects)."
