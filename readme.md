Domination Modification for Teeworlds
=====================================

*Domination* is a team-based modifcation for *Teeworlds*. You can find 
all relevant information about Teeworlds below. Domination introduces the new 
game element *domination spot*. Each map can have up to 5 of them.
Domination Spots are initially neutral, but can be captured by any team. 
Their effect on scoring varies based on the chosen gametype. You can find
all relevant information about the different gametypes below.

Currently it is developed by *Slayer* and *Fisico*. Originally it was 
written by *ziltoide* and *Oy*.

Please visit https://www.teeworlds.com/forum/viewtopic.php?id=3289 for 
up-to-date information about this mod, including new versions, custom 
maps and much more.

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software. See license.txt for full license
text including copyright information.

Gametype: Domination
--------------------
* capture the domination spots for your team by remaining in the capture area around the flags
* a domination spot has 3 states:
	- neutral: no teams gain points
	- red: red team gains points for this spot
	- blue: blue team gains points for this spot
* if multiple player of one team capture together, the capturing runs faster
* teamscore: every spot increases the teamscore over time
* player score: increases by killing an opponent tee (+1), capturing a spot (neutral, own team) and killing an opponent capturing player
* handicap: if a team is outnumbered, their players can capture faster
* spawning: players spawn near their own flags, otherwise on a random spawn point on the map

Gametype: Kill Domination
-------------------------
* capturing and player scoring work as in domination
* difference to domination:
  1) team gets score for killing opponent tees
     - team has 0 dom-spots: 0 points
     - team has 1 dom-spots: 1 points
     - team has 2 dom-spots: 2 points
     - team has 3 dom-spots: 3 points
     - team has 4 dom-spots: 4 points
     - team has 5 dom-spots: 5 points


Gametype: Conquest
-------------------------
* capturing and player scoring work as in domination
* your team starts with one spot (red: A, blue: E)
* you can only capture the next spot (red: B, blue: D)
* win the game by capturing and holding all spots
* teamscore displays the number of spots your team holds
* you spawn behind your last spot


Teeworlds [![CircleCI](https://circleci.com/gh/teeworlds/teeworlds.svg?style=svg)](https://circleci.com/gh/teeworlds/teeworlds)
=========

A retro multiplayer shooter
---------------------------

Teeworlds is a free online multiplayer game, available for all major
operating systems. Battle with up to 16 players in a variety of game
modes, including Team Deathmatch and Capture The Flag. You can even
design your own maps!

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software. See license.txt for full license
text including copyright information.

Please visit https://www.teeworlds.com/ for up-to-date information about
the game, including new versions, custom maps and much more.

Instructions to build the game can be found at 
https://teeworlds.com/?page=docs&wiki=compiling_everything. In
particular, you will need SDL2 and FreeType installed.

Originally written by Magnus Auvinen.
