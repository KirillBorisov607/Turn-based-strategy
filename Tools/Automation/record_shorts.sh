#!/bin/bash
# Footage for the shorts: every hero's abilities on the squad screen, battles on many levels and objectives.
cd /c/ProjectsUE
export WARMUP=4
for H in Warrior Ranger Priest Mage Rogue Barbarian; do
  timeout 1500 _tools/record.sh /Game/Maps/L_MainMenu hero_$H 13 "1.5:squad;3:squadhero=$H,-1;5:squadhero=$H,0;9:squadhero=$H,1;13:squadhero=$H,2"
done
rec() { EXTRA="$3" timeout 2400 _tools/record.sh "$1" "$2" 45 "0.2:autoplay;0.2:follow;0.2:speed=2.5"; }
rec /Game/Maps/Combats/L_Combat_ChickenHunt b_chickens ""
rec /Game/Maps/Combats/L_Combat_FlyingIsCheating b_bats ""
rec /Game/Maps/Combats/L_Combat_TablesHaveTurned b_tables ""
rec /Game/Maps/Combats/L_Combat_FourPlayersChess b_chess ""
rec /Game/Maps/Slice/L_Slice_Chicken b_protect ""
rec /Game/Maps/Slice/L_Slice_Hex b_mercs "-KeepersObjective=leader"
rec /Game/Maps/Combats/L_Combat_SoSlimy b_slimes ""
EXTRA="" timeout 2400 _tools/record.sh /Game/Maps/Combats/L_Combat_TooManyEnemies b_orbit 40 "0.2:autoplay;0.2:orbit;0.2:speed=2.5"
echo ALLDONE
